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
 * Driver for Atheros AR9170 based parts.
 *
 * For now this driver doesn't support 802.11n.
 * Supporting 802.11n transmit requires TX aggregation handling
 * to be done.
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

#include "if_otus_sysctl.h"

#include "fwcmd.h"
#include "hw.h"

/* unaligned little endian access */
#define LE_READ_2(p)							\
	((u_int16_t)							\
	 ((((u_int8_t *)(p))[0]      ) | (((u_int8_t *)(p))[1] <<  8)))
#define LE_READ_4(p)							\
	((u_int32_t)							\
	 ((((u_int8_t *)(p))[0]      ) | (((u_int8_t *)(p))[1] <<  8) |	\
	  (((u_int8_t *)(p))[2] << 16) | (((u_int8_t *)(p))[3] << 24)))

/* recognized device vendors/products */
static const STRUCT_USB_HOST_ID otus_devs[] = {
	/* TP-Link TL-WN821N v2 - XXX has WPS button and one LED */
	{ USB_VP(0x0cf3, 0x1002) },
};

static usb_callback_t	otus_bulk_tx_callback;
static usb_callback_t	otus_bulk_rx_callback;
static usb_callback_t	otus_bulk_irq_callback;
static usb_callback_t	otus_bulk_cmd_callback;
static int		otus_attach(device_t self);
static int		otus_detach(device_t self);

static const struct usb_config otus_config[OTUS_N_XFER] = {
	[OTUS_BULK_TX] = {
	.type = UE_BULK,
	.endpoint = UE_ADDR_ANY,
	.direction = UE_DIR_OUT,
	.bufsize = 0x200,
	.flags = {.pipe_bof = 1,.force_short_xfer = 1,},
	.callback = otus_bulk_tx_callback,
	.timeout = 5000,        /* ms */
	},
	[OTUS_BULK_RX] = {
	.type = UE_BULK,
	.endpoint = UE_ADDR_ANY,
	.direction = UE_DIR_IN,
	.bufsize = MCLBYTES,
	.flags = { .ext_buffer = 1, .pipe_bof = 1,.short_xfer_ok = 1,},
	.callback = otus_bulk_rx_callback,
	},
	[OTUS_BULK_IRQ] = {
	.type = UE_INTERRUPT,
	.endpoint = UE_ADDR_ANY,
	.direction = UE_DIR_IN,
	.bufsize = OTUS_MAX_CTRLSZ,
	.flags = {.pipe_bof = 1,.short_xfer_ok = 1,},
	.callback = otus_bulk_irq_callback,
	},
	[OTUS_BULK_CMD] = {
	.type = UE_INTERRUPT,
	.endpoint = UE_ADDR_ANY,
	.direction = UE_DIR_OUT,
	.bufsize = OTUS_MAX_CTRLSZ,
	.flags = {.pipe_bof = 1,.force_short_xfer = 1,},
	.callback = otus_bulk_cmd_callback,
	.timeout = 5000,        /* ms */
	},
};

static int
otus_match(device_t dev)
{
	struct usb_attach_arg *uaa = device_get_ivars(dev);

	if (uaa->usb_mode != USB_MODE_HOST)
		return (ENXIO);

	if (uaa->info.bConfigIndex != OTUS_CONFIG_INDEX)
		return (ENXIO);
	if (uaa->info.bIfaceIndex != OTUS_IFACE_INDEX)
		return (ENXIO);

	return (usbd_lookup_id_by_uaa(otus_devs, sizeof(otus_devs), uaa));
}

/*
 * Command send/receive/queue functions.
 *
 * These (partially) belong in if_otus_cmd.[ch], however until the
 * "right" abstraction is written up, this'll have to do.
 */
/*
 * Low-level function to send read or write commands to the firmware.
 */
