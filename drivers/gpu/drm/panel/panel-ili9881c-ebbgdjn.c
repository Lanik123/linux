// SPDX-License-Identifier: GPL-2.0-only
// Copyright (c) 2023 FIXME
// Generated with linux-mdss-dsi-panel-driver-generator from vendor device tree:
//   Copyright (c) 2013, The Linux Foundation. All rights reserved. (FIXME)

#include <linux/delay.h>
#include <linux/gpio/consumer.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/regulator/consumer.h>

#include <video/mipi_display.h>

#include <drm/drm_mipi_dsi.h>
#include <drm/drm_modes.h>
#include <drm/drm_panel.h>

struct ili9881c_ebbgdjn {
	struct drm_panel panel;
	struct mipi_dsi_device *dsi;
	struct regulator_bulk_data supplies[2];
	struct gpio_desc *reset_gpio;
	bool prepared;
};

static inline
struct ili9881c_ebbgdjn *to_ili9881c_ebbgdjn(struct drm_panel *panel)
{
	return container_of(panel, struct ili9881c_ebbgdjn, panel);
}

static void ili9881c_ebbgdjn_reset(struct ili9881c_ebbgdjn *ctx)
{
	gpiod_set_value_cansleep(ctx->reset_gpio, 0);
	usleep_range(5000, 6000);
	gpiod_set_value_cansleep(ctx->reset_gpio, 1);
	usleep_range(10000, 11000);
	gpiod_set_value_cansleep(ctx->reset_gpio, 0);
	msleep(80);
}

static int ili9881c_ebbgdjn_on(struct ili9881c_ebbgdjn *ctx)
{
	struct mipi_dsi_device *dsi = ctx->dsi;
	struct device *dev = &dsi->dev;
	int ret;

	mipi_dsi_dcs_write_seq(dsi, 0xff, 0x98, 0x81, 0x00, 0xff);
	mipi_dsi_dcs_write_seq(dsi, 0xff, 0x98, 0x81, 0x04);
	mipi_dsi_dcs_write_seq(dsi, 0xff, 0x98, 0x81, 0x04);
	mipi_dsi_dcs_write_seq(dsi, 0xe5, 0x00);
	mipi_dsi_dcs_write_seq(dsi, 0xff, 0x98, 0x81, 0x00);
	usleep_range(10000, 11000);
	mipi_dsi_dcs_write_seq(dsi, 0xff, 0x98, 0x81, 0x00);
	usleep_range(10000, 11000);
	mipi_dsi_dcs_write_seq(dsi, 0x11, 0x00);
	msleep(180);
	mipi_dsi_dcs_write_seq(dsi, 0x29, 0x00);

	ret = mipi_dsi_dcs_set_tear_on(dsi, MIPI_DSI_DCS_TEAR_MODE_VBLANK);
	if (ret < 0) {
		dev_err(dev, "Failed to set tear on: %d\n", ret);
		return ret;
	}
	usleep_range(10000, 11000);

	ret = mipi_dsi_dcs_set_display_brightness(dsi, 0xff0f);
	if (ret < 0) {
		dev_err(dev, "Failed to set display brightness: %d\n", ret);
		return ret;
	}

	mipi_dsi_dcs_write_seq(dsi, MIPI_DCS_WRITE_CONTROL_DISPLAY, 0x2c);
	mipi_dsi_dcs_write_seq(dsi, MIPI_DCS_WRITE_POWER_SAVE, 0x00);
	mipi_dsi_dcs_write_seq(dsi, 0x68, 0x05);
	mipi_dsi_dcs_write_seq(dsi, MIPI_DCS_SET_CABC_MIN_BRIGHTNESS,
			       0x00, 0xff);
	mipi_dsi_dcs_write_seq(dsi, 0xff, 0x98, 0x81, 0x03);
	mipi_dsi_dcs_write_seq(dsi, 0xff, 0x98, 0x81, 0x03);
	mipi_dsi_dcs_write_seq(dsi, MIPI_DCS_WRITE_MEMORY_START, 0x0c);
	mipi_dsi_dcs_write_seq(dsi, 0xff, 0x98, 0x81, 0x02);
	mipi_dsi_dcs_write_seq(dsi, 0xff, 0x98, 0x81, 0x02);
	mipi_dsi_dcs_write_seq(dsi, 0x04, 0x16);
	mipi_dsi_dcs_write_seq(dsi, 0x05, 0x22);
	mipi_dsi_dcs_write_seq(dsi, 0x06, 0x40);
	mipi_dsi_dcs_write_seq(dsi, 0x07, 0x04);
	mipi_dsi_dcs_write_seq(dsi, 0xff, 0x98, 0x81, 0x00);
	mipi_dsi_dcs_write_seq(dsi, 0xff, 0x98, 0x81, 0x00);

	return 0;
}

static int ili9881c_ebbgdjn_off(struct ili9881c_ebbgdjn *ctx)
{
	struct mipi_dsi_device *dsi = ctx->dsi;
	struct device *dev = &dsi->dev;
	int ret;

	mipi_dsi_dcs_write_seq(dsi, 0xff, 0x98, 0x81, 0x00);
	mipi_dsi_dcs_write_seq(dsi, 0xff, 0x98, 0x81, 0x00);

	ret = mipi_dsi_dcs_set_display_off(dsi);
	if (ret < 0) {
		dev_err(dev, "Failed to set display off: %d\n", ret);
		return ret;
	}
	usleep_range(10000, 11000);

	ret = mipi_dsi_dcs_enter_sleep_mode(dsi);
	if (ret < 0) {
		dev_err(dev, "Failed to enter sleep mode: %d\n", ret);
		return ret;
	}
	msleep(150);

	return 0;
}

