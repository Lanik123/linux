// SPDX-License-Identifier: GPL-2.0-only

#include <linux/clk.h>
#include <linux/clk-provider.h>

#include "dsi_phy.h"

#define DSIPHY_PLL_POWERUP_CTRL					0x034
#define DSIPHY_PLL_PROP_CHRG_PUMP_CTRL			0x038
#define DSIPHY_PLL_INTEG_CHRG_PUMP_CTRL			0x03c
#define DSIPHY_PLL_ANA_TST_LOCK_ST_OVR_CTRL	0x044
#define DSIPHY_PLL_VCO_CTRL				0x048
#define DSIPHY_PLL_GMP_CTRL_DIG_TST			0x04c
#define DSIPHY_PLL_PHA_ERR_CTRL_0			0x050
#define DSIPHY_PLL_LOCK_FILTER				0x054
#define DSIPHY_PLL_UNLOCK_FILTER			0x058
#define DSIPHY_PLL_INPUT_DIV_PLL_OVR			0x05c
#define DSIPHY_PLL_LOOP_DIV_RATIO_0			0x060
#define DSIPHY_PLL_INPUT_LOOP_DIV_RAT_CTRL		0x064
#define DSIPHY_PLL_PRO_DLY_RELOCK			0x06c
#define DSIPHY_PLL_CHAR_PUMP_BIAS_CTRL			0x070
#define DSIPHY_PLL_LOCK_DET_MODE_SEL			0x074
#define DSIPHY_PLL_ANA_PROG_CTRL			0x07c
#define DSIPHY_HS_FREQ_RAN_SEL				0x110
#define DSIPHY_SLEWRATE_FSM_OVR_CTRL			0x280
#define DSIPHY_SLEWRATE_DDL_LOOP_CTRL			0x28c
#define DSIPHY_SLEWRATE_DDL_CYC_FRQ_ADJ_0		0x290
#define DSIPHY_PLL_PHA_ERR_CTRL_1			0x2e4
#define DSIPHY_PLL_LOOP_DIV_RATIO_1			0x2e8
#define DSIPHY_SLEWRATE_DDL_CYC_FRQ_ADJ_1		0x328
#define DSIPHY_SSC0					0x394
#define DSIPHY_SSC7					0x3b0
#define DSIPHY_SSC8					0x3b4
#define DSIPHY_SSC1					0x398
#define DSIPHY_SSC2					0x39c
#define DSIPHY_SSC3					0x3a0
#define DSIPHY_SSC4					0x3a4
#define DSIPHY_SSC5					0x3a8
#define DSIPHY_SSC6					0x3ac
#define DSIPHY_SSC10					0x360
#define DSIPHY_SSC11					0x364
#define DSIPHY_SSC12					0x368
#define DSIPHY_SSC13					0x36c
#define DSIPHY_SSC14					0x370
#define DSIPHY_SSC15					0x374
#define DSIPHY_SSC7					0x3b0
#define DSIPHY_SSC8					0x3b4
#define DSIPHY_SSC9					0x3b8
#define DSIPHY_STAT0					0x3e0
#define DSIPHY_CTRL0					0x3e8
#define DSIPHY_SYS_CTRL					0x3f0
#define DSIPHY_PLL_CTRL					0x3f8

/*
 * Clock tree model for generating DSI byte clock and pclk for 12nm DSI PLL
 *
 *
 *                                          +---------------+
 *                               +----------|    vco_clk    |----------+
 *                               |          +---------------+          |
 *                               |                                     |
 *                               |                                     |
 *                               |                                     |
 *      +---------+---------+----+----+---------+---------+            |
 *      |         |         |         |         |         |            |
 *      |         |         |         |         |         |            |
 *      |         |         |         |         |         |            |
 *  +---v---+ +---v---+ +---v---+ +---v---+ +---v---+ +---v---+        |
 *  | DIV(1)| | DIV(2)| | DIV(4)| | DIV(8)| |DIV(16)| |DIV(32)|        |
 *  +---+---+ +---+---+ +---+---+ +---+---+ +---+---+ +---+---+        |
 *      |         |         |         |         |         |            |
 *      |         |         +---+ +---+         |         |            |
 *      |         +-----------+ | | +-----------+         |            |
 *      +-------------------+ | | | | +-------------------+            |
 *                          | | | | | |                                |
 *                       +--v-v-v-v-v-v---+                            |
 *                        \ post_div_mux /                             |
 *                         \            /                              |
 *                          +-----+----+         +---------------------+
 *                                |              |
 *       +------------------------+              |
 *       |                                       |
 *  +----v----+         +---------+---------+----+----+---------+---------+
 *  |  DIV-4  |         |         |         |         |         |         |
 *  +----+----+         |         |         |         |         |         |
 *       |          +---v---+ +---v---+ +---v---+ +---v---+ +---v---+ +---v---+
 *       |          | DIV(1)| | DIV(2)| | DIV(4)| | DIV(8)| |DIV(16)| |DIV(32)|
 *       |          +---+---+ +---+---+ +---+---+ +---+---+ +---+---+ +---+---+
 *       |              |         |         |         |         |         |
 *       v              |         |         +---+ +---+         |         |
 *  byte_clk_src        |         +-----------+ | | +-----------+         |
 *                      +-------------------+ | | | | +-------------------+
 *                                          | | | | | |
 *                                       +--v-v-v-v-v-v---+
 *                                        \ gp_cntrl_mux /
 *                                         \            /
 *                                          +-----+----+
 *                                                |
 *                                                |
 *                                        +-------v-------+
 *                                        |   (DIV + 1)   |
 *                                        | DIV = 0...127 |
 *                                        +-------+-------+
 *                                                |
 *                                                |
 *                                                v
 *                              dsi_pclk input to Clock Controller MND
 */

#define POLL_MAX_READS			15
#define POLL_TIMEOUT_US			1000

#define VCO_REF_CLK_RATE		19200000
#define VCO_MIN_RATE			1000000000
#define VCO_MAX_RATE			2000000000

struct dsi_pll_config {
	u64 vco_current_rate;

	bool enable_ssc;	/* SSC enable/disable */

	/* fixed params */
	u32 ssc_center;
	u32 ssc_freq;
	u32 ssc_offset;
	u32 ssc_adj_per;

	/* out */
	u32 mpll_ssc_peak_i;
	u32 mpll_stepsize_i;
	u32 mpll_mint_i;
	u32 mpll_frac_den;
	u32 mpll_frac_quot_i;
	u32 mpll_frac_rem;
};

struct pll_12nm_cached_state {
	unsigned long vco_rate;
	u8 cfg0;
	u8 cfg1;
	u8 postdiv1;
	u8 postdiv3;
};

struct dsi_pll_12nm {
	struct clk_hw clk_hw;

	struct msm_dsi_phy *phy;

	spinlock_t postdiv_lock;
	spinlock_t gpdiv_lock;

	struct pll_12nm_cached_state cached_state;
};

#define to_pll_12nm(x)	container_of(x, struct dsi_pll_12nm, clk_hw)

struct dsi_pll_12nm_postdiv {
	struct clk_hw hw;

	/* divider params */
	u8 shift;
	u8 width;
	u8 flags; /* same flags as used by clk_divider struct */

	struct dsi_pll_12nm *pll;
};

#define to_pll_12nm_postdiv(_hw) container_of(_hw, struct dsi_pll_12nm_postdiv, hw)

struct dsi_pll_12nm_gpdiv {
	struct clk_hw hw;

	/* divider params */
	u8 shift;
	u8 width;
	u8 flags; /* same flags as used by clk_divider struct */

	struct dsi_pll_12nm *pll;
};

#define to_pll_12nm_gpdiv(_hw) container_of(_hw, struct dsi_pll_12nm_gpdiv, hw)

struct dsi_pll_12nm_pixel {
	struct clk_hw hw;

	/* divider params */
	u8 shift;
	u8 width;
	u8 flags; /* same flags as used by clk_divider struct */

	struct dsi_pll_12nm *pll;
};

#define to_pll_12nm_pixel(_hw) container_of(_hw, struct dsi_pll_12nm_pixel, hw)

