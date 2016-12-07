/*
 * Copyright (c) 2005-2011 Atheros Communications Inc.
 * Copyright (c) 2011-2014 Qualcomm Atheros, Inc.
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#ifndef _WMI_OPS_H_
#define _WMI_OPS_H_

struct ath10k;
struct athp_buf;

struct wmi_ops {
	void (*rx)(struct ath10k *ar, struct athp_buf *pbuf);
	void (*map_svc)(const __le32 *in, unsigned long *out, size_t len);

	int (*pull_scan)(struct ath10k *ar, struct athp_buf *pbuf,
			 struct wmi_scan_ev_arg *arg);
	int (*pull_mgmt_rx)(struct ath10k *ar, struct athp_buf *pbuf,
			    struct wmi_mgmt_rx_ev_arg *arg);
	int (*pull_ch_info)(struct ath10k *ar, struct athp_buf *pbuf,
			    struct wmi_ch_info_ev_arg *arg);
	int (*pull_vdev_start)(struct ath10k *ar, struct athp_buf *pbuf,
			       struct wmi_vdev_start_ev_arg *arg);
	int (*pull_peer_kick)(struct ath10k *ar, struct athp_buf *pbuf,
			      struct wmi_peer_kick_ev_arg *arg);
	int (*pull_swba)(struct ath10k *ar, struct athp_buf *pbuf,
			 struct wmi_swba_ev_arg *arg);
	int (*pull_phyerr_hdr)(struct ath10k *ar, struct athp_buf *pbuf,
			       struct wmi_phyerr_hdr_arg *arg);
	int (*pull_phyerr)(struct ath10k *ar, const void *phyerr_buf,
			   int left_len, struct wmi_phyerr_ev_arg *arg);
	int (*pull_svc_rdy)(struct ath10k *ar, struct athp_buf *pbuf,
			    struct wmi_svc_rdy_ev_arg *arg);
	int (*pull_rdy)(struct ath10k *ar, struct athp_buf *pbuf,
			struct wmi_rdy_ev_arg *arg);
	int (*pull_fw_stats)(struct ath10k *ar, struct athp_buf *pbuf,
			     struct ath10k_fw_stats *stats);
	int (*pull_roam_ev)(struct ath10k *ar, struct athp_buf *pbuf,
			    struct wmi_roam_ev_arg *arg);
	int (*pull_wow_event)(struct ath10k *ar, struct athp_buf *pbuf,
			      struct wmi_wow_ev_arg *arg);
	enum wmi_txbf_conf (*get_txbf_conf_scheme)(struct ath10k *ar);

	struct athp_buf *(*gen_pdev_suspend)(struct ath10k *ar, u32 suspend_opt);
	struct athp_buf *(*gen_pdev_resume)(struct ath10k *ar);
	struct athp_buf *(*gen_pdev_set_rd)(struct ath10k *ar, u16 rd, u16 rd2g,
					   u16 rd5g, u16 ctl2g, u16 ctl5g,
					   enum wmi_dfs_region dfs_reg);
	struct athp_buf *(*gen_pdev_set_param)(struct ath10k *ar, u32 id,
					      u32 value);
	struct athp_buf *(*gen_init)(struct ath10k *ar);
	struct athp_buf *(*gen_start_scan)(struct ath10k *ar,
					  const struct wmi_start_scan_arg *arg);
	struct athp_buf *(*gen_stop_scan)(struct ath10k *ar,
					 const struct wmi_stop_scan_arg *arg);
	struct athp_buf *(*gen_vdev_create)(struct ath10k *ar, u32 vdev_id,
					   enum wmi_vdev_type type,
					   enum wmi_vdev_subtype subtype,
					   const u8 macaddr[ETH_ALEN]);
	struct athp_buf *(*gen_vdev_delete)(struct ath10k *ar, u32 vdev_id);
	struct athp_buf *(*gen_vdev_start)(struct ath10k *ar,
					  const struct wmi_vdev_start_request_arg *arg,
					  bool restart);
	struct athp_buf *(*gen_vdev_stop)(struct ath10k *ar, u32 vdev_id);
	struct athp_buf *(*gen_vdev_up)(struct ath10k *ar, u32 vdev_id, u32 aid,
				       const u8 *bssid);
	struct athp_buf *(*gen_vdev_down)(struct ath10k *ar, u32 vdev_id);
	struct athp_buf *(*gen_vdev_set_param)(struct ath10k *ar, u32 vdev_id,
					      u32 param_id, u32 param_value);
	struct athp_buf *(*gen_vdev_install_key)(struct ath10k *ar,
						const struct wmi_vdev_install_key_arg *arg);
	struct athp_buf *(*gen_vdev_spectral_conf)(struct ath10k *ar,
						  const struct wmi_vdev_spectral_conf_arg *arg);
	struct athp_buf *(*gen_vdev_spectral_enable)(struct ath10k *ar, u32 vdev_id,
						    u32 trigger, u32 enable);
	struct athp_buf *(*gen_vdev_wmm_conf)(struct ath10k *ar, u32 vdev_id,
					     const struct wmi_wmm_params_all_arg *arg);
	struct athp_buf *(*gen_peer_create)(struct ath10k *ar, u32 vdev_id,
					   const u8 peer_addr[ETH_ALEN],
					   enum wmi_peer_type peer_type);
	struct athp_buf *(*gen_peer_delete)(struct ath10k *ar, u32 vdev_id,
					   const u8 peer_addr[ETH_ALEN]);
	struct athp_buf *(*gen_peer_flush)(struct ath10k *ar, u32 vdev_id,
					  const u8 peer_addr[ETH_ALEN],
					  u32 tid_bitmap);
	struct athp_buf *(*gen_peer_set_param)(struct ath10k *ar, u32 vdev_id,
					      const u8 *peer_addr,
					      enum wmi_peer_param param_id,
					      u32 param_value);
	struct athp_buf *(*gen_peer_assoc)(struct ath10k *ar,
					  const struct wmi_peer_assoc_complete_arg *arg);
	struct athp_buf *(*gen_set_psmode)(struct ath10k *ar, u32 vdev_id,
					  enum wmi_sta_ps_mode psmode);
	struct athp_buf *(*gen_set_sta_ps)(struct ath10k *ar, u32 vdev_id,
					  enum wmi_sta_powersave_param param_id,
					  u32 value);
	struct athp_buf *(*gen_set_ap_ps)(struct ath10k *ar, u32 vdev_id,
					 const u8 *mac,
					 enum wmi_ap_ps_peer_param param_id,
					 u32 value);
	struct athp_buf *(*gen_scan_chan_list)(struct ath10k *ar,
					      const struct wmi_scan_chan_list_arg *arg);
	struct athp_buf *(*gen_beacon_dma)(struct ath10k *ar, u32 vdev_id,
					  const void *bcn, size_t bcn_len,
					  u32 bcn_paddr, bool dtim_zero,
					  bool deliver_cab);
	struct athp_buf *(*gen_pdev_set_wmm)(struct ath10k *ar,
					    const struct wmi_wmm_params_all_arg *arg);
	struct athp_buf *(*gen_request_stats)(struct ath10k *ar, u32 stats_mask);
	struct athp_buf *(*gen_force_fw_hang)(struct ath10k *ar,
					     enum wmi_force_fw_hang_type type,
					     u32 delay_ms);
	struct athp_buf *(*gen_mgmt_tx)(struct ath10k *ar, struct athp_buf *pbuf);
	struct athp_buf *(*gen_dbglog_cfg)(struct ath10k *ar, u32 module_enable,
					  u32 log_level);
	struct athp_buf *(*gen_pktlog_enable)(struct ath10k *ar, u32 filter);
	struct athp_buf *(*gen_pktlog_disable)(struct ath10k *ar);
	struct athp_buf *(*gen_pdev_set_quiet_mode)(struct ath10k *ar,
						   u32 period, u32 duration,
						   u32 next_offset,
						   u32 enabled);
	struct athp_buf *(*gen_pdev_get_temperature)(struct ath10k *ar);
	struct athp_buf *(*gen_addba_clear_resp)(struct ath10k *ar, u32 vdev_id,
						const u8 *mac);
	struct athp_buf *(*gen_addba_send)(struct ath10k *ar, u32 vdev_id,
					  const u8 *mac, u32 tid, u32 buf_size);
	struct athp_buf *(*gen_addba_set_resp)(struct ath10k *ar, u32 vdev_id,
					      const u8 *mac, u32 tid,
					      u32 status);
	struct athp_buf *(*gen_delba_send)(struct ath10k *ar, u32 vdev_id,
					  const u8 *mac, u32 tid, u32 initiator,
					  u32 reason);
	struct athp_buf *(*gen_bcn_tmpl)(struct ath10k *ar, u32 vdev_id,
					u32 tim_ie_offset, struct athp_buf *bcn,
					u32 prb_caps, u32 prb_erp,
					void *prb_ies, size_t prb_ies_len);
	struct athp_buf *(*gen_prb_tmpl)(struct ath10k *ar, u32 vdev_id,
					struct athp_buf *bcn);
	struct athp_buf *(*gen_p2p_go_bcn_ie)(struct ath10k *ar, u32 vdev_id,
					     const u8 *p2p_ie);
	struct athp_buf *(*gen_vdev_sta_uapsd)(struct ath10k *ar, u32 vdev_id,
					      const u8 peer_addr[ETH_ALEN],
					      const struct wmi_sta_uapsd_auto_trig_arg *args,
					      u32 num_ac);
	struct athp_buf *(*gen_sta_keepalive)(struct ath10k *ar,
					     const struct wmi_sta_keepalive_arg *arg);
	struct athp_buf *(*gen_wow_enable)(struct ath10k *ar);
	struct athp_buf *(*gen_wow_add_wakeup_event)(struct ath10k *ar, u32 vdev_id,
						    enum wmi_wow_wakeup_event event,
						    u32 enable);
	struct athp_buf *(*gen_wow_host_wakeup_ind)(struct ath10k *ar);
	struct athp_buf *(*gen_wow_add_pattern)(struct ath10k *ar, u32 vdev_id,
					       u32 pattern_id,
					       const u8 *pattern,
					       const u8 *mask,
					       int pattern_len,
					       int pattern_offset);
	struct athp_buf *(*gen_wow_del_pattern)(struct ath10k *ar, u32 vdev_id,
					       u32 pattern_id);
	struct athp_buf *(*gen_update_fw_tdls_state)(struct ath10k *ar,
						    u32 vdev_id,
						    enum wmi_tdls_state state);
	struct athp_buf *(*gen_tdls_peer_update)(struct ath10k *ar,
						const struct wmi_tdls_peer_update_cmd_arg *arg,
						const struct wmi_tdls_peer_capab_arg *cap,
						const struct wmi_channel_arg *chan);
	struct athp_buf *(*gen_adaptive_qcs)(struct ath10k *ar, bool enable);
};

int ath10k_wmi_cmd_send(struct ath10k *ar, struct athp_buf *pbuf, u32 cmd_id);

static inline int
ath10k_wmi_rx(struct ath10k *ar, struct athp_buf *pbuf)
{
	if (WARN_ON_ONCE(!ar->wmi.ops->rx))
		return -EOPNOTSUPP;

	ar->wmi.ops->rx(ar, pbuf);
	return 0;
}

static inline int
ath10k_wmi_map_svc(struct ath10k *ar, const __le32 *in, unsigned long *out,
		   size_t len)
{
	if (!ar->wmi.ops->map_svc)
		return -EOPNOTSUPP;

	ar->wmi.ops->map_svc(in, out, len);
	return 0;
}

static inline int
ath10k_wmi_pull_scan(struct ath10k *ar, struct athp_buf *pbuf,
		     struct wmi_scan_ev_arg *arg)
{
	if (!ar->wmi.ops->pull_scan)
		return -EOPNOTSUPP;

	return ar->wmi.ops->pull_scan(ar, pbuf, arg);
}

static inline int
ath10k_wmi_pull_mgmt_rx(struct ath10k *ar, struct athp_buf *pbuf,
			struct wmi_mgmt_rx_ev_arg *arg)
{
	if (!ar->wmi.ops->pull_mgmt_rx)
		return -EOPNOTSUPP;

	return ar->wmi.ops->pull_mgmt_rx(ar, pbuf, arg);
}

static inline int
ath10k_wmi_pull_ch_info(struct ath10k *ar, struct athp_buf *pbuf,
			struct wmi_ch_info_ev_arg *arg)
{
	if (!ar->wmi.ops->pull_ch_info)
		return -EOPNOTSUPP;

	return ar->wmi.ops->pull_ch_info(ar, pbuf, arg);
}

static inline int
ath10k_wmi_pull_vdev_start(struct ath10k *ar, struct athp_buf *pbuf,
			   struct wmi_vdev_start_ev_arg *arg)
{
	if (!ar->wmi.ops->pull_vdev_start)
		return -EOPNOTSUPP;

	return ar->wmi.ops->pull_vdev_start(ar, pbuf, arg);
}

static inline int
ath10k_wmi_pull_peer_kick(struct ath10k *ar, struct athp_buf *pbuf,
			  struct wmi_peer_kick_ev_arg *arg)
{
	if (!ar->wmi.ops->pull_peer_kick)
		return -EOPNOTSUPP;

	return ar->wmi.ops->pull_peer_kick(ar, pbuf, arg);
}

static inline int
ath10k_wmi_pull_swba(struct ath10k *ar, struct athp_buf *pbuf,
		     struct wmi_swba_ev_arg *arg)
{
	if (!ar->wmi.ops->pull_swba)
		return -EOPNOTSUPP;

	return ar->wmi.ops->pull_swba(ar, pbuf, arg);
}

static inline int
ath10k_wmi_pull_phyerr_hdr(struct ath10k *ar, struct athp_buf *pbuf,
			   struct wmi_phyerr_hdr_arg *arg)
{
	if (!ar->wmi.ops->pull_phyerr_hdr)
		return -EOPNOTSUPP;

	return ar->wmi.ops->pull_phyerr_hdr(ar, pbuf, arg);
}

static inline int
ath10k_wmi_pull_phyerr(struct ath10k *ar, const void *phyerr_buf,
		       int left_len, struct wmi_phyerr_ev_arg *arg)
{
	if (!ar->wmi.ops->pull_phyerr)
		return -EOPNOTSUPP;

	return ar->wmi.ops->pull_phyerr(ar, phyerr_buf, left_len, arg);
}

static inline int
ath10k_wmi_pull_svc_rdy(struct ath10k *ar, struct athp_buf *pbuf,
			struct wmi_svc_rdy_ev_arg *arg)
{
	if (!ar->wmi.ops->pull_svc_rdy)
		return -EOPNOTSUPP;

	return ar->wmi.ops->pull_svc_rdy(ar, pbuf, arg);
}

static inline int
ath10k_wmi_pull_rdy(struct ath10k *ar, struct athp_buf *pbuf,
		    struct wmi_rdy_ev_arg *arg)
{
	if (!ar->wmi.ops->pull_rdy)
		return -EOPNOTSUPP;

	return ar->wmi.ops->pull_rdy(ar, pbuf, arg);
}

static inline int
ath10k_wmi_pull_fw_stats(struct ath10k *ar, struct athp_buf *pbuf,
			 struct ath10k_fw_stats *stats)
{
	if (!ar->wmi.ops->pull_fw_stats)
		return -EOPNOTSUPP;

	return ar->wmi.ops->pull_fw_stats(ar, pbuf, stats);
}

static inline int
ath10k_wmi_pull_roam_ev(struct ath10k *ar, struct athp_buf *pbuf,
			struct wmi_roam_ev_arg *arg)
{
	if (!ar->wmi.ops->pull_roam_ev)
		return -EOPNOTSUPP;

	return ar->wmi.ops->pull_roam_ev(ar, pbuf, arg);
}

static inline int
ath10k_wmi_pull_wow_event(struct ath10k *ar, struct athp_buf *pbuf,
			  struct wmi_wow_ev_arg *arg)
{
	if (!ar->wmi.ops->pull_wow_event)
		return -EOPNOTSUPP;

	return ar->wmi.ops->pull_wow_event(ar, pbuf, arg);
}

static inline enum wmi_txbf_conf
ath10k_wmi_get_txbf_conf_scheme(struct ath10k *ar)
{
	if (!ar->wmi.ops->get_txbf_conf_scheme)
		return WMI_TXBF_CONF_UNSUPPORTED;

	return ar->wmi.ops->get_txbf_conf_scheme(ar);
}

static inline int
ath10k_wmi_mgmt_tx(struct ath10k *ar, struct athp_buf *msdu)
{
//	struct ieee80211_tx_info *info = ATH10K_SKB_TX_CB(msdu);
	struct athp_buf *pbuf;
	int ret;

	if (!ar->wmi.ops->gen_mgmt_tx)
		return -EOPNOTSUPP;

	pbuf = ar->wmi.ops->gen_mgmt_tx(ar, msdu);
	if (IS_ERR(pbuf))
		return PTR_ERR(pbuf);

	ret = ath10k_wmi_cmd_send(ar, pbuf, ar->wmi.cmd->mgmt_tx_cmdid);
	if (ret)
		return ret;

#if 0
	/* FIXME There's no ACK event for Management Tx. This probably
	 * shouldn't be called here either. */
	info->flags |= IEEE80211_TX_STAT_ACK;
	ieee80211_tx_status_irqsafe(ar->hw, msdu);
