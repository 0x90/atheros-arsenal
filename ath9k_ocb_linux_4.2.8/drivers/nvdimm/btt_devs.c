/*
 * Copyright(c) 2013-2015 Intel Corporation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of version 2 of the GNU General Public License as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 */
#include <linux/blkdev.h>
#include <linux/device.h>
#include <linux/genhd.h>
#include <linux/sizes.h>
#include <linux/slab.h>
#include <linux/fs.h>
#include <linux/mm.h>
#include "nd-core.h"
#include "btt.h"
#include "nd.h"

static void __nd_btt_detach_ndns(struct nd_btt *nd_btt)
{
	struct nd_namespace_common *ndns = nd_btt->ndns;

	dev_WARN_ONCE(&nd_btt->dev, !mutex_is_locked(&ndns->dev.mutex)
			|| ndns->claim != &nd_btt->dev,
			"%s: invalid claim\n", __func__);
	ndns->claim = NULL;
	nd_btt->ndns = NULL;
	put_device(&ndns->dev);
}

static void nd_btt_detach_ndns(struct nd_btt *nd_btt)
{
	struct nd_namespace_common *ndns = nd_btt->ndns;

	if (!ndns)
		return;
	get_device(&ndns->dev);
	device_lock(&ndns->dev);
	__nd_btt_detach_ndns(nd_btt);
	device_unlock(&ndns->dev);
	put_device(&ndns->dev);
}

static bool __nd_btt_attach_ndns(struct nd_btt *nd_btt,
		struct nd_namespace_common *ndns)
{
	if (ndns->claim)
		return false;
	dev_WARN_ONCE(&nd_btt->dev, !mutex_is_locked(&ndns->dev.mutex)
			|| nd_btt->ndns,
			"%s: invalid claim\n", __func__);
	ndns->claim = &nd_btt->dev;
	nd_btt->ndns = ndns;
	get_device(&ndns->dev);
	return true;
}

static bool nd_btt_attach_ndns(struct nd_btt *nd_btt,
		struct nd_namespace_common *ndns)
{
	bool claimed;

	device_lock(&ndns->dev);
	claimed = __nd_btt_attach_ndns(nd_btt, ndns);
	device_unlock(&ndns->dev);
	return claimed;
}

static void nd_btt_release(struct device *dev)
{
	struct nd_region *nd_region = to_nd_region(dev->parent);
	struct nd_btt *nd_btt = to_nd_btt(dev);

	dev_dbg(dev, "%s\n", __func__);
	nd_btt_detach_ndns(nd_btt);
	ida_simple_remove(&nd_region->btt_ida, nd_btt->id);
	kfree(nd_btt->uuid);
	kfree(nd_btt);
}

static struct device_type nd_btt_device_type = {
	.name = "nd_btt",
	.release = nd_btt_release,
};

bool is_nd_btt(struct device *dev)
{
	return dev->type == &nd_btt_device_type;
}
EXPORT_SYMBOL(is_nd_btt);

struct nd_btt *to_nd_btt(struct device *dev)
{
	struct nd_btt *nd_btt = container_of(dev, struct nd_btt, dev);

	WARN_ON(!is_nd_btt(dev));
	return nd_btt;
}
EXPORT_SYMBOL(to_nd_btt);

static const unsigned long btt_lbasize_supported[] = { 512, 520, 528,
	4096, 4104, 4160, 4224, 0 };

static ssize_t sector_size_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct nd_btt *nd_btt = to_nd_btt(dev);

	return nd_sector_size_show(nd_btt->lbasize, btt_lbasize_supported, buf);
}

static ssize_t sector_size_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t len)
{
	struct nd_btt *nd_btt = to_nd_btt(dev);
	ssize_t rc;

	device_lock(dev);
	nvdimm_bus_lock(dev);
	rc = nd_sector_size_store(dev, buf, &nd_btt->lbasize,
			btt_lbasize_supported);
	dev_dbg(dev, "%s: result: %zd wrote: %s%s", __func__,
			rc, buf, buf[len - 1] == '\n' ? "" : "\n");
	nvdimm_bus_unlock(dev);
	device_unlock(dev);

	return rc ? rc : len;
}
static DEVICE_ATTR_RW(sector_size);

static ssize_t uuid_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct nd_btt *nd_btt = to_nd_btt(dev);

	if (nd_btt->uuid)
		return sprintf(buf, "%pUb\n", nd_btt->uuid);
	return sprintf(buf, "\n");
}