static int
otus_cmdsend(struct otus_softc *sc, uint32_t code, const void *idata,
    int ilen, void *odata, int olen, int flags)
{
	struct carl9170_cmd_head *hdr;
	struct otus_cmd *cmd;
	int error;

	OTUS_LOCK_ASSERT(sc);

	/* grab a xfer */
	cmd = otus_get_cmdbuf(sc);
	if (cmd == NULL) {
		DPRINTF(sc, OTUS_DEBUG_CMDS,
		    "%s: empty inactive queue\n",
		    __func__);
		return (ENOBUFS);
	}

	/*
	 * carl9170 header fields:
	 * + len
	 * + cmd
	 * + seq
	 * + ext
	 *
	 * XXX the Linux headers use u8 fields which the compiler may decide
	 * to align.  We need to ensure this doesn't occur!
	 */
	hdr = (struct carl9170_cmd_head *)cmd->buf;
	bzero(hdr, sizeof (struct carl9170_cmd_head));	/* XXX not needed */
	hdr->h.c.cmd = code;
	hdr->h.c.len = ilen;
	hdr->h.c.seq = 0; /* XXX */
	hdr->h.c.ext = 0; /* XXX */

	/* Copy the payload, if needed */
	/* XXX TODO: check payload len? */
	bcopy(idata, (uint8_t *)(hdr + 1), ilen);

	/*
	 * Finalise buffer configuration.
	 */
	cmd->flags = flags;

	/* XXX should I ensure I always TX a multiple of 4 bytes? */
	/* XXX 4 == (sizeof(struct carl9170_cmd_head)) */
	cmd->buflen = ilen + 4;

#ifdef OTUS_DEBUG
	if (sc->sc_debug & OTUS_DEBUG_CMDS) {
	printf("%s: send  %d [flags 0x%x] olen %d\n",
	    __func__, code, cmd->flags, olen);
#if 0
	if (sc->sc_debug & OTUS_DEBUG_CMDS_DUMP)
		otus_dump_cmd(cmd->buf, cmd->buflen, '+');
	}
#endif
#endif

	cmd->odata = odata;
	KASSERT(odata == NULL ||
	    olen < OTUS_MAX_CMDSZ - sizeof(*hdr) + sizeof(uint32_t),
	    ("odata %p olen %u", odata, olen));
	 cmd->olen = olen;

	STAILQ_INSERT_TAIL(&sc->sc_cmd_pending, cmd, next);
	OTUS_STAT_INC(sc, st_cmd_pending);
	
	/* Kick off the actual USB command queue transaction */
	usbd_transfer_start(sc->sc_xfer[OTUS_BULK_CMD]);

	/*
	 * carl9170 posts responses on the RX queue (interspersed with
	 * data) and that's already running, so just add it to the
	 * queue.
	 */
	if (cmd->flags & OTUS_CMD_FLAG_READ) {
		usbd_transfer_start(sc->sc_xfer[OTUS_BULK_RX]);

		/*
		 * For now, let's not support synchronous command
		 * sending.
		 *
		 * I'll look into supporting this in a more generic
		 * fashion later.
		 */
                /* wait at most two seconds for command reply */
                error = mtx_sleep(cmd, &sc->sc_mtx, 0, "otuscmd", 2 * hz);
                cmd->odata = NULL;      /* in case reply comes too late */
                if (error != 0) {
                        device_printf(sc->sc_dev, "timeout waiting for reply "
                            "to cmd 0x%x (%u)\n", code, code);
                } else if (cmd->olen != olen) {
                        device_printf(sc->sc_dev, "unexpected reply data count "
                            "to cmd 0x%x (%u), got %u, expected %u\n",
                            code, code, cmd->olen, olen);
                        error = EINVAL;
                }
                return (error);
	}
	return (0);
}

static int
otus_cmd_read(struct otus_softc *sc, uint32_t code, const void *idata,
    int ilen, void *odata, int olen, int flags)
{

	flags |= OTUS_CMD_FLAG_READ;
	return otus_cmdsend(sc, code, idata, ilen, odata, olen, flags);
}

#if 0
static int
otus_cmd_write(struct otus_softc *sc, uint32_t code, const void *data,
    int len, int flags)
{

	flags &= ~OTUS_CMD_FLAG_READ;
	return otus_cmdsend(sc, code, data, len, NULL, 0, flags);
}
#endif

static int
otus_cmd_send_ping(struct otus_softc *sc, uint32_t value)
{
	uint32_t retval = 0xf0f0f0f0;
	int r;

	/*
	 * ECHO request / response is a READ request with the response
	 * coming back in the read response.  Nice and straightforward.
	 */
	r = otus_cmd_read(sc, CARL9170_CMD_ECHO, (void *) &value, 4,
	    (void *) &retval, 4, 0);

	if (r != 0) {
		device_printf(sc->sc_dev, "%s: cmd_send failed\n", __func__);
//		return (EIO);
		return (0);
	}

	if (retval != value) {
		device_printf(sc->sc_dev, "%s: PING: sent 0x%08x, got 0x%08x\n",
		    __func__,
		    value,
		    retval);
		
//		return (EIO);
		return (0);
	}

	device_printf(sc->sc_dev, "%s: PING: OK! sent 0x%08x, got 0x%08x\n",
	    __func__,
	    value,
	    retval);

	return (0);
}