#endif
	device_printf(ar->sc_dev, "%s: TODO\n", __func__);
	return 0;
}

static inline int
ath10k_wmi_pdev_set_regdomain(struct ath10k *ar, u16 rd, u16 rd2g, u16 rd5g,
			      u16 ctl2g, u16 ctl5g,
			      enum wmi_dfs_region dfs_reg)
{
	struct athp_buf *pbuf;

	if (!ar->wmi.ops->gen_pdev_set_rd)
		return -EOPNOTSUPP;

	pbuf = ar->wmi.ops->gen_pdev_set_rd(ar, rd, rd2g, rd5g, ctl2g, ctl5g,
					   dfs_reg);
	if (IS_ERR(pbuf))
		return PTR_ERR(pbuf);

	return ath10k_wmi_cmd_send(ar, pbuf,
				   ar->wmi.cmd->pdev_set_regdomain_cmdid);
}

static inline int
ath10k_wmi_pdev_suspend_target(struct ath10k *ar, u32 suspend_opt)
{
	struct athp_buf *pbuf;

	if (!ar->wmi.ops->gen_pdev_suspend)
		return -EOPNOTSUPP;

	pbuf = ar->wmi.ops->gen_pdev_suspend(ar, suspend_opt);
	if (IS_ERR(pbuf))
		return PTR_ERR(pbuf);

	return ath10k_wmi_cmd_send(ar, pbuf, ar->wmi.cmd->pdev_suspend_cmdid);
}