static void pll_12nm_setup_config(struct dsi_pll_config *config)
{
	config->ssc_freq = 31500;
	config->ssc_offset = 5000;
	config->ssc_adj_per = 2;

	/* TODO: ssc enable */
	config->enable_ssc = true;
	config->ssc_center = true;
}

static bool pll_12nm_poll_for_ready(struct dsi_pll_12nm *pll_12nm,
				u32 nb_tries, u32 timeout_us)
{
	u32 status;
	bool pll_locked;

	while (nb_tries--) {
		status = dsi_phy_read(pll_12nm->phy->pll_base + DSIPHY_STAT0);
		pll_locked = !!(status & BIT(1));

		if (pll_locked)
			break;

		udelay(timeout_us);
	}
	pr_err("DSI PLL is %slocked", pll_locked ? "" : "*not* ");

	return pll_locked;
}

/*
 * Clock Callbacks
 */

static u32 _get_hsfreqrange(u64 target_freq)
{
	u64 bitclk_rate_mhz = div_u64((target_freq * 2), 1000000);

	if (bitclk_rate_mhz >= 80 && bitclk_rate_mhz < 90)
		return 0x00;
	else if (bitclk_rate_mhz >= 90 && bitclk_rate_mhz < 100)
		return 0x10;
	else if (bitclk_rate_mhz >= 100 && bitclk_rate_mhz < 110)
		return  0x20;
	else if (bitclk_rate_mhz >= 110 && bitclk_rate_mhz < 120)
		return  0x30;
	else if (bitclk_rate_mhz >= 120 && bitclk_rate_mhz < 130)
		return  0x01;
	else if (bitclk_rate_mhz >= 130 && bitclk_rate_mhz < 140)
		return  0x11;
	else if (bitclk_rate_mhz >= 140 && bitclk_rate_mhz < 150)
		return  0x21;
	else if (bitclk_rate_mhz >= 150 && bitclk_rate_mhz < 160)
		return  0x31;
	else if (bitclk_rate_mhz >= 160 && bitclk_rate_mhz < 170)
		return  0x02;
	else if (bitclk_rate_mhz >= 170 && bitclk_rate_mhz < 180)
		return  0x12;
	else if (bitclk_rate_mhz >= 180 && bitclk_rate_mhz < 190)
		return  0x22;
	else if (bitclk_rate_mhz >= 190 && bitclk_rate_mhz < 205)
		return  0x32;
	else if (bitclk_rate_mhz >= 205 && bitclk_rate_mhz < 220)
		return  0x03;
	else if (bitclk_rate_mhz >= 220 && bitclk_rate_mhz < 235)
		return  0x13;
	else if (bitclk_rate_mhz >= 235 && bitclk_rate_mhz < 250)
		return  0x23;
	else if (bitclk_rate_mhz >= 250 && bitclk_rate_mhz < 275)
		return  0x33;
	else if (bitclk_rate_mhz >= 275 && bitclk_rate_mhz < 300)
		return  0x04;
	else if (bitclk_rate_mhz >= 300 && bitclk_rate_mhz < 325)
		return  0x14;
	else if (bitclk_rate_mhz >= 325 && bitclk_rate_mhz < 350)
		return  0x25;
	else if (bitclk_rate_mhz >= 350 && bitclk_rate_mhz < 400)
		return  0x35;
	else if (bitclk_rate_mhz >= 400 && bitclk_rate_mhz < 450)
		return  0x05;
	else if (bitclk_rate_mhz >= 450 && bitclk_rate_mhz < 500)
		return  0x16;
	else if (bitclk_rate_mhz >= 500 && bitclk_rate_mhz < 550)
		return  0x26;
	else if (bitclk_rate_mhz >= 550 && bitclk_rate_mhz < 600)
		return  0x37;
	else if (bitclk_rate_mhz >= 600 && bitclk_rate_mhz < 650)
		return  0x07;
	else if (bitclk_rate_mhz >= 650 && bitclk_rate_mhz < 700)
		return  0x18;
	else if (bitclk_rate_mhz >= 700 && bitclk_rate_mhz < 750)
		return  0x28;
	else if (bitclk_rate_mhz >= 750 && bitclk_rate_mhz < 800)
		return  0x39;
	else if (bitclk_rate_mhz >= 800 && bitclk_rate_mhz < 850)
		return  0x09;
	else if (bitclk_rate_mhz >= 850 && bitclk_rate_mhz < 900)
		return  0x19;
	else if (bitclk_rate_mhz >= 900 && bitclk_rate_mhz < 950)
		return  0x29;
	else if (bitclk_rate_mhz >= 950 && bitclk_rate_mhz < 1000)
		return  0x3a;
	else if (bitclk_rate_mhz >= 1000 && bitclk_rate_mhz < 1050)
		return  0x0a;
	else if (bitclk_rate_mhz >= 1050 && bitclk_rate_mhz < 1100)
		return  0x1a;
	else if (bitclk_rate_mhz >= 1100 && bitclk_rate_mhz < 1150)
		return  0x2a;
	else if (bitclk_rate_mhz >= 1150 && bitclk_rate_mhz < 1200)
		return  0x3b;
	else if (bitclk_rate_mhz >= 1200 && bitclk_rate_mhz < 1250)
		return  0x0b;
	else if (bitclk_rate_mhz >= 1250 && bitclk_rate_mhz < 1300)
		return  0x1b;
	else if (bitclk_rate_mhz >= 1300 && bitclk_rate_mhz < 1350)
		return  0x2b;
	else if (bitclk_rate_mhz >= 1350 && bitclk_rate_mhz < 1400)
		return  0x3c;
	else if (bitclk_rate_mhz >= 1400 && bitclk_rate_mhz < 1450)
		return  0x0c;
	else if (bitclk_rate_mhz >= 1450 && bitclk_rate_mhz < 1500)
		return  0x1c;
	else if (bitclk_rate_mhz >= 1500 && bitclk_rate_mhz < 1550)
		return  0x2c;
	else if (bitclk_rate_mhz >= 1550 && bitclk_rate_mhz < 1600)
		return  0x3d;
	else if (bitclk_rate_mhz >= 1600 && bitclk_rate_mhz < 1650)
		return  0x0d;
	else if (bitclk_rate_mhz >= 1650 && bitclk_rate_mhz < 1700)
		return  0x1d;
	else if (bitclk_rate_mhz >= 1700 && bitclk_rate_mhz < 1750)
		return  0x2e;
	else if (bitclk_rate_mhz >= 1750 && bitclk_rate_mhz < 1800)
		return  0x3e;
	else if (bitclk_rate_mhz >= 1800 && bitclk_rate_mhz < 1850)
		return  0x0e;
	else if (bitclk_rate_mhz >= 1850 && bitclk_rate_mhz < 1900)
		return  0x1e;
	else if (bitclk_rate_mhz >= 1900 && bitclk_rate_mhz < 1950)
		return  0x2f;
	else if (bitclk_rate_mhz >= 1950 && bitclk_rate_mhz < 2000)
		return  0x3f;
	else if (bitclk_rate_mhz >= 2000 && bitclk_rate_mhz < 2050)
		return  0x0f;
	else if (bitclk_rate_mhz >= 2050 && bitclk_rate_mhz < 2100)
		return  0x40;
	else if (bitclk_rate_mhz >= 2100 && bitclk_rate_mhz < 2150)
		return  0x41;
	else if (bitclk_rate_mhz >= 2150 && bitclk_rate_mhz < 2200)
		return  0x42;
	else if (bitclk_rate_mhz >= 2200 && bitclk_rate_mhz <= 2249)
		return  0x43;
	else if (bitclk_rate_mhz > 2249 && bitclk_rate_mhz < 2300)
		return  0x44;
	else if (bitclk_rate_mhz >= 2300 && bitclk_rate_mhz < 2350)
		return  0x45;
	else if (bitclk_rate_mhz >= 2350 && bitclk_rate_mhz < 2400)
		return  0x46;
	else if (bitclk_rate_mhz >= 2400 && bitclk_rate_mhz < 2450)
		return  0x47;
	else if (bitclk_rate_mhz >= 2450 && bitclk_rate_mhz < 2500)
		return  0x48;
	else
		return  0x49;
}

