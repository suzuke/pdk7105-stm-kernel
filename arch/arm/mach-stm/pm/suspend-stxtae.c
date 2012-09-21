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
#include <linux/stm/clk.h>
#include <linux/stm/wakeup_devices.h>

#include "../suspend.h"
#include <mach/hardware.h>


/*
 * TAE Plls has 1 cfg register
 */
#define TAE_CLKA_PLL_CFG(_nr)		((_nr) * 0x4)
#define TAE_CLKA_PLL_LOCK_REG(_nr)	TAE_CLKA_PLL_CFG(_nr)
#define TAE_CLKA_PLL_LOCK_STATUS		(1 << 31)
#define TAE_CLKA_POWER_CFG		0x010
#define TAE_CLKA_SWITCH_CFG(x)		(0x014 + (x) * 0x10)


/*
 * There is 1 system clock tree in the TAE
 */
static void __iomem *tae_clk_a_base;
static long tae_clk_a_switch_cfg[2];

static int stx_tae_suspend_pre_enter(suspend_state_t state,
	struct stm_wakeup_devices *wkd)
{
	unsigned long cfg_0, cfg_1;
	unsigned long pwr;
	int i;

	cfg_0 = 0x0;
	cfg_1 = 0x0;
	pwr = 0x3;

	/* Save the original parents */
	for (i = 0; i < 2; ++i)
		tae_clk_a_switch_cfg[i] = ioread32(tae_clk_a_base +
			TAE_CLKA_SWITCH_CFG(i));

	/* Manage the A tree */
	iowrite32(cfg_0, tae_clk_a_base + TAE_CLKA_SWITCH_CFG(0));
	iowrite32(cfg_1, tae_clk_a_base + TAE_CLKA_SWITCH_CFG(1));
	iowrite32(pwr, tae_clk_a_base + TAE_CLKA_POWER_CFG);

	pr_debug("stm pm TAE1: ClockGen A: saved\n");
	return 0;
}

static void stx_tae_suspend_post_enter(suspend_state_t state)
{
	int i;

	/* turn-on PLLs */
	iowrite32(0, tae_clk_a_base + TAE_CLKA_POWER_CFG);

	/* for each PLL in the tree */
	for (i = 0; i < 2; ++i)
		/* wait the PLL is locked */
		while (!(ioread32(tae_clk_a_base + TAE_CLKA_PLL_LOCK_REG(i))))
				;

	/* apply the original parents */
	for (i = 0; i < 2; ++i)
		iowrite32(tae_clk_a_switch_cfg[i], tae_clk_a_base +
				TAE_CLKA_SWITCH_CFG(i));

	pr_debug("stm pm: TAE1: ClockGen A: restored\n");
}

static struct stm_mcm_suspend stx_tae_suspend = {
	.pre_enter = stx_tae_suspend_pre_enter,
	.post_enter = stx_tae_suspend_post_enter,
};

struct stm_mcm_suspend * __init stx_tae_suspend_setup(void)
{
	tae_clk_a_base = ioremap_nocache(0xFEE62000, 0x1000);

	if (!tae_clk_a_base)
		goto err_0;

	return &stx_tae_suspend;

	iounmap(tae_clk_a_base);
err_0:
	return NULL;
}