static inline int
ath10k_wmi_pdev_resume_target(struct ath10k *ar)
{
	struct athp_buf *pbuf;

	if (!ar->wmi.ops->gen_pdev_resume)
		return -EOPNOTSUPP;

	pbuf = ar->wmi.ops->gen_pdev_resume(ar);
	if (IS_ERR(pbuf))
		return PTR_ERR(pbuf);

	return ath10k_wmi_cmd_send(ar, pbuf, ar->wmi.cmd->pdev_resume_cmdid);
}

static inline int
ath10k_wmi_pdev_set_param(struct ath10k *ar, u32 id, u32 value)
{
	struct athp_buf *pbuf;

	if (!ar->wmi.ops->gen_pdev_set_param)
		return -EOPNOTSUPP;

	pbuf = ar->wmi.ops->gen_pdev_set_param(ar, id, value);
	if (IS_ERR(pbuf))
		return PTR_ERR(pbuf);

	return ath10k_wmi_cmd_send(ar, pbuf, ar->wmi.cmd->pdev_set_param_cmdid);
}

static inline int
ath10k_wmi_cmd_init(struct ath10k *ar)
{
	struct athp_buf *pbuf;

	if (!ar->wmi.ops->gen_init)
		return -EOPNOTSUPP;

	pbuf = ar->wmi.ops->gen_init(ar);
	if (IS_ERR(pbuf))
		return PTR_ERR(pbuf);

	return ath10k_wmi_cmd_send(ar, pbuf, ar->wmi.cmd->init_cmdid);
}