static u32 _get_osc_freq_target(u64 target_freq)
{
	u64 target_freq_mhz = div_u64(target_freq, 1000000);

	if (target_freq_mhz <= 1000)
		return 1315;
	else if (target_freq_mhz > 1000 && target_freq_mhz <= 1500)
		return 1839;
	else
		return 0;
}

static u32 _get_fsm_ovr_ctrl(u64 target_freq)
{
	u64 bitclk_rate_mhz = div_u64((target_freq * 2), 1000000);

	if (bitclk_rate_mhz > 1500 && bitclk_rate_mhz <= 2500)
		return 0;
	else
		return BIT(6);
}

static u32 _get_multi_intX100(u64 vco_rate, u32 *rem)
{
	u32 reminder = 0;
	u64 temp = 0;
	const u32 ref_clk_rate = VCO_REF_CLK_RATE, quarterX100 = 25;

	temp = div_u64_rem(vco_rate, ref_clk_rate, &reminder);
	temp *= 100;

	/*
	 * Multiplication integer needs to be floored in steps of 0.25
	 * Hence multi_intX100 needs to be rounded off in steps of 25
	 */
	if (reminder < (ref_clk_rate / 4)) {
		*rem = reminder;
		return temp;
	} else if ((reminder >= (ref_clk_rate / 4)) &&
		reminder < (ref_clk_rate / 2)) {
		*rem = (reminder - (ref_clk_rate / 4));
		return (temp + quarterX100);
	} else if ((reminder >= (ref_clk_rate / 2)) &&
		(reminder < ((3 * ref_clk_rate) / 4))) {
		*rem = (reminder - (ref_clk_rate / 2));
		return (temp + (quarterX100 * 2));
	}

	*rem = (reminder - ((3 * ref_clk_rate) / 4));
	return (temp + (quarterX100 * 3));
}

static u32 __calc_gcd(u32 num1, u32 num2)
{
	if (num2 != 0)
		return __calc_gcd(num2, (num1 % num2));
	else
		return num1;
}

static void pll_12nm_ssc_param_calc(struct dsi_pll_12nm *pll_12nm,
					struct dsi_pll_config *config)
{
	u64 multi_intX100 = 0, temp = 0;
	u32 temp_rem1 = 0, temp_rem2 = 0;
	const u64 power_2_17 = 131072, power_2_10 = 1024;
	const u32 ref_clk_rate = VCO_REF_CLK_RATE;

	multi_intX100 = _get_multi_intX100(config->vco_current_rate,
		&temp_rem1);

	/* Calculation for mpll_ssc_peak_i */
	temp = (multi_intX100 * config->ssc_offset * power_2_17);
	temp = div_u64(temp, 100); /* 100 div for multi_intX100 */
	config->mpll_ssc_peak_i =
		(u32) div_u64(temp, 1000000); /*10^6 for SSC PPM */

	/* Calculation for mpll_stepsize_i */
	config->mpll_stepsize_i = (u32) div_u64((config->mpll_ssc_peak_i *
		config->ssc_freq * power_2_10), ref_clk_rate);

	/* Calculation for mpll_mint_i */
	config->mpll_mint_i = (u32) (div_u64((multi_intX100 * 4), 100) - 32);

	/* Calculation for mpll_frac_den */
	config->mpll_frac_den = (u32) div_u64(ref_clk_rate,
		__calc_gcd((u32)config->vco_current_rate, ref_clk_rate));

	/* Calculation for mpll_frac_quot_i */
	temp = (temp_rem1 * power_2_17);
	config->mpll_frac_quot_i =
		(u32) div_u64_rem(temp, ref_clk_rate, &temp_rem2);

	/* Calculation for mpll_frac_rem */
	config->mpll_frac_rem = (u32) div_u64(((u64)temp_rem2 *
		config->mpll_frac_den), ref_clk_rate);
}

static void pll_12nm_ssc_commit(struct dsi_pll_12nm *pll_12nm,
					struct dsi_pll_config *config)
{
	void __iomem *base = pll_12nm->phy->pll_base;
	char data = 0;

	dsi_phy_write(base + DSIPHY_SSC0, 0x27);

	data = (config->mpll_mint_i & 0xff);
	dsi_phy_write(base + DSIPHY_SSC7, data);

	data = ((config->mpll_mint_i & 0xff00) >> 8);
	dsi_phy_write(base + DSIPHY_SSC8, data);

	data = (config->mpll_ssc_peak_i & 0xff);
	dsi_phy_write(base + DSIPHY_SSC1, data);

	data = ((config->mpll_ssc_peak_i & 0xff00) >> 8);
	dsi_phy_write(base + DSIPHY_SSC2, data);

	data = ((config->mpll_ssc_peak_i & 0xf0000) >> 16);
	dsi_phy_write(base + DSIPHY_SSC3, data);

	data = (config->mpll_stepsize_i & 0xff);
	dsi_phy_write(base + DSIPHY_SSC4, data);

	data = ((config->mpll_stepsize_i & 0xff00) >> 8);
	dsi_phy_write(base + DSIPHY_SSC5, data);

	data = ((config->mpll_stepsize_i & 0x1f0000) >> 16);
	dsi_phy_write(base + DSIPHY_SSC6, data);

	data = (config->mpll_frac_quot_i & 0xff);
	dsi_phy_write(base + DSIPHY_SSC10, data);

	data = ((config->mpll_frac_quot_i & 0xff00) >> 8);
	dsi_phy_write(base + DSIPHY_SSC11, data);

	data = (config->mpll_frac_rem & 0xff);
	dsi_phy_write(base + DSIPHY_SSC12, data);

	data = ((config->mpll_frac_rem & 0xff00) >> 8);
	dsi_phy_write(base + DSIPHY_SSC13, data);

	data = (config->mpll_frac_den & 0xff);
	dsi_phy_write(base + DSIPHY_SSC14, data);

	data = ((config->mpll_frac_den & 0xff00) >> 8);
	dsi_phy_write(base + DSIPHY_SSC15, data);
}

