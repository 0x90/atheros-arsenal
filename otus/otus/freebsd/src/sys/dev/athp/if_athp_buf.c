/*-
 * Copyright (c) 2015 Adrian Chadd <adrian@FreeBSD.org>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include "opt_wlan.h"

#include <sys/param.h>
#include <sys/proc.h>
#include <sys/condvar.h>
#include <sys/endian.h>
#include <sys/sockio.h>
#include <sys/mbuf.h>
#include <sys/kernel.h>
#include <sys/socket.h>
#include <sys/systm.h>
#include <sys/conf.h>
#include <sys/bus.h>
#include <sys/rman.h>
#include <sys/firmware.h>
#include <sys/module.h>
#include <sys/taskqueue.h>

#include <machine/bus.h>
#include <machine/resource.h>

#include <net/bpf.h>
#include <net/if.h>
#include <net/if_var.h>
#include <net/if_arp.h>
#include <net/if_dl.h>
#include <net/if_media.h>
#include <net/if_types.h>

#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/in_var.h>
#include <netinet/if_ether.h>
#include <netinet/ip.h>

#include <net80211/ieee80211_var.h>
#include <net80211/ieee80211_regdomain.h>
#include <net80211/ieee80211_radiotap.h>
#include <net80211/ieee80211_ratectl.h>
#include <net80211/ieee80211_input.h>
#ifdef	IEEE80211_SUPPORT_SUPERG
#include <net80211/ieee80211_superg.h>
#endif

#include "hal/linux_compat.h"
#include "hal/targaddrs.h"
#include "hal/htc.h"
#include "hal/hw.h"
#include "hal/wmi.h"

#include "if_athp_debug.h"
#include "if_athp_regio.h"
#include "if_athp_stats.h"
#include "if_athp_wmi.h"
#include "if_athp_core.h"
#include "if_athp_htc.h"
#include "if_athp_desc.h"
#include "if_athp_var.h"
#include "if_athp_hif.h"
#include "if_athp_bmi.h"

#include "if_athp_buf.h"

/*
 * This is a simpleish implementation of mbuf + local state buffers.
 * It's intended to be used for basic bring-up where we have some mbufs
 * things we'd like to send/receive over the copy engine paths.
 *
 * Later on it'll grow to include tx/rx using scatter/gather DMA, etc.
 */

/*
 * XXX TODO: let's move the buffer state itself into a struct that gets
 * included by if_athp_var.h into athp_softc so we don't need the whole
 * driver worth of includes for what should just be pointers to things.
 */

MALLOC_DECLARE(M_ATHPDEV);

/*
 * Driver buffers!
 */

/*
 * Unmap a buffer.
 *
 * This unloads the DMA map for the given buffer.
 *
 * It's used in the buffer free path and in the buffer "it's mine now!"
 * claiming path when the driver wants the mbuf for itself.
 */
static void
athp_unmap_buf(struct ath10k *ar, struct athp_buf_ring *br,
    struct athp_buf *bf)
{

	/* no mbuf? skip */
	if (bf->m == NULL)
		return;

	athp_dma_mbuf_unload(ar, &br->dh, &bf->mb);
}

/*
 * Free an individual buffer.
 *
 * This doesn't update the linked list state; it just handles freeing it.
 */
static void
_athp_free_buf(struct ath10k *ar, struct athp_buf_ring *br,
    struct athp_buf *bf)
{

	ATHP_BUF_LOCK_ASSERT(ar);

	/* If there's an mbuf, then unmap, and free */
	if (bf->m != NULL) {
		athp_unmap_buf(ar, br, bf);
		m_freem(bf->m);
	}
}

/*
 * Free all buffers in the rx ring.
 *
 * This should only be called during driver teardown; it will unmap/free each
 * mbuf without worrying about the linked list / allocation state.
 */
void
athp_free_list(struct ath10k *ar, struct athp_buf_ring *br)
{
	int i;

	ATHP_BUF_LOCK(ar);

	/* prevent further allocations from RX list(s) */
	TAILQ_INIT(&br->br_inactive);

	for (i = 0; i < br->br_count; i++) {
		struct athp_buf *dp = &br->br_list[i];
		_athp_free_buf(ar, br, dp);
	}

	ATHP_BUF_UNLOCK(ar);

	free(br->br_list, M_ATHPDEV);
	br->br_list = NULL;
}

/*
 * Setup the driver side of the list allocations and insert them
 * all into the inactive list.
 */
