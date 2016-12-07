#ifndef	__LINUX_COMPAT_H__
#define	__LINUX_COMPAT_H__

#include <net80211/ieee80211.h>

#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/completion.h>
#include <linux/wait.h>
#include <linux/if_ether.h>
#include <linux/err.h>
#include <linux/etherdevice.h>
#include <linux/workqueue.h>
#include <linux/dmapool.h>
#include <linux/dma-mapping.h>
#include <linux/idr.h>
#include <linux/dma-mapping.h>

#if 0
#include <sys/libkern.h>

typedef uint8_t		u8;
typedef uint16_t	u16;
typedef uint32_t	u32;
typedef uint64_t	u64;

typedef int8_t		s8;
typedef int16_t		s16;
typedef int32_t		s32;
typedef int64_t		s64;

typedef uint16_t	__le16;
typedef uint32_t	__le32;
typedef uint64_t	__le64;

typedef uint16_t	__be16;
typedef uint32_t	__be32;
typedef uint64_t	__be64;

//#define	PTR_ALIGN(ptr, a)

static inline unsigned long
roundup_pow_of_two(unsigned long n)
{

	return 1UL << flsl(n - 1);
}

#define BUILD_BUG_ON(x) CTASSERT(!(x))

#define	unlikely(x)	(x)



#define	ARRAY_SIZE(n)	nitems(n)

/* Bitfield things; include sys/bitstring.h */
#include <sys/bitstring.h>

#define	DECLARE_BITMAP(n, s)	bitstr_t bit_decl(n, s)
#define	test_bit(i, n)		bit_test(n, i)
#define	__set_bit(i, n)		bit_set(n, i)
#define	clear_bit(i, n)		bit_clear(n, i)

#define	set_bit(i, n)		__set_bit(i, n)

#define	min_t(t, a, b)		MIN(a, b)

#define	BIT(x)			(1 << (x))

#define	ETH_ALEN		ETHER_ADDR_LEN

/* XXX TODO: only for 32 bit values */
static inline int
ilog2(uint32_t val)
{
	return fls(val);
}


#define	ECOMM		ESTALE

#define	HZ		hz

#define	DIV_ROUND_UP(x, n)	howmany(x, n)
#endif

/* Bits not implemented by our linuxkpi layer so far */
#define	__cpu_to_le32(a)	htole32(a)
#define	__cpu_to_le16(a)	htole16(a)
#define	__le32_to_cpu(a)	le32toh(a)
#define	__le16_to_cpu(a)	le16toh(a)
#define	might_sleep()
#define scnprintf(...) snprintf(__VA_ARGS__)
#define	__ALIGN_KERNEL(x, a)		__ALIGN_KERNEL_MASK(x, (__typeof__(x))(a) - 1)
#define	__ALIGN_KERNEL_MASK(x, mask)	(((x) + (mask)) & ~(mask))
#define	ALIGN_LINUX(x, a) __ALIGN_KERNEL((x), (a))
#undef	le32_to_cpup
#define	le32_to_cpup(v)		le32toh(*(v))

static inline int
IS_ALIGNED(unsigned long ptr, int a)
{

	return (ptr % a == 0);
}

/*
 * This isn't strictly speaking "linux compat"; it's bits that are
 * missing from net80211 that we should really port.
 */
/* XXX TODO: implement! */
#define	IEEE80211_IS_ACTION(a)		0
#define	IEEE80211_IS_DEAUTH(a)		0
#define	IEEE80211_IS_DISASSOC(a)	0
#define	IEEE80211_IS_QOS(a)		0
#define	IEEE80211_HAS_PROT(a)		0
#define	IEEE80211_IS_MGMT(a)		0

/* XXX temp uAPSD */
/* U-APSD queue for WMM IEs sent by AP */
#define IEEE80211_WMM_IE_AP_QOSINFO_UAPSD       (1<<7)
#define IEEE80211_WMM_IE_AP_QOSINFO_PARAM_SET_CNT_MASK  0x0f

