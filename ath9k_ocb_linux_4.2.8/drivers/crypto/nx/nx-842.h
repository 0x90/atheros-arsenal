
#ifndef __NX_842_H__
#define __NX_842_H__

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/sw842.h>
#include <linux/of.h>
#include <linux/slab.h>
#include <linux/io.h>
#include <linux/mm.h>
#include <linux/ratelimit.h>

/* Restrictions on Data Descriptor List (DDL) and Entry (DDE) buffers
 *
 * From NX P8 workbook, sec 4.9.1 "842 details"
 *   Each DDE buffer is 128 byte aligned
 *   Each DDE buffer size is a multiple of 32 bytes (except the last)
 *   The last DDE buffer size is a multiple of 8 bytes
 */
#define DDE_BUFFER_ALIGN	(128)
#define DDE_BUFFER_SIZE_MULT	(32)
#define DDE_BUFFER_LAST_MULT	(8)

/* Arbitrary DDL length limit
 * Allows max buffer size of MAX-1 to MAX pages
 * (depending on alignment)
 */
#define DDL_LEN_MAX		(17)

/* CCW 842 CI/FC masks
 * NX P8 workbook, section 4.3.1, figure 4-6
 * "CI/FC Boundary by NX CT type"
 */
#define CCW_CI_842		(0x00003ff8)
#define CCW_FC_842		(0x00000007)

/* CCW Function Codes (FC) for 842
 * NX P8 workbook, section 4.9, table 4-28
 * "Function Code Definitions for 842 Memory Compression"
 */
#define CCW_FC_842_COMP_NOCRC	(0)
#define CCW_FC_842_COMP_CRC	(1)
#define CCW_FC_842_DECOMP_NOCRC	(2)
#define CCW_FC_842_DECOMP_CRC	(3)
#define CCW_FC_842_MOVE		(4)

/* CSB CC Error Types for 842
 * NX P8 workbook, section 4.10.3, table 4-30
 * "Reported Error Types Summary Table"
 */
/* These are all duplicates of existing codes defined in icswx.h. */
#define CSB_CC_TRANSLATION_DUP1	(80)
#define CSB_CC_TRANSLATION_DUP2	(82)
#define CSB_CC_TRANSLATION_DUP3	(84)
#define CSB_CC_TRANSLATION_DUP4	(86)
#define CSB_CC_TRANSLATION_DUP5	(92)
#define CSB_CC_TRANSLATION_DUP6	(94)
#define CSB_CC_PROTECTION_DUP1	(81)
#define CSB_CC_PROTECTION_DUP2	(83)
#define CSB_CC_PROTECTION_DUP3	(85)
#define CSB_CC_PROTECTION_DUP4	(87)
#define CSB_CC_PROTECTION_DUP5	(93)
#define CSB_CC_PROTECTION_DUP6	(95)
#define CSB_CC_RD_EXTERNAL_DUP1	(89)
#define CSB_CC_RD_EXTERNAL_DUP2	(90)
#define CSB_CC_RD_EXTERNAL_DUP3	(91)
/* These are specific to NX */
/* 842 codes */
#define CSB_CC_TPBC_GT_SPBC	(64) /* no error, but >1 comp ratio */
#define CSB_CC_CRC_MISMATCH	(65) /* decomp crc mismatch */
#define CSB_CC_TEMPL_INVALID	(66) /* decomp invalid template value */
#define CSB_CC_TEMPL_OVERFLOW	(67) /* decomp template shows data after end */
/* sym crypt codes */
#define CSB_CC_DECRYPT_OVERFLOW	(64)
/* asym crypt codes */
#define CSB_CC_MINV_OVERFLOW	(128)
/* These are reserved for hypervisor use */
#define CSB_CC_HYP_RESERVE_START	(240)
#define CSB_CC_HYP_RESERVE_END		(253)
#define CSB_CC_HYP_NO_HW		(254)
#define CSB_CC_HYP_HANG_ABORTED		(255)

/* CCB Completion Modes (CM) for 842
 * NX P8 workbook, section 4.3, figure 4-5
 * "CRB Details - Normal Cop_Req (CL=00, C=1)"
 */
#define CCB_CM_EXTRA_WRITE	(CCB_CM0_ALL_COMPLETIONS & CCB_CM12_STORE)
#define CCB_CM_INTERRUPT	(CCB_CM0_ALL_COMPLETIONS & CCB_CM12_INTERRUPT)

#define LEN_ON_SIZE(pa, size)	((size) - ((pa) & ((size) - 1)))
#define LEN_ON_PAGE(pa)		LEN_ON_SIZE(pa, PAGE_SIZE)

static inline unsigned long nx842_get_pa(void *addr)
{
	if (!is_vmalloc_addr(addr))
		return __pa(addr);

	return page_to_phys(vmalloc_to_page(addr)) + offset_in_page(addr);
}

/* Get/Set bit fields */
#define MASK_LSH(m)		(__builtin_ffsl(m) - 1)
#define GET_FIELD(v, m)		(((v) & (m)) >> MASK_LSH(m))
#define SET_FIELD(v, m, val)	(((v) & ~(m)) | (((val) << MASK_LSH(m)) & (m)))

struct nx842_constraints {
	int alignment;
	int multiple;
	int minimum;
	int maximum;
};

struct nx842_driver {
	char *name;
	struct module *owner;
	size_t workmem_size;

	struct nx842_constraints *constraints;

	int (*compress)(const unsigned char *in, unsigned int in_len,
			unsigned char *out, unsigned int *out_len,
			void *wrkmem);
	int (*decompress)(const unsigned char *in, unsigned int in_len,
			  unsigned char *out, unsigned int *out_len,
			  void *wrkmem);
};

struct nx842_driver *nx842_platform_driver(void);
bool nx842_platform_driver_set(struct nx842_driver *driver);
void nx842_platform_driver_unset(struct nx842_driver *driver);
bool nx842_platform_driver_get(void);
void nx842_platform_driver_put(void);

size_t nx842_workmem_size(void);

int nx842_constraints(struct nx842_constraints *constraints);

int nx842_compress(const unsigned char *in, unsigned int in_len,
		   unsigned char *out, unsigned int *out_len, void *wrkmem);
int nx842_decompress(const unsigned char *in, unsigned int in_len,
		     unsigned char *out, unsigned int *out_len, void *wrkmem);

#endif /* __NX_842_H__ */
