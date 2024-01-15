// SPDX-License-Identifier: GPL-2.0

#include <linux/clk.h>
#include <linux/clk-provider.h>
#include <linux/kernel.h>
#include <linux/platform_device.h>
#include <linux/pm_opp.h>
#include <linux/regmap.h>
#include <linux/module.h>

#include "clk-hfpll.h"
#include "clk-regmap.h"

static struct hfpll_data hdata = {
	.mode_reg = 0x00,
	.l_reg = 0x04,
	.m_reg = 0x08,
	.n_reg = 0x0c,
	.user_reg = 0x10,
	.config_reg = 0x14,
	.status_reg = 0x1c,
	.lock_bit = 16,
};

static const struct regmap_config a53hfpll_regmap_config = {
	.reg_bits		= 32,
	.reg_stride		= 4,
	.val_bits		= 32,
	.max_register		= 0x40,
	.fast_io		= true,
};

struct hfpll_clk_of_match_data {
	u32 config_val;
	u32 user_val;
};

static const struct hfpll_clk_of_match_data msm8937_c0_data = {
	.config_val = 0x4c015765, .user_val = 0x0100000f,
};

static const struct hfpll_clk_of_match_data sdm439_c0_data = {
	.config_val = 0x44024665, .user_val = 0x0100000f,
};

static const struct hfpll_clk_of_match_data msm8937_c1_data = {
	.config_val = 0, .user_val = 0x0100000f,
};

static int qcom_a53hfpll_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct device_node *np = dev->of_node;
	const struct hfpll_clk_of_match_data *match_data;
	struct clk_hfpll *hfpll;
	struct regmap *regmap;
	void __iomem *base;
	struct clk_init_data init = { };
	u32 min_rate, max_rate;
	int ret;

	hfpll = devm_kzalloc(dev, sizeof(*hfpll), GFP_KERNEL);
	if (!hfpll)
		return -ENOMEM;

	base = devm_platform_ioremap_resource(pdev, 0);
	if (IS_ERR(base))
		return PTR_ERR(base);

	regmap = devm_regmap_init_mmio(dev, base, &a53hfpll_regmap_config);
	if (IS_ERR(regmap))
		return PTR_ERR(regmap);

	if (of_property_read_u32(np, "clock-output-rate-min", &min_rate))
		return -ENODEV;

	if (of_property_read_u32(np, "clock-output-rate-max", &max_rate))
		return -ENODEV;

	hdata.min_rate = min_rate;
	hdata.max_rate = max_rate;

	match_data = of_device_get_match_data(dev);
	if (match_data) {
		if (match_data->config_val) {
			hdata.config_val = match_data->config_val;
		}

		if (match_data->user_val) {
			hdata.user_val = match_data->user_val;
		}
	}

	/* Use an unique name by appending @unit-address */
	init.name = devm_kasprintf(dev, GFP_KERNEL, "a53hfpll%s",
				   strchrnul(np->full_name, '@'));
	if (!init.name)
		return -ENOMEM;

	init.parent_data = &(const struct clk_parent_data){
		.fw_name = "xo", .name = "xo_board",
	};
	init.num_parents = 1;
	init.flags = CLK_IGNORE_UNUSED;
	init.ops = &clk_ops_hfpll;

	hfpll->d = &hdata;
	hfpll->clkr.hw.init = &init;
	spin_lock_init(&hfpll->lock);

	ret = devm_clk_register_regmap(dev, &hfpll->clkr);
	if (ret) {
		dev_err(dev, "failed to register regmap clock: %d\n", ret);
		return ret;
	}

	ret = devm_of_clk_add_hw_provider(dev, of_clk_hw_simple_get,
					   &hfpll->clkr.hw);
	if (ret) {
		dev_err(dev, "failed to add clock provider: %d\n", ret);
		return ret;
	}

	dev_err(dev, "HFPLL is registered!");

	return 0;
}

static const struct of_device_id qcom_a53hfpll_match_table[] = {
	{ .compatible = "qcom,msm8937-c0-a53hfpll", .data = &msm8937_c0_data },
	{ .compatible = "qcom,msm8937-c1-a53hfpll", .data = &msm8937_c1_data },
	{ .compatible = "qcom,sdm439-c0-a53hfpll", .data = &sdm439_c0_data },
	{ .compatible = "qcom,sdm439-c1-a53hfpll", .data = &msm8937_c1_data },
	{ }
};
MODULE_DEVICE_TABLE(of, qcom_a53hfpll_match_table);

static struct platform_driver qcom_a53hfpll_driver = {
	.probe = qcom_a53hfpll_probe,
	.driver = {
		.name = "qcom-a53hfpll",
		.of_match_table = qcom_a53hfpll_match_table,
	},
};
module_platform_driver(qcom_a53hfpll_driver);

MODULE_DESCRIPTION("Qualcomm A53 HFPLL Driver");
MODULE_LICENSE("GPL v2");
