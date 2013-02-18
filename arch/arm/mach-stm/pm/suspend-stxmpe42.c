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

#include <linux/stm/sysconf.h>
#include <linux/stm/clk.h>
#include <linux/stm/wakeup_devices.h>
#include <linux/stm/mpe42-periphs.h>

#include <linux/stm/poke_table.h>
#include <linux/stm/synopsys_dwc_ddr32.h>

#include "../suspend.h"
#include <mach/hardware.h>

#define MPE42_POWER_CFG			0x018

/*
 * MPE42 Plls has 3 cfg registers
 */
#define MPE42_PLL_CFG(_nr, _reg)	((_reg) * 0x4 + (_nr) * 0xc)
#define MPE42_PLL_LOCK_REG(_nr)		MPE42_PLL_CFG(_nr, 1)
#define MPE42_PLL_LOCK_STATUS		(1 << 31)

#define MPE42_SWITCH_CFG(x)		(0x01C + (x) * 0x04)

#define MPE42_A9_CLK_BASE		IO_ADDRESS(MPE42_CPU_SYSCONF_BASE)
#define MPE42_A9_CLK_REGS(x)			\
		(MPE42_A9_CLK_BASE + ((x) - 7000) * 0x4)
#define MPE42_A9_CLK_PLL_SWITCH		MPE42_A9_CLK_REGS(7555)
#define MPE42_A9_CLK_PLL_ENABLE		MPE42_A9_CLK_REGS(7556)
#define MPE42_A9_CLK_STATUS_IOMEM	MPE42_A9_CLK_REGS(7583)

/*
 * There are 3 system clock tree in the MPE42
 */
static void __iomem *mpe42_clk_a_base[3];
static long *mpe42_clk_a_switch_cfg;

static int stx_mpe42_suspend_pre_enter(suspend_state_t state,
		struct stm_wakeup_devices *wkd)
{
	unsigned long cfg_0_0, cfg_0_1;
	unsigned long cfg_1_0, cfg_1_1;
	unsigned long cfg_2_0, cfg_2_1;
	int i;
	unsigned long tmp;

#define clk_off(id)	(0x3 << (((id) > 15 ? ((id) - 16) : (id)) * 2))
	/* A10 */
	cfg_0_0 = ~(clk_off(6) | clk_off(10));
	cfg_0_1 = ~(clk_off(17) | clk_off(18) | clk_off(21) | clk_off(30));
	/* A11 */
	cfg_1_0 = ~(clk_off(4) | clk_off(11) | clk_off(12) | clk_off(13));
	cfg_1_1 = ~(clk_off(17));
	/* A12 */
	cfg_2_0 = ~(clk_off(0) | clk_off(1) | clk_off(2) | clk_off(3));
	cfg_2_1 = ~(clk_off(18));

	mpe42_clk_a_switch_cfg = kmalloc(sizeof(long) * 2 *
		ARRAY_SIZE(mpe42_clk_a_base), GFP_ATOMIC);

	if (!mpe42_clk_a_switch_cfg)
		return -ENOMEM;

	for (i = 0; i < ARRAY_SIZE(mpe42_clk_a_base); ++i) {
		/* Save the original parents */
		mpe42_clk_a_switch_cfg[i * 2] =
			ioread32(mpe42_clk_a_base[i] + MPE42_SWITCH_CFG(0));
		mpe42_clk_a_switch_cfg[i * 2 + 1] =
			ioread32(mpe42_clk_a_base[i] + MPE42_SWITCH_CFG(1));
		/* and move the clock under the extern oscillator (30 MHz) */
		iowrite32(0, mpe42_clk_a_base[i] + MPE42_SWITCH_CFG(0));
		iowrite32(0, mpe42_clk_a_base[i] + MPE42_SWITCH_CFG(1));
	}

	/* turn-off PLLs */
	for (i = 0; i < ARRAY_SIZE(mpe42_clk_a_base); ++i)
		iowrite32(3, mpe42_clk_a_base[i] + MPE42_POWER_CFG);

