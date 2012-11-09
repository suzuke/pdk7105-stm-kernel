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

#include "../suspend.h"
#include <mach/hardware.h>

#include <linux/stm/poke_table.h>
#include <linux/stm/synopsys_dwc_ddr32.h>
#include <linux/stm/mpe41-periphs.h>

#define MPE_POWER_CFG			0x018
/*
 * MPE Plls has 3 cfg registers
 */
#define MPE_PLL_CFG(_nr, _reg)		((_reg) * 0x4 + (_nr) * 0xc)
#define MPE_PLL_LOCK_REG(_nr)		MPE_PLL_CFG(_nr, 1)
#define MPE_PLL_LOCK_STATUS		(1 << 31)

#define MPE_SWITCH_CFG(x)		(0x01C + (x) * 0x04)

#define A9_CLK_CONFIG_IOMEM		(IO_ADDRESS(0xfdde0000) + 0x0d8)
#define A9_CLK_STATUS_IOMEM		(IO_ADDRESS(0xfdde0000) + 0x144)
/*
 * There are 3 system clock tree in the MPE
 */
static void __iomem *clks_base[3];

static int stx_mpe41_suspend_core(suspend_state_t state,
	struct stm_wakeup_devices *wkd, int suspending)
{
	static long *switch_cfg;
	unsigned long cfg_0_0, cfg_0_1;
	unsigned long cfg_1_0, cfg_1_1;
	unsigned long cfg_2_0, cfg_2_1;
	int i, j;
	unsigned long tmp;

	if (suspending)
		goto on_suspending;

	tmp = ioread32(A9_CLK_CONFIG_IOMEM);
	iowrite32(tmp & ~0x1, A9_CLK_CONFIG_IOMEM);	/* Reenabling PLL */

	while (!(ioread32(A9_CLK_STATUS_IOMEM) & 0x1))
		cpu_relax();

	iowrite32(tmp & ~(0x4 | 0x1), A9_CLK_CONFIG_IOMEM);/* Disabling PLL */

	/* turn-on PLLs */
	for (i = 0; i < ARRAY_SIZE(clks_base); ++i)
		iowrite32(0, clks_base[i] + MPE_POWER_CFG);


	/* wait for stable PLLs */
	/* for each Clock tree */
	for (i = 0; i < ARRAY_SIZE(clks_base); ++i)
		/* for each PLL in the tree */
		for (j = 0; j < 2; ++j)
			/* wait the PLL is locked */
			while (!(ioread32(clks_base[i] + MPE_PLL_LOCK_REG(j))))
				;

	/* apply the original parents */
	for (i = 0; i < ARRAY_SIZE(clks_base); ++i) {
		iowrite32(switch_cfg[i * 2], clks_base[i] + MPE_SWITCH_CFG(0));
		iowrite32(switch_cfg[i * 2 + 1], clks_base[i] +
			MPE_SWITCH_CFG(1));
	}

	kfree(switch_cfg);
	switch_cfg = NULL;

	pr_debug("[STM][PM] MPE41: ClockGens A: restored\n");
	return 0;

on_suspending:
	cfg_0_0 = 0xffc3fcff;
	cfg_0_1 = 0xf;
	cfg_1_0 = 0xf3ffffff;
	cfg_1_1 = 0xf;
	cfg_2_0 = 0xf3ffff0f; /* Validation suggests 0xf3ffffff but
			       * in this manner the:
			       * CLKM_STAC_PHY and CLKM_STAC_SYS
			       * are turned-off... not clear
			       * how the MPE should communicate with SASG...
			       */
	cfg_2_1 = 0xf;

	switch_cfg = kmalloc(sizeof(long) * 2 * ARRAY_SIZE(clks_base),
		GFP_ATOMIC);

	if (!switch_cfg)
		return -ENOMEM;

	for (i = 0; i < ARRAY_SIZE(clks_base); ++i) {
		/* Save the original parents */
		switch_cfg[i * 2] = ioread32(clks_base[i] + MPE_SWITCH_CFG(0));
		switch_cfg[i * 2 + 1] = ioread32(clks_base[i] +
			MPE_SWITCH_CFG(1));
		/* and move the clock under the extern oscillator (30 MHz) */
		iowrite32(0, clks_base[i] + MPE_SWITCH_CFG(0));
		iowrite32(0, clks_base[i] + MPE_SWITCH_CFG(1));
	}