int dsi_pll_12nm_clk_set_rate(struct clk_hw *hw, unsigned long rate,
			unsigned long parent_rate)
{
	struct dsi_pll_12nm *pll_12nm = to_pll_12nm(hw);
	void __iomem *base = pll_12nm->phy->pll_base;
	u32 hsfreqrange, osc_freq_target, m_div, fsm_ovr_ctrl;
	u32	prop_cntrl = 0x05, int_cntrl = 0x0, gmp_cntrl = 0x1;
	u32 vco_cntrl, cpbias_cntrl;
	unsigned int post_div_mux = 0;
	struct dsi_pll_config config;
	char data = 0;
	pr_err("dsi_pll_12nm_clk_set_rate\n");
	pll_12nm_setup_config(&config);

	config.vco_current_rate = rate;

	vco_cntrl = dsi_phy_read(base + DSIPHY_PLL_VCO_CTRL);
	vco_cntrl &= 0x30;

	cpbias_cntrl = dsi_phy_read(base + DSIPHY_PLL_CHAR_PUMP_BIAS_CTRL);
	cpbias_cntrl = ((cpbias_cntrl >> 6) & 0x1);

	if (cpbias_cntrl == 0) {
		if (vco_cntrl == 0x00)
			post_div_mux = 0;
		else if (vco_cntrl == 0x10)
			post_div_mux = 2;
		else if (vco_cntrl == 0x20)
			post_div_mux = 3;
		else if (vco_cntrl == 0x30)
			post_div_mux = 4;
	} else if (cpbias_cntrl == 1) {
		if (vco_cntrl == 0x30)
			post_div_mux = 2;
		else if (vco_cntrl == 0x00)
			post_div_mux = 5;
	}
	
	u64 target_freq = div_u64(rate, BIT(post_div_mux));

	hsfreqrange = _get_hsfreqrange(target_freq);
	osc_freq_target = _get_osc_freq_target(target_freq);
	m_div = (u32) div_u64((rate * 4), VCO_REF_CLK_RATE);
	fsm_ovr_ctrl = _get_fsm_ovr_ctrl(target_freq);

	if(config.enable_ssc) {
		pll_12nm_ssc_param_calc(pll_12nm, &config);
	}

	dsi_phy_write(base + DSIPHY_CTRL0, 0x01);
	dsi_phy_write(base + DSIPHY_PLL_CTRL, 0x05);
	dsi_phy_write(base + DSIPHY_SLEWRATE_DDL_LOOP_CTRL, 0x01);

	data = ((hsfreqrange & 0x7f) | BIT(7));
	dsi_phy_write(base + DSIPHY_HS_FREQ_RAN_SEL, data);

	data = (osc_freq_target & 0x7f);
	dsi_phy_write(base + DSIPHY_SLEWRATE_DDL_CYC_FRQ_ADJ_0, data);

	data = ((osc_freq_target & 0xf80) >> 7);
	dsi_phy_write(base + DSIPHY_SLEWRATE_DDL_CYC_FRQ_ADJ_1, data);
	dsi_phy_write(base + DSIPHY_PLL_INPUT_LOOP_DIV_RAT_CTRL, 0x30);

	data = (m_div & 0x3f);
	dsi_phy_write(base + DSIPHY_PLL_LOOP_DIV_RATIO_0, data);

	data = ((m_div & 0xfc0) >> 6);
	dsi_phy_write(base + DSIPHY_PLL_LOOP_DIV_RATIO_1, data);
	dsi_phy_write(base + DSIPHY_PLL_INPUT_DIV_PLL_OVR, 0x60);

	data = (prop_cntrl & 0x3f);
	dsi_phy_write(base + DSIPHY_PLL_PROP_CHRG_PUMP_CTRL, data);

	data = (int_cntrl & 0x3f);
	dsi_phy_write(base + DSIPHY_PLL_INTEG_CHRG_PUMP_CTRL, data);

	data = ((gmp_cntrl & 0x3) << 4);
	dsi_phy_write(base + DSIPHY_PLL_GMP_CTRL_DIG_TST, data);

	dsi_phy_write(base + DSIPHY_PLL_ANA_PROG_CTRL, 0x03);
	dsi_phy_write(base + DSIPHY_PLL_ANA_TST_LOCK_ST_OVR_CTRL, 0x50);
	dsi_phy_write(base + DSIPHY_SLEWRATE_FSM_OVR_CTRL, fsm_ovr_ctrl);
	dsi_phy_write(base + DSIPHY_PLL_PHA_ERR_CTRL_0, 0x01);
	dsi_phy_write(base + DSIPHY_PLL_PHA_ERR_CTRL_1, 0x00);
	dsi_phy_write(base + DSIPHY_PLL_LOCK_FILTER, 0xff);
	dsi_phy_write(base + DSIPHY_PLL_UNLOCK_FILTER, 0x03);
	dsi_phy_write(base + DSIPHY_PLL_PRO_DLY_RELOCK, 0x0c);
	dsi_phy_write(base + DSIPHY_PLL_LOCK_DET_MODE_SEL, 0x02);

	if(config.enable_ssc) {
		pll_12nm_ssc_commit(pll_12nm, &config);
	}
	return 0;
}

static int dsi_pll_12nm_clk_is_enabled(struct clk_hw *hw)
{
	struct dsi_pll_12nm *pll_12nm = to_pll_12nm(hw);

	return pll_12nm_poll_for_ready(pll_12nm, POLL_MAX_READS,
					POLL_TIMEOUT_US);
}

unsigned long dsi_pll_12nm_clk_recalc_rate(struct clk_hw *hw,
					unsigned long parent_rate)
{
	struct dsi_pll_12nm *pll_12nm = to_pll_12nm(hw);
	void __iomem *base = pll_12nm->phy->pll_base;
	unsigned long vco_current_rate = clk_hw_get_rate(hw);
	const u64 ref_clk = VCO_REF_CLK_RATE;
	u64 vco_rate = 0;
	u32 m_div_5_0 = 0, m_div_11_6 = 0, m_div = 0;

	if (vco_current_rate != 0) {
		return vco_current_rate;
	}

	m_div_5_0 = dsi_phy_read(base +
			DSIPHY_PLL_LOOP_DIV_RATIO_0);
	m_div_5_0 &= 0x3f;
	m_div_11_6 = dsi_phy_read(base +
			DSIPHY_PLL_LOOP_DIV_RATIO_1);
	m_div_11_6 &= 0x3f;
	m_div = ((m_div_11_6 << 6) | (m_div_5_0));
	vco_rate = div_u64((ref_clk * m_div), 4);

	pr_err("vco rate in recalc = %lld\n", vco_rate);

	return (unsigned long)vco_rate;
}

int dsi_pll_12nm_vco_prepare(struct clk_hw *hw)
{
	
	struct dsi_pll_12nm *pll_12nm = to_pll_12nm(hw);
	void __iomem *base = pll_12nm->phy->pll_base;
	u32 data = 0;
	bool locked;
	pr_err("dsi_pll_12nm_vco_prepare\n");
	if (unlikely(pll_12nm->phy->pll_on))
		return 0;

	/*
	 * For cases where  DSI PHY is already enabled like:
	 * 1.) LP-11 during static screen
	 * 2.) ULPS during static screen
	 * 3.) Boot up with cont splash enabled where PHY is programmed in LK
	 * Execute the Re-lock sequence to enable the DSI PLL.
	 */
	data = dsi_phy_read(base + DSIPHY_SYS_CTRL);
	if (data & BIT(7)) {
		data = dsi_phy_read(base + DSIPHY_PLL_POWERUP_CTRL);
		data &= ~BIT(1); /* remove ONPLL_OVR_EN bit */
		data |= 0x1; /* set ONPLL_OVN to 0x1 */
		dsi_phy_write_ndelay(base + DSIPHY_PLL_POWERUP_CTRL, data, 500);
		dsi_phy_write_udelay(base + DSIPHY_SYS_CTRL, 0x49, 5);
		dsi_phy_write_udelay(base + DSIPHY_SYS_CTRL, 0xc9, 50);

		locked = pll_12nm_poll_for_ready(pll_12nm, POLL_MAX_READS,
					 POLL_TIMEOUT_US);

		if (unlikely(!locked)) {
			pr_err("DSI PLL lock failed! 1\n");
			return -EINVAL;
		}

		data = dsi_phy_read(base + DSIPHY_PLL_CTRL);
		data |= 0x01; /* set CLK_SEL bits to 0x1 */
		dsi_phy_write_ndelay(base + DSIPHY_PLL_CTRL, data, 500);
	}

	dsi_phy_write_udelay(base + DSIPHY_SYS_CTRL, 0x49, 5);
	dsi_phy_write_udelay(base + DSIPHY_SYS_CTRL, 0xc9, 50);

	locked = pll_12nm_poll_for_ready(pll_12nm, POLL_MAX_READS,
					 POLL_TIMEOUT_US);

	if (unlikely(!locked)) {
		pr_err("DSI PLL lock failed! 2\n");
		return -EINVAL;
	}

	pll_12nm->phy->pll_on = true;

	return 0;
}

void dsi_pll_12nm_vco_unprepare(struct clk_hw *hw)
{
	struct dsi_pll_12nm *pll_12nm = to_pll_12nm(hw);
	void __iomem *base = pll_12nm->phy->pll_base;
	u32 data = 0;

	if (unlikely(!pll_12nm->phy->pll_on))
		return;

	data = dsi_phy_read(base + DSIPHY_SSC0);
	data &= ~BIT(6); /* disable GP_CLK_EN */
	dsi_phy_write_ndelay(base + DSIPHY_SSC0, data, 500);

	data = dsi_phy_read(base + DSIPHY_PLL_CTRL);
	data &= ~0x03; /* remove CLK_SEL bits */
	dsi_phy_write_ndelay(base + DSIPHY_PLL_CTRL, data, 500);

	data = dsi_phy_read(base + DSIPHY_PLL_POWERUP_CTRL);
	data &= ~0x1; /* remove ONPLL_OVR bit */
	data |= BIT(1); /* set ONPLL_OVR_EN to 0x1 */
	dsi_phy_write_ndelay(base + DSIPHY_PLL_POWERUP_CTRL, data, 500);

	pll_12nm->phy->pll_on = false;
}

