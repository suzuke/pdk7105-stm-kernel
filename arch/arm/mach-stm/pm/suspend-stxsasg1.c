/*
 * -------------------------------------------------------------------------
 * Copyright (C) 2012  STMicroelectronics
 * Author: Francesco M. Virlinzi  <francesco.virlinzi@st.com>
 *
 * May be copied or modified under the terms of the GNU General Public
 * License V.2 ONLY.  See linux/COPYING for more information.
 *
 * ------------------------------------------------------------------------- */

#include <linux/init.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/suspend.h>
#include <linux/errno.h>
#include <linux/time.h>
#include <linux/delay.h>
#include <linux/irqflags.h>
#include <linux/io.h>
#include <linux/slab.h>
#include <linux/err.h>

#include <linux/stm/sysconf.h>
#include <linux/clk.h>
#include <linux/stm/wakeup_devices.h>

#include "../suspend.h"
#include <mach/hardware.h>


#define SASG_POWER_CFG			0x010

/*
 * SASG Plls has 1 cfg register
 */
#define SASG_PLL_CFG(_nr)		((_nr) * 0x4)
#define SASG_PLL_LOCK_REG(_nr)		SASG_PLL_CFG(_nr)
#define SASG_PLL_LOCK_STATUS		(1 << 31)

#define SASG_SWITCH_CFG(x)		(0x014 + (x) * 0x10)


#define CLKS_IC_REG		0x4
#define CLKS_IC_IF_0		0x5
#define CLKS_ETH1_PHY		0x8

static struct clk *a0_pll1;
static struct clk *a0_eth_phy_clk;
/*
 * There are 2 system clock tree in the SASG
 */
static void __iomem *clks_base[2];

static int stx_sasg1_suspend_core(suspend_state_t state,
		struct stm_wakeup_devices *wkd, int suspending)
{
	static long *switch_cfg;
	unsigned long cfg_0_0, cfg_0_1, cfg_1_0, cfg_1_1;
	unsigned long pwr_0, pwr_1;
	int i, j;

	if (suspending)
		goto on_suspending;

	/* turn-on PLLs */
	for (i = 0; i < ARRAY_SIZE(clks_base); ++i)
		iowrite32(0, clks_base[i] + SASG_POWER_CFG);


	/* wait for stable PLLs for each Clock tree */
	for (i = 0; i < ARRAY_SIZE(clks_base); ++i)
		/* for each PLL in the tree */
		for (j = 0; j < 2; ++j)
			/* wait the PLL is locked */
			while (!(ioread32(clks_base[i] + SASG_PLL_LOCK_REG(j))))
				;

	/* apply the original parents */
	for (i = 0; i < ARRAY_SIZE(clks_base); ++i) {
		iowrite32(switch_cfg[i * 2], clks_base[i] +
				SASG_SWITCH_CFG(0));
		iowrite32(switch_cfg[i * 2 + 1], clks_base[i] +
				SASG_SWITCH_CFG(1));
	}

	kfree(switch_cfg);
	switch_cfg = NULL;

	pr_debug("[STM][PM] SASG1: ClockGens A: restored\n");
	return 0;

on_suspending:
	cfg_0_0 = 0xffc3fcff;
	cfg_0_1 = 0xf;
	cfg_1_0 = 0xf3ffffff;
	cfg_1_1 = 0xf;
	pwr_0 = pwr_1 = 0x3;

	switch_cfg = kmalloc(sizeof(long) * 2 * ARRAY_SIZE(clks_base),
		GFP_ATOMIC);

	if (!switch_cfg)
		return -ENOMEM;

	for (i = 0; i < ARRAY_SIZE(clks_base); ++i) {
		/* Save the original parents */
		switch_cfg[i * 2] = ioread32(clks_base[i] +
			SASG_SWITCH_CFG(0));
		switch_cfg[i * 2 + 1] = ioread32(clks_base[i] +
			SASG_SWITCH_CFG(1));
	}

	/* Manage the A1 tree */

/*	iowrite32(cfg_1_0, clks_base[1] + SASG_SWITCH_CFG(0));*/
	iowrite32(0, clks_base[1] + SASG_SWITCH_CFG(0));
	iowrite32(cfg_1_1, clks_base[1] + SASG_SWITCH_CFG(1));
	iowrite32(pwr_1, clks_base[1] + SASG_POWER_CFG);

	/* Manage the A0 tree */
	if (wkd->stm_mac1_can_wakeup) {
		int pll_id = (a0_pll1 == clk_get_parent(a0_eth_phy_clk) ?
			2 : 1);
		cfg_0_0 &= ~(0x3 << (CLKS_IC_REG * 2));
		cfg_0_0 &= ~(0x3 << (CLKS_IC_IF_0 * 2));
		cfg_0_0 &= ~(0x3 << (CLKS_ETH1_PHY * 2));
		cfg_0_0 |= (pll_id << (CLKS_ETH1_PHY * 2));
		pwr_0 &= ~pll_id;
	}

	iowrite32(cfg_0_0, clks_base[0] + SASG_SWITCH_CFG(0));
	iowrite32(cfg_0_1, clks_base[0] + SASG_SWITCH_CFG(1));
	iowrite32(pwr_0, clks_base[0] + SASG_POWER_CFG);

	pr_debug("[STM][PM] SASG1: ClockGens A: saved\n");
	return 0;
}

static int stx_sasg1_suspend_pre_enter(suspend_state_t state,
	struct stm_wakeup_devices *wkd)
{
	return stx_sasg1_suspend_core(state, wkd, 1);
}

static void stx_sasg1_suspend_post_enter(suspend_state_t state)
{
	stx_sasg1_suspend_core(state, NULL, 0);
}

static struct stm_mcm_suspend stx_sasg1_suspend = {
	.pre_enter = stx_sasg1_suspend_pre_enter,
	.post_enter = stx_sasg1_suspend_post_enter,
};

struct stm_mcm_suspend * __init stx_sasg1_suspend_setup(void)
{
	clks_base[0] = ioremap_nocache(0xFEE62000, 0x1000);

	if (!clks_base[0])
		goto err_0;
	clks_base[1] = ioremap_nocache(0xFEE81000, 0x1000);
	if (!clks_base[1])
		goto err_1;

	a0_pll1 = clk_get(NULL, "CLKS_A0_PLL1");

	if (IS_ERR(a0_pll1))
		goto err_2;

	a0_eth_phy_clk = clk_get(NULL, "CLKS_ETH1_PHY");
	if (IS_ERR(a0_eth_phy_clk))
		goto err_3;

	return &stx_sasg1_suspend;


err_3:
	clk_put(a0_pll1);
err_2:
	iounmap(clks_base[1]);
err_1:
	iounmap(clks_base[0]);
err_0:
	return NULL;
}