static inline int
ath10k_wmi_start_scan(struct ath10k *ar,
		      const struct wmi_start_scan_arg *arg)
{
	struct athp_buf *pbuf;

	if (!ar->wmi.ops->gen_start_scan)
		return -EOPNOTSUPP;

	pbuf = ar->wmi.ops->gen_start_scan(ar, arg);
	if (IS_ERR(pbuf))
		return PTR_ERR(pbuf);

	return ath10k_wmi_cmd_send(ar, pbuf, ar->wmi.cmd->start_scan_cmdid);
}

static inline int
ath10k_wmi_stop_scan(struct ath10k *ar, const struct wmi_stop_scan_arg *arg)
{
	struct athp_buf *pbuf;

	if (!ar->wmi.ops->gen_stop_scan)
		return -EOPNOTSUPP;

	pbuf = ar->wmi.ops->gen_stop_scan(ar, arg);
	if (IS_ERR(pbuf))
		return PTR_ERR(pbuf);

	return ath10k_wmi_cmd_send(ar, pbuf, ar->wmi.cmd->stop_scan_cmdid);
}

static inline int
ath10k_wmi_vdev_create(struct ath10k *ar, u32 vdev_id,
		       enum wmi_vdev_type type,
		       enum wmi_vdev_subtype subtype,
		       const u8 macaddr[ETH_ALEN])
{
	struct athp_buf *pbuf;

	if (!ar->wmi.ops->gen_vdev_create)
		return -EOPNOTSUPP;

	pbuf = ar->wmi.ops->gen_vdev_create(ar, vdev_id, type, subtype, macaddr);
	if (IS_ERR(pbuf))
		return PTR_ERR(pbuf);

	return ath10k_wmi_cmd_send(ar, pbuf, ar->wmi.cmd->vdev_create_cmdid);
}

static inline int
ath10k_wmi_vdev_delete(struct ath10k *ar, u32 vdev_id)
{
	struct athp_buf *pbuf;

	if (!ar->wmi.ops->gen_vdev_delete)
		return -EOPNOTSUPP;

	pbuf = ar->wmi.ops->gen_vdev_delete(ar, vdev_id);
	if (IS_ERR(pbuf))
		return PTR_ERR(pbuf);

	return ath10k_wmi_cmd_send(ar, pbuf, ar->wmi.cmd->vdev_delete_cmdid);
}

static inline int
ath10k_wmi_vdev_start(struct ath10k *ar,
		      const struct wmi_vdev_start_request_arg *arg)
{
	struct athp_buf *pbuf;

	if (!ar->wmi.ops->gen_vdev_start)
		return -EOPNOTSUPP;

	pbuf = ar->wmi.ops->gen_vdev_start(ar, arg, false);
	if (IS_ERR(pbuf))
		return PTR_ERR(pbuf);

	return ath10k_wmi_cmd_send(ar, pbuf,
				   ar->wmi.cmd->vdev_start_request_cmdid);
}

static inline int
ath10k_wmi_vdev_restart(struct ath10k *ar,
			const struct wmi_vdev_start_request_arg *arg)
{
	struct athp_buf *pbuf;

	if (!ar->wmi.ops->gen_vdev_start)
		return -EOPNOTSUPP;

	pbuf = ar->wmi.ops->gen_vdev_start(ar, arg, true);
	if (IS_ERR(pbuf))
		return PTR_ERR(pbuf);

	return ath10k_wmi_cmd_send(ar, pbuf,
				   ar->wmi.cmd->vdev_restart_request_cmdid);
}

static inline int
ath10k_wmi_vdev_stop(struct ath10k *ar, u32 vdev_id)
{
	struct athp_buf *pbuf;

	if (!ar->wmi.ops->gen_vdev_stop)
		return -EOPNOTSUPP;

	pbuf = ar->wmi.ops->gen_vdev_stop(ar, vdev_id);
	if (IS_ERR(pbuf))
		return PTR_ERR(pbuf);

	return ath10k_wmi_cmd_send(ar, pbuf, ar->wmi.cmd->vdev_stop_cmdid);
}

static inline int
ath10k_wmi_vdev_up(struct ath10k *ar, u32 vdev_id, u32 aid, const u8 *bssid)
{
	struct athp_buf *pbuf;

	if (!ar->wmi.ops->gen_vdev_up)
		return -EOPNOTSUPP;

	pbuf = ar->wmi.ops->gen_vdev_up(ar, vdev_id, aid, bssid);
	if (IS_ERR(pbuf))
		return PTR_ERR(pbuf);

	return ath10k_wmi_cmd_send(ar, pbuf, ar->wmi.cmd->vdev_up_cmdid);
}

static inline int
ath10k_wmi_vdev_down(struct ath10k *ar, u32 vdev_id)
{
	struct athp_buf *pbuf;

	if (!ar->wmi.ops->gen_vdev_down)
		return -EOPNOTSUPP;

	pbuf = ar->wmi.ops->gen_vdev_down(ar, vdev_id);
	if (IS_ERR(pbuf))
		return PTR_ERR(pbuf);

	return ath10k_wmi_cmd_send(ar, pbuf, ar->wmi.cmd->vdev_down_cmdid);
}

