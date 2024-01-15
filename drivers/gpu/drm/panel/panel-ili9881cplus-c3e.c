// SPDX-License-Identifier: GPL-2.0-only
// Copyright (c) 2024 FIXME
// Generated with linux-mdss-dsi-panel-driver-generator from vendor device tree:
//   Copyright (c) 2013, The Linux Foundation. All rights reserved. (FIXME)

#include <linux/delay.h>
#include <linux/gpio/consumer.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/regulator/consumer.h>

#include <drm/drm_mipi_dsi.h>
#include <drm/drm_modes.h>
#include <drm/drm_panel.h>

struct ili9881cplus_c3e {
	struct drm_panel panel;
	struct mipi_dsi_device *dsi;
	struct regulator_bulk_data supplies[2];
	struct gpio_desc *reset_gpio;
	bool prepared;
};

static inline
struct ili9881cplus_c3e *to_ili9881cplus_c3e(struct drm_panel *panel)
{
	return container_of(panel, struct ili9881cplus_c3e, panel);
}

static void ili9881cplus_c3e_reset(struct ili9881cplus_c3e *ctx)
{
	gpiod_set_value_cansleep(ctx->reset_gpio, 0);
	msleep(20);
	gpiod_set_value_cansleep(ctx->reset_gpio, 1);
	usleep_range(2000, 3000);
	gpiod_set_value_cansleep(ctx->reset_gpio, 0);
	msleep(20);
}

static int ili9881cplus_c3e_on(struct ili9881cplus_c3e *ctx)
{
	struct mipi_dsi_device *dsi = ctx->dsi;

	mipi_dsi_generic_write_seq(dsi, 0xff, 0x98, 0x81, 0x00);
	mipi_dsi_generic_write_seq(dsi, 0x11, 0x00);
	msleep(120);
	mipi_dsi_generic_write_seq(dsi, 0xff, 0x98, 0x81, 0x02);
	mipi_dsi_generic_write_seq(dsi, 0x06, 0x10);
	mipi_dsi_generic_write_seq(dsi, 0x07, 0x00);
	mipi_dsi_generic_write_seq(dsi, 0xff, 0x98, 0x81, 0x02);
	mipi_dsi_generic_write_seq(dsi, 0x03, 0x6e);
	mipi_dsi_generic_write_seq(dsi, 0x04, 0x16);
	mipi_dsi_generic_write_seq(dsi, 0x05, 0x11);
	mipi_dsi_generic_write_seq(dsi, 0x00, 0x25);
	mipi_dsi_generic_write_seq(dsi, 0x01, 0x32);
	mipi_dsi_generic_write_seq(dsi, 0x36, 0xbe);
	mipi_dsi_generic_write_seq(dsi, 0x33, 0xb5);
	mipi_dsi_generic_write_seq(dsi, 0x32, 0x77);
	mipi_dsi_generic_write_seq(dsi, 0x31, 0x78);
	mipi_dsi_generic_write_seq(dsi, 0x30, 0x39);
	mipi_dsi_generic_write_seq(dsi, 0x2f, 0x3a);
	mipi_dsi_generic_write_seq(dsi, 0x2e, 0x3b);
	mipi_dsi_generic_write_seq(dsi, 0x2d, 0x3d);
	mipi_dsi_generic_write_seq(dsi, 0x2c, 0x3d);
	mipi_dsi_generic_write_seq(dsi, 0x29, 0x29);
	mipi_dsi_generic_write_seq(dsi, 0x28, 0x32);
	mipi_dsi_generic_write_seq(dsi, 0x27, 0x33);
	mipi_dsi_generic_write_seq(dsi, 0x26, 0x34);
	mipi_dsi_generic_write_seq(dsi, 0x24, 0x39);
	mipi_dsi_generic_write_seq(dsi, 0x23, 0x3c);
	mipi_dsi_generic_write_seq(dsi, 0x22, 0x3c);
	mipi_dsi_generic_write_seq(dsi, 0x20, 0x3e);
	mipi_dsi_generic_write_seq(dsi, 0xff, 0x98, 0x81, 0x0b);
	mipi_dsi_generic_write_seq(dsi, 0x07, 0xa1);
	mipi_dsi_generic_write_seq(dsi, 0x2d, 0xff);
	mipi_dsi_generic_write_seq(dsi, 0x2e, 0xff);
	mipi_dsi_generic_write_seq(dsi, 0x2f, 0xff);
	mipi_dsi_generic_write_seq(dsi, 0x28, 0x90);
	mipi_dsi_generic_write_seq(dsi, 0xff, 0x98, 0x81, 0x00);
	mipi_dsi_generic_write_seq(dsi, 0x51, 0x0f, 0xff);
	mipi_dsi_generic_write_seq(dsi, 0x53, 0x2c);
	mipi_dsi_generic_write_seq(dsi, 0x55, 0x00);
	mipi_dsi_generic_write_seq(dsi, 0x35, 0x00);
	mipi_dsi_generic_write_seq(dsi, 0x29, 0x00);
	msleep(20);

	return 0;
}