static void
otus_cmd_dump(struct otus_softc *sc, struct otus_cmd *cmd)
{
	int i;

	device_printf(sc->sc_dev, "CMD:");
	for (i = 0; i < cmd->buflen; i++) {
		printf(" %02x", cmd->buf[i] & 0xff);
	}
	printf("\n");
}

#define	AR_FW_DOWNLOAD		0x30
#define	AR_FW_DOWNLOAD_COMPLETE	0x31

static int
otus_load_microcode(struct otus_softc *sc)
{
	usb_device_request_t req;
//	int error;
	const uint8_t *ptr;
	int addr;
	int size, mlen;
	char *buf;

	ptr = sc->fwinfo.fw->data;
	addr = sc->fwinfo.address;
	size = sc->fwinfo.fw->datasize;

	/*
	 * Skip the offset
	 */
	ptr += sc->fwinfo.offset;
	size -= sc->fwinfo.offset;

	buf = malloc(4096, M_TEMP, M_NOWAIT | M_ZERO);
	if (buf == NULL) {
		device_printf(sc->sc_dev, "%s: malloc failed\n",
		    __func__);
		return (ENOMEM);
	}

	OTUS_LOCK(sc);

	/*
	 * Initial firmware load.
	 */
	bzero(&req, sizeof(req));
	req.bmRequestType = UT_WRITE_VENDOR_DEVICE;
	req.bRequest = AR_FW_DOWNLOAD;
	USETW(req.wIndex, 0);

	addr >>= 8;
	while (size > 0) {
		mlen = MIN(size, 4096);
		memcpy(buf, ptr, mlen);

		USETW(req.wValue, addr);
		USETW(req.wLength, mlen);
		device_printf(sc->sc_dev, "%s: loading %d bytes at 0x%08x\n",
		    __func__,
		    mlen,
		    addr);
		if (usbd_do_request(sc->sc_udev, &sc->sc_mtx, &req, buf)) {
			device_printf(sc->sc_dev,
			    "%s: failed to write firmware\n", __func__);
			OTUS_UNLOCK(sc);
			free(buf, M_TEMP);
			return (EIO);
		}
		addr += mlen >> 8;
		ptr  += mlen;
		size -= mlen;
	}

	/*
	 * Firmware complete.
	 */
	bzero(&req, sizeof(req));
	req.bmRequestType = UT_WRITE_VENDOR_DEVICE;
	req.bRequest = AR_FW_DOWNLOAD_COMPLETE;
	USETW(req.wValue, 0);
	USETW(req.wIndex, 0);
	USETW(req.wLength, 0);
	if (usbd_do_request(sc->sc_udev, &sc->sc_mtx, &req, NULL)) {
		device_printf(sc->sc_dev,
		    "%s: firmware initialization failed\n",
		    __func__);
		OTUS_UNLOCK(sc);
		free(buf, M_TEMP);
		return (EIO);
	}

	OTUS_UNLOCK(sc);
	free(buf, M_TEMP);

	/*
	 * It's up to the caller to check whether we were successful.
	 */
	return (0);
}

static void
if_otus_ping_callout(void *arg)
{
	struct otus_softc *sc = arg;

	/* Detaching? Don't bother running. */
	if (sc->sc_detaching) {
		return;
	}

	taskqueue_enqueue(sc->sc_tq, &sc->sc_ping_task);
}

static void
otus_ping_task(void *arg, int npending)
{
	struct otus_softc *sc = arg;

	OTUS_LOCK(sc);

	/* Detaching? Don't bother running. */
	if (sc->sc_detaching) {
		OTUS_UNLOCK(sc);
		return;
	}

	(void) otus_cmd_send_ping(sc, 0x89abcdef);
	callout_reset(&sc->sc_ping_callout, 500, if_otus_ping_callout, sc);
	OTUS_UNLOCK(sc);
}

