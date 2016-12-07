/*
 * Copyright 2012  Luis R. Rodriguez <mcgrof@frijolero.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Backport functionality introduced in Linux 3.4.
 */

#include <linux/fs.h>
#include <linux/module.h>
#include <linux/wait.h>

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3,2,0))
#include <linux/regmap.h>
#include <linux/i2c.h>
#include <linux/spi/spi.h>
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3,3,0))
#if (LINUX_VERSION_CODE < KERNEL_VERSION(3,4,0))
#if defined(CPTCFG_VIDEO_DEV_MODULE)
#include <media/soc_camera.h>
#include <media/v4l2-common.h>
#include <media/v4l2-ioctl.h>
#include <media/v4l2-dev.h>
#include <media/videobuf-core.h>
#include <media/videobuf2-core.h>
#include <media/soc_mediabus.h>
#include <linux/regulator/consumer.h>
#endif /* defined(CPTCFG_VIDEO_DEV_MODULE) */
#endif /* (LINUX_VERSION_CODE < KERNEL_VERSION(3,4,0)) */
#endif /* (LINUX_VERSION_CODE >= KERNEL_VERSION(3,3,0)) */
#endif /* (LINUX_VERSION_CODE >= KERNEL_VERSION(3,2,0)) */

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3,2,0))

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3,3,0))
#if (LINUX_VERSION_CODE < KERNEL_VERSION(3,4,0))

#if defined(CPTCFG_VIDEO_V4L2_MODULE)
#include <media/v4l2-clk.h>
int soc_camera_power_on(struct device *dev,
			struct soc_camera_subdev_desc *ssdd,
			struct v4l2_clk *clk)
{
	int ret = clk ? v4l2_clk_enable(clk) : 0;
	if (ret < 0) {
		dev_err(dev, "Cannot enable clock: %d\n", ret);
		return ret;
	}
	ret = regulator_bulk_enable(ssdd->num_regulators,
					ssdd->regulators);
	if (ret < 0) {
		dev_err(dev, "Cannot enable regulators\n");
		goto eregenable;
	}

	if (ssdd->power) {
		ret = ssdd->power(dev, 1);
		if (ret < 0) {
			dev_err(dev,
				"Platform failed to power-on the camera.\n");
			goto epwron;
		}
	}

	return 0;

epwron:
	regulator_bulk_disable(ssdd->num_regulators,
			       ssdd->regulators);
eregenable:
	if (clk)
		v4l2_clk_disable(clk);

	return ret;
}
EXPORT_SYMBOL_GPL(soc_camera_power_on);

int soc_camera_power_off(struct device *dev,
			 struct soc_camera_subdev_desc *ssdd,
			 struct v4l2_clk *clk)
{
	int ret = 0;
	int err;

	if (ssdd->power) {
		err = ssdd->power(dev, 0);
		if (err < 0) {
			dev_err(dev,
			        "Platform failed to power-off the camera.\n");
			ret = err;
		}
	}

	err = regulator_bulk_disable(ssdd->num_regulators,
				     ssdd->regulators);
	if (err < 0) {
		dev_err(dev, "Cannot disable regulators\n");
		ret = ret ? : err;
	}

	if (clk)
		v4l2_clk_disable(clk);

	return ret;
}
EXPORT_SYMBOL_GPL(soc_camera_power_off);
#endif /* defined(CPTCFG_VIDEO_V4L2_MODULE) */

#endif /* (LINUX_VERSION_CODE < KERNEL_VERSION(3,4,0)) */
#endif /* (LINUX_VERSION_CODE >= KERNEL_VERSION(3,3,0)) */

#if defined(CONFIG_REGMAP)
static void devm_regmap_release(struct device *dev, void *res)
{
	regmap_exit(*(struct regmap **)res);
}

#if defined(CONFIG_REGMAP_I2C)
static int regmap_i2c_write(
			    struct device *dev,
			    const void *data,
			    size_t count)
{
	struct i2c_client *i2c = to_i2c_client(dev);
	int ret;

	ret = i2c_master_send(i2c, data, count);
	if (ret == count)
		return 0;
	else if (ret < 0)
		return ret;
	else
		return -EIO;
}