static int ili9881cplus_c3e_off(struct ili9881cplus_c3e *ctx)
{
	struct mipi_dsi_device *dsi = ctx->dsi;

	mipi_dsi_generic_write_seq(dsi, 0xff, 0x98, 0x81, 0x00);
	mipi_dsi_generic_write_seq(dsi, 0x28, 0x00);
	msleep(20);
	mipi_dsi_generic_write_seq(dsi, 0x10, 0x00);
	msleep(120);

	return 0;
}

static int ili9881cplus_c3e_prepare(struct drm_panel *panel)
{
	struct ili9881cplus_c3e *ctx = to_ili9881cplus_c3e(panel);
	struct device *dev = &ctx->dsi->dev;
	int ret;

	if (ctx->prepared)
		return 0;

	ret = regulator_bulk_enable(ARRAY_SIZE(ctx->supplies), ctx->supplies);
	if (ret < 0) {
		dev_err(dev, "Failed to enable regulators: %d\n", ret);
		return ret;
	}

	ili9881cplus_c3e_reset(ctx);

	ret = ili9881cplus_c3e_on(ctx);
	if (ret < 0) {
		dev_err(dev, "Failed to initialize panel: %d\n", ret);
		gpiod_set_value_cansleep(ctx->reset_gpio, 1);
		regulator_bulk_disable(ARRAY_SIZE(ctx->supplies), ctx->supplies);
		return ret;
	}

	ctx->prepared = true;
	return 0;
}

static int ili9881cplus_c3e_unprepare(struct drm_panel *panel)
{
	struct ili9881cplus_c3e *ctx = to_ili9881cplus_c3e(panel);
	struct device *dev = &ctx->dsi->dev;
	int ret;

	if (!ctx->prepared)
		return 0;

	ret = ili9881cplus_c3e_off(ctx);
	if (ret < 0)
		dev_err(dev, "Failed to un-initialize panel: %d\n", ret);

	gpiod_set_value_cansleep(ctx->reset_gpio, 1);
	regulator_bulk_disable(ARRAY_SIZE(ctx->supplies), ctx->supplies);

	ctx->prepared = false;
	return 0;
}

static const struct drm_display_mode ili9881cplus_c3e_mode = {
	.clock = (720 + 192 + 16 + 192) * (1440 + 20 + 8 + 20) * 60 / 1000,
	.hdisplay = 720,
	.hsync_start = 720 + 192,
	.hsync_end = 720 + 192 + 16,
	.htotal = 720 + 192 + 16 + 192,
	.vdisplay = 1440,
	.vsync_start = 1440 + 20,
	.vsync_end = 1440 + 20 + 8,
	.vtotal = 1440 + 20 + 8 + 20,
	.width_mm = 62,
	.height_mm = 124,
};

static int ili9881cplus_c3e_get_modes(struct drm_panel *panel,
				      struct drm_connector *connector)
{
	struct drm_display_mode *mode;

	mode = drm_mode_duplicate(connector->dev, &ili9881cplus_c3e_mode);
	if (!mode)
		return -ENOMEM;