static inline int
ath10k_wmi_vdev_set_param(struct ath10k *ar, u32 vdev_id, u32 param_id,
			  u32 param_value)
{
	struct athp_buf *pbuf;

	if (!ar->wmi.ops->gen_vdev_set_param)
		return -EOPNOTSUPP;

	pbuf = ar->wmi.ops->gen_vdev_set_param(ar, vdev_id, param_id,
					      param_value);
	if (IS_ERR(pbuf))
		return PTR_ERR(pbuf);

	return ath10k_wmi_cmd_send(ar, pbuf, ar->wmi.cmd->vdev_set_param_cmdid);
}

static inline int
ath10k_wmi_vdev_install_key(struct ath10k *ar,
			    const struct wmi_vdev_install_key_arg *arg)
{
	struct athp_buf *pbuf;

	if (!ar->wmi.ops->gen_vdev_install_key)
		return -EOPNOTSUPP;

	pbuf = ar->wmi.ops->gen_vdev_install_key(ar, arg);
	if (IS_ERR(pbuf))
		return PTR_ERR(pbuf);

	return ath10k_wmi_cmd_send(ar, pbuf,
				   ar->wmi.cmd->vdev_install_key_cmdid);
}

static inline int
ath10k_wmi_vdev_spectral_conf(struct ath10k *ar,
			      const struct wmi_vdev_spectral_conf_arg *arg)
{
	struct athp_buf *pbuf;
	u32 cmd_id;

	pbuf = ar->wmi.ops->gen_vdev_spectral_conf(ar, arg);
	if (IS_ERR(pbuf))
		return PTR_ERR(pbuf);

	cmd_id = ar->wmi.cmd->vdev_spectral_scan_configure_cmdid;
	return ath10k_wmi_cmd_send(ar, pbuf, cmd_id);
}

static inline int
ath10k_wmi_vdev_spectral_enable(struct ath10k *ar, u32 vdev_id, u32 trigger,
				u32 enable)
{
	struct athp_buf *pbuf;
	u32 cmd_id;

	pbuf = ar->wmi.ops->gen_vdev_spectral_enable(ar, vdev_id, trigger,
						    enable);
	if (IS_ERR(pbuf))
		return PTR_ERR(pbuf);

	cmd_id = ar->wmi.cmd->vdev_spectral_scan_enable_cmdid;
	return ath10k_wmi_cmd_send(ar, pbuf, cmd_id);
}

static inline int
ath10k_wmi_vdev_sta_uapsd(struct ath10k *ar, u32 vdev_id,
			  const u8 peer_addr[ETH_ALEN],
			  const struct wmi_sta_uapsd_auto_trig_arg *args,
			  u32 num_ac)
{
	struct athp_buf *pbuf;
	u32 cmd_id;

	if (!ar->wmi.ops->gen_vdev_sta_uapsd)
		return -EOPNOTSUPP;

	pbuf = ar->wmi.ops->gen_vdev_sta_uapsd(ar, vdev_id, peer_addr, args,
					      num_ac);
	if (IS_ERR(pbuf))
		return PTR_ERR(pbuf);

	cmd_id = ar->wmi.cmd->sta_uapsd_auto_trig_cmdid;
	return ath10k_wmi_cmd_send(ar, pbuf, cmd_id);
}

static inline int
ath10k_wmi_vdev_wmm_conf(struct ath10k *ar, u32 vdev_id,
			 const struct wmi_wmm_params_all_arg *arg)
{
	struct athp_buf *pbuf;
	u32 cmd_id;

	pbuf = ar->wmi.ops->gen_vdev_wmm_conf(ar, vdev_id, arg);
	if (IS_ERR(pbuf))
		return PTR_ERR(pbuf);

	cmd_id = ar->wmi.cmd->vdev_set_wmm_params_cmdid;
	return ath10k_wmi_cmd_send(ar, pbuf, cmd_id);
}

static inline int
ath10k_wmi_peer_create(struct ath10k *ar, u32 vdev_id,
		       const u8 peer_addr[ETH_ALEN],
		       enum wmi_peer_type peer_type)
{
	struct athp_buf *pbuf;

	if (!ar->wmi.ops->gen_peer_create)
		return -EOPNOTSUPP;

	pbuf = ar->wmi.ops->gen_peer_create(ar, vdev_id, peer_addr, peer_type);
	if (IS_ERR(pbuf))
		return PTR_ERR(pbuf);

	return ath10k_wmi_cmd_send(ar, pbuf, ar->wmi.cmd->peer_create_cmdid);
}

static inline int
ath10k_wmi_peer_delete(struct ath10k *ar, u32 vdev_id,
		       const u8 peer_addr[ETH_ALEN])
{
	struct athp_buf *pbuf;

	if (!ar->wmi.ops->gen_peer_delete)
		return -EOPNOTSUPP;

	pbuf = ar->wmi.ops->gen_peer_delete(ar, vdev_id, peer_addr);
	if (IS_ERR(pbuf))
		return PTR_ERR(pbuf);

	return ath10k_wmi_cmd_send(ar, pbuf, ar->wmi.cmd->peer_delete_cmdid);
}

static inline int
ath10k_wmi_peer_flush(struct ath10k *ar, u32 vdev_id,
		      const u8 peer_addr[ETH_ALEN], u32 tid_bitmap)
{
	struct athp_buf *pbuf;

	if (!ar->wmi.ops->gen_peer_flush)
		return -EOPNOTSUPP;

	pbuf = ar->wmi.ops->gen_peer_flush(ar, vdev_id, peer_addr, tid_bitmap);
	if (IS_ERR(pbuf))
		return PTR_ERR(pbuf);

	return ath10k_wmi_cmd_send(ar, pbuf, ar->wmi.cmd->peer_flush_tids_cmdid);
}

static inline int
ath10k_wmi_peer_set_param(struct ath10k *ar, u32 vdev_id, const u8 *peer_addr,
			  enum wmi_peer_param param_id, u32 param_value)
{
	struct athp_buf *pbuf;

	if (!ar->wmi.ops->gen_peer_set_param)
		return -EOPNOTSUPP;

	pbuf = ar->wmi.ops->gen_peer_set_param(ar, vdev_id, peer_addr, param_id,
					      param_value);
	if (IS_ERR(pbuf))
		return PTR_ERR(pbuf);

	return ath10k_wmi_cmd_send(ar, pbuf, ar->wmi.cmd->peer_set_param_cmdid);
}

