#ifndef __BACKPORT_DEBUGFS_H_
#define __BACKPORT_DEBUGFS_H_
#include_next <linux/debugfs.h>
#include <linux/version.h>
#include <linux/device.h>
#include <generated/utsrelease.h>

#if LINUX_VERSION_CODE < KERNEL_VERSION(3,19,0)
#define debugfs_create_devm_seqfile LINUX_BACKPORT(debugfs_create_devm_seqfile)
#if defined(CONFIG_DEBUG_FS)
struct dentry *debugfs_create_devm_seqfile(struct device *dev, const char *name,
					   struct dentry *parent,
					   int (*read_fn)(struct seq_file *s,
							  void *data));
#else
static inline struct dentry *debugfs_create_devm_seqfile(struct device *dev,
							 const char *name,
							 struct dentry *parent,
					   int (*read_fn)(struct seq_file *s,
							  void *data))
{
	return ERR_PTR(-ENODEV);
}
#endif /* CONFIG_DEBUG_FS */
#endif /* LINUX_VERSION_CODE < KERNEL_VERSION(3,19,0) */

#endif /* __BACKPORT_DEBUGFS_H_ */