static long dsi_pll_12nm_clk_round_rate(struct clk_hw *hw,
		unsigned long rate, unsigned long *parent_rate)
{
	struct dsi_pll_12nm *pll_12nm = to_pll_12nm(hw);

	if      (rate < pll_12nm->phy->cfg->min_pll_rate)
		return  pll_12nm->phy->cfg->min_pll_rate;
	else if (rate > pll_12nm->phy->cfg->max_pll_rate)
		return  pll_12nm->phy->cfg->max_pll_rate;
	else
		return rate;
}

static int dsi_pll_12nm_clk_enable(struct clk_hw *hw)
{
	struct dsi_pll_12nm *pll_12nm = to_pll_12nm(hw);
	void __iomem *base = pll_12nm->phy->pll_base;
	u32 data = 0;
	pr_err("dsi_pll_12nm_clk_enable\n");
	if (unlikely(!pll_12nm->phy->pll_on))
		return 0;

	data = dsi_phy_read(base + DSIPHY_SSC0);
	data |= BIT(6); /* enable GP_CLK_EN */
	dsi_phy_write(base + DSIPHY_SSC0, data);
	wmb(); /* make sure register committed before enabling branch clocks */

	return 0;
}

static const struct clk_ops clk_ops_dsi_pll_12nm_vco = {
	.round_rate = dsi_pll_12nm_clk_round_rate,
	.set_rate = dsi_pll_12nm_clk_set_rate,
	.recalc_rate = dsi_pll_12nm_clk_recalc_rate,
	.prepare = dsi_pll_12nm_vco_prepare,
	.unprepare = dsi_pll_12nm_vco_unprepare,
	.is_enabled = dsi_pll_12nm_clk_is_enabled,
	.enable = dsi_pll_12nm_clk_enable,
};

#define div_mask(width)	((1 << (width)) - 1)
static u8 dsi_pll_12nm_postdiv_get_parent(struct clk_hw *hw)
{
	struct dsi_pll_12nm_postdiv *postdiv = to_pll_12nm_postdiv(hw);
	struct dsi_pll_12nm *pll_12nm = postdiv->pll;
	void __iomem *base = pll_12nm->phy->pll_base;
	u32 val = 0, vco_cntrl = 0, cpbias_cntrl = 0;

	vco_cntrl = dsi_phy_read(base + DSIPHY_PLL_VCO_CTRL);
	vco_cntrl &= 0x30;

	cpbias_cntrl = dsi_phy_read(base +
		DSIPHY_PLL_CHAR_PUMP_BIAS_CTRL);
	cpbias_cntrl = ((cpbias_cntrl >> 6) & 0x1);

	if (cpbias_cntrl == 0) {
		if (vco_cntrl == 0x00)
			val = 0;
		else if (vco_cntrl == 0x10)
			val = 2;
		else if (vco_cntrl == 0x20)
			val = 3;
		else if (vco_cntrl == 0x30)
			val = 4;
	} else if (cpbias_cntrl == 1) {
		if (vco_cntrl == 0x30)
			val = 2;
		else if (vco_cntrl == 0x00)
			val = 5;
	}

	return val;
}

static int dsi_pll_12nm_postdiv_set_parent(struct clk_hw *hw, u8 index)
{
	struct dsi_pll_12nm_postdiv *postdiv = to_pll_12nm_postdiv(hw);
	struct dsi_pll_12nm *pll_12nm = postdiv->pll;
	void __iomem *base = pll_12nm->phy->base;
	spinlock_t *lock = &pll_12nm->postdiv_lock;
	unsigned long flags = 0;
	u32 vco_cntrl = 0, cpbias_cntrl = 0;
	char data = 0;

	long long vco_rate = clk_hw_get_rate(pll_12nm->phy->vco_hw);
	pr_err("vco rate = %lld\n", vco_rate);

	spin_lock_irqsave(lock, flags);

	long long rate = div_u64(vco_rate, BIT(index));
	u64 target_freq_mhz = div_u64(rate, 1000000);

	if (index == 0) {
		vco_cntrl = 0x00;
		cpbias_cntrl = 0;
	} else if (index == 1) {
		vco_cntrl = 0x30;
		cpbias_cntrl = 1;
	} else if (index == 2) {
		vco_cntrl = 0x10;
		cpbias_cntrl = 0;
	} else if (index == 3) {
		vco_cntrl = 0x20;
		cpbias_cntrl = 0;
	} else if (index == 4) {
		vco_cntrl = 0x30;
		cpbias_cntrl = 0;
	} else if (index == 5) {
		vco_cntrl = 0x00;
		cpbias_cntrl = 1;
	} else {
		pr_err("%d index not found\n", index);
	}

	if (target_freq_mhz <= 1250 && target_freq_mhz >= 1092)
		vco_cntrl = vco_cntrl | 2;
	else if (target_freq_mhz < 1092 && target_freq_mhz >= 950)
		vco_cntrl =  vco_cntrl | 3;
	else if (target_freq_mhz < 950 && target_freq_mhz >= 712)
		vco_cntrl = vco_cntrl | 1;
	else if (target_freq_mhz < 712 && target_freq_mhz >= 546)
		vco_cntrl =  vco_cntrl | 2;
	else if (target_freq_mhz < 546 && target_freq_mhz >= 475)
		vco_cntrl = vco_cntrl | 3;
	else if (target_freq_mhz < 475 && target_freq_mhz >= 356)
		vco_cntrl =  vco_cntrl | 1;
	else if (target_freq_mhz < 356 && target_freq_mhz >= 273)
		vco_cntrl = vco_cntrl | 2;
	else if (target_freq_mhz < 273 && target_freq_mhz >= 237)
		vco_cntrl =  vco_cntrl | 3;
	else if (target_freq_mhz < 237 && target_freq_mhz >= 178)
		vco_cntrl = vco_cntrl | 1;
	else if (target_freq_mhz < 178 && target_freq_mhz >= 136)
		vco_cntrl =  vco_cntrl | 2;
	else if (target_freq_mhz < 136 && target_freq_mhz >= 118)
		vco_cntrl = vco_cntrl | 3;
	else if (target_freq_mhz < 118 && target_freq_mhz >= 89)
		vco_cntrl =  vco_cntrl | 1;
	else if (target_freq_mhz < 89 && target_freq_mhz >= 68)
		vco_cntrl = vco_cntrl | 2;
	else if (target_freq_mhz < 68 && target_freq_mhz >= 57)
		vco_cntrl =  vco_cntrl | 3;
	else if (target_freq_mhz < 57 && target_freq_mhz >= 44)
		vco_cntrl = vco_cntrl | 1;
	else
		vco_cntrl =  vco_cntrl | 2;

	data = ((vco_cntrl & 0x3f) | BIT(6));
	dsi_phy_write(base +  DSIPHY_PLL_VCO_CTRL, data);

	data = ((cpbias_cntrl & 0x1) << 6) | BIT(4);
	dsi_phy_write(base +  DSIPHY_PLL_CHAR_PUMP_BIAS_CTRL, data);

	spin_unlock_irqrestore(lock, flags);

	return 0;
}

static int dsi_pll_12nm_postdiv_determine_rate(struct clk_hw *hw,
				  struct clk_rate_request *req)
{
	struct dsi_pll_12nm_postdiv *postdiv = to_pll_12nm_postdiv(hw);

	return clk_mux_determine_rate_flags(hw, req, postdiv->flags);
}

static const struct clk_ops clk_ops_dsi_pll_12nm_postdiv = {
	.get_parent = dsi_pll_12nm_postdiv_get_parent,
	.set_parent = dsi_pll_12nm_postdiv_set_parent,
	.determine_rate = dsi_pll_12nm_postdiv_determine_rate,
};

static u8 dsi_pll_12nm_gpdiv_get_parent(struct clk_hw *hw)
{
	struct dsi_pll_12nm_gpdiv *gpdiv = to_pll_12nm_gpdiv(hw);
	struct dsi_pll_12nm *pll_12nm = gpdiv->pll;
	void __iomem *base = pll_12nm->phy->pll_base;
	u32 val = 0;

	val = dsi_phy_read(base + DSIPHY_PLL_CTRL);
	val = (val >> 5) & 0x7;
	pr_err("get_gp_val = %lu\n", val);

	return val;
}

