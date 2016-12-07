
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

/*
 * Playground for QCA988x chipsets.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include "opt_wlan.h"

#include <sys/param.h>
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
#include <sys/condvar.h>

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

#include <dev/pci/pcivar.h>
#include <dev/pci/pcireg.h>

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
#include "hal/hw.h"
#include "hal/htc.h"
#include "hal/wmi.h"

#include "if_athp_debug.h"
#include "if_athp_regio.h"
#include "if_athp_stats.h"
#include "if_athp_core.h"
#include "if_athp_desc.h"
#include "if_athp_htc.h"
#include "if_athp_buf.h"
#include "if_athp_wmi.h"
#include "if_athp_wmi_tlv.h"
#include "if_athp_var.h"
#include "if_athp_wmi_ops.h"
#include "if_athp_hif.h"
#include "if_athp_bmi.h"
#include "if_athp_mac.h"

MALLOC_DECLARE(M_ATHPDEV);

/* Placeholders for the MAC routines; the port of these will come later */


struct ath10k *
ath10k_mac_create(size_t priv_size)
{
	printf("%s: called\n", __func__);
	return NULL;
}

void
ath10k_mac_destroy(struct ath10k *ar)
{

	printf("%s: called\n", __func__);
}

int
ath10k_mac_register(struct ath10k *ar)
{

	printf("%s: called\n", __func__);
	return (-EINVAL);
}

void
ath10k_mac_unregister(struct ath10k *ar)
{

	printf("%s: called\n", __func__);
}

struct ath10k_vif *
ath10k_get_arvif(struct ath10k *ar, u32 vdev_id)
{

	printf("%s: called\n", __func__);
	return (NULL);
}

void
ath10k_mac_handle_beacon(struct ath10k *ar, struct athp_buf *pbuf)
{

	printf("%s: called\n", __func__);
}

void
ath10k_mac_handle_beacon_miss(struct ath10k *ar, u32 vdev_id)
{

	printf("%s: called\n", __func__);
}

void
ath10k_mac_handle_tx_pause_vdev(struct ath10k *ar, u32 vdev_id,
    enum wmi_tlv_tx_pause_id pause_id, enum wmi_tlv_tx_pause_action action)
{

	printf("%s: called\n", __func__);
}


/***************/
/* TX handlers */
/***************/

void ath10k_mac_tx_lock(struct ath10k *ar, int reason)
{
	ATHP_HTT_TX_LOCK_ASSERT(&ar->htt);

	WARN_ON(reason >= ATH10K_TX_PAUSE_MAX);
	ar->tx_paused |= BIT(reason);
#if 0
	ieee80211_stop_queues(ar->hw);
#else
	device_printf(ar->sc_dev, "%s: TODO: called!\n", __func__);
#endif
}

#if 0
static void ath10k_mac_tx_unlock_iter(void *data, u8 *mac,
				      struct ieee80211_vif *vif)
{
	struct ath10k *ar = data;
	struct ath10k_vif *arvif = ath10k_vif_to_arvif(vif);

	if (arvif->tx_paused)
		return;

#if 0
	ieee80211_wake_queue(ar->hw, arvif->vdev_id);
#else
	device_printf(ar->sc_dev, "%s: TODO: called!\n", __func__);
#endif
}
#endif

void ath10k_mac_tx_unlock(struct ath10k *ar, int reason)
{
	ATHP_HTT_TX_LOCK_ASSERT(&ar->htt);

	WARN_ON(reason >= ATH10K_TX_PAUSE_MAX);
	ar->tx_paused &= ~BIT(reason);

	if (ar->tx_paused)
		return;
#if 0
	ieee80211_iterate_active_interfaces_atomic(ar->hw,
						   IEEE80211_IFACE_ITER_RESUME_ALL,
						   ath10k_mac_tx_unlock_iter,
						   ar);

	ieee80211_wake_queue(ar->hw, ar->hw->offchannel_tx_hw_queue);
#else
	device_printf(ar->sc_dev, "%s: TODO: called!\n", __func__);
#endif
}
