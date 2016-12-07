#ifndef __BACKPORT_NET_GENETLINK_H
#define __BACKPORT_NET_GENETLINK_H
#include_next <net/genetlink.h>

/* this is for patches we apply */
#if LINUX_VERSION_CODE < KERNEL_VERSION(3,7,0)
#define genl_info_snd_portid(__genl_info) (__genl_info->snd_pid)
#else
#define genl_info_snd_portid(__genl_info) (__genl_info->snd_portid)
#endif

#ifndef GENLMSG_DEFAULT_SIZE
#define GENLMSG_DEFAULT_SIZE (NLMSG_DEFAULT_SIZE - GENL_HDRLEN)
#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION(3,1,0)
#define genl_dump_check_consistent(cb, user_hdr, family)
#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,37)
struct compat_genl_info {
	struct genl_info *info;

	u32 snd_seq;
	u32 snd_pid;
	struct genlmsghdr *genlhdr;
	struct nlattr **attrs;
	void *user_ptr[2];
};
#define genl_info compat_genl_info

struct compat_genl_ops {
	struct genl_ops ops;

	u8 cmd;
	u8 internal_flags;
	unsigned int flags;
	const struct nla_policy *policy;

	int (*doit)(struct sk_buff *skb, struct genl_info *info);
	int (*dumpit)(struct sk_buff *skb, struct netlink_callback *cb);
	int (*done)(struct netlink_callback *cb);
};
#define genl_ops compat_genl_ops

struct compat_genl_family {
	struct genl_family family;

	struct list_head list;

	unsigned int id, hdrsize, version, maxattr;
	const char *name;
	bool netnsok;

	struct nlattr **attrbuf;

	int (*pre_doit)(struct genl_ops *ops, struct sk_buff *skb,
			struct genl_info *info);

	void (*post_doit)(struct genl_ops *ops, struct sk_buff *skb,
			  struct genl_info *info);
};

#define genl_family compat_genl_family

#define genl_register_family_with_ops compat_genl_register_family_with_ops

int genl_register_family_with_ops(struct genl_family *family,
				  struct genl_ops *ops, size_t n_ops);

#define genl_unregister_family compat_genl_unregister_family

int genl_unregister_family(struct genl_family *family);

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,32))
#define genl_info_net(_info) genl_info_net((_info)->info)
#endif

#define genlmsg_reply(_msg, _info) genlmsg_reply(_msg, (_info)->info)
#define genlmsg_put(_skb, _pid, _seq, _fam, _flags, _cmd) genlmsg_put(_skb, _pid, _seq, &(_fam)->family, _flags, _cmd)
#define genl_register_mc_group(_fam, _grp) genl_register_mc_group(&(_fam)->family, _grp)
#define genl_unregister_mc_group(_fam, _grp) genl_unregister_mc_group(&(_fam)->family, _grp)
#endif /* < 2.6.37 */

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,32)
/*
 * struct genl_multicast_group was made netns aware through
 * patch "genetlink: make netns aware" by johannes, we just
 * force this to always use the default init_net
 */
#define genl_info_net(x) &init_net
/* Just use init_net for older kernels */
#define get_net_ns_by_pid(x) &init_net

/* net namespace is lost */
#define genlmsg_multicast_netns(a, b, c, d, e)	genlmsg_multicast(b, c, d, e)
#define genlmsg_multicast_allns(a, b, c, d)	genlmsg_multicast(a, b, c, d)
#define genlmsg_unicast(net, skb, pid)	genlmsg_unicast(skb, pid)
#endif

#endif /* __BACKPORT_NET_GENETLINK_H */