static int dsi_pll_12nm_gpdiv_set_parent(struct clk_hw *hw, u8 index)
{
	struct dsi_pll_12nm_gpdiv *gpdiv = to_pll_12nm_gpdiv(hw);
	struct dsi_pll_12nm *pll_12nm = gpdiv->pll;
	void __iomem *base = pll_12nm->phy->base;
	spinlock_t *lock = &pll_12nm->gpdiv_lock;
	unsigned long flags = 0;
	u32 val = 0;

	spin_lock_irqsave(lock, flags);

	val = ((index & 0x7) << 5) | 0x5;
	pr_err("set_gp_val = %lu\n", val);
	dsi_phy_write(base +  DSIPHY_PLL_CTRL, val);

	spin_unlock_irqrestore(lock, flags);

	return 0;
}

static int dsi_pll_12nm_gpdiv_determine_rate(struct clk_hw *hw,
				  struct clk_rate_request *req)
{
	struct dsi_pll_12nm_gpdiv *gpdiv = to_pll_12nm_gpdiv(hw);

	return clk_mux_determine_rate_flags(hw, req, gpdiv->flags);
}

static const struct clk_ops clk_ops_dsi_pll_12nm_gpdiv = {
	.get_parent = dsi_pll_12nm_gpdiv_get_parent,
	.set_parent = dsi_pll_12nm_gpdiv_set_parent,
	.determine_rate = dsi_pll_12nm_gpdiv_determine_rate,
};

#define div_mask(width)	((1 << (width)) - 1)
static unsigned long dsi_pll_12nm_pixel_recalc_rate(struct clk_hw *hw,
						      unsigned long parent_rate)
{
	struct dsi_pll_12nm_pixel *postdiv = to_pll_12nm_pixel(hw);
	struct dsi_pll_12nm *pll_12nm = postdiv->pll;
	void __iomem *base = pll_12nm->phy->base;
	u8 shift = postdiv->shift;
	u8 width = postdiv->width;
	u32 val;

	pr_err("DSI%d PLL parent rate=%lu\n", pll_12nm->phy->id, parent_rate);

	val = dsi_phy_read(base + DSIPHY_SSC9) & 0x7F;
	pr_err("pixel_val = %lu\n", val);

	return divider_recalc_rate(hw, parent_rate, val, NULL,
				   postdiv->flags, width);
}

static long dsi_pll_12nm_pixel_round_rate(struct clk_hw *hw,
					    unsigned long rate,
					    unsigned long *prate)
{
	struct dsi_pll_12nm_pixel *postdiv = to_pll_12nm_pixel(hw);
	struct dsi_pll_12nm *pll_12nm = postdiv->pll;

	DBG("DSI%d PLL parent rate=%lu", pll_12nm->phy->id, rate);

	return divider_round_rate(hw, rate, prate, NULL,
				  postdiv->width,
				  postdiv->flags);
}

static int dsi_pll_12nm_pixel_set_rate(struct clk_hw *hw, unsigned long rate,
					 unsigned long parent_rate)
{
	struct dsi_pll_12nm_pixel *postdiv = to_pll_12nm_pixel(hw);
	struct dsi_pll_12nm *pll_12nm = postdiv->pll;
	void __iomem *base = pll_12nm->phy->base;
	spinlock_t *lock = &pll_12nm->postdiv_lock;
	u8 shift = postdiv->shift;
	u8 width = postdiv->width;
	unsigned int value;
	unsigned long flags = 0;
	u32 val;

	DBG("DSI%d PLL parent rate=%lu parent rate %lu", pll_12nm->phy->id, rate,
	    parent_rate);

	value = divider_get_val(rate, parent_rate, NULL, postdiv->width,
				postdiv->flags);

	pr_err("div_val = %lu\n", value);

	spin_lock_irqsave(lock, flags);

	val = dsi_phy_read(base + DSIPHY_SSC9) & 0x7F;

	val |= value;
	dsi_phy_write(base + DSIPHY_SSC9, val);

	spin_unlock_irqrestore(lock, flags);

	return 0;
}

static const struct clk_ops clk_ops_dsi_pll_12nm_pixel = {
	.recalc_rate = dsi_pll_12nm_pixel_recalc_rate,
	.round_rate = dsi_pll_12nm_pixel_round_rate,
	.set_rate = dsi_pll_12nm_pixel_set_rate,
};

/*
 * PLL Callbacks
 */

static void dsi_12nm_pll_save_state(struct msm_dsi_phy *phy)
{
	struct dsi_pll_12nm *pll_12nm = to_pll_12nm(phy->vco_hw);
	struct pll_12nm_cached_state *cached_state = &pll_12nm->cached_state;
	void __iomem *base = pll_12nm->phy->pll_base;

	cached_state->cfg0 = dsi_phy_read(base + DSIPHY_PLL_VCO_CTRL);
	cached_state->cfg1 = dsi_phy_read(base + DSIPHY_PLL_CHAR_PUMP_BIAS_CTRL);
	cached_state->postdiv1 = dsi_phy_read(base + DSIPHY_PLL_CTRL);
	cached_state->postdiv3 = dsi_phy_read(base + DSIPHY_SSC9);
	if (dsi_pll_12nm_clk_is_enabled(phy->vco_hw))
		cached_state->vco_rate = clk_hw_get_rate(phy->vco_hw);
	else
		cached_state->vco_rate = 0;
}

static int dsi_12nm_pll_restore_state(struct msm_dsi_phy *phy)
{
	struct dsi_pll_12nm *pll_12nm = to_pll_12nm(phy->vco_hw);
	struct pll_12nm_cached_state *cached_state = &pll_12nm->cached_state;
	void __iomem *base = pll_12nm->phy->pll_base;
	int ret;

	ret = dsi_pll_12nm_clk_set_rate(phy->vco_hw,
					cached_state->vco_rate, 0);
	if (ret) {
		DRM_DEV_ERROR(&pll_12nm->phy->pdev->dev,
			"restore vco rate failed. ret=%d\n", ret);
		return ret;
	}

	dsi_phy_write(base + DSIPHY_PLL_VCO_CTRL, cached_state->cfg0);
	dsi_phy_write(base + DSIPHY_PLL_CHAR_PUMP_BIAS_CTRL,
		      cached_state->cfg1);
	dsi_phy_write(base + DSIPHY_PLL_CTRL, cached_state->postdiv3);
	dsi_phy_write(base + DSIPHY_SSC9, cached_state->postdiv1);

	return 0;
}

static struct clk_hw *pll_12nm_postdiv_register(struct dsi_pll_12nm *pll_12nm,
						const char *name,
						const struct clk_hw **parent_hws,
						unsigned long flags)
{
	struct dsi_pll_12nm_postdiv *pll_postdiv;
	struct device *dev = &pll_12nm->phy->pdev->dev;
	struct clk_init_data postdiv_init = {
		.parent_hws = parent_hws,
		.num_parents = 6,
		.name = name,
		.flags = flags,
		.ops = &clk_ops_dsi_pll_12nm_postdiv,
	};
	int ret;

	pll_postdiv = devm_kzalloc(dev, sizeof(*pll_postdiv), GFP_KERNEL);
	if (!pll_postdiv)
		return ERR_PTR(-ENOMEM);

	pll_postdiv->pll = pll_12nm;
	pll_postdiv->shift = 0;
	pll_postdiv->width = 3;
	pll_postdiv->flags = flags;
	pll_postdiv->hw.init = &postdiv_init;

	ret = devm_clk_hw_register(dev, &pll_postdiv->hw);
	if (ret)
		return ERR_PTR(ret);

	return &pll_postdiv->hw;
}

static struct clk_hw *pll_12nm_gpdiv_register(struct dsi_pll_12nm *pll_12nm,
						const char *name,
						const struct clk_hw **parent_hws,
						unsigned long flags)
{
	struct dsi_pll_12nm_gpdiv *pll_gpdiv;
	struct device *dev = &pll_12nm->phy->pdev->dev;
	struct clk_init_data gpdiv_init = {
		.parent_hws = parent_hws,
		.num_parents = 6,
		.name = name,
		.flags = flags,
		.ops = &clk_ops_dsi_pll_12nm_gpdiv,
	};
	int ret;

	pll_gpdiv = devm_kzalloc(dev, sizeof(*pll_gpdiv), GFP_KERNEL);
	if (!pll_gpdiv)
		return ERR_PTR(-ENOMEM);

	pll_gpdiv->pll = pll_12nm;
	pll_gpdiv->shift = 0;
	pll_gpdiv->width = 3;
	pll_gpdiv->flags = flags;
	pll_gpdiv->hw.init = &gpdiv_init;

	ret = devm_clk_hw_register(dev, &pll_gpdiv->hw);
	if (ret)
		return ERR_PTR(ret);

	return &pll_gpdiv->hw;
}