static int
otus_attach(device_t dev)
{
	struct otus_softc *sc = device_get_softc(dev);
	struct usb_attach_arg *uaa = device_get_ivars(dev);
	uint8_t iface_index;
	int error;

	device_printf(dev, "%s: called\n", __func__);

	device_set_usb_desc(dev);
	sc->sc_udev = uaa->device;
	sc->sc_dev = dev;

	mtx_init(&sc->sc_mtx, device_get_nameunit(sc->sc_dev),
	    MTX_NETWORK_LOCK, MTX_DEF);

	/*
	 * Allocate xfers for firmware commands.
	 */
	error = otus_alloc_cmd_list(sc, sc->sc_cmd, OTUS_CMD_LIST_COUNT,
	    OTUS_MAX_CMDSZ);
	if (error != 0) {
		device_printf(sc->sc_dev,
		    "could not allocate Tx command list\n");
		goto detach;
	}

	/*
	 * Setup USB transfer pipes.
	 */
	iface_index = OTUS_IFACE_INDEX;
	error = usbd_transfer_setup(uaa->device, &iface_index,
		sc->sc_xfer, otus_config, OTUS_N_XFER, sc, &sc->sc_mtx);
	if (error) {
		device_printf(dev, "could not allocate USB transfers, "
		    "err=%s\n", usbd_errstr(error));
		goto detach;
	}

	/*
	 * Attach sysctl nodes.
	 */
	if_otus_sysctl_attach(sc);

	sc->sc_debug = 0xffffffffUL;

	/*
	 * XXX Can we do a detach at any point? Ie, can we defer the
	 * XXX rest of this setup path and if attach fails, just
	 * XXX call detach from some taskqueue/callout?
	 */

	/*
	 * Load in the firmware, so we know the firmware config and
	 * capabilities.
	 */
	error = otus_firmware_load(&sc->fwinfo);
	if (error != 0)
		goto detach;

	/*
	 * Squeeze in firmware at attach phase for now.
	 */
	error = otus_load_microcode(sc);
	if (error != 0)
		goto detach;

	/* Taskqueue */
	sc->sc_tq = taskqueue_create("otus_taskq", M_NOWAIT,
	    taskqueue_thread_enqueue, &sc->sc_tq);
	/* XXX should be using ifp for the taskq name */
	taskqueue_start_threads(&sc->sc_tq, 1, PI_NET, "%s taskq", "otus");
	TASK_INIT(&sc->sc_ping_task, 0, otus_ping_task, sc);

	/* Periodic ping */
	callout_init_mtx(&sc->sc_ping_callout, &sc->sc_mtx, 0);

	OTUS_LOCK(sc);


	/*
	 * Start off the receive paths.
	 */
	usbd_transfer_start(sc->sc_xfer[OTUS_BULK_RX]);
	usbd_transfer_start(sc->sc_xfer[OTUS_BULK_IRQ]);

	/* XXX doing a ping here is likely evil (we're in probe/attach!) .. */
	(void) otus_cmd_send_ping(sc, 0x89abcdef);

	/* And whilst we're here .. */
	callout_reset(&sc->sc_ping_callout, 1000, if_otus_ping_callout, sc);
	OTUS_UNLOCK(sc);

	/* XXX read eeprom, device setup, etc */

	/* XXX setup ifp */

	/* XXX setup net80211 */

	return (0);

detach:
	otus_detach(dev);
	return (ENXIO);
}

