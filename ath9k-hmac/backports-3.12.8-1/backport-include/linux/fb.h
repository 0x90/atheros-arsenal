#ifndef __BACKPORT_FB_H
#define __BACKPORT_FB_H
#include_next <linux/fb.h>
#include <linux/version.h>

#if LINUX_VERSION_CODE < KERNEL_VERSION(3,10,0)
/*
 * This is a linux-next data structure element collateral evolution,
 * we use a wrapper to avoid #ifdef hell to backport it. This allows
 * us to use a simple fb_info_skip_vt_switch() replacement for when
 * the new data structure element is used. If coccinelle SmPL grammar
 * could be used to express the transformation for us on compat-drivers
 * it means we'd need to express it only once. If the structure element
 * collateral evolution were to be used *at development* time and we'd
 * have a way to express the inverse through SmPL we'd be able to
 * backport this collateral evolution automatically for any new driver
 * that used it. We'd use coccinelle to look for it and do the
 * transformations for us based on the original commit (maybe SmPL
 * would be listed on the commit log.
 *
 * We may need the LINUX_BACKPORT() call that adds the backport_
 * prefix for older kernels than 3.10 if distros decide to
 * add this same static inline themselves (although unlikely).
 */
#define fb_enable_skip_vt_switch LINUX_BACKPORT(fb_enable_skip_vt_switch)
static inline void fb_enable_skip_vt_switch(struct fb_info *info)
{
}
#else /* kernel is >= 3.10 */
/*
 * We'd delete this upstream ever got this, we use our
 * backport_ prefix with LINUX_BACKPORT() so that if this
 * does get upstream we would not have to add another ifdef
 * here for the kernels in between v3.10.. up to the point
 * the routine would have gotten added, we'd just delete this
 * #else condition completely. If we didn't have this and
 * say 3.12 added the static inline upstream, we'd have a
 * clash on the backport for 3.12 as the routine would
 * already be defined *but* we'd need it for 3.11.
 */
#define fb_enable_skip_vt_switch LINUX_BACKPORT(fb_enable_skip_vt_switch)
static inline void fb_enable_skip_vt_switch(struct fb_info *info)
{
	info->skip_vt_switch = true;
}
#endif /* LINUX_VERSION_CODE < KERNEL_VERSION(3,10,0) */

#endif /* __BACKPORT_FB_H */