int
athp_alloc_list(struct ath10k *ar, struct athp_buf_ring *br, int count)
{
	int i;

	/* Allocate initial buffer list */
	br->br_list = malloc(sizeof(struct athp_buf) * count, M_ATHPDEV,
	    M_ZERO | M_NOWAIT);
	if (br->br_list == NULL) {
		ath10k_err(ar, "%s: malloc failed!\n", __func__);
		return (-1);
	}

	/* Setup initial state for each entry */
	/* XXX it's all zero, so we're okay for now */

	/* Lists */
	TAILQ_INIT(&br->br_inactive);

	for (i = 0; i < count; i++)
		TAILQ_INSERT_HEAD(&br->br_inactive, &br->br_list[i], next);

	return (0);
}

/*
 * Return an RX buffer.
 *
 * This doesn't allocate the mbuf.
 */
static struct athp_buf *
_athp_getbuf(struct ath10k *ar, struct athp_buf_ring *br)
{
	struct athp_buf *bf;

	ATHP_BUF_LOCK_ASSERT(ar);

	/* Allocate a buffer */
	bf = TAILQ_FIRST(&br->br_inactive);
	if (bf != NULL)
		TAILQ_REMOVE(&br->br_inactive, bf, next);
	else
		bf = NULL;
	return (bf);
}

void
athp_freebuf(struct ath10k *ar, struct athp_buf_ring *br,
    struct athp_buf *bf)
{

	ATHP_BUF_LOCK(ar);

	/* if there's an mbuf - unmap (if needed) and free it */
	if (bf->m != NULL)
		_athp_free_buf(ar, br, bf);

	/* Push it into the inactive queue */
	TAILQ_INSERT_TAIL(&br->br_inactive, bf, next);
	ATHP_BUF_UNLOCK(ar);

}

/*
 * Return an buffer with an mbuf allocated.
 *
 * Note: the mbuf length is just that - the mbuf length.
 * It's up to the caller to reserve the required header/descriptor
 * bits before the actual payload.
 *
 * XXX TODO: need to pass in a dmatag to use, rather than a global
 * XXX TX/RX tag.  Check ath10k_pci_alloc_pipes() - each pipe has
 * XXX a different dmatag with different properties.
 *
 * Note: this doesn't load anything; that's done by the caller
 * before it passes it into the hardware.
 */
struct athp_buf *
athp_getbuf(struct ath10k *ar, struct athp_buf_ring *br, int bufsize)
{
	struct athp_buf *bf;
	struct mbuf *m;

	/* Allocate mbuf; fail if we can't allocate one */
	m = m_getjcl(M_NOWAIT, MT_DATA, M_PKTHDR, bufsize);
	if (m == NULL) {
		device_printf(ar->sc_dev, "%s: failed to allocate mbuf\n", __func__);
		return (NULL);
	}

	/* Allocate buffer */
	ATHP_BUF_LOCK(ar);
	bf = _athp_getbuf(ar, br);
	ATHP_BUF_UNLOCK(ar);
	if (! bf) {
		m_freem(m);
		return (NULL);
	}

	/* Setup initial mbuf tracking state */
	bf->m_size = bufsize;

	return (bf);
}

struct athp_buf *
athp_getbuf_tx(struct ath10k *ar, struct athp_buf_ring *br)
{
	struct athp_buf *bf;

	ATHP_BUF_LOCK(ar);
	bf = _athp_getbuf(ar, br);
	ATHP_BUF_UNLOCK(ar);
	if (bf == NULL)
		return NULL;

	/* No mbuf yet! */
	bf->m_size = 0;

	return bf;
}

/*
 * XXX TODO: write a routine to assign a pbuf to a given mbuf or
 * something, for the transmit side to have everything it needs
 * to transmit a payload, complete with correct 'len'.
 */

/*
 * XXX TODO: need to setup the tx/rx buffer dma tags in if_athp_pci.c.
 * (Since it's a function of the bus/chip..)
 */

void
athp_buf_cb_clear(struct athp_buf *bf)
{

	bzero(&bf->tx, sizeof(bf->tx));
	bzero(&bf->rx, sizeof(bf->rx));
}

void
athp_buf_set_len(struct athp_buf *bf, int len)
{
	if (bf->m == NULL) {
		printf("%s: called on NULL mbuf!\n", __func__);
		return;
	}
	bf->m->m_len = len;
	bf->m->m_pkthdr.len = len;
}

void
athp_buf_list_flush(struct ath10k *ar, struct athp_buf_ring *br,
    athp_buf_head *bl)
{
	struct athp_buf *pbuf, *pbuf_next;

	TAILQ_FOREACH_SAFE(pbuf, bl, next, pbuf_next) {
		TAILQ_REMOVE(bl, pbuf, next);
		athp_freebuf(ar, br, pbuf);
	}
}