static int
otus_detach(device_t dev)
{
	struct otus_softc *sc = device_get_softc(dev);

	/*
	 * Time to tear things down, so we should now signal
	 * any parallel-running things (like ioctls) that they
	 * should not start running, then wake up any of the
	 * things waiting for commands.
	 */
	OTUS_LOCK(sc);
	sc->sc_detaching = 1;

	/*
	 * Wake up any commands that are currently pending;
	 * move them to the inactive list so they're freed.
	 *
	 * XXX TODO!
	 */
	otus_wakeup_waiting_list(sc);

	/*
	 * Now, drain the callouts so they don't occur any longer.
	 */
	callout_drain(&sc->sc_ping_callout);

	/*
	 * .. the taskqueue stuff has to happen outside of the
	 * otus lock as it's not recursive.
	 */
	OTUS_UNLOCK(sc);

	/*
	 * Tear down the USB transfer endpoints.
	 */
	if (sc->sc_xfer != NULL)
		usbd_transfer_unsetup(sc->sc_xfer, OTUS_N_XFER);

	/*
	 * Drain the taskqueue.  If the tasks are running then we'll
	 * end up waiting until they complete.
	 */
	taskqueue_drain(sc->sc_tq, &sc->sc_ping_task);
	/*
	 * Free the taskqueue.
	 */
	taskqueue_free(sc->sc_tq);

	/* XXX net80211 tasks? */

	/*
	 * Next, we need to ensure that any pending TX/RX, ioctls,
	 * net80211 callbacks and such are properly wrapped up.
	 *
	 * XXX TODO!
	 */

	/*
	 * Ok, so now we re-acquire the lock.  The taskqueues,
	 * callouts, TX/RX processing, net80211 callbacks,
	 * various other bits and pieces should all (a) be
	 * done or (b) hit the lock, seen we're detaching
	 * and just quit out. So we can finish wrapping up
	 * the driver now.
	 */

	OTUS_LOCK(sc);

	/*
	 * Free command list.
	 */
	otus_free_cmd_list(sc, sc->sc_cmd, OTUS_CMD_LIST_COUNT);

	/*
	 * Free staging RX mbuf.
	 */
	if (sc->sc_rx_m != NULL)
		m_freem(sc->sc_rx_m);
	OTUS_UNLOCK(sc);

	/*
	 * Free the firmware state.
	 */
	otus_firmware_cleanup(&sc->fwinfo);

	device_printf(dev, "%s: called\n", __func__);
	mtx_destroy(&sc->sc_mtx);
	return (0);
}

/*
 * Receive dispatch routines.
 */


/*
 * Process a single receive frame.
 *
 * XXX I really would like to be able to pass in a real mbuf here
 * and seek into it; however due to how the carl9170 firmware works
 * it isn't easy to do.
 *
 * So, for now, create a new mbuf based on this particular extent
 * and worry about optimising things later.
 */
static void
otus_rx_data(struct otus_softc *sc, const uint8_t *buf, int len)
{

	/* XXX TODO */
}

/*
 * Check whether the current sequence number is the expected;
 * update local state.
 */
static int
otus_check_sequence(struct otus_softc *sc, uint8_t seq)
{
	OTUS_LOCK_ASSERT(sc);

	/* XXX for now, just return true */
	return (0);
}

/*
 * Handle a single command response.
 *
 * If a command response was due to a queued command
 * that we have a sleeping requestor on, ensure that
 * we copy in the response and then wake up the caller.
 */
static void
otus_handle_cmd_response(struct otus_softc *sc,
    const struct carl9170_rsp *cmd)
{
	OTUS_LOCK_ASSERT(sc);
	struct carl9170_cmd *hdr;
	struct otus_cmd *c;

	DPRINTF(sc, OTUS_DEBUG_RECV_STREAM,
	    "%s: code=%d, len=%d\n",
	    __func__,
	    cmd->hdr.h.c.cmd,
	    cmd->hdr.h.c.len);

	/*
	 * If the RSP flags don't match, treat the message
	 * as an announcement.  Punt it elsewhere.
	 */

	/*
	 * XXX For now, just ignore handling non-response
	 * messages
	 */
	if (cmd->hdr.h.c.cmd & CARL9170_RSP_FLAG) {
		device_printf(sc->sc_dev,
		    "%s: ignoring async message! (0x%x)\n",
		    __func__,
		    cmd->hdr.h.c.cmd);
		return;
	}

	/*
	 * XXX put in if_otus_cmd.[ch] !
	 */
	c = STAILQ_FIRST(&sc->sc_cmd_waiting);
	if (c == NULL) {
		device_printf(sc->sc_dev,
		    "%s: response but with no buffer waiting!\n",
		    __func__);
		return;
	}

	/*
	 * Pop it.
	 */
	STAILQ_REMOVE_HEAD(&sc->sc_cmd_waiting, next);
	OTUS_STAT_DEC(sc, sc_cmd_waiting);
	hdr = (struct carl9170_cmd *) c->buf;

	/*
	 * Debug.
	 */
	device_printf(sc->sc_dev,
	    "%s: resp cmd=%d, len=%d, seq=%d; req cmd=%d, len=%d, seq=%d\n",
	    __func__,
	    cmd->hdr.h.c.cmd,
	    cmd->hdr.h.c.len,
	    cmd->hdr.h.c.seq,
	    hdr->hdr.h.c.cmd,
	    hdr->hdr.h.c.len,
	    hdr->hdr.h.c.seq);

	/*
	 * Sanity check - command and sequence numbers need to match.
	 */
	/* XXX check for dropped sequence number (sc_cur_rxcmdseq) */

	/* XXX Check that the response code matches */
	if (cmd->hdr.h.c.cmd != hdr->hdr.h.c.cmd) {
		device_printf(sc->sc_dev,
		    "%s: mis-matched commands! (%d != %d)\n",
		    __func__,
		    cmd->hdr.h.c.cmd,
		    hdr->hdr.h.c.cmd);
	} else if (c->odata != NULL) {
		/* Matched? Copy the payload */
		/* XXX flag if the received payload is too big? */
		memcpy(c->odata, &hdr->c.data,
		    MIN(hdr->hdr.h.c.len, c->olen));
	}

	/* Wakeup the listener */
	wakeup_one(c);

	/* Return to the inactive queue */
	STAILQ_INSERT_TAIL(&sc->sc_cmd_inactive, c, next);
	OTUS_STAT_INC(sc, sc_cmd_inactive);
}

