#ifndef __BACKPORT_LINUX_VGAARB_H
#define __BACKPORT_LINUX_VGAARB_H
#include <linux/version.h>
#if LINUX_VERSION_CODE < KERNEL_VERSION(3,4,0)
struct pci_dev;
#endif
#include <video/vga.h>
#include_next <linux/vgaarb.h>

#endif /* __BACKPORT_LINUX_VGAARB_H */