static int regmap_i2c_gather_write(
				   struct device *dev,
				   const void *reg, size_t reg_size,
				   const void *val, size_t val_size)
{
	struct i2c_client *i2c = to_i2c_client(dev);
	struct i2c_msg xfer[2];
	int ret;

	/* If the I2C controller can't do a gather tell the core, it
	 * will substitute in a linear write for us.
	 */
	if (!i2c_check_functionality(i2c->adapter, I2C_FUNC_NOSTART))
		return -ENOTSUPP;

	xfer[0].addr = i2c->addr;
	xfer[0].flags = 0;
	xfer[0].len = reg_size;
	xfer[0].buf = (void *)reg;

	xfer[1].addr = i2c->addr;
	xfer[1].flags = I2C_M_NOSTART;
	xfer[1].len = val_size;
	xfer[1].buf = (void *)val;

	ret = i2c_transfer(i2c->adapter, xfer, 2);
	if (ret == 2)
		return 0;
	if (ret < 0)
		return ret;
	else
		return -EIO;
}

static int regmap_i2c_read(
			   struct device *dev,
			   const void *reg, size_t reg_size,
			   void *val, size_t val_size)
{
	struct i2c_client *i2c = to_i2c_client(dev);
	struct i2c_msg xfer[2];
	int ret;

	xfer[0].addr = i2c->addr;
	xfer[0].flags = 0;
	xfer[0].len = reg_size;
	xfer[0].buf = (void *)reg;

	xfer[1].addr = i2c->addr;
	xfer[1].flags = I2C_M_RD;
	xfer[1].len = val_size;
	xfer[1].buf = val;

	ret = i2c_transfer(i2c->adapter, xfer, 2);
	if (ret == 2)
		return 0;
	else if (ret < 0)
		return ret;
	else
		return -EIO;
}

static struct regmap_bus regmap_i2c = {
	.write = regmap_i2c_write,
	.gather_write = regmap_i2c_gather_write,
	.read = regmap_i2c_read,
};
#endif /* defined(CONFIG_REGMAP_I2C) */

/**
 * devm_regmap_init(): Initialise managed register map
 *
 * @dev: Device that will be interacted with
 * @bus: Bus-specific callbacks to use with device
 * @bus_context: Data passed to bus-specific callbacks
 * @config: Configuration for register map
 *
 * The return value will be an ERR_PTR() on error or a valid pointer
 * to a struct regmap.  This function should generally not be called
 * directly, it should be called by bus-specific init functions.  The
 * map will be automatically freed by the device management code.
 */
struct regmap *devm_regmap_init(struct device *dev,
				const struct regmap_bus *bus,
				const struct regmap_config *config)
{
	struct regmap **ptr, *regmap;

	ptr = devres_alloc(devm_regmap_release, sizeof(*ptr), GFP_KERNEL);
	if (!ptr)
		return ERR_PTR(-ENOMEM);

	regmap = regmap_init(dev,
			     bus,
			     config);
	if (!IS_ERR(regmap)) {
		*ptr = regmap;
		devres_add(dev, ptr);
	} else {
		devres_free(ptr);
	}

	return regmap;
}
EXPORT_SYMBOL_GPL(devm_regmap_init);

#if defined(CONFIG_REGMAP_I2C)
/**
 * devm_regmap_init_i2c(): Initialise managed register map
 *
 * @i2c: Device that will be interacted with
 * @config: Configuration for register map
 *
 * The return value will be an ERR_PTR() on error or a valid pointer
 * to a struct regmap.  The regmap will be automatically freed by the
 * device management code.
 */
struct regmap *devm_regmap_init_i2c(struct i2c_client *i2c,
				    const struct regmap_config *config)
{
	return devm_regmap_init(&i2c->dev, &regmap_i2c, config);
}
EXPORT_SYMBOL_GPL(devm_regmap_init_i2c);
#endif /* defined(CONFIG_REGMAP_I2C) */

#endif /* defined(CONFIG_REGMAP) */
#endif /* (LINUX_VERSION_CODE >= KERNEL_VERSION(3,2,0)) */

int simple_open(struct inode *inode, struct file *file)
{
	if (inode->i_private)
		file->private_data = inode->i_private;
	return 0;
}
EXPORT_SYMBOL_GPL(simple_open);