/*
 * Loop over the RX command in this payload and handle each one.
 *
 * Some will be respones to queued commands and those must be
 * matched up against pending commands.
 */
static void
otus_rx_cmds(struct otus_softc *sc, const uint8_t *buf, uint8_t len)
{
	const struct carl9170_rsp *cmd;
	int i = 0;

	OTUS_LOCK_ASSERT(sc);

	while (i < len) {
		cmd = (const struct carl9170_rsp *) &buf[i];

		i += cmd->hdr.h.c.len + 4;
		if (i > len)
			break;

		if (otus_check_sequence(sc, cmd->hdr.h.c.seq))
			break;

		otus_handle_cmd_response(sc, cmd);
	}

	/*
	 * If we parsed everything - great. Otherwise log an error.
	 */
	if (i == len)
		return;

	/*
	 * XXX Should pass an error back up the stack!
	 */
	device_printf(sc->sc_dev,
	    "%s: %d bytes left whilst decoding responses!\n",
	    __func__,
	    len - i);
}

/*
 * XXX from carl9170, but explained!
 */
static void
otus_rx_dispatch(struct otus_softc *sc, const uint8_t *buf, int len)
{
	int i = 0;

	/*
	 * Check if the PLCP header is all 0xff's - this says its
	 * a command.
	 */
	while (len > 2 && i < 12 && buf[0] == 0xff && buf[1] == 0xff) {
		i += 2;
		len -= 2;
		buf += 2;
	}

	/*
	 * If the resulting payload isn't long enough for at least
	 * one command response, don't continue.
	 *
	 * XXX should log / print something!
	 */
	if (len < 4)
		return;

	/*
	 * PLCP?
	 */
	if (i == 12)
		otus_rx_cmds(sc, buf, len);
	else
		otus_rx_data(sc, buf, len);
}

static void
otus_dump_usb_rx_page(struct otus_softc *sc, const char *buf, int actlen)
{
	int i;

	device_printf(sc->sc_dev, "USBRX:");
	for (i = 0; i < actlen; i++) {
		printf(" %02x", buf[i] & 0xff);
	}
	printf("\n");
}

/*
 * The later AR9170 firmware releases support a "RX stream" mode, where
 * multiple receive frames are pushed into the same USB transaction.
 * So we need to (a) pull those apart, and (b) incomplete frames
 * need to be joined to the next RX transaction.
 */