/* U-APSD queues for WMM IEs sent by STA */
#define IEEE80211_WMM_IE_STA_QOSINFO_AC_VO      (1<<0)
#define IEEE80211_WMM_IE_STA_QOSINFO_AC_VI      (1<<1)
#define IEEE80211_WMM_IE_STA_QOSINFO_AC_BK      (1<<2)
#define IEEE80211_WMM_IE_STA_QOSINFO_AC_BE      (1<<3)
#define IEEE80211_WMM_IE_STA_QOSINFO_AC_MASK    0x0f

/* U-APSD max SP length for WMM IEs sent by STA */
#define IEEE80211_WMM_IE_STA_QOSINFO_SP_ALL     0x00
#define IEEE80211_WMM_IE_STA_QOSINFO_SP_2       0x01
#define IEEE80211_WMM_IE_STA_QOSINFO_SP_4       0x02
#define IEEE80211_WMM_IE_STA_QOSINFO_SP_6       0x03
#define IEEE80211_WMM_IE_STA_QOSINFO_SP_MASK    0x03
#define IEEE80211_WMM_IE_STA_QOSINFO_SP_SHIFT   5

/* Crpyto length definitions we don't have? Hm */
#define IEEE80211_WEP_IV_LEN            4
#define IEEE80211_WEP_ICV_LEN           4
#define IEEE80211_CCMP_HDR_LEN          8
#define IEEE80211_CCMP_MIC_LEN          8
#define IEEE80211_CCMP_PN_LEN           6
#define IEEE80211_CCMP_256_HDR_LEN      8
#define IEEE80211_CCMP_256_MIC_LEN      16
#define IEEE80211_CCMP_256_PN_LEN       6
#define IEEE80211_TKIP_IV_LEN           8
#define IEEE80211_TKIP_ICV_LEN          4
#define IEEE80211_CMAC_PN_LEN           6
#define IEEE80211_GMAC_PN_LEN           6
#define IEEE80211_GCMP_HDR_LEN          8
#define IEEE80211_GCMP_MIC_LEN          16
#define IEEE80211_GCMP_PN_LEN           6

/* They store it as 16 bit value, not two 8 bit values.. */
#define IEEE80211_FCTL_VERS             0x0003
#define IEEE80211_FCTL_FTYPE            0x000c
#define IEEE80211_FCTL_STYPE            0x00f0
#define IEEE80211_FCTL_TODS             0x0100
#define IEEE80211_FCTL_FROMDS           0x0200
#define IEEE80211_FCTL_MOREFRAGS        0x0400
#define IEEE80211_FCTL_RETRY            0x0800
#define IEEE80211_FCTL_PM               0x1000
#define IEEE80211_FCTL_MOREDATA         0x2000
#define IEEE80211_FCTL_PROTECTED        0x4000
#define IEEE80211_FCTL_ORDER            0x8000
#define IEEE80211_FCTL_CTL_EXT          0x0f00

#define IEEE80211_SCTL_FRAG             0x000F
#define IEEE80211_SCTL_SEQ              0xFFF0

#define IEEE80211_FTYPE_MGMT            0x0000
#define IEEE80211_FTYPE_CTL             0x0004
#define IEEE80211_FTYPE_DATA            0x0008
#define IEEE80211_FTYPE_EXT             0x000c

static inline u8 *ieee80211_get_DA(struct ieee80211_frame *hdr)
{
	if (IEEE80211_IS_DSTODS(hdr))
		return hdr->i_addr3;
	else
		return hdr->i_addr1;
}

static inline bool ieee80211_has_a4(struct ieee80211_frame *hdr)
{

	return (hdr->i_fc[1] & 0x3) == 0x3; /* TODS | FROMDS */
}


static inline u8 *ieee80211_get_qos_ctl(struct ieee80211_frame *hdr)
{
        if (ieee80211_has_a4(hdr))
                return (u8 *)hdr + 30;
        else
                return (u8 *)hdr + 24;
}

static inline int ieee80211_has_protected(struct ieee80211_frame *hdr)
{
	return !! (hdr->i_fc[1] |= IEEE80211_FC1_PROTECTED);
}


#endif	/* __LINUX_COMPAT_H__ */