static inline int
ath10k_wmi_set_psmode(struct ath10k *ar, u32 vdev_id,
		      enum wmi_sta_ps_mode psmode)
{
	struct athp_buf *pbuf;

	if (!ar->wmi.ops->gen_set_psmode)
		return -EOPNOTSUPP;

	pbuf = ar->wmi.ops->gen_set_psmode(ar, vdev_id, psmode);
	if (IS_ERR(pbuf))
		return PTR_ERR(pbuf);

	return ath10k_wmi_cmd_send(ar, pbuf,
				   ar->wmi.cmd->sta_powersave_mode_cmdid);
}

static inline int
ath10k_wmi_set_sta_ps_param(struct ath10k *ar, u32 vdev_id,
			    enum wmi_sta_powersave_param param_id, u32 value)
{
	struct athp_buf *pbuf;

	if (!ar->wmi.ops->gen_set_sta_ps)
		return -EOPNOTSUPP;

	pbuf = ar->wmi.ops->gen_set_sta_ps(ar, vdev_id, param_id, value);
	if (IS_ERR(pbuf))
		return PTR_ERR(pbuf);

	return ath10k_wmi_cmd_send(ar, pbuf,
				   ar->wmi.cmd->sta_powersave_param_cmdid);
}

static inline int
ath10k_wmi_set_ap_ps_param(struct ath10k *ar, u32 vdev_id, const u8 *mac,
			   enum wmi_ap_ps_peer_param param_id, u32 value)
{
	struct athp_buf *pbuf;

	if (!ar->wmi.ops->gen_set_ap_ps)
		return -EOPNOTSUPP;

	pbuf = ar->wmi.ops->gen_set_ap_ps(ar, vdev_id, mac, param_id, value);
	if (IS_ERR(pbuf))
		return PTR_ERR(pbuf);

	return ath10k_wmi_cmd_send(ar, pbuf,
				   ar->wmi.cmd->ap_ps_peer_param_cmdid);
}

static inline int
ath10k_wmi_scan_chan_list(struct ath10k *ar,
			  const struct wmi_scan_chan_list_arg *arg)
{
	struct athp_buf *pbuf;

	if (!ar->wmi.ops->gen_scan_chan_list)
		return -EOPNOTSUPP;

	pbuf = ar->wmi.ops->gen_scan_chan_list(ar, arg);
	if (IS_ERR(pbuf))
		return PTR_ERR(pbuf);

	return ath10k_wmi_cmd_send(ar, pbuf, ar->wmi.cmd->scan_chan_list_cmdid);
}

static inline int
ath10k_wmi_peer_assoc(struct ath10k *ar,
		      const struct wmi_peer_assoc_complete_arg *arg)
{
	struct athp_buf *pbuf;

	if (!ar->wmi.ops->gen_peer_assoc)
		return -EOPNOTSUPP;

	pbuf = ar->wmi.ops->gen_peer_assoc(ar, arg);
	if (IS_ERR(pbuf))
		return PTR_ERR(pbuf);

	return ath10k_wmi_cmd_send(ar, pbuf, ar->wmi.cmd->peer_assoc_cmdid);
}

static inline int
ath10k_wmi_beacon_send_ref_nowait(struct ath10k *ar, u32 vdev_id,
				  const void *bcn, size_t bcn_len,
				  u32 bcn_paddr, bool dtim_zero,
				  bool deliver_cab)
{
	struct athp_buf *pbuf;
	int ret;

	if (!ar->wmi.ops->gen_beacon_dma)
		return -EOPNOTSUPP;

	pbuf = ar->wmi.ops->gen_beacon_dma(ar, vdev_id, bcn, bcn_len, bcn_paddr,
					  dtim_zero, deliver_cab);
	if (IS_ERR(pbuf))
		return PTR_ERR(pbuf);

	ret = ath10k_wmi_cmd_send_nowait(ar, pbuf,
					 ar->wmi.cmd->pdev_send_bcn_cmdid);
	if (ret) {
		/* XXX TODO: should just embed which list in pbuf */
		athp_freebuf(ar, &ar->buf_tx, pbuf);
		return ret;
	}

	return 0;
}

static inline int
ath10k_wmi_pdev_set_wmm_params(struct ath10k *ar,
			       const struct wmi_wmm_params_all_arg *arg)
{
	struct athp_buf *pbuf;

	if (!ar->wmi.ops->gen_pdev_set_wmm)
		return -EOPNOTSUPP;

	pbuf = ar->wmi.ops->gen_pdev_set_wmm(ar, arg);
	if (IS_ERR(pbuf))
		return PTR_ERR(pbuf);

	return ath10k_wmi_cmd_send(ar, pbuf,
				   ar->wmi.cmd->pdev_set_wmm_params_cmdid);
}

static inline int
ath10k_wmi_request_stats(struct ath10k *ar, u32 stats_mask)
{
	struct athp_buf *pbuf;

	if (!ar->wmi.ops->gen_request_stats)
		return -EOPNOTSUPP;

	pbuf = ar->wmi.ops->gen_request_stats(ar, stats_mask);
	if (IS_ERR(pbuf))
		return PTR_ERR(pbuf);

	return ath10k_wmi_cmd_send(ar, pbuf, ar->wmi.cmd->request_stats_cmdid);
}

static inline int
ath10k_wmi_force_fw_hang(struct ath10k *ar,
			 enum wmi_force_fw_hang_type type, u32 delay_ms)
{
	struct athp_buf *pbuf;

	if (!ar->wmi.ops->gen_force_fw_hang)
		return -EOPNOTSUPP;

	pbuf = ar->wmi.ops->gen_force_fw_hang(ar, type, delay_ms);
	if (IS_ERR(pbuf))
		return PTR_ERR(pbuf);

	return ath10k_wmi_cmd_send(ar, pbuf, ar->wmi.cmd->force_fw_hang_cmdid);
}

static inline int
ath10k_wmi_dbglog_cfg(struct ath10k *ar, u32 module_enable, u32 log_level)
{
	struct athp_buf *pbuf;

	if (!ar->wmi.ops->gen_dbglog_cfg)
		return -EOPNOTSUPP;

	pbuf = ar->wmi.ops->gen_dbglog_cfg(ar, module_enable, log_level);
	if (IS_ERR(pbuf))
		return PTR_ERR(pbuf);

	return ath10k_wmi_cmd_send(ar, pbuf, ar->wmi.cmd->dbglog_cfg_cmdid);
}

