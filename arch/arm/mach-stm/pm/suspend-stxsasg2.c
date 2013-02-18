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
#include <linux/clk.h>

#include <linux/stm/sysconf.h>
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


#define CLK_S_ICN_REG_0		0x4
#define CLK_S_ICN_IF_0		0x5
#define CLK_S_ETH1_PHY		0x8

static struct clk *a0_pll1;
static struct clk *a0_eth_phy_clk;
/*
 * There are 2 system clock tree in the SASG-2
 */
static void __iomem *sasg2_clk_a_base[2];
static long *sasg2_clk_a_switch_cfg;

static int stx_sasg2_suspend_pre_enter(suspend_state_t state,
	struct stm_wakeup_devices *wkd)
{
	unsigned long cfg_0_0, cfg_0_1, cfg_1_0, cfg_1_1;
	unsigned long pwr_0, pwr_1;
	int i;

#define clk_off(id)	(0x3 << ((id) * 2))
	cfg_0_0 = ~(clk_off(4) | clk_off(5));
	cfg_0_1 = 0xffffffff;
	cfg_1_0 = clk_off(4) | clk_off(5) | clk_off(7) | clk_off(8) |
		  clk_off(10) | clk_off(12);
	cfg_1_1 = 0xffffffff;

	pwr_0 = pwr_1 = 0x3;

	sasg2_clk_a_switch_cfg = kmalloc(sizeof(long) * 2 *
		ARRAY_SIZE(sasg2_clk_a_base), GFP_ATOMIC);

	if (!sasg2_clk_a_switch_cfg)
		return -ENOMEM;

	for (i = 0; i < ARRAY_SIZE(sasg2_clk_a_base); ++i) {
		/* Save the original parents */
		sasg2_clk_a_switch_cfg[i * 2] =
			ioread32(sasg2_clk_a_base[i] + SASG_SWITCH_CFG(0));
		sasg2_clk_a_switch_cfg[i * 2 + 1] =
			ioread32(sasg2_clk_a_base[i] + SASG_SWITCH_CFG(1));
	}

	/* Manage the A1 tree */
	iowrite32(cfg_1_0, sasg2_clk_a_base[1] + SASG_SWITCH_CFG(0));
	iowrite32(cfg_1_1, sasg2_clk_a_base[1] + SASG_SWITCH_CFG(1));
	iowrite32(pwr_1, sasg2_clk_a_base[1] + SASG_POWER_CFG);

	/* Manage the A0 tree */
	if (wkd->stm_mac1_can_wakeup) {
		int pll_id = (a0_pll1 == clk_get_parent(a0_eth_phy_clk) ?
			2 : 1);
		cfg_0_0 &= ~(0x3 << (CLK_S_ICN_REG_0 * 2));
		cfg_0_0 &= ~(0x3 << (CLK_S_ICN_IF_0 * 2));
		cfg_0_0 &= ~(0x3 << (CLK_S_ETH1_PHY * 2));
		cfg_0_0 |= (pll_id << (CLK_S_ETH1_PHY * 2));
		pwr_0 &= ~pll_id;
	}

	iowrite32(cfg_0_0, sasg2_clk_a_base[0] + SASG_SWITCH_CFG(0));
	iowrite32(cfg_0_1, sasg2_clk_a_base[0] + SASG_SWITCH_CFG(1));
	iowrite32(pwr_0, sasg2_clk_a_base[0] + SASG_POWER_CFG);

	pr_debug("stm suspend sasg-2: ClockGens A: saved\n");
	return 0;
}

static void stx_sasg2_suspend_post_enter(suspend_state_t state)
{
	int i, j;

	/* turn-on PLLs */
	for (i = 0; i < ARRAY_SIZE(sasg2_clk_a_base); ++i)
		iowrite32(0, sasg2_clk_a_base[i] + SASG_POWER_CFG);


	/* wait for stable PLLs for each Clock tree */
	for (i = 0; i < ARRAY_SIZE(sasg2_clk_a_base); ++i)
		/* for each PLL in the tree */
		for (j = 0; j < 2; ++j)
			/* wait the PLL is locked */
			while (!(ioread32(sasg2_clk_a_base[i] +
				SASG_PLL_LOCK_REG(j))))
				;

	/* apply the original parents */
	for (i = 0; i < ARRAY_SIZE(sasg2_clk_a_base); ++i) {
		iowrite32(sasg2_clk_a_switch_cfg[i * 2], sasg2_clk_a_base[i] +
				SASG_SWITCH_CFG(0));
		iowrite32(sasg2_clk_a_switch_cfg[i * 2 + 1],
			sasg2_clk_a_base[i] + SASG_SWITCH_CFG(1));
	}

	kfree(sasg2_clk_a_switch_cfg);
	sasg2_clk_a_switch_cfg = NULL;

	pr_debug("stm suspend sasg-2: ClockGens A: restored\n");
}

static struct stm_mcm_suspend stx_sasg2_suspend = {
	.pre_enter = stx_sasg2_suspend_pre_enter,
	.post_enter = stx_sasg2_suspend_post_enter,
};

struct stm_mcm_suspend * __init stx_sasg2_suspend_setup(void)
{
	sasg2_clk_a_base[0] = ioremap_nocache(0xFEE62000, 0x1000);

	if (!sasg2_clk_a_base[0])
		goto err_0;
	sasg2_clk_a_base[1] = ioremap_nocache(0xFEE81000, 0x1000);
	if (!sasg2_clk_a_base[1])
		goto err_1;

	a0_pll1 = clk_get(NULL, "CLK_S_A0_PLL1");

	if (IS_ERR(a0_pll1))
		goto err_2;

	a0_eth_phy_clk = clk_get(NULL, "CLK_S_ETH1_PHY");
	if (IS_ERR(a0_eth_phy_clk))
		goto err_3;

	return &stx_sasg2_suspend;

err_3:
	clk_put(a0_pll1);
err_2:
	iounmap(sasg2_clk_a_base[1]);
err_1:
	iounmap(sasg2_clk_a_base[0]);
err_0:
	return NULL;
}
