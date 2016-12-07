/*
 * driver.h -- SoC Regulator driver support.
 *
 * Copyright (C) 2007, 2008 Wolfson Microelectronics PLC.
 *
 * Author: Liam Girdwood <lrg@slimlogic.co.uk>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Regulator Driver Interface.
 */

#ifndef __BACKPORT_LINUX_REGULATOR_DRIVER_H_
#define __BACKPORT_LINUX_REGULATOR_DRIVER_H_

#include <linux/version.h>
#include_next <linux/regulator/driver.h>

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3,6,0))
int regulator_map_voltage_ascend(struct regulator_dev *rdev,
				 int min_uV, int max_uV);
#endif /* (LINUX_VERSION_CODE >= KERNEL_VERSION(3,6,0)) */

#endif /* __BACKPORT_LINUX_REGULATOR_DRIVER_H_ */