static void
otus_rx_frame(struct otus_softc *sc, struct mbuf *m, int len)
{
	const char *buf;
	const char *tbuf;
	int tlen, wlen, clen;
	const struct ar9170_stream *rx_stream;

	buf = mtod(m, const char *);
	otus_dump_usb_rx_page(sc, buf, len);

	tbuf = buf;
	tlen = len;

	while (tlen >= 4) {
		rx_stream = (const struct ar9170_stream *) tbuf;
		/* Grab the frame length and pad it to a DWORD boundary */
		clen = le16toh(rx_stream->length);
		wlen = roundup2(clen, 4);

		/*
		 * Check whether there's a header - if it isn't a valid
		 * header, we may need to stitch this together with the
		 * next frame.
		 */
		if (le16toh(rx_stream->tag) != AR9170_RX_STREAM_TAG) {
			device_printf(sc->sc_dev, "%s: tag=%d (not %d)\n",
			    __func__,
			    le16toh(rx_stream->tag),
			    AR9170_RX_STREAM_TAG);
			break;
		}

		/*
		 * See what the length is.  If it's bigger than this buffer,
		 * we may need to stitch this with a future frame.
		 * But for now we'll just complain loudly whilst I debug this.
		 */
		if (wlen > tlen) {
			device_printf(sc->sc_dev,
			    "%s: payload too big (clen=%d, wlan=%d > tlen=%d)\n",
			    __func__,
			    clen,
			    wlen,
			    tlen);
			break;
		}

		/*
		 * Punt it up.
		 */
		DPRINTF(sc, OTUS_DEBUG_RECV_STREAM,
		    "%s: valid tag! clen=%d, wlen=%d\n",
		    __func__,
		    clen,
		    wlen);
		otus_rx_dispatch(sc, tbuf + 4, clen);

		/*
		 * Move to next frame, skipping the DWORD stream header
		 */
		tbuf += wlen + 4;
		tlen -= wlen + 4;
	}

	/* XXX what's left in the buffer? */

	/* Consume mbuf */
	m_freem(m);
}

/*
 * USB data pipeline completion routines.
 */

static void
otus_bulk_tx_callback(struct usb_xfer *xfer, usb_error_t error)
{
	struct otus_softc *sc = usbd_xfer_softc(xfer);
	int actlen;
	int sumlen;

	usbd_xfer_status(xfer, &actlen, &sumlen, NULL, NULL);
	DPRINTF(sc, OTUS_DEBUG_USB_XFER,
	    "%s: called; state=%d\n",
	    __func__,
	    USB_GET_STATE(xfer));

	switch (USB_GET_STATE(xfer)) {
	case USB_ST_SETUP:
		/*
		 * Setup xfer frame lengths/count and data
		 */
		usbd_transfer_submit(xfer);
	break;

	case USB_ST_TRANSFERRED:
		/*
		 * Read usb frame data, if any.
		 * "actlen" has the total length for all frames
		 * transferred.
		 */
		break;
	default: /* Error */
		/*
		 * Print error message and clear stall
		 * for example.
		 */
		break;
	}
}

static void
otus_bulk_rx_callback(struct usb_xfer *xfer, usb_error_t error)
{
	struct otus_softc *sc = usbd_xfer_softc(xfer);
	int actlen;
	int sumlen;
	struct mbuf *m;

	usbd_xfer_status(xfer, &actlen, &sumlen, NULL, NULL);
	DPRINTF(sc, OTUS_DEBUG_USB_XFER,
	    "%s: called; state=%d\n",
	    __func__,
	    USB_GET_STATE(xfer));

	switch (USB_GET_STATE(xfer)) {

	case USB_ST_TRANSFERRED:
		/*
		 * Read usb frame data, if any.
		 * "actlen" has the total length for all frames
		 * transferred.
		 */
		DPRINTF(sc, OTUS_DEBUG_USB_XFER,
		    "%s: comp; %d bytes\n",
		    __func__,
		    actlen);

		m = sc->sc_rx_m;
		sc->sc_rx_m = NULL;

		/*
		 * This decodes the RX stream and figures out which
		 * frames are commands, which are responses and which
		 * are RX frames.
		 */
		otus_rx_frame(sc, m, actlen);

		/* XXX fallthrough */

	case USB_ST_SETUP:
		/*
		 * Setup xfer frame lengths/count and data
		 */
		DPRINTF(sc, OTUS_DEBUG_USB_XFER,
		    "%s: setup\n",
		    __func__);

		/*
		 * If there's already an mbuf, use it.
		 *
		 * XXX temporary! I need to correctly populate
		 * RX mbuf chains elsewhere!
		 */
		if (sc->sc_rx_m == NULL) {
			sc->sc_rx_m = m_getcl(M_DONTWAIT, MT_DATA, M_PKTHDR);
			if (sc->sc_rx_m == NULL)
				break;
		}

		usbd_xfer_set_frame_data(xfer, 0,
		    mtod(sc->sc_rx_m, uint8_t *),
		    usbd_xfer_max_len(xfer));

		usbd_transfer_submit(xfer);
		break;

	default: /* Error */
		/*
		 * Print error message and clear stall
		 * for example.
		 */
		device_printf(sc->sc_dev, "%s: ERROR?\n", __func__);
		break;
	}
}

