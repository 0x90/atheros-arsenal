/*
 * Rockchip SoC DP (Display Port) interface driver.
 *
 * Copyright (C) Fuzhou Rockchip Electronics Co., Ltd.
 * Author: Andy Yan <andy.yan@rock-chips.com>
 *         Yakir Yang <ykk@rock-chips.com>
 *         Jeff Chen <jeff.chen@rock-chips.com>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */

#include <linux/component.h>
#include <linux/mfd/syscon.h>
#include <linux/of_graph.h>
#include <linux/regmap.h>
#include <linux/reset.h>
#include <linux/clk.h>

#include <drm/drmP.h>
#include <drm/drm_crtc_helper.h>
#include <drm/drm_dp_helper.h>
#include <drm/drm_of.h>
#include <drm/drm_panel.h>

#include <video/of_videomode.h>
#include <video/videomode.h>

#include <drm/bridge/analogix_dp.h>

#include "rockchip_drm_drv.h"
#include "rockchip_drm_vop.h"

#define to_dp(nm)	container_of(nm, struct rockchip_dp_device, nm)

/* dp grf register offset */
#define GRF_SOC_CON6                            0x025c
#define GRF_EDP_LCD_SEL_MASK                    BIT(5)
#define GRF_EDP_SEL_VOP_LIT                     BIT(5)
#define GRF_EDP_SEL_VOP_BIG                     0

struct rockchip_dp_device {
	struct drm_device        *drm_dev;
	struct device            *dev;
	struct drm_encoder       encoder;
	struct drm_display_mode  mode;

	struct clk               *pclk;
	struct regmap            *grf;
	struct reset_control     *rst;

	struct analogix_dp_plat_data plat_data;
};

static int rockchip_dp_pre_init(struct rockchip_dp_device *dp)
{
	reset_control_assert(dp->rst);
	usleep_range(10, 20);
	reset_control_deassert(dp->rst);

	return 0;
}

static int rockchip_dp_poweron(struct analogix_dp_plat_data *plat_data)
{
	struct rockchip_dp_device *dp = to_dp(plat_data);
	int ret;

	ret = clk_prepare_enable(dp->pclk);
	if (ret < 0) {
		dev_err(dp->dev, "failed to enable pclk %d\n", ret);
		return ret;
	}

	ret = rockchip_dp_pre_init(dp);
	if (ret < 0) {
		dev_err(dp->dev, "failed to dp pre init %d\n", ret);
		return ret;
	}

	return 0;
}

static int rockchip_dp_powerdown(struct analogix_dp_plat_data *plat_data)
{
	struct rockchip_dp_device *dp = to_dp(plat_data);

	clk_disable_unprepare(dp->pclk);

	return 0;
}

static bool
rockchip_dp_drm_encoder_mode_fixup(struct drm_encoder *encoder,
				   const struct drm_display_mode *mode,
				   struct drm_display_mode *adjusted_mode)
{
	/* do nothing */
	return true;
}

static void rockchip_dp_drm_encoder_mode_set(struct drm_encoder *encoder,
					     struct drm_display_mode *mode,
					     struct drm_display_mode *adjusted)
{
	/* do nothing */
}

static void rockchip_dp_drm_encoder_enable(struct drm_encoder *encoder)
{
	struct rockchip_dp_device *dp = to_dp(encoder);
	int ret;
	u32 val;

	ret = drm_of_encoder_active_endpoint_id(dp->dev->of_node, encoder);
	if (ret < 0)
		return;

	if (ret)
		val = GRF_EDP_SEL_VOP_LIT | (GRF_EDP_LCD_SEL_MASK << 16);
	else
		val = GRF_EDP_SEL_VOP_BIG | (GRF_EDP_LCD_SEL_MASK << 16);

	dev_dbg(dp->dev, "vop %s output to dp\n", (ret) ? "LIT" : "BIG");

	ret = regmap_write(dp->grf, GRF_SOC_CON6, val);
	if (ret != 0) {
		dev_err(dp->dev, "Could not write to GRF: %d\n", ret);
		return;
	}
}

static void rockchip_dp_drm_encoder_nop(struct drm_encoder *encoder)
{
	/* do nothing */
}

