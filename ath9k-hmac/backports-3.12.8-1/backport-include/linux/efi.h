#ifndef __BACKPORT_EFI_H
#define __BACKPORT_EFI_H
#include_next <linux/efi.h>
#include <linux/version.h>

#if (LINUX_VERSION_CODE < KERNEL_VERSION(3,8,0))

/* This backports:
 *
 * commit 83e68189745ad931c2afd45d8ee3303929233e7f
 * Author: Matt Fleming <matt.fleming@intel.com>
 * Date:   Wed Nov 14 09:42:35 2012 +0000
 *
 *     efi: Make 'efi_enabled' a function to query EFI facilities
 *
 */
/* check first if this was already backported */
#ifndef EFI_BOOT
/*
 * We play games with efi_enabled so that the compiler will, if
 * possible, remove EFI-related code altogether.
 */
#define EFI_BOOT		0	/* Were we booted from EFI? */
#define EFI_SYSTEM_TABLES	1	/* Can we use EFI system tables? */
#define EFI_CONFIG_TABLES	2	/* Can we use EFI config tables? */
#define EFI_RUNTIME_SERVICES	3	/* Can we use runtime services? */
#define EFI_MEMMAP		4	/* Can we use EFI memory map? */
#define EFI_64BIT		5	/* Is the firmware 64-bit? */

#ifdef CONFIG_EFI
# ifdef CONFIG_X86
static inline int compat_efi_enabled(int facility)
{
	switch (facility) {
	case EFI_BOOT:
		return efi_enabled;
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3,4,0))
	case EFI_64BIT:
		return efi_64bit;
#endif
	default:
		printk(KERN_ERR "can not translate efi_enabled() to old values completly\n");
		return efi_enabled;
	}
}
# else
static inline int compat_efi_enabled(int facility)
{
	return 1;
}
# endif
#else
static inline int compat_efi_enabled(int facility)
{
	return 0;
}
#endif
#ifdef efi_enabled
#undef efi_enabled
#endif
#define efi_enabled(facility) compat_efi_enabled(facility)
#endif /* EFI_BOOT */

#endif /* (LINUX_VERSION_CODE < KERNEL_VERSION(3,8,0)) */

#endif /* __BACKPORT_EFI_H */