static inline int
ath10k_wmi_pdev_pktlog_enable(struct ath10k *ar, u32 filter)
{
	struct athp_buf *pbuf;

	if (!ar->wmi.ops->gen_pktlog_enable)
		return -EOPNOTSUPP;

	pbuf = ar->wmi.ops->gen_pktlog_enable(ar, filter);
	if (IS_ERR(pbuf))
		return PTR_ERR(pbuf);

	return ath10k_wmi_cmd_send(ar, pbuf, ar->wmi.cmd->pdev_pktlog_enable_cmdid);
}

static inline int
ath10k_wmi_pdev_pktlog_disable(struct ath10k *ar)
{
	struct athp_buf *pbuf;

	if (!ar->wmi.ops->gen_pktlog_disable)
		return -EOPNOTSUPP;

	pbuf = ar->wmi.ops->gen_pktlog_disable(ar);
	if (IS_ERR(pbuf))
		return PTR_ERR(pbuf);

	return ath10k_wmi_cmd_send(ar, pbuf,
				   ar->wmi.cmd->pdev_pktlog_disable_cmdid);
}

static inline int
ath10k_wmi_pdev_set_quiet_mode(struct ath10k *ar, u32 period, u32 duration,
			       u32 next_offset, u32 enabled)
{
	struct athp_buf *pbuf;

	if (!ar->wmi.ops->gen_pdev_set_quiet_mode)
		return -EOPNOTSUPP;

	pbuf = ar->wmi.ops->gen_pdev_set_quiet_mode(ar, period, duration,
						   next_offset, enabled);
	if (IS_ERR(pbuf))
		return PTR_ERR(pbuf);

	return ath10k_wmi_cmd_send(ar, pbuf,
				   ar->wmi.cmd->pdev_set_quiet_mode_cmdid);
}

static inline int
ath10k_wmi_pdev_get_temperature(struct ath10k *ar)
{
	struct athp_buf *pbuf;

	if (!ar->wmi.ops->gen_pdev_get_temperature)
		return -EOPNOTSUPP;

	pbuf = ar->wmi.ops->gen_pdev_get_temperature(ar);
	if (IS_ERR(pbuf))
		return PTR_ERR(pbuf);

	return ath10k_wmi_cmd_send(ar, pbuf,
				   ar->wmi.cmd->pdev_get_temperature_cmdid);
}

static inline int
ath10k_wmi_addba_clear_resp(struct ath10k *ar, u32 vdev_id, const u8 *mac)
{
	struct athp_buf *pbuf;

	if (!ar->wmi.ops->gen_addba_clear_resp)
		return -EOPNOTSUPP;

	pbuf = ar->wmi.ops->gen_addba_clear_resp(ar, vdev_id, mac);
	if (IS_ERR(pbuf))
		return PTR_ERR(pbuf);

	return ath10k_wmi_cmd_send(ar, pbuf,
				   ar->wmi.cmd->addba_clear_resp_cmdid);
}

static inline int
ath10k_wmi_addba_send(struct ath10k *ar, u32 vdev_id, const u8 *mac,
		      u32 tid, u32 buf_size)
{
	struct athp_buf *pbuf;

	if (!ar->wmi.ops->gen_addba_send)
		return -EOPNOTSUPP;

	pbuf = ar->wmi.ops->gen_addba_send(ar, vdev_id, mac, tid, buf_size);
	if (IS_ERR(pbuf))
		return PTR_ERR(pbuf);

	return ath10k_wmi_cmd_send(ar, pbuf,
				   ar->wmi.cmd->addba_send_cmdid);
}

static inline int
ath10k_wmi_addba_set_resp(struct ath10k *ar, u32 vdev_id, const u8 *mac,
			  u32 tid, u32 status)
{
	struct athp_buf *pbuf;

	if (!ar->wmi.ops->gen_addba_set_resp)
		return -EOPNOTSUPP;

	pbuf = ar->wmi.ops->gen_addba_set_resp(ar, vdev_id, mac, tid, status);
	if (IS_ERR(pbuf))
		return PTR_ERR(pbuf);

	return ath10k_wmi_cmd_send(ar, pbuf,
				   ar->wmi.cmd->addba_set_resp_cmdid);
}

static inline int
ath10k_wmi_delba_send(struct ath10k *ar, u32 vdev_id, const u8 *mac,
		      u32 tid, u32 initiator, u32 reason)
{
	struct athp_buf *pbuf;

	if (!ar->wmi.ops->gen_delba_send)
		return -EOPNOTSUPP;

	pbuf = ar->wmi.ops->gen_delba_send(ar, vdev_id, mac, tid, initiator,
					  reason);
	if (IS_ERR(pbuf))
		return PTR_ERR(pbuf);

	return ath10k_wmi_cmd_send(ar, pbuf,
				   ar->wmi.cmd->delba_send_cmdid);
}

static inline int
ath10k_wmi_bcn_tmpl(struct ath10k *ar, u32 vdev_id, u32 tim_ie_offset,
		    struct athp_buf *bcn, u32 prb_caps, u32 prb_erp,
		    void *prb_ies, size_t prb_ies_len)
{
	struct athp_buf *pbuf;

	if (!ar->wmi.ops->gen_bcn_tmpl)
		return -EOPNOTSUPP;

	pbuf = ar->wmi.ops->gen_bcn_tmpl(ar, vdev_id, tim_ie_offset, bcn,
					prb_caps, prb_erp, prb_ies,
					prb_ies_len);
	if (IS_ERR(pbuf))
		return PTR_ERR(pbuf);

	return ath10k_wmi_cmd_send(ar, pbuf, ar->wmi.cmd->bcn_tmpl_cmdid);
}

static inline int
ath10k_wmi_prb_tmpl(struct ath10k *ar, u32 vdev_id, struct athp_buf *prb)
{
	struct athp_buf *pbuf;

	if (!ar->wmi.ops->gen_prb_tmpl)
		return -EOPNOTSUPP;

	pbuf = ar->wmi.ops->gen_prb_tmpl(ar, vdev_id, prb);
	if (IS_ERR(pbuf))
		return PTR_ERR(pbuf);

	return ath10k_wmi_cmd_send(ar, pbuf, ar->wmi.cmd->prb_tmpl_cmdid);
}

