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
#include <linux/irq.h>
#include <linux/io.h>

#include <linux/stm/stig125.h>
#include <linux/stm/stig125-periphs.h>
#include <linux/stm/sysconf.h>
#include <linux/stm/clk.h>
#include <linux/stm/wakeup_devices.h>

#include "../suspend.h"
#include <mach/hardware.h>
#include <mach/soc-stig125.h>
#include <asm/hardware/gic.h>	/* gic offset and struct gic_chip_data */

#include <linux/stm/poke_table.h>
#include <linux/stm/synopsys_dwc_ddr32.h>

/*
 * SASC Plls has 1 cfg register
 */
#define SASC_PLL_CFG(pll_nr, cfg_nr)	((pll_nr) * 0xc + (cfg_nr) * 0x4)
#define SASC_PLL_LOCK_REG(pll_nr)	SASC_PLL_CFG(pll_nr, 1)
#define SASC_PLL_LOCK_STATUS		(1 << 31)
#define SASC_POWER_CFG			0x18

#define SASC_SWITCH_CFG(x)		(0x1c + (x) * 0x4)

#define CLK_A1_BASE			0xFEA20000

#define DDR_CLK_ID			5
#define DDR_CLK_SHIFT			(DDR_CLK_ID * 2)

#define SYSCONF_CPU_722	(STIG125_SYSCONF_CPU_BASE + 0x58)
#define SYSCONF_CPU_760	(STIG125_SYSCONF_CPU_BASE + 0xf0)

static void __iomem *clks_base[2];
static struct stm_wakeup_devices stig125_wkd;

static long stig125_ddr0_enter[] = {
synopsys_ddr32_in_self_refresh(STIG125_DDR_PCTL_BASE),

synopsys_ddr32_phy_standby_enter(STIG125_DDR_PCTL_BASE),

POKE32(CLK_A1_BASE + SASC_SWITCH_CFG(0), 0),
POKE32(CLK_A1_BASE + SASC_POWER_CFG, 0x3),

/* bypass and disable the A9.PLL */
OR32(SYSCONF_CPU_722, 1 << 2),
OR32(SYSCONF_CPU_722, 1),

END_MARKER,
};

static long stig125_ddr0_exit[] = {
/* enable, wait and don't bypass the A9.PLL */
UPDATE32(SYSCONF_CPU_722, ~1, 0),
WHILE_NE32(SYSCONF_CPU_760, 1, 1),
UPDATE32(SYSCONF_CPU_722, ~(1 << 2), 0),

/* turn-on A1.PLLs */
POKE32(CLK_A1_BASE + SASC_POWER_CFG, 0x0),
/* Wait A1.PLLs are locked */
WHILE_NE32(CLK_A1_BASE + SASC_PLL_LOCK_REG(0), SASC_PLL_LOCK_STATUS,
	SASC_PLL_LOCK_STATUS),
WHILE_NE32(CLK_A1_BASE + SASC_PLL_LOCK_REG(1), SASC_PLL_LOCK_STATUS,
	SASC_PLL_LOCK_STATUS),

OR32(CLK_A1_BASE + SASC_SWITCH_CFG(0), 0x1 << DDR_CLK_SHIFT),
synopsys_ddr32_phy_standby_exit(STIG125_DDR_PCTL_BASE),

synopsys_ddr32_out_of_self_refresh(STIG125_DDR_PCTL_BASE),

END_MARKER,
};


#define SUSPEND_TBL(_enter, _exit) {				\
	.enter = _enter,					\
	.enter_size = ARRAY_SIZE(_enter) * sizeof(long),	\
	.exit = _exit,						\
	.exit_size = ARRAY_SIZE(_exit) * sizeof(long),		\
}

static struct stm_suspend_table stig125_suspend_tables[] = {
	SUSPEND_TBL(stig125_ddr0_enter, stig125_ddr0_exit)
};

static int stig125_suspend_begin(suspend_state_t state)
{
	int ret = 0;

	pr_info("[STM][PM] Analyzing the wakeup devices\n");

	stm_check_wakeup_devices(&stig125_wkd);

	return ret;
}

static long *switch_cfg;

