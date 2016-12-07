#ifndef _COMPAT_LINUX_FS_H
#define _COMPAT_LINUX_FS_H
#include_next <linux/fs.h>
#include <linux/version.h>
/*
 * some versions don't have this and thus don't
 * include it from the original fs.h
 */
#include <linux/uidgid.h>

#if (LINUX_VERSION_CODE < KERNEL_VERSION(3,4,0))
#define simple_open LINUX_BACKPORT(simple_open)
extern int simple_open(struct inode *inode, struct file *file);
#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION(3,9,0)
/**
 * backport of:
 *
 * commit 496ad9aa8ef448058e36ca7a787c61f2e63f0f54
 * Author: Al Viro <viro@zeniv.linux.org.uk>
 * Date:   Wed Jan 23 17:07:38 2013 -0500
 *
 *     new helper: file_inode(file)
 */
static inline struct inode *file_inode(struct file *f)
{
	return f->f_path.dentry->d_inode;
}
#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,35)
#define noop_llseek LINUX_BACKPORT(noop_llseek)
extern loff_t noop_llseek(struct file *file, loff_t offset, int origin);

#define simple_write_to_buffer LINUX_BACKPORT(simple_write_to_buffer)
extern ssize_t simple_write_to_buffer(void *to, size_t available, loff_t *ppos,
		const void __user *from, size_t count);
#endif

#endif	/* _COMPAT_LINUX_FS_H */