static inline int
ath10k_wmi_p2p_go_bcn_ie(struct ath10k *ar, u32 vdev_id, const u8 *p2p_ie)
{
	struct athp_buf *pbuf;

	if (!ar->wmi.ops->gen_p2p_go_bcn_ie)
		return -EOPNOTSUPP;

	pbuf = ar->wmi.ops->gen_p2p_go_bcn_ie(ar, vdev_id, p2p_ie);
	if (IS_ERR(pbuf))
		return PTR_ERR(pbuf);

	return ath10k_wmi_cmd_send(ar, pbuf, ar->wmi.cmd->p2p_go_set_beacon_ie);
}

static inline int
ath10k_wmi_sta_keepalive(struct ath10k *ar,
			 const struct wmi_sta_keepalive_arg *arg)
{
	struct athp_buf *pbuf;
	u32 cmd_id;

	if (!ar->wmi.ops->gen_sta_keepalive)
		return -EOPNOTSUPP;

	pbuf = ar->wmi.ops->gen_sta_keepalive(ar, arg);
	if (IS_ERR(pbuf))
		return PTR_ERR(pbuf);

	cmd_id = ar->wmi.cmd->sta_keepalive_cmd;
	return ath10k_wmi_cmd_send(ar, pbuf, cmd_id);
}

static inline int
ath10k_wmi_wow_enable(struct ath10k *ar)
{
	struct athp_buf *pbuf;
	u32 cmd_id;

	if (!ar->wmi.ops->gen_wow_enable)
		return -EOPNOTSUPP;

	pbuf = ar->wmi.ops->gen_wow_enable(ar);
	if (IS_ERR(pbuf))
		return PTR_ERR(pbuf);

	cmd_id = ar->wmi.cmd->wow_enable_cmdid;
	return ath10k_wmi_cmd_send(ar, pbuf, cmd_id);
}

static inline int
ath10k_wmi_wow_add_wakeup_event(struct ath10k *ar, u32 vdev_id,
				enum wmi_wow_wakeup_event event,
				u32 enable)
{
	struct athp_buf *pbuf;
	u32 cmd_id;

	if (!ar->wmi.ops->gen_wow_add_wakeup_event)
		return -EOPNOTSUPP;

	pbuf = ar->wmi.ops->gen_wow_add_wakeup_event(ar, vdev_id, event, enable);
	if (IS_ERR(pbuf))
		return PTR_ERR(pbuf);

	cmd_id = ar->wmi.cmd->wow_enable_disable_wake_event_cmdid;
	return ath10k_wmi_cmd_send(ar, pbuf, cmd_id);
}

static inline int
ath10k_wmi_wow_host_wakeup_ind(struct ath10k *ar)
{
	struct athp_buf *pbuf;
	u32 cmd_id;

	if (!ar->wmi.ops->gen_wow_host_wakeup_ind)
		return -EOPNOTSUPP;

	pbuf = ar->wmi.ops->gen_wow_host_wakeup_ind(ar);
	if (IS_ERR(pbuf))
		return PTR_ERR(pbuf);

	cmd_id = ar->wmi.cmd->wow_hostwakeup_from_sleep_cmdid;
	return ath10k_wmi_cmd_send(ar, pbuf, cmd_id);
}

static inline int
ath10k_wmi_wow_add_pattern(struct ath10k *ar, u32 vdev_id, u32 pattern_id,
			   const u8 *pattern, const u8 *mask,
			   int pattern_len, int pattern_offset)
{
	struct athp_buf *pbuf;
	u32 cmd_id;

	if (!ar->wmi.ops->gen_wow_add_pattern)
		return -EOPNOTSUPP;

	pbuf = ar->wmi.ops->gen_wow_add_pattern(ar, vdev_id, pattern_id,
					       pattern, mask, pattern_len,
					       pattern_offset);
	if (IS_ERR(pbuf))
		return PTR_ERR(pbuf);

	cmd_id = ar->wmi.cmd->wow_add_wake_pattern_cmdid;
	return ath10k_wmi_cmd_send(ar, pbuf, cmd_id);
}

static inline int
ath10k_wmi_wow_del_pattern(struct ath10k *ar, u32 vdev_id, u32 pattern_id)
{
	struct athp_buf *pbuf;
	u32 cmd_id;

	if (!ar->wmi.ops->gen_wow_del_pattern)
		return -EOPNOTSUPP;

	pbuf = ar->wmi.ops->gen_wow_del_pattern(ar, vdev_id, pattern_id);
	if (IS_ERR(pbuf))
		return PTR_ERR(pbuf);

	cmd_id = ar->wmi.cmd->wow_del_wake_pattern_cmdid;
	return ath10k_wmi_cmd_send(ar, pbuf, cmd_id);
}

static inline int
ath10k_wmi_update_fw_tdls_state(struct ath10k *ar, u32 vdev_id,
				enum wmi_tdls_state state)
{
	struct athp_buf *pbuf;

	if (!ar->wmi.ops->gen_update_fw_tdls_state)
		return -EOPNOTSUPP;

	pbuf = ar->wmi.ops->gen_update_fw_tdls_state(ar, vdev_id, state);
	if (IS_ERR(pbuf))
		return PTR_ERR(pbuf);

	return ath10k_wmi_cmd_send(ar, pbuf, ar->wmi.cmd->tdls_set_state_cmdid);
}

static inline int
ath10k_wmi_tdls_peer_update(struct ath10k *ar,
			    const struct wmi_tdls_peer_update_cmd_arg *arg,
			    const struct wmi_tdls_peer_capab_arg *cap,
			    const struct wmi_channel_arg *chan)
{
	struct athp_buf *pbuf;

	if (!ar->wmi.ops->gen_tdls_peer_update)
		return -EOPNOTSUPP;

	pbuf = ar->wmi.ops->gen_tdls_peer_update(ar, arg, cap, chan);
	if (IS_ERR(pbuf))
		return PTR_ERR(pbuf);

	return ath10k_wmi_cmd_send(ar, pbuf,
				   ar->wmi.cmd->tdls_peer_update_cmdid);
}

static inline int
ath10k_wmi_adaptive_qcs(struct ath10k *ar, bool enable)
{
	struct athp_buf *pbuf;

	if (!ar->wmi.ops->gen_adaptive_qcs)
		return -EOPNOTSUPP;

	pbuf = ar->wmi.ops->gen_adaptive_qcs(ar, enable);
	if (IS_ERR(pbuf))
		return PTR_ERR(pbuf);

	return ath10k_wmi_cmd_send(ar, pbuf, ar->wmi.cmd->adaptive_qcs_cmdid);
}

#endif
