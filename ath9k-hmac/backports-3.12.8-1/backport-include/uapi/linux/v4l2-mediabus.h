#ifndef __BACKPORT_LINUX_V4L2_MEDIABUS_H
#define __BACKPORT_LINUX_V4L2_MEDIABUS_H
#include <linux/version.h>

/*
 * SOC_CAMERA is only enabled on 3.4 as it depends on some
 * newer regulator functionality, however there are some SOC
 * cameras that can rely on the 3.3 regulatory built-in core
 * and the 3.3 SOC_CAMERA module however two routines are
 * not exported in that version of SOC_CAMERA that newer
 * SOC cameras do require. Backport that functionality.
 *
 * Technically this should go into <media/soc_camera.h>
 * given that is where its where its exported on linux-next
 * but in practice only placing it here actually fixes linking
 * errors for 3.3 for all SOC camera drivers we make available
 * for 3.3.
 */
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3,3,0))
#if (LINUX_VERSION_CODE < KERNEL_VERSION(3,4,0))
#if defined(CPTCFG_VIDEO_DEV_MODULE)

#include <media/v4l2-clk.h>

struct soc_camera_subdev_desc;

#define soc_camera_power_on LINUX_BACKPORT(soc_camera_power_on)
int soc_camera_power_on(struct device *dev,
			struct soc_camera_subdev_desc *ssdd,
			struct v4l2_clk *clk);

#define soc_camera_power_off LINUX_BACKPORT(soc_camera_power_off)
int soc_camera_power_off(struct device *dev,
			struct soc_camera_subdev_desc *ssdd,
			struct v4l2_clk *clk);

#endif /* defined(CPTCFG_VIDEO_DEV_MODULE) */
#endif /* (LINUX_VERSION_CODE < KERNEL_VERSION(3,4,0)) */
#endif /* (LINUX_VERSION_CODE >= KERNEL_VERSION(3,3,0)) */

#include_next <linux/v4l2-mediabus.h>

#endif /* __BACKPORT_LINUX_V4L2_MEDIABUS_H */