static void
otus_bulk_cmd_callback(struct usb_xfer *xfer, usb_error_t error)
{
	struct otus_softc *sc = usbd_xfer_softc(xfer);
	struct otus_cmd *cmd;
	int actlen;
	int sumlen;

	usbd_xfer_status(xfer, &actlen, &sumlen, NULL, NULL);
	DPRINTF(sc, OTUS_DEBUG_USB_XFER,
	    "%s: called; state=%d\n",
	    __func__,
	    USB_GET_STATE(xfer));

	switch (USB_GET_STATE(xfer)) {

	case USB_ST_TRANSFERRED:
		/*
		 * Read usb frame data, if any.
		 * "actlen" has the total length for all frames
		 * transferred.
		 */

		/*
		 * Complete a frame.
		 */
		DPRINTF(sc, OTUS_DEBUG_USB_XFER, "%s: comp\n", __func__);
		(void) otus_comp_cmdbuf(sc);

		/* XXX FALLTHROUGH */

	case USB_ST_SETUP:
		/*
		 * Setup xfer frame lengths/count and data
		 */

		/*
		 * If we have anything next to queue, do so.
		 */
		cmd = otus_get_next_cmdbuf(sc);
		if (cmd != NULL) {
			DPRINTF(sc, OTUS_DEBUG_USB_XFER,
			    "%s: setup - %d bytes\n",
			    __func__,
			    cmd->buflen);
			if (DODEBUG(sc, OTUS_DEBUG_CMDS_DUMP))
				otus_cmd_dump(sc, cmd);
			usbd_xfer_set_frame_data(xfer, 0, cmd->buf,
			    cmd->buflen);
			usbd_transfer_submit(xfer);
		}
		break;

	default: /* Error */
		/*
		 * Print error message and clear stall
		 * for example.
		 */
		break;
	}
}


/*
 * This isn't used by carl9170; it however may be used by the
 * initial bootloader.
 */
static void
otus_bulk_irq_callback(struct usb_xfer *xfer, usb_error_t error)
{
	struct otus_softc *sc = usbd_xfer_softc(xfer);
	int actlen;
	int sumlen;

	usbd_xfer_status(xfer, &actlen, &sumlen, NULL, NULL);
	DPRINTF(sc, OTUS_DEBUG_USB_XFER,
	    "%s: called; state=%d\n",
	    __func__,
	    USB_GET_STATE(xfer));

	switch (USB_GET_STATE(xfer)) {
	case USB_ST_TRANSFERRED:
		/*
		 * Read usb frame data, if any.
		 * "actlen" has the total length for all frames
		 * transferred.
		 */
		DPRINTF(sc, OTUS_DEBUG_USB_XFER,
		    "%s: comp; %d bytes\n",
		    __func__,
		    actlen);
#if 0
		pc = usbd_xfer_get_frame(xfer, 0);
		otus_dump_usb_rx_page(sc, pc, actlen);
#endif
		/* XXX fallthrough */
	case USB_ST_SETUP:
		/*
		 * Setup xfer frame lengths/count and data
		 */
		DPRINTF(sc, OTUS_DEBUG_USB_XFER,
		    "%s: setup\n", __func__);
		usbd_xfer_set_frame_len(xfer, 0, usbd_xfer_max_len(xfer));
		usbd_transfer_submit(xfer);
		break;

	default: /* Error */
		/*
		 * Print error message and clear stall
		 * for example.
		 */
		device_printf(sc->sc_dev, "%s: ERROR?\n", __func__);
		break;
	}
}

static device_method_t otus_methods[] = {
	DEVMETHOD(device_probe, otus_match),
	DEVMETHOD(device_attach, otus_attach),
	DEVMETHOD(device_detach, otus_detach),
	{ 0, 0 }
};
static driver_t otus_driver = {
	.name = "otus",
	.methods = otus_methods,
	.size = sizeof(struct otus_softc)
};
static devclass_t otus_devclass;

DRIVER_MODULE(otus, uhub, otus_driver, otus_devclass, NULL, 0);
MODULE_DEPEND(otus, wlan, 1, 1, 1);
MODULE_DEPEND(otus, usb, 1, 1, 1);
MODULE_VERSION(otus, 1);
