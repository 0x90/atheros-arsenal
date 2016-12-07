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
 * Firmware sanity check and capability / information loader.
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

#include "if_otus_firmware.h"

static const uint8_t otus_magic[4] = { OTUS_MAGIC };


/*
 * XXX from carl9170
 */
static const struct carl9170fw_desc_head *
carl9170_find_fw_desc(const uint8_t *fw_data, const size_t len)
{
	int scan = 0, found = 0;

	if (!carl9170fw_size_check(len)) {
		printf("%s: firmware size is out of bound.\n", __func__);
		return (NULL);
	}

	while (scan < len - sizeof(struct carl9170fw_desc_head)) {
		if (fw_data[scan++] == otus_magic[found])
			found++;
		else
			found = 0;

		if (scan >= len)
			break;

		if (found == sizeof(otus_magic))
			break;
	}

	if (found != sizeof(otus_magic))
		return (NULL);

	return (const void *)&fw_data[scan - found];
}

/*
 * XXX from carl9170
 */
static const void *
carl9170_fw_find_desc(struct carl9170_firmware_info *fwinfo,
    const uint8_t descid[4], const unsigned int len,
    const uint8_t compatible_revision)
{
        const struct carl9170fw_desc_head *iter;

        carl9170fw_for_each_hdr(iter, fwinfo->desc) {
                if (carl9170fw_desc_cmp(iter, descid, len,
                                        compatible_revision))
                        return (const void *)iter;
        }

        /* needed to find the LAST desc */
        if (carl9170fw_desc_cmp(iter, descid, len,
                                compatible_revision))
                return (const void *)iter;

        return NULL;
}

/*
 * XXX from carl9170
 */
static void
carl9170_fw_info(struct carl9170_firmware_info *fwinfo)
{
        const struct carl9170fw_motd_desc *motd_desc;
        unsigned int str_ver_len;
        uint32_t fw_date;

        motd_desc = carl9170_fw_find_desc(fwinfo, MOTD_MAGIC,
                sizeof(*motd_desc), CARL9170FW_MOTD_DESC_CUR_VER);

        if (motd_desc) {
                str_ver_len = strnlen(motd_desc->release,
                        CARL9170FW_MOTD_RELEASE_LEN);

                fw_date = le32_to_cpu(motd_desc->fw_year_month_day);
                printf("firmware API: %.*s 2%03d-%02d-%02d\n",
                         str_ver_len, motd_desc->release,
                         CARL9170FW_GET_YEAR(fw_date),
                         CARL9170FW_GET_MONTH(fw_date),
                         CARL9170FW_GET_DAY(fw_date));
        }
}

/*
 * XXX from carl9170
 *
 * XXX this needs to be significantly fleshed out before it can be
 * used by a live driver!
 */
static int
carl9170_fw(struct carl9170_firmware_info *fwinfo, const uint8_t *data,
    size_t len)
{
        const struct carl9170fw_otus_desc *otus_desc;

        otus_desc = carl9170_fw_find_desc(fwinfo, OTUS_MAGIC,
                sizeof(*otus_desc), CARL9170FW_OTUS_DESC_CUR_VER);
        if (!otus_desc) {
                return ENXIO;
        }

#define SUPP(feat)                                              \
        (carl9170fw_supports(otus_desc->feature_set, feat))

        if (!SUPP(CARL9170FW_DUMMY_FEATURE)) {
                return EINVAL;
        }

        fwinfo->api_version = otus_desc->api_ver;

	fwinfo->vif_num = otus_desc->vif_num;
	fwinfo->cmd_bufs = otus_desc->cmd_bufs;
	fwinfo->address = le32_to_cpu(otus_desc->fw_address);
	fwinfo->rx_size = le16_to_cpu(otus_desc->rx_max_frame_len);
	fwinfo->mem_blocks = MIN(otus_desc->tx_descs, 0xfe);
//	atomic_set(&ar->mem_free_blocks, ar->fw.mem_blocks);
	fwinfo->mem_block_size = le16_to_cpu(otus_desc->tx_frag_len);

        if (SUPP(CARL9170FW_MINIBOOT))
                fwinfo->offset = le16_to_cpu(otus_desc->miniboot_size);
        else
                fwinfo->offset = 0;

	return (0);
#undef SUPP
}

int
otus_firmware_load(struct carl9170_firmware_info *fwinfo)
{
	int error;

	/* Free firmware that may have been loaded already */
	if (fwinfo->fw) {
		firmware_put(fwinfo->fw, FIRMWARE_UNLOAD);
	}

	/* Clear the previous state */
	bzero(fwinfo, sizeof(*fwinfo));

	/* Load in the initial firmware */
	fwinfo->fw = firmware_get("otusfw");
	if (fwinfo->fw == NULL) {
		printf("%s: failed firmware_get(otus_fw)\n",
		    __func__);
		return (ENOENT);
	}

	/*
	 * Obtain the firmware descriptor.  If we can't find it,
	 * we can't check for firmware options and such.
	 */
	fwinfo->desc = carl9170_find_fw_desc(fwinfo->fw->data,
	    fwinfo->fw->datasize);
	if (fwinfo->desc == NULL) {
		printf("%s: couldn't find firmware descriptor\n",
		    __func__);
		return (EINVAL);
	}

	/*
	 * We found firmware, so let's print out the identifying
	 * information and then parse things.
	 */
	carl9170_fw_info(fwinfo);

	error = carl9170_fw(fwinfo, fwinfo->fw->data, fwinfo->fw->datasize);
	if (error != 0) {
		printf("%s: couldn't parse firmware descriptor!\n",
		    __func__);
		return (EINVAL);
	}

	printf("%s: firmware api=%d, offset=%d bytes, address=0x%08x\n",
	    __func__,
	    fwinfo->api_version,
	    fwinfo->offset,
	    fwinfo->address);

	/*
	 * Completed!
	 */
	printf("%s: success!\n", __func__);
	return (0);
}


void
otus_firmware_cleanup(struct carl9170_firmware_info *fwinfo)
{

	if (fwinfo->fw) {
		firmware_put(fwinfo->fw, FIRMWARE_UNLOAD);
		fwinfo->fw = NULL;
	}

	bzero(fwinfo, sizeof(*fwinfo));
}
