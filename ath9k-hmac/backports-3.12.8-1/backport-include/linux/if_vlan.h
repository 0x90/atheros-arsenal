#ifndef __BACKPORT_LINUX_IF_VLAN_H_
#define __BACKPORT_LINUX_IF_VLAN_H_
#include_next <linux/if_vlan.h>

#if LINUX_VERSION_CODE < KERNEL_VERSION(3,10,0)
#define vlan_insert_tag(__skb, __vlan_proto, __vlan_tci)	vlan_insert_tag(__skb, __vlan_tci)
#define __vlan_put_tag(__skb, __vlan_proto, __vlan_tci)		__vlan_put_tag(__skb, __vlan_tci)
#define vlan_put_tag(__skb, __vlan_proto, __vlan_tci)		vlan_put_tag(__skb, __vlan_tci)
#define __vlan_hwaccel_put_tag(__skb, __vlan_proto, __vlan_tag)	__vlan_hwaccel_put_tag(__skb, __vlan_tag)

static inline bool vlan_hw_offload_capable(netdev_features_t features,
					   __be16 proto)
{
	if (proto == htons(ETH_P_8021Q) && features & NETIF_F_HW_VLAN_CTAG_TX)
		return true;
	return false;
}
#endif 

#endif /* __BACKPORT_LINUX_IF_VLAN_H_ */
