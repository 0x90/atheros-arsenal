#ifndef __BACKPORT_WATCHDOG_H
#define __BACKPORT_WATCHDOG_H
#include_next <linux/watchdog.h>

#if (RHEL_RELEASE_CODE < RHEL_RELEASE_VERSION(6,4))
#if LINUX_VERSION_CODE < KERNEL_VERSION(3,1,0)
struct watchdog_device {
};
#endif
#endif

#endif /* __BACKPORT_WATCHDOG_H */