static int
rockchip_dp_drm_encoder_atomic_check(struct drm_encoder *encoder,
				      struct drm_crtc_state *crtc_state,
				      struct drm_connector_state *conn_state)
{
	struct rockchip_crtc_state *s = to_rockchip_crtc_state(crtc_state);

	/*
	 * FIXME(Yakir): driver should configure the CRTC output video
	 * mode with the display information which indicated the monitor
	 * support colorimetry.
	 *
	 * But don't know why the CRTC driver seems could only output the
	 * RGBaaa rightly. For example, if connect the "innolux,n116bge"
	 * eDP screen, EDID would indicated that screen only accepted the
	 * 6bpc mode. But if I configure CRTC to RGB666 output, then eDP
	 * screen would show a blue picture (RGB888 show a green picture).
	 * But if I configure CTRC to RGBaaa, and eDP driver still keep
	 * RGB666 input video mode, then screen would works prefect.
	 */
	s->output_mode = ROCKCHIP_OUT_MODE_AAAA;
	s->output_type = DRM_MODE_CONNECTOR_eDP;

	return 0;
}

static struct drm_encoder_helper_funcs rockchip_dp_encoder_helper_funcs = {
	.mode_fixup = rockchip_dp_drm_encoder_mode_fixup,
	.mode_set = rockchip_dp_drm_encoder_mode_set,
	.enable = rockchip_dp_drm_encoder_enable,
	.disable = rockchip_dp_drm_encoder_nop,
	.atomic_check = rockchip_dp_drm_encoder_atomic_check,
};

static void rockchip_dp_drm_encoder_destroy(struct drm_encoder *encoder)
{
	drm_encoder_cleanup(encoder);
}

static struct drm_encoder_funcs rockchip_dp_encoder_funcs = {
	.destroy = rockchip_dp_drm_encoder_destroy,
};

static int rockchip_dp_init(struct rockchip_dp_device *dp)
{
	struct device *dev = dp->dev;
	struct device_node *np = dev->of_node;
	int ret;

	dp->grf = syscon_regmap_lookup_by_phandle(np, "rockchip,grf");
	if (IS_ERR(dp->grf)) {
		dev_err(dev, "failed to get rockchip,grf property\n");
		return PTR_ERR(dp->grf);
	}

	dp->pclk = devm_clk_get(dev, "pclk");
	if (IS_ERR(dp->pclk)) {
		dev_err(dev, "failed to get pclk property\n");
		return PTR_ERR(dp->pclk);
	}

	dp->rst = devm_reset_control_get(dev, "dp");
	if (IS_ERR(dp->rst)) {
		dev_err(dev, "failed to get dp reset control\n");
		return PTR_ERR(dp->rst);
	}

	ret = clk_prepare_enable(dp->pclk);
	if (ret < 0) {
		dev_err(dp->dev, "failed to enable pclk %d\n", ret);
		return ret;
	}

	ret = rockchip_dp_pre_init(dp);
	if (ret < 0) {
		dev_err(dp->dev, "failed to pre init %d\n", ret);
		return ret;
	}

	return 0;
}

static int rockchip_dp_drm_create_encoder(struct rockchip_dp_device *dp)
{
	struct drm_encoder *encoder = &dp->encoder;
	struct drm_device *drm_dev = dp->drm_dev;
	struct device *dev = dp->dev;
	int ret;

	encoder->possible_crtcs = drm_of_find_possible_crtcs(drm_dev,
							     dev->of_node);
	DRM_DEBUG_KMS("possible_crtcs = 0x%x\n", encoder->possible_crtcs);

	ret = drm_encoder_init(drm_dev, encoder, &rockchip_dp_encoder_funcs,
			       DRM_MODE_ENCODER_TMDS, NULL);
	if (ret) {
		DRM_ERROR("failed to initialize encoder with drm\n");
		return ret;
	}

	drm_encoder_helper_add(encoder, &rockchip_dp_encoder_helper_funcs);

	return 0;
}