	/* turn-off PLLs */
	for (i = 0; i < ARRAY_SIZE(clks_base); ++i)
		iowrite32(3, clks_base[i] + MPE_POWER_CFG);

	iowrite32(cfg_0_0, clks_base[0] + MPE_SWITCH_CFG(0));
	/*
	 * The switch config cfg_0_1 the validation suggested
	 * currently doesn't work
	 */
	/* iowrite32(cfg_0_1, clks_base[0] + MPE_SWITCH_CFG(1)); */
	iowrite32(cfg_1_0, clks_base[1] + MPE_SWITCH_CFG(0));
	iowrite32(cfg_1_1, clks_base[1] + MPE_SWITCH_CFG(1));
	iowrite32(cfg_2_0, clks_base[2] + MPE_SWITCH_CFG(0));
	iowrite32(cfg_2_1, clks_base[2] + MPE_SWITCH_CFG(1));

	tmp = ioread32(A9_CLK_CONFIG_IOMEM);
	iowrite32(tmp | 0x4, A9_CLK_CONFIG_IOMEM);	/* Bypassing PLL */
	iowrite32(tmp | 0x4 | 0x1, A9_CLK_CONFIG_IOMEM);/* Disabling PLL */

	pr_debug("[STM][PM] MPE41: ClockGens A: saved\n");
	return 0;
}

static int stx_mpe41_suspend_pre_enter(suspend_state_t state,
		struct stm_wakeup_devices *wkd)
{
	return stx_mpe41_suspend_core(state, wkd, 1);
}

static void stx_mpe41_suspend_post_enter(suspend_state_t state)
{
	stx_mpe41_suspend_core(state, NULL, 0);
}

static const long stx_mpe41_ddr0_enter[] = {
	synopsys_ddr32_in_self_refresh(MPE41_DDR0_PCTL_BASE),
	synopsys_ddr32_phy_standby_enter(MPE41_DDR0_PCTL_BASE),
};

static const long stx_mpe41_ddr1_enter[] = {
	synopsys_ddr32_in_self_refresh(MPE41_DDR1_PCTL_BASE),
	synopsys_ddr32_phy_standby_enter(MPE41_DDR1_PCTL_BASE),
};

static const long stx_mpe41_ddr0_exit[] = {
	synopsys_ddr32_phy_standby_exit(MPE41_DDR0_PCTL_BASE),
	synopsys_ddr32_out_of_self_refresh(MPE41_DDR0_PCTL_BASE),
};

static const long stx_mpe41_ddr1_exit[] = {
	synopsys_ddr32_phy_standby_exit(MPE41_DDR1_PCTL_BASE),
	synopsys_ddr32_out_of_self_refresh(MPE41_DDR1_PCTL_BASE),
};

#define SUSPEND_TBL(_enter, _exit) {			\
	.enter = _enter,				\
	.enter_size = ARRAY_SIZE(_enter) * sizeof(long),\
	.exit = _exit,					\
	.exit_size = ARRAY_SIZE(_exit) * sizeof(long),	\
}

static struct stm_suspend_table stx_mpe41_suspend_tables[] = {
	SUSPEND_TBL(stx_mpe41_ddr0_enter, stx_mpe41_ddr0_exit),
	SUSPEND_TBL(stx_mpe41_ddr1_enter, stx_mpe41_ddr1_exit),
};

static struct stm_mcm_suspend stx_mpe41_suspend = {
	.tables = stx_mpe41_suspend_tables,
	.nr_tables = ARRAY_SIZE(stx_mpe41_suspend_tables),
	.pre_enter = stx_mpe41_suspend_pre_enter,
	.post_enter = stx_mpe41_suspend_post_enter,
};

struct stm_mcm_suspend * __init stx_mpe41_suspend_setup(void)
{
	clks_base[0] = ioremap_nocache(0xfde12000, 0x1000);
	if (!clks_base[0])
		goto err_0;

	clks_base[1] = ioremap_nocache(0xfd6db000, 0x1000);
	if (!clks_base[1])
		goto err_1;

	clks_base[2] = ioremap_nocache(0xfd345000, 0x1000);
	if (!clks_base[2])
		goto err_2;

	return &stx_mpe41_suspend;

err_2:
	iounmap(clks_base[1]);
err_1:
	iounmap(clks_base[0]);
err_0:
	return NULL;
}