static int stig125_suspend_pre_enter(suspend_state_t state)
{
	unsigned long cfg_0_0, cfg_0_1, cfg_1_0, cfg_1_1;
	unsigned long pwr_0, pwr_1;
	int i, j;

	cfg_0_0 = cfg_0_1 = cfg_1_0 = cfg_1_1 = 0x0;
	pwr_0 = pwr_1 = 0x3;

	switch_cfg = kmalloc(sizeof(long) * 2 * ARRAY_SIZE(clks_base),
		GFP_ATOMIC);

	if (!switch_cfg)
		return -ENOMEM;

	for (i = 0; i < ARRAY_SIZE(clks_base); ++i) {
		/* Save the original parents */
		switch_cfg[i * 2] = ioread32(clks_base[i] +
			SASC_SWITCH_CFG(0));
		switch_cfg[i * 2 + 1] = ioread32(clks_base[i] +
			SASC_SWITCH_CFG(1));
	}

	/* Manage the A1 tree */

/*
 * The DDR subsystem uses an clock-channel coming direclty from A1.
 * This mean we have to be really carefully in the A1 management
 *
 * Here check the system isn't goint to break the DDR Subsystem
 * for this reason the A1.PLLs and the A1.switch config are
 * managed in the mem_table
 */
	/*
	 * maintain the DDR_clk parent as it is!
	 */
	iowrite32(cfg_1_1, clks_base[1] + SASC_SWITCH_CFG(1));

	/* Manage the A0 tree */
	iowrite32(cfg_0_0, clks_base[0] + SASC_SWITCH_CFG(0));
	iowrite32(cfg_0_1, clks_base[0] + SASC_SWITCH_CFG(1));
	iowrite32(pwr_0, clks_base[0] + SASC_POWER_CFG);

	pr_debug("[STM][PM] SASC1: ClockGens A: saved\n");

	return 0;
}

static void stig125_suspend_post_enter(suspend_state_t state)
{
	int i, j;

	/* turn-on PLLs */
	for (i = 0; i < ARRAY_SIZE(clks_base); ++i)
		iowrite32(0, clks_base[i] + SASC_POWER_CFG);


	/* wait for stable PLLs for each Clock tree */
	for (i = 0; i < ARRAY_SIZE(clks_base); ++i)
		/* for each PLL in the tree */
		for (j = 0; j < 2; ++j)
			/* wait the PLL is locked */
			while (!(ioread32(clks_base[i] + SASC_PLL_LOCK_REG(j)) &
				SASC_PLL_LOCK_STATUS))
				;

	/* apply the original parents */
	for (i = 0; i < ARRAY_SIZE(clks_base); ++i) {
		iowrite32(switch_cfg[i * 2], clks_base[i] +
				SASC_SWITCH_CFG(0));
		iowrite32(switch_cfg[i * 2 + 1], clks_base[i] +
				SASC_SWITCH_CFG(1));
	}

	kfree(switch_cfg);
	switch_cfg = NULL;

	pr_debug("[STM][PM] SASC1: ClockGens A: restored\n");
	return 0;
}

static int stig125_get_wake_irq(void)
{
	int irq = 0;
	struct irq_data *d;
	void *gic_cpu = __io_address(STIG125_GIC_CPU_BASE);

	irq = readl(gic_cpu + GIC_CPU_INTACK);
	d = irq_get_irq_data(irq);
	writel(d->hwirq, gic_cpu + GIC_CPU_EOI);

	return irq;
}

static struct stm_platform_suspend stig125_suspend = {
	.ops.begin = stig125_suspend_begin,

	.pre_enter = stig125_suspend_pre_enter,
	.post_enter = stig125_suspend_post_enter,

	.eram_iomem = (void *)0xfe240000,
	.get_wake_irq = stig125_get_wake_irq,

};

static int __init stig125_suspend_setup(void)
{
	int i;

	clks_base[0] = ioremap_nocache(0xFEE48000, 0x1000);

	if (!clks_base[0])
		goto err_0;
	clks_base[1] = ioremap_nocache(CLK_A1_BASE, 0x1000);
	if (!clks_base[1])
		goto err_1;

	INIT_LIST_HEAD(&stig125_suspend.mem_tables);

	 for (i = 0; i < ARRAY_SIZE(stig125_suspend_tables); ++i)
		list_add_tail(&stig125_suspend_tables[i].node,
			&stig125_suspend.mem_tables);

	return stm_suspend_register(&stig125_suspend);
err_1:
	iounmap(clks_base[0]);
err_0:
	return -EINVAL;
}

module_init(stig125_suspend_setup);
