/*-
 * Copyright (c) 2013 Adrian Chadd <adrian@freebsd.org>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer,
 *    without modification.
 * 2. Redistributions in binary form must reproduce at minimum a disclaimer
 *    similar to the "NO WARRANTY" disclaimer below ("Disclaimer") and any
 *    redistribution must be conditioned upon including a substantially
 *    similar Disclaimer requirement for further binary redistribution.
 *
 * NO WARRANTY
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF NONINFRINGEMENT, MERCHANTIBILITY
 * AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL
 * THE COPYRIGHT HOLDERS OR CONTRIBUTORS BE LIABLE FOR SPECIAL, EXEMPLARY,
 * OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER
 * IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGES.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

/*-
 * Framework for sending / receiving commands with the firmware.
 */
#include <sys/param.h>
#include <sys/sockio.h>
#include <sys/sysctl.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/mbuf.h>
#include <sys/kernel.h>
#include <sys/socket.h>
#include <sys/systm.h>
#include <sys/malloc.h>
#include <sys/module.h>
#include <sys/bus.h>
#include <sys/endian.h>
#include <sys/linker.h>
#include <sys/firmware.h>
#include <sys/kdb.h>

#include <machine/bus.h>
#include <machine/resource.h>
#include <sys/rman.h>

#include <net/bpf.h>
#include <net/if.h>
#include <net/if_arp.h>
#include <net/ethernet.h>
#include <net/if_dl.h>
#include <net/if_media.h>
#include <net/if_types.h>

#ifdef INET
#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/in_var.h>
#include <netinet/if_ether.h>
#include <netinet/ip.h>
#endif

#include <net80211/ieee80211_var.h>
#include <net80211/ieee80211_regdomain.h>
#include <net80211/ieee80211_radiotap.h>

#include <dev/usb/usb.h>
#include <dev/usb/usbdi.h>

#include "usbdevs.h"

#include "if_otusreg.h"
#include "if_otus_firmware.h"
#include "if_otus_cmd.h"
#include "if_otusvar.h"
#include "if_otus_debug.h"

#include "fwcmd.h"

void
otus_free_cmd_list(struct otus_softc *sc, struct otus_cmd cmds[], int ncmd)
{
	int i;

	for (i = 0; i < ncmd; i++)
		if (cmds[i].buf != NULL)
			free(cmds[i].buf, M_USBDEV);
}

int
otus_alloc_cmd_list(struct otus_softc *sc, struct otus_cmd cmds[],
	int ncmd, int maxsz)
{
	int i, error;

	STAILQ_INIT(&sc->sc_cmd_active);
	STAILQ_INIT(&sc->sc_cmd_pending);
	STAILQ_INIT(&sc->sc_cmd_waiting);
	STAILQ_INIT(&sc->sc_cmd_inactive);

	for (i = 0; i < ncmd; i++) {
		struct otus_cmd *cmd = &cmds[i];

		cmd->sc = sc;	/* backpointer for callbacks */
		cmd->msgid = i;
		cmd->buf = malloc(maxsz, M_USBDEV, M_NOWAIT);
		if (cmd->buf == NULL) {
			device_printf(sc->sc_dev,
			    "could not allocate xfer buffer\n");
			error = ENOMEM;
			goto fail;
		}
		STAILQ_INSERT_TAIL(&sc->sc_cmd_inactive, cmd, next);
		OTUS_STAT_INC(sc, st_cmd_inactive);
	}
	return (0);

fail:   otus_free_cmd_list(sc, cmds, ncmd);
	return (error);
}

void
otus_wakeup_waiting_list(struct otus_softc *sc)
{

	struct otus_cmd *c;

	OTUS_LOCK_ASSERT(sc);

	while ( (c = STAILQ_FIRST(&sc->sc_cmd_pending)) != NULL) {
		STAILQ_REMOVE_HEAD(&sc->sc_cmd_pending, next);
		OTUS_STAT_DEC(sc, st_cmd_pending);

		/* Wake up the sleepers */
		wakeup(c);

		STAILQ_INSERT_TAIL(&sc->sc_cmd_inactive, c, next);
		OTUS_STAT_INC(sc, st_cmd_inactive);
	}
}