static ssize_t uuid_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t len)
{
	struct nd_btt *nd_btt = to_nd_btt(dev);
	ssize_t rc;

	device_lock(dev);
	rc = nd_uuid_store(dev, &nd_btt->uuid, buf, len);
	dev_dbg(dev, "%s: result: %zd wrote: %s%s", __func__,
			rc, buf, buf[len - 1] == '\n' ? "" : "\n");
	device_unlock(dev);

	return rc ? rc : len;
}
static DEVICE_ATTR_RW(uuid);

static ssize_t namespace_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct nd_btt *nd_btt = to_nd_btt(dev);
	ssize_t rc;

	nvdimm_bus_lock(dev);
	rc = sprintf(buf, "%s\n", nd_btt->ndns
			? dev_name(&nd_btt->ndns->dev) : "");
	nvdimm_bus_unlock(dev);
	return rc;
}

static int namespace_match(struct device *dev, void *data)
{
	char *name = data;

	return strcmp(name, dev_name(dev)) == 0;
}

static bool is_nd_btt_idle(struct device *dev)
{
	struct nd_region *nd_region = to_nd_region(dev->parent);
	struct nd_btt *nd_btt = to_nd_btt(dev);

	if (nd_region->btt_seed == dev || nd_btt->ndns || dev->driver)
		return false;
	return true;
}

static ssize_t __namespace_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t len)
{
	struct nd_btt *nd_btt = to_nd_btt(dev);
	struct nd_namespace_common *ndns;
	struct device *found;
	char *name;

	if (dev->driver) {
		dev_dbg(dev, "%s: -EBUSY\n", __func__);
		return -EBUSY;
	}

	name = kstrndup(buf, len, GFP_KERNEL);
	if (!name)
		return -ENOMEM;
	strim(name);

	if (strncmp(name, "namespace", 9) == 0 || strcmp(name, "") == 0)
		/* pass */;
	else {
		len = -EINVAL;
		goto out;
	}

	ndns = nd_btt->ndns;
	if (strcmp(name, "") == 0) {
		/* detach the namespace and destroy / reset the btt device */
		nd_btt_detach_ndns(nd_btt);
		if (is_nd_btt_idle(dev))
			nd_device_unregister(dev, ND_ASYNC);
		else {
			nd_btt->lbasize = 0;
			kfree(nd_btt->uuid);
			nd_btt->uuid = NULL;
		}
		goto out;
	} else if (ndns) {
		dev_dbg(dev, "namespace already set to: %s\n",
				dev_name(&ndns->dev));
		len = -EBUSY;
		goto out;
	}

	found = device_find_child(dev->parent, name, namespace_match);
	if (!found) {
		dev_dbg(dev, "'%s' not found under %s\n", name,
				dev_name(dev->parent));
		len = -ENODEV;
		goto out;
	}

	ndns = to_ndns(found);
	if (__nvdimm_namespace_capacity(ndns) < SZ_16M) {
		dev_dbg(dev, "%s too small to host btt\n", name);
		len = -ENXIO;
		goto out_attach;
	}

	WARN_ON_ONCE(!is_nvdimm_bus_locked(&nd_btt->dev));
	if (!nd_btt_attach_ndns(nd_btt, ndns)) {
		dev_dbg(dev, "%s already claimed\n",
				dev_name(&ndns->dev));
		len = -EBUSY;
	}

 out_attach:
	put_device(&ndns->dev); /* from device_find_child */
 out:
	kfree(name);
	return len;
}

static ssize_t namespace_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t len)
{
	ssize_t rc;

	nvdimm_bus_lock(dev);
	device_lock(dev);
	rc = __namespace_store(dev, attr, buf, len);
	dev_dbg(dev, "%s: result: %zd wrote: %s%s", __func__,
			rc, buf, buf[len - 1] == '\n' ? "" : "\n");
	device_unlock(dev);
	nvdimm_bus_unlock(dev);

	return rc;
}
static DEVICE_ATTR_RW(namespace);

static struct attribute *nd_btt_attributes[] = {
	&dev_attr_sector_size.attr,
	&dev_attr_namespace.attr,
	&dev_attr_uuid.attr,
	NULL,
};

static struct attribute_group nd_btt_attribute_group = {
	.attrs = nd_btt_attributes,
};

static const struct attribute_group *nd_btt_attribute_groups[] = {
	&nd_btt_attribute_group,
	&nd_device_attribute_group,
	&nd_numa_attribute_group,
	NULL,
};