	iowrite32(cfg_0_0, mpe42_clk_a_base[0] + MPE42_SWITCH_CFG(0));
	iowrite32(cfg_0_1, mpe42_clk_a_base[0] + MPE42_SWITCH_CFG(1));
	iowrite32(cfg_1_0, mpe42_clk_a_base[1] + MPE42_SWITCH_CFG(0));
	iowrite32(cfg_1_1, mpe42_clk_a_base[1] + MPE42_SWITCH_CFG(1));
	iowrite32(cfg_2_0, mpe42_clk_a_base[2] + MPE42_SWITCH_CFG(0));
	iowrite32(cfg_2_1, mpe42_clk_a_base[2] + MPE42_SWITCH_CFG(1));

	tmp = ioread32(MPE42_A9_CLK_PLL_SWITCH);
	iowrite32(tmp | 0x2, MPE42_A9_CLK_PLL_SWITCH);	/* Bypassing PLL */
	tmp = ioread32(MPE42_A9_CLK_PLL_ENABLE);
	iowrite32(tmp | 0x1, MPE42_A9_CLK_PLL_ENABLE);/* Disabling PLL */

	pr_debug("stm pm mpe42: ClockGens A: saved\n");
	return 0;
}

static void stx_mpe42_suspend_post_enter(suspend_state_t state)
{
	int i, j;
	unsigned long tmp;

	tmp = ioread32(MPE42_A9_CLK_PLL_ENABLE);
	iowrite32(tmp & ~0x1, MPE42_A9_CLK_PLL_ENABLE);	/* Reenabling PLL */

	while (!(ioread32(MPE42_A9_CLK_STATUS_IOMEM) & 0x1))
		cpu_relax();

	tmp = ioread32(MPE42_A9_CLK_PLL_SWITCH);
	iowrite32(tmp & ~0x2, MPE42_A9_CLK_PLL_SWITCH);/* Disabling PLL */

	/* turn-on PLLs */
	for (i = 0; i < ARRAY_SIZE(mpe42_clk_a_base); ++i)
		iowrite32(0, mpe42_clk_a_base[i] + MPE42_POWER_CFG);


	/* wait for stable PLLs */
	/* for each Clock tree */
	for (i = 0; i < ARRAY_SIZE(mpe42_clk_a_base); ++i)
		/* for each PLL in the tree */
		for (j = 0; j < 2; ++j)
			/* wait the PLL is locked */
			while (!(ioread32(mpe42_clk_a_base[i] +
				MPE42_PLL_LOCK_REG(j))))
				;

	/* apply the original parents */
	for (i = 0; i < ARRAY_SIZE(mpe42_clk_a_base); ++i) {
		iowrite32(mpe42_clk_a_switch_cfg[i * 2],
			mpe42_clk_a_base[i] + MPE42_SWITCH_CFG(0));
		iowrite32(mpe42_clk_a_switch_cfg[i * 2 + 1],
			mpe42_clk_a_base[i] + MPE42_SWITCH_CFG(1));
	}

	kfree(mpe42_clk_a_switch_cfg);
	mpe42_clk_a_switch_cfg = NULL;

	pr_debug("stm pm mpe42: ClockGens A: restored\n");
}

#define MPE42_DDR_PLL_CFG       MPE42_DDR_SYSCFG(7502)
#define MPE42_DDR_PLL_STATUS    MPE42_DDR_SYSCFG(7569)


#define SELF_REFRESH_ON_PCTL    1

static const long stx_mpe42_ddr0_enter[] = {

#ifdef SELF_REFRESH_ON_PCTL
synopsys_ddr32_in_self_refresh(MPE42_DDR_PCTL_BASE(0)),
#else
UPDATE32(MPE42_DDR_PWR_DWN(0), ~MPE42_DDR_PWR_DWN_REQ, 0),
WHILE_NE32(MPE42_DDR_PWR_STATUS(0), MPE42_DDR_PWR_STATUS_ACK, 0),
#endif

synopsys_ddr32_phy_standby_enter(MPE42_DDR_PCTL_BASE(0)),
};