struct otus_cmd *
otus_get_cmdbuf(struct otus_softc *sc)
{
	struct otus_cmd *uc;

	OTUS_LOCK_ASSERT(sc);

	uc = STAILQ_FIRST(&sc->sc_cmd_inactive);
	if (uc != NULL) {
		STAILQ_REMOVE_HEAD(&sc->sc_cmd_inactive, next);
		OTUS_STAT_DEC(sc, st_cmd_inactive);
	} else
		uc = NULL;
	if (uc == NULL)
		DPRINTF(sc, OTUS_DEBUG_CMDS, "%s: %s\n", __func__,
		    "out of command xmit buffers");
	return (uc);
}

/*
 * Prepare the next command buffer on the top of the
 * active stack for transmission to the target.
 *
 * Return 0 if no buffer was found, 1 if a buffer was
 * found and prepared, and < 0 on error.
 */
int
otus_comp_cmdbuf(struct otus_softc *sc)
{
	struct otus_cmd *cmd;

	OTUS_LOCK_ASSERT(sc);
	cmd = STAILQ_FIRST(&sc->sc_cmd_active);
	/* Nothing there? Fall through */
	if (cmd == NULL)
		return (0);
	STAILQ_REMOVE_HEAD(&sc->sc_cmd_active, next);
	OTUS_STAT_DEC(sc, st_cmd_active);
	STAILQ_INSERT_TAIL((cmd->flags & OTUS_CMD_FLAG_READ) ?
	    &sc->sc_cmd_waiting : &sc->sc_cmd_inactive, cmd, next);
	if (cmd->flags & OTUS_CMD_FLAG_READ)
		OTUS_STAT_INC(sc, st_cmd_waiting);
	else
		OTUS_STAT_INC(sc, st_cmd_inactive);
	return (1);
}

/*
 * Prepare the next command buffer to be pushed to the
 * USB driver.
 *
 * Returns the command in question to push to the hardware,
 * or NULL if nothing was found.
 */
struct otus_cmd *
otus_get_next_cmdbuf(struct otus_softc *sc)
{
	struct otus_cmd *cmd;

	cmd = STAILQ_FIRST(&sc->sc_cmd_pending);
	if (cmd == NULL) {
		DPRINTF(sc, OTUS_DEBUG_CMDS, "%s: empty pending queue\n",
		    __func__);
		return (NULL);
	}
	STAILQ_REMOVE_HEAD(&sc->sc_cmd_pending, next);
	OTUS_STAT_DEC(sc, st_cmd_pending);
	STAILQ_INSERT_TAIL((cmd->flags & OTUS_CMD_FLAG_ASYNC) ?
	    &sc->sc_cmd_inactive : &sc->sc_cmd_active, cmd, next);
	if (cmd->flags & OTUS_CMD_FLAG_ASYNC)
		OTUS_STAT_INC(sc, st_cmd_inactive);
	else
		OTUS_STAT_INC(sc, st_cmd_active);

	return (cmd);
}

#if 0
/*
 * Send a command to the firmware.
 */
int
otus_send_cmd(struct ar9170 *ar, struct carl9170_cmd *cmd)
{

	if (cmd->hdr.len > CARL9170_MAX_CMD_LEN - 4) {
		return (EINVAL);
	}

	/*
	 * Commands are sent on the CMD endpoint.
	 */

	usb_fill_int_urb(urb, ar->udev, usb_sndintpipe(ar->udev,
		AR9170_USB_EP_CMD), cmd, cmd->hdr.len + 4,
		carl9170_usb_cmd_complete, ar, 1);

	if (free_buf)
		urb->transfer_flags |= URB_FREE_BUFFER;

	usb_anchor_urb(urb, &ar->tx_cmd);
	usb_free_urb(urb);

	return carl9170_usb_submit_cmd_urb(ar);
}
#endif
