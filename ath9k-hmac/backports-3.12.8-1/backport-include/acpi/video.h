#ifndef __BACKPORT_ACPI_VIDEO_H
#define __BACKPORT_ACPI_VIDEO_H
#include_next <acpi/video.h>
#include <linux/version.h>

#if LINUX_VERSION_CODE < KERNEL_VERSION(3,12,0)
static inline int acpi_video_register_with_quirks(void)
{
	return acpi_video_register();
}
#endif

#endif /*  __BACKPORT_ACPI_VIDEO_H */