static const long stx_mpe42_ddr1_enter[] = {

#ifdef SELF_REFRESH_ON_PCTL
synopsys_ddr32_in_self_refresh(MPE42_DDR_PCTL_BASE(1)),
#else
UPDATE32(MPE42_DDR_PWR_DWN(1), ~MPE42_DDR_PWR_DWN_REQ, 0),
WHILE_NE32(MPE42_DDR_PWR_STATUS(1), MPE42_DDR_PWR_STATUS_ACK, 0),
#endif

synopsys_ddr32_phy_standby_enter(MPE42_DDR_PCTL_BASE(1)),
};

static const long stx_mpe42_ddr0_exit[] = {
synopsys_ddr32_phy_standby_exit(MPE42_DDR_PCTL_BASE(0)),

#ifdef SELF_REFRESH_ON_PCTL
synopsys_ddr32_out_of_self_refresh(MPE42_DDR_PCTL_BASE(0)),
#else
OR32(MPE42_DDR_PWR_DWN(0), MPE42_DDR_PWR_DWN_REQ),
WHILE_NE32(MPE42_DDR_PWR_STATUS(0), MPE42_DDR_PWR_STATUS_ACK,
	MPE42_DDR_PWR_STATUS_ACK),
#endif
};

static const long stx_mpe42_ddr1_exit[] = {
synopsys_ddr32_phy_standby_exit(MPE42_DDR_PCTL_BASE(1)),

#ifdef SELF_REFRESH_ON_PCTL
synopsys_ddr32_out_of_self_refresh(MPE42_DDR_PCTL_BASE(1)),
#else
OR32(MPE42_DDR_PWR_DWN(1), MPE42_DDR_PWR_DWN_REQ),
WHILE_NE32(MPE42_DDR_PWR_STATUS(1), MPE42_DDR_PWR_STATUS_ACK,
	MPE42_DDR_PWR_STATUS_ACK),
#endif
};

static const long stx_mpe42_ddr_pll_enter[] = {
OR32(MPE42_DDR_PLL_CFG, 1),
};

static const long stx_mpe42_ddr_pll_exit[] = {
UPDATE32(MPE42_DDR_PLL_CFG, ~1, 0),
WHILE_NE32(MPE42_DDR_PLL_STATUS, 1, 1),
};

#define SUSPEND_TBL(_enter, _exit) {                    \
	.enter = _enter,                                \
	.enter_size = ARRAY_SIZE(_enter) * sizeof(long),\
	.exit = _exit,                                  \
	.exit_size = ARRAY_SIZE(_exit) * sizeof(long),  \
}

static struct stm_suspend_table stx_mpe42_suspend_tables[] = {
	SUSPEND_TBL(stx_mpe42_ddr0_enter, stx_mpe42_ddr0_exit),
	SUSPEND_TBL(stx_mpe42_ddr1_enter, stx_mpe42_ddr1_exit),
	SUSPEND_TBL(stx_mpe42_ddr_pll_enter, stx_mpe42_ddr_pll_exit),
};

static struct stm_mcm_suspend stx_mpe42_suspend = {
	.tables = stx_mpe42_suspend_tables,
	.nr_tables = ARRAY_SIZE(stx_mpe42_suspend_tables),
	.pre_enter = stx_mpe42_suspend_pre_enter,
	.post_enter = stx_mpe42_suspend_post_enter,
};

struct stm_mcm_suspend * __init stx_mpe42_suspend_setup(void)
{
	mpe42_clk_a_base[0] = ioremap_nocache(0xfde12000, 0x1000);
	if (!mpe42_clk_a_base[0])
		goto err_0;

	mpe42_clk_a_base[1] = ioremap_nocache(0xfd6db000, 0x1000);
	if (!mpe42_clk_a_base[1])
		goto err_1;

	mpe42_clk_a_base[2] = ioremap_nocache(0xfd345000, 0x1000);
	if (!mpe42_clk_a_base[2])
		goto err_2;

	return &stx_mpe42_suspend;

err_2:
	iounmap(mpe42_clk_a_base[1]);
err_1:
	iounmap(mpe42_clk_a_base[0]);
err_0:
	return NULL;
}