static int pll_12nm_register(struct dsi_pll_12nm *pll_12nm, struct clk_hw **provided_clocks)
{
	char clk_name[32];
	struct clk_init_data vco_init = {
		.parent_data = &(const struct clk_parent_data) {
			.fw_name = "ref", .name = "xo",
		},
		.num_parents = 1,
		.name = clk_name,
		.flags = CLK_IGNORE_UNUSED,
		.ops = &clk_ops_dsi_pll_12nm_vco,
	};
	struct device *dev = &pll_12nm->phy->pdev->dev;
	struct clk_hw *hw, *post_div_mux, *gp_div_mux;
	struct clk_hw *postdiv1, *postdiv2, *postdiv4, *postdiv8, *postdiv16, *postdiv32;
	struct clk_hw *gpdiv1, *gpdiv2, *gpdiv4, *gpdiv8, *gpdiv16, *gpdiv32;
	int ret;

	DBG("%d", pll_12nm->phy->id);

	snprintf(clk_name, sizeof(clk_name), "dsi%dvco_clk", pll_12nm->phy->id);
	pll_12nm->clk_hw.init = &vco_init;
	ret = devm_clk_hw_register(dev, &pll_12nm->clk_hw);
	if (ret)
		return ret;

	snprintf(clk_name, sizeof(clk_name), "dsi%dpostdiv%d_clk", pll_12nm->phy->id, 1);
	postdiv1 = devm_clk_hw_register_fixed_factor_parent_hw(dev,
		clk_name, &pll_12nm->clk_hw, CLK_SET_RATE_PARENT, 1, 1);
	if (IS_ERR(postdiv1))
		return PTR_ERR(postdiv1);

	snprintf(clk_name, sizeof(clk_name), "dsi%dpostdiv%d_clk", pll_12nm->phy->id, 2);
	postdiv2 = devm_clk_hw_register_fixed_factor_parent_hw(dev,
		clk_name, &pll_12nm->clk_hw, CLK_SET_RATE_PARENT, 1, 2);
	if (IS_ERR(postdiv2))
		return PTR_ERR(postdiv2);

	snprintf(clk_name, sizeof(clk_name), "dsi%dpostdiv%d_clk", pll_12nm->phy->id, 4);
	postdiv4 = devm_clk_hw_register_fixed_factor_parent_hw(dev,
		clk_name, &pll_12nm->clk_hw, CLK_SET_RATE_PARENT, 1, 4);
	if (IS_ERR(postdiv4))
		return PTR_ERR(postdiv4);

	snprintf(clk_name, sizeof(clk_name), "dsi%dpostdiv%d_clk", pll_12nm->phy->id, 8);
	postdiv8 = devm_clk_hw_register_fixed_factor_parent_hw(dev,
		clk_name, &pll_12nm->clk_hw, CLK_SET_RATE_PARENT, 1, 8);
	if (IS_ERR(postdiv8))
		return PTR_ERR(postdiv8);

	snprintf(clk_name, sizeof(clk_name), "dsi%dpostdiv%d_clk", pll_12nm->phy->id, 16);
	postdiv16 = devm_clk_hw_register_fixed_factor_parent_hw(dev,
		clk_name, &pll_12nm->clk_hw, CLK_SET_RATE_PARENT, 1, 16);
	if (IS_ERR(postdiv16))
		return PTR_ERR(postdiv16);

	snprintf(clk_name, sizeof(clk_name), "dsi%dpostdiv%d_clk", pll_12nm->phy->id, 32);
	postdiv32 = devm_clk_hw_register_fixed_factor_parent_hw(dev,
		clk_name, &pll_12nm->clk_hw, CLK_SET_RATE_PARENT, 1, 32);
	if (IS_ERR(postdiv32))
		return PTR_ERR(postdiv32);

	snprintf(clk_name, sizeof(clk_name), "dsi%dpostdiv_mux", pll_12nm->phy->id);
	post_div_mux = pll_12nm_postdiv_register(pll_12nm, clk_name, 
			((const struct clk_hw *[]){
				postdiv1, postdiv2, postdiv4, postdiv8, postdiv16, postdiv32
			}), CLK_SET_RATE_PARENT);
	if (IS_ERR(post_div_mux))
		return PTR_ERR(post_div_mux);

	snprintf(clk_name, sizeof(clk_name), "dsi%dpllbyte", pll_12nm->phy->id);
	hw = devm_clk_hw_register_fixed_factor_parent_hw(dev, clk_name,
			post_div_mux, CLK_SET_RATE_PARENT, 1, 4);
	if (IS_ERR(hw))
		return PTR_ERR(hw);
	provided_clocks[DSI_BYTE_PLL_CLK] = hw;

	snprintf(clk_name, sizeof(clk_name), "dsi%dgpdiv%d_clk", pll_12nm->phy->id, 1);
	gpdiv1 = devm_clk_hw_register_fixed_factor_parent_hw(dev,
		clk_name, &pll_12nm->clk_hw, 0, 1, 1);
	if (IS_ERR(gpdiv1))
		return PTR_ERR(gpdiv1);

	snprintf(clk_name, sizeof(clk_name), "dsi%dgpdiv%d_clk", pll_12nm->phy->id, 2);
	gpdiv2 = devm_clk_hw_register_fixed_factor_parent_hw(dev,
		clk_name, &pll_12nm->clk_hw, 0, 1, 2);
	if (IS_ERR(gpdiv2))
		return PTR_ERR(gpdiv2);

	snprintf(clk_name, sizeof(clk_name), "dsi%dgpdiv%d_clk", pll_12nm->phy->id, 4);
	gpdiv4 = devm_clk_hw_register_fixed_factor_parent_hw(dev,
		clk_name, &pll_12nm->clk_hw, 0, 1, 4);
	if (IS_ERR(gpdiv4))
		return PTR_ERR(gpdiv4);

	snprintf(clk_name, sizeof(clk_name), "dsi%dgpdiv%d_clk", pll_12nm->phy->id, 8);
	gpdiv8 = devm_clk_hw_register_fixed_factor_parent_hw(dev,
		clk_name, &pll_12nm->clk_hw, 0, 1, 8);
	if (IS_ERR(gpdiv8))
		return PTR_ERR(gpdiv8);

	snprintf(clk_name, sizeof(clk_name), "dsi%dgpdiv%d_clk", pll_12nm->phy->id, 16);
	gpdiv16 = devm_clk_hw_register_fixed_factor_parent_hw(dev,
		clk_name, &pll_12nm->clk_hw, 0, 1, 16);
	if (IS_ERR(gpdiv16))
		return PTR_ERR(gpdiv16);

	snprintf(clk_name, sizeof(clk_name), "dsi%dgpdiv%d_clk", pll_12nm->phy->id, 32);
	gpdiv32 = devm_clk_hw_register_fixed_factor_parent_hw(dev,
		clk_name, &pll_12nm->clk_hw, 0, 1, 32);
	if (IS_ERR(gpdiv32))
		return PTR_ERR(gpdiv32);

	snprintf(clk_name, sizeof(clk_name), "dsi%dgpdiv_mux", pll_12nm->phy->id);
	gp_div_mux = pll_12nm_gpdiv_register(pll_12nm, clk_name, 
			((const struct clk_hw *[]){
				gpdiv1, gpdiv2, gpdiv4, gpdiv8, gpdiv16, gpdiv32
			}), CLK_SET_RATE_PARENT);
	if (IS_ERR(gp_div_mux))
		return PTR_ERR(gp_div_mux);	

	snprintf(clk_name, sizeof(clk_name), "dsi%dpll", pll_12nm->phy->id);
	hw = devm_clk_hw_register_divider_parent_hw(dev, clk_name,
			gp_div_mux, 0, pll_12nm->phy->pll_base +
				DSIPHY_SSC9,
			0, 8, 0, NULL);
		 
	if (IS_ERR(hw))
		return PTR_ERR(hw);
	provided_clocks[DSI_PIXEL_PLL_CLK] = hw;

	return 0;
}