static int ili9881c_ebbgdjn_prepare(struct drm_panel *panel)
{
	struct ili9881c_ebbgdjn *ctx = to_ili9881c_ebbgdjn(panel);
	struct device *dev = &ctx->dsi->dev;
	int ret;

	if (ctx->prepared)
		return 0;

	ret = regulator_bulk_enable(ARRAY_SIZE(ctx->supplies), ctx->supplies);
	if (ret < 0) {
		dev_err(dev, "Failed to enable regulators: %d\n", ret);
		return ret;
	}

	ili9881c_ebbgdjn_reset(ctx);

	ret = ili9881c_ebbgdjn_on(ctx);
	if (ret < 0) {
		dev_err(dev, "Failed to initialize panel: %d\n", ret);
		gpiod_set_value_cansleep(ctx->reset_gpio, 1);
		regulator_bulk_disable(ARRAY_SIZE(ctx->supplies), ctx->supplies);
		return ret;
	}

	ctx->prepared = true;
	return 0;
}

static int ili9881c_ebbgdjn_unprepare(struct drm_panel *panel)
{
	struct ili9881c_ebbgdjn *ctx = to_ili9881c_ebbgdjn(panel);
	struct device *dev = &ctx->dsi->dev;
	int ret;

	if (!ctx->prepared)
		return 0;

	ret = ili9881c_ebbgdjn_off(ctx);
	if (ret < 0)
		dev_err(dev, "Failed to un-initialize panel: %d\n", ret);

	gpiod_set_value_cansleep(ctx->reset_gpio, 1);
	regulator_bulk_disable(ARRAY_SIZE(ctx->supplies), ctx->supplies);

	ctx->prepared = false;
	return 0;
}

static const struct drm_display_mode ili9881c_ebbgdjn_mode = {
	.clock = (720 + 114 + 8 + 180) * (1280 + 10 + 4 + 18) * 60 / 1000,
	.hdisplay = 720,
	.hsync_start = 720 + 114,
	.hsync_end = 720 + 114 + 8,
	.htotal = 720 + 114 + 8 + 180,
	.vdisplay = 1280,
	.vsync_start = 1280 + 10,
	.vsync_end = 1280 + 10 + 4,
	.vtotal = 1280 + 10 + 4 + 18,
	.width_mm = 62,
	.height_mm = 110,
};

static int ili9881c_ebbgdjn_get_modes(struct drm_panel *panel,
				      struct drm_connector *connector)
{
	struct drm_display_mode *mode;

	mode = drm_mode_duplicate(connector->dev, &ili9881c_ebbgdjn_mode);
	if (!mode)
		return -ENOMEM;

	drm_mode_set_name(mode);

	mode->type = DRM_MODE_TYPE_DRIVER | DRM_MODE_TYPE_PREFERRED;
	connector->display_info.width_mm = mode->width_mm;
	connector->display_info.height_mm = mode->height_mm;
	drm_mode_probed_add(connector, mode);

	return 1;
}

static const struct drm_panel_funcs ili9881c_ebbgdjn_panel_funcs = {
	.prepare = ili9881c_ebbgdjn_prepare,
	.unprepare = ili9881c_ebbgdjn_unprepare,
	.get_modes = ili9881c_ebbgdjn_get_modes,
};

static int ili9881c_ebbgdjn_probe(struct mipi_dsi_device *dsi)
{
	struct device *dev = &dsi->dev;
	struct ili9881c_ebbgdjn *ctx;
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
			  MIPI_DSI_CLOCK_NON_CONTINUOUS;

	drm_panel_init(&ctx->panel, dev, &ili9881c_ebbgdjn_panel_funcs,
		       DRM_MODE_CONNECTOR_DSI);
	ctx->panel.prepare_prev_first = true;

	ret = drm_panel_of_backlight(&ctx->panel);
	if (ret)
		return dev_err_probe(dev, ret, "Failed to get backlight\n");

	drm_panel_add(&ctx->panel);

	ret = mipi_dsi_attach(dsi);
	if (ret < 0) {
		dev_err(dev, "Failed to attach to DSI host: %d\n", ret);
		drm_panel_remove(&ctx->panel);
		return ret;
	}

	return 0;
}

static void ili9881c_ebbgdjn_remove(struct mipi_dsi_device *dsi)
{
	struct ili9881c_ebbgdjn *ctx = mipi_dsi_get_drvdata(dsi);
	int ret;

	ret = mipi_dsi_detach(dsi);
	if (ret < 0)
		dev_err(&dsi->dev, "Failed to detach from DSI host: %d\n", ret);

	drm_panel_remove(&ctx->panel);
}

static const struct of_device_id ili9881c_ebbgdjn_of_match[] = {
	{ .compatible = "mdss,ili9881c-ebbgdjn" }, // FIXME
	{ /* sentinel */ }
};
MODULE_DEVICE_TABLE(of, ili9881c_ebbgdjn_of_match);

static struct mipi_dsi_driver ili9881c_ebbgdjn_driver = {
	.probe = ili9881c_ebbgdjn_probe,
	.remove = ili9881c_ebbgdjn_remove,
	.driver = {
		.name = "panel-ili9881c-ebbgdjn",
		.of_match_table = ili9881c_ebbgdjn_of_match,
	},
};
module_mipi_dsi_driver(ili9881c_ebbgdjn_driver);

MODULE_AUTHOR("linux-mdss-dsi-panel-driver-generator <fix@me>"); // FIXME
MODULE_DESCRIPTION("DRM driver for ili9881c_HD720p_video_EbbgDJN");
MODULE_LICENSE("GPL");