static struct device *__nd_btt_create(struct nd_region *nd_region,
		unsigned long lbasize, u8 *uuid,
		struct nd_namespace_common *ndns)
{
	struct nd_btt *nd_btt;
	struct device *dev;

	nd_btt = kzalloc(sizeof(*nd_btt), GFP_KERNEL);
	if (!nd_btt)
		return NULL;

	nd_btt->id = ida_simple_get(&nd_region->btt_ida, 0, 0, GFP_KERNEL);
	if (nd_btt->id < 0) {
		kfree(nd_btt);
		return NULL;
	}

	nd_btt->lbasize = lbasize;
	if (uuid)
		uuid = kmemdup(uuid, 16, GFP_KERNEL);
	nd_btt->uuid = uuid;
	dev = &nd_btt->dev;
	dev_set_name(dev, "btt%d.%d", nd_region->id, nd_btt->id);
	dev->parent = &nd_region->dev;
	dev->type = &nd_btt_device_type;
	dev->groups = nd_btt_attribute_groups;
	device_initialize(&nd_btt->dev);
	if (ndns && !__nd_btt_attach_ndns(nd_btt, ndns)) {
		dev_dbg(&ndns->dev, "%s failed, already claimed by %s\n",
				__func__, dev_name(ndns->claim));
		put_device(dev);
		return NULL;
	}
	return dev;
}

struct device *nd_btt_create(struct nd_region *nd_region)
{
	struct device *dev = __nd_btt_create(nd_region, 0, NULL, NULL);

	if (dev)
		__nd_device_register(dev);
	return dev;
}

/*
 * nd_btt_sb_checksum: compute checksum for btt info block
 *
 * Returns a fletcher64 checksum of everything in the given info block
 * except the last field (since that's where the checksum lives).
 */
u64 nd_btt_sb_checksum(struct btt_sb *btt_sb)
{
	u64 sum;
	__le64 sum_save;

	sum_save = btt_sb->checksum;
	btt_sb->checksum = 0;
	sum = nd_fletcher64(btt_sb, sizeof(*btt_sb), 1);
	btt_sb->checksum = sum_save;
	return sum;
}
EXPORT_SYMBOL(nd_btt_sb_checksum);

static int __nd_btt_probe(struct nd_btt *nd_btt,
		struct nd_namespace_common *ndns, struct btt_sb *btt_sb)
{
	u64 checksum;

	if (!btt_sb || !ndns || !nd_btt)
		return -ENODEV;

	if (nvdimm_read_bytes(ndns, SZ_4K, btt_sb, sizeof(*btt_sb)))
		return -ENXIO;

	if (nvdimm_namespace_capacity(ndns) < SZ_16M)
		return -ENXIO;

	if (memcmp(btt_sb->signature, BTT_SIG, BTT_SIG_LEN) != 0)
		return -ENODEV;

	checksum = le64_to_cpu(btt_sb->checksum);
	btt_sb->checksum = 0;
	if (checksum != nd_btt_sb_checksum(btt_sb))
		return -ENODEV;
	btt_sb->checksum = cpu_to_le64(checksum);

	nd_btt->lbasize = le32_to_cpu(btt_sb->external_lbasize);
	nd_btt->uuid = kmemdup(btt_sb->uuid, 16, GFP_KERNEL);
	if (!nd_btt->uuid)
		return -ENOMEM;

	__nd_device_register(&nd_btt->dev);

	return 0;
}

int nd_btt_probe(struct nd_namespace_common *ndns, void *drvdata)
{
	int rc;
	struct device *dev;
	struct btt_sb *btt_sb;
	struct nd_region *nd_region = to_nd_region(ndns->dev.parent);

	if (ndns->force_raw)
		return -ENODEV;

	nvdimm_bus_lock(&ndns->dev);
	dev = __nd_btt_create(nd_region, 0, NULL, ndns);
	nvdimm_bus_unlock(&ndns->dev);
	if (!dev)
		return -ENOMEM;
	dev_set_drvdata(dev, drvdata);
	btt_sb = kzalloc(sizeof(*btt_sb), GFP_KERNEL);
	rc = __nd_btt_probe(to_nd_btt(dev), ndns, btt_sb);
	kfree(btt_sb);
	dev_dbg(&ndns->dev, "%s: btt: %s\n", __func__,
			rc == 0 ? dev_name(dev) : "<none>");
	if (rc < 0) {
		__nd_btt_detach_ndns(to_nd_btt(dev));
		put_device(dev);
	}

	return rc;
}
EXPORT_SYMBOL(nd_btt_probe);