static int dsi_pll_12nm_init(struct msm_dsi_phy *phy)
{
	struct platform_device *pdev = phy->pdev;
	struct dsi_pll_12nm *pll_12nm;
	int ret;

	if (!pdev)
		return -ENODEV;

	pll_12nm = devm_kzalloc(&pdev->dev, sizeof(*pll_12nm), GFP_KERNEL);
	if (!pll_12nm)
		return -ENOMEM;

	pll_12nm->phy = phy;

	ret = pll_12nm_register(pll_12nm, phy->provided_clocks->hws);
	if (ret) {
		DRM_DEV_ERROR(&pdev->dev, "failed to register PLL: %d\n", ret);
		return ret;
	}

	phy->vco_hw = &pll_12nm->clk_hw;
	long long rate = clk_hw_get_rate(phy->vco_hw);
	pr_err("vco rate = %lld\n", rate);

	return 0;
}

#define T_TA_GO_TIM_COUNT                    0x014
#define T_TA_SURE_TIM_COUNT                  0x018
#define HSTX_DRIV_INDATA_CTRL_CLKLANE        0x0c0
#define HSTX_DATAREV_CTRL_CLKLANE            0x0d4
#define HSTX_DRIV_INDATA_CTRL_LANE0          0x100
#define HSTX_READY_DLY_DATA_REV_CTRL_LANE0   0x114
#define HSTX_DRIV_INDATA_CTRL_LANE1          0x140
#define HSTX_READY_DLY_DATA_REV_CTRL_LANE1   0x154
#define HSTX_CLKLANE_REQSTATE_TIM_CTRL       0x180
#define HSTX_CLKLANE_HS0STATE_TIM_CTRL       0x188
#define HSTX_CLKLANE_TRALSTATE_TIM_CTRL      0x18c
#define HSTX_CLKLANE_EXITSTATE_TIM_CTRL      0x190
#define HSTX_CLKLANE_CLKPOSTSTATE_TIM_CTRL   0x194
#define HSTX_DATALANE_REQSTATE_TIM_CTRL      0x1c0
#define HSTX_DATALANE_HS0STATE_TIM_CTRL      0x1c8
#define HSTX_DATALANE_TRAILSTATE_TIM_CTRL    0x1cc
#define HSTX_DATALANE_EXITSTATE_TIM_CTRL     0x1d0
#define HSTX_DRIV_INDATA_CTRL_LANE2          0x200
#define HSTX_READY_DLY_DATA_REV_CTRL_LANE2   0x214
#define HSTX_READY_DLY_DATA_REV_CTRL_LANE3   0x254
#define HSTX_DRIV_INDATA_CTRL_LANE3          0x240
#define CTRL0                                0x3e8
#define SYS_CTRL                             0x3f0
#define REQ_DLY                              0x3fc

static int dsi_12nm_phy_enable(struct msm_dsi_phy *phy,
				struct msm_dsi_phy_clk_request *clk_req)
{
	struct msm_dsi_dphy_timing *timing = &phy->timing;

	if (msm_dsi_dphy_timing_calc_v1_2(timing, clk_req)) {
		DRM_DEV_ERROR(&phy->pdev->dev,
			      "%s: timing calculation failed\n",
			      __func__);
		return -EINVAL;
	}

	/* CTRL0: CFG_CLK_EN */
	dsi_phy_write(phy->base + CTRL0, BIT(0));

	/* DSI PHY clock lane timings */
	dsi_phy_write(phy->base + HSTX_CLKLANE_HS0STATE_TIM_CTRL,
		(timing->clk_zero | BIT(7)));
	dsi_phy_write(phy->base + HSTX_CLKLANE_TRALSTATE_TIM_CTRL,
		(timing->clk_trail | BIT(6)));
	dsi_phy_write(phy->base + HSTX_CLKLANE_CLKPOSTSTATE_TIM_CTRL,
		(timing->clk_post | BIT(6)));
	dsi_phy_write(phy->base + HSTX_CLKLANE_REQSTATE_TIM_CTRL,
		timing->clk_rqst);
	dsi_phy_write(phy->base + HSTX_CLKLANE_EXITSTATE_TIM_CTRL,
		(timing->hs_exit | BIT(6) | BIT(7)));

	/* DSI PHY data lane timings */
	dsi_phy_write(phy->base + HSTX_DATALANE_HS0STATE_TIM_CTRL,
		(timing->hs_zero | BIT(7)));
	dsi_phy_write(phy->base + HSTX_DATALANE_TRAILSTATE_TIM_CTRL,
		(timing->hs_trail | BIT(6)));
	dsi_phy_write(phy->base + HSTX_DATALANE_REQSTATE_TIM_CTRL,
		timing->hs_rqst);
	dsi_phy_write(phy->base + HSTX_DATALANE_EXITSTATE_TIM_CTRL,
		(timing->hs_exit | BIT(6) | BIT(7)));

	dsi_phy_write(phy->base + T_TA_GO_TIM_COUNT, 0x03);
	dsi_phy_write(phy->base + T_TA_SURE_TIM_COUNT, 0x01);
	dsi_phy_write(phy->base + REQ_DLY, 0x85);

	/* DSI lane control registers */
	dsi_phy_write(phy->base +
		HSTX_READY_DLY_DATA_REV_CTRL_LANE0, 0x00);
	dsi_phy_write(phy->base +
		HSTX_READY_DLY_DATA_REV_CTRL_LANE1, 0x00);
	dsi_phy_write(phy->base +
		HSTX_READY_DLY_DATA_REV_CTRL_LANE2, 0x00);
	dsi_phy_write(phy->base +
		HSTX_READY_DLY_DATA_REV_CTRL_LANE3, 0x00);
	dsi_phy_write(phy->base + HSTX_DATAREV_CTRL_CLKLANE, 0x00);
	wmb(); /* make sure DSI PHY registers are programmed */

	return 0;
}

static void dsi_12nm_phy_disable(struct msm_dsi_phy *phy)
{
	dsi_phy_write(phy->base + SYS_CTRL, BIT(0) | BIT(3));

	/*
	 * Wait for the registers writes to complete in order to
	 * ensure that the phy is completely disabled
	 */
	wmb();
}

void mdss_dsi_12nm_phy_hstx_drv_ctrl(struct msm_dsi_phy *phy,
				bool enable)
{
	u32 data = 0;

	if (enable)
		data = BIT(2) | BIT(3);

	dsi_phy_write(phy->base + HSTX_DRIV_INDATA_CTRL_CLKLANE, data);
	dsi_phy_write(phy->base + HSTX_DRIV_INDATA_CTRL_LANE0, data);
	dsi_phy_write(phy->base + HSTX_DRIV_INDATA_CTRL_LANE1, data);
	dsi_phy_write(phy->base + HSTX_DRIV_INDATA_CTRL_LANE2, data);
	dsi_phy_write(phy->base + HSTX_DRIV_INDATA_CTRL_LANE3, data);
	wmb(); /* make sure DSI PHY registers are programmed */
}

static const struct regulator_bulk_data dsi_phy_12nm_regulators[] = {
	{ .supply = "vddio", .init_load_uA = 100000 },
};

const struct msm_dsi_phy_cfg dsi_phy_12nm_cfgs = {
	.has_phy_regulator = true,
	.regulator_data = dsi_phy_12nm_regulators,
	.num_regulators = ARRAY_SIZE(dsi_phy_12nm_regulators),
	.ops = {
		.enable = dsi_12nm_phy_enable,
		.disable = dsi_12nm_phy_disable,
		.pll_init = dsi_pll_12nm_init,
		.save_pll_state = dsi_12nm_pll_save_state,
		.restore_pll_state = dsi_12nm_pll_restore_state,
		.hstx_drv_ctrl = mdss_dsi_12nm_phy_hstx_drv_ctrl,
	},
	.min_pll_rate = VCO_MIN_RATE,
	.max_pll_rate = VCO_MAX_RATE,
	.io_start = { 0x1a94400, 0x1a96400 },
	.num_dsi_phy = 2,
};