static int rockchip_dp_bind(struct device *dev, struct device *master,
			    void *data)
{
	struct rockchip_dp_device *dp = dev_get_drvdata(dev);
	struct drm_device *drm_dev = data;
	int ret;

	/*
	 * Just like the probe function said, we don't need the
	 * device drvrate anymore, we should leave the charge to
	 * analogix dp driver, set the device drvdata to NULL.
	 */
	dev_set_drvdata(dev, NULL);

	ret = rockchip_dp_init(dp);
	if (ret < 0)
		return ret;

	dp->drm_dev = drm_dev;

	ret = rockchip_dp_drm_create_encoder(dp);
	if (ret) {
		DRM_ERROR("failed to create drm encoder\n");
		return ret;
	}

	dp->plat_data.encoder = &dp->encoder;

	dp->plat_data.dev_type = RK3288_DP;
	dp->plat_data.power_on = rockchip_dp_poweron;
	dp->plat_data.power_off = rockchip_dp_powerdown;

	return analogix_dp_bind(dev, dp->drm_dev, &dp->plat_data);
}

static void rockchip_dp_unbind(struct device *dev, struct device *master,
			       void *data)
{
	return analogix_dp_unbind(dev, master, data);
}

static const struct component_ops rockchip_dp_component_ops = {
	.bind = rockchip_dp_bind,
	.unbind = rockchip_dp_unbind,
};

static int rockchip_dp_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct device_node *panel_node, *port, *endpoint;
	struct rockchip_dp_device *dp;
	struct drm_panel *panel;

	port = of_graph_get_port_by_id(dev->of_node, 1);
	if (!port) {
		dev_err(dev, "can't find output port\n");
		return -EINVAL;
	}

	endpoint = of_get_child_by_name(port, "endpoint");
	of_node_put(port);
	if (!endpoint) {
		dev_err(dev, "no output endpoint found\n");
		return -EINVAL;
	}

	panel_node = of_graph_get_remote_port_parent(endpoint);
	of_node_put(endpoint);
	if (!panel_node) {
		dev_err(dev, "no output node found\n");
		return -EINVAL;
	}

	panel = of_drm_find_panel(panel_node);
	if (!panel) {
		DRM_ERROR("failed to find panel\n");
		of_node_put(panel_node);
		return -EPROBE_DEFER;
	}

	of_node_put(panel_node);

	dp = devm_kzalloc(dev, sizeof(*dp), GFP_KERNEL);
	if (!dp)
		return -ENOMEM;

	dp->dev = dev;

	dp->plat_data.panel = panel;

	/*
	 * We just use the drvdata until driver run into component
	 * add function, and then we would set drvdata to null, so
	 * that analogix dp driver could take charge of the drvdata.
	 */
	platform_set_drvdata(pdev, dp);

	return component_add(dev, &rockchip_dp_component_ops);
}

static int rockchip_dp_remove(struct platform_device *pdev)
{
	component_del(&pdev->dev, &rockchip_dp_component_ops);

	return 0;
}

#ifdef CONFIG_PM_SLEEP
static int rockchip_dp_suspend(struct device *dev)
{
	return analogix_dp_suspend(dev);
}

static int rockchip_dp_resume(struct device *dev)
{
	return analogix_dp_resume(dev);
}
#endif

static const struct dev_pm_ops rockchip_dp_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(rockchip_dp_suspend, rockchip_dp_resume)
};

static const struct of_device_id rockchip_dp_dt_ids[] = {
	{.compatible = "rockchip,rk3288-dp",},
	{}
};
MODULE_DEVICE_TABLE(of, rockchip_dp_dt_ids);

static struct platform_driver rockchip_dp_driver = {
	.probe = rockchip_dp_probe,
	.remove = rockchip_dp_remove,
	.driver = {
		   .name = "rockchip-dp",
		   .owner = THIS_MODULE,
		   .pm = &rockchip_dp_pm_ops,
		   .of_match_table = of_match_ptr(rockchip_dp_dt_ids),
	},
};

module_platform_driver(rockchip_dp_driver);

MODULE_AUTHOR("Yakir Yang <ykk@rock-chips.com>");
MODULE_AUTHOR("Jeff chen <jeff.chen@rock-chips.com>");
MODULE_DESCRIPTION("Rockchip Specific Analogix-DP Driver Extension");
MODULE_LICENSE("GPL v2");