	drm_mode_set_name(mode);

	mode->type = DRM_MODE_TYPE_DRIVER | DRM_MODE_TYPE_PREFERRED;
	connector->display_info.width_mm = mode->width_mm;
	connector->display_info.height_mm = mode->height_mm;
	drm_mode_probed_add(connector, mode);

	return 1;
}

static const struct drm_panel_funcs ili9881cplus_c3e_panel_funcs = {
	.prepare = ili9881cplus_c3e_prepare,
	.unprepare = ili9881cplus_c3e_unprepare,
	.get_modes = ili9881cplus_c3e_get_modes,
};

static int ili9881cplus_c3e_probe(struct mipi_dsi_device *dsi)
{
	struct device *dev = &dsi->dev;
	struct ili9881cplus_c3e *ctx;
	int ret;

	ctx = devm_kzalloc(dev, sizeof(*ctx), GFP_KERNEL);
	if (!ctx)
		return -ENOMEM;

	ctx->supplies[0].supply = "vsn";
	ctx->supplies[1].supply = "vsp";
	ret = devm_regulator_bulk_get(dev, ARRAY_SIZE(ctx->supplies),
				      ctx->supplies);
	if (ret < 0)
		return dev_err_probe(dev, ret, "Failed to get regulators\n");

	ctx->reset_gpio = devm_gpiod_get(dev, "reset", GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->reset_gpio))
		return dev_err_probe(dev, PTR_ERR(ctx->reset_gpio),
				     "Failed to get reset-gpios\n");

	ctx->dsi = dsi;
	mipi_dsi_set_drvdata(dsi, ctx);

	dsi->lanes = 4;
	dsi->format = MIPI_DSI_FMT_RGB888;
	dsi->mode_flags = MIPI_DSI_MODE_VIDEO | MIPI_DSI_MODE_VIDEO_BURST |
			  MIPI_DSI_MODE_VIDEO_HSE | MIPI_DSI_MODE_NO_EOT_PACKET |
			  MIPI_DSI_CLOCK_NON_CONTINUOUS | MIPI_DSI_MODE_LPM;

	drm_panel_init(&ctx->panel, dev, &ili9881cplus_c3e_panel_funcs,
		       DRM_MODE_CONNECTOR_DSI);
	ctx->panel.prepare_prev_first = true;

	drm_panel_add(&ctx->panel);

	ret = mipi_dsi_attach(dsi);
	if (ret < 0) {
		dev_err(dev, "Failed to attach to DSI host: %d\n", ret);
		drm_panel_remove(&ctx->panel);
		return ret;
	}

	return 0;
}

static void ili9881cplus_c3e_remove(struct mipi_dsi_device *dsi)
{
	struct ili9881cplus_c3e *ctx = mipi_dsi_get_drvdata(dsi);
	int ret;

	ret = mipi_dsi_detach(dsi);
	if (ret < 0)
		dev_err(&dsi->dev, "Failed to detach from DSI host: %d\n", ret);

	drm_panel_remove(&ctx->panel);
}

static const struct of_device_id ili9881cplus_c3e_of_match[] = {
	{ .compatible = "mdss,ili9881cplus-c3e" }, // FIXME
	{ /* sentinel */ }
};
MODULE_DEVICE_TABLE(of, ili9881cplus_c3e_of_match);

static struct mipi_dsi_driver ili9881cplus_c3e_driver = {
	.probe = ili9881cplus_c3e_probe,
	.remove = ili9881cplus_c3e_remove,
	.driver = {
		.name = "panel-ili9881cplus-c3e",
		.of_match_table = ili9881cplus_c3e_of_match,
	},
};
module_mipi_dsi_driver(ili9881cplus_c3e_driver);

MODULE_AUTHOR("linux-mdss-dsi-panel-driver-generator <fix@me>"); // FIXME
MODULE_DESCRIPTION("DRM driver for ili9881c hdplus c3e video mode dsi panel");
MODULE_LICENSE("GPL");
