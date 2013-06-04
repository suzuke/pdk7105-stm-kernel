/*
 * -------------------------------------------------------------------------
 * Copyright (C) 2013  STMicroelectronics
 * Author: Francesco M. Virlinzi  <francesco.virlinzi@st.com>
 *
 * May be copied or modified under the terms of the GNU General Public
 * License V.2 ONLY.  See linux/COPYING for more information.
 *
 * -------------------------------------------------------------------------
 */
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

#include <linux/stm/stid127.h>
#include <linux/stm/sysconf.h>
#include <linux/stm/clk.h>
#include <linux/stm/wakeup_devices.h>

#include "../suspend.h"
#include <mach/hardware.h>
#include <mach/soc-stid127.h>
/* gic offset and struct gic_chip_data */
#include <asm/hardware/gic.h>

#include <linux/stm/poke_table.h>
#include <linux/stm/synopsys_dwc_ddr32.h>

#define CLK_A1_BASE			0xFEA20000
#define CLKA_PLL_CFG(pll_nr, cfg_nr)	((pll_nr) * 0xc + (cfg_nr) * 0x4)
#define CLKA_PLL_LOCK_REG(pll_nr)	CLKA_PLL_CFG(pll_nr, 1)
#define CLKA_PLL_LOCK_STATUS		(1 << 31)
#define CLKA_POWER_CFG			(0x18)
#define CLKA_SWITCH_CFG(x)		(0x1c + (x) * 0x4)

#define A1_DDR_CLK_ID			5

#define SYSCONF_A9_PLL_CFG	(STID127_SYSCONF_CPU_BASE + 0x58) /* 722 */
#define SYSCONF_A9_PLL_STA	(STID127_SYSCONF_CPU_BASE + 0xf0) /* 760 */

static void __iomem *clks_base[2];
static struct stm_wakeup_devices stid127_wkd;

static long stid127_ddr0_enter[] = {
	synopsys_ddr32_in_self_refresh(STID127_DDR_PCTL_BASE),

	synopsys_ddr32_phy_standby_enter(STID127_DDR_PCTL_BASE),

#if 0
	/*
	 * FIXME: the A1_DDR_CLK_ID could be turned-off (like STiG125) but
	 * in STiD127 it stuck the system. So it is better and safer to
	 * keep it on during the suspend phase. It will be fixed later (not
	 * mandatory).
	 */
	UPDATE32(CLK_A1_BASE + CLKA_SWITCH_CFG(0), ~(3 << (A1_DDR_CLK_ID * 2)),
		 0),
	OR32(CLK_A1_BASE + CLKA_POWER_CFG, 0x1),
#endif
	/* bypass and disable the A9.PLL */
	OR32(SYSCONF_A9_PLL_CFG, 1 << 2),	/* bypass PLL */
	OR32(SYSCONF_A9_PLL_CFG, 1),	/* disable PLL */

	END_MARKER,
};

static long stid127_ddr0_exit[] = {
	/* enable, wait and don't bypass the A9.PLL */
	UPDATE32(SYSCONF_A9_PLL_CFG, ~1, 0),
	WHILE_NE32(SYSCONF_A9_PLL_STA, 1, 1),
	UPDATE32(SYSCONF_A9_PLL_CFG, ~(1 << 2), 0),

	/* turn-on A1.PLLs */
	POKE32(CLK_A1_BASE + CLKA_POWER_CFG, 0x0),
	/* Wait A1.PLLs are locked */
	WHILE_NE32(CLK_A1_BASE + CLKA_PLL_LOCK_REG(0), CLKA_PLL_LOCK_STATUS,
		   CLKA_PLL_LOCK_STATUS),

	OR32(CLK_A1_BASE + CLKA_SWITCH_CFG(0), 0x1 << (A1_DDR_CLK_ID * 2)),

	synopsys_ddr32_phy_standby_exit(STID127_DDR_PCTL_BASE),

	synopsys_ddr32_out_of_self_refresh(STID127_DDR_PCTL_BASE),

	END_MARKER,
};

#define SUSPEND_TBL(_enter, _exit) {				\
	.enter = _enter,					\
	.enter_size = ARRAY_SIZE(_enter) * sizeof(long),	\
	.exit = _exit,						\
	.exit_size = ARRAY_SIZE(_exit) * sizeof(long),		\
}

static struct stm_suspend_table stid127_suspend_tables[] = {
	SUSPEND_TBL(stid127_ddr0_enter, stid127_ddr0_exit)
};

static int stid127_suspend_begin(suspend_state_t state)
{
	pr_debug("stm pm: Analyzing the wakeup devices\n");

	stm_check_wakeup_devices(&stid127_wkd);

	return 0;
}

static long *switch_cfg;

static int stid127_suspend_pre_enter(suspend_state_t state)
{
	unsigned long cfg_0_0, cfg_0_1, cfg_1_0, cfg_1_1;
	unsigned long pwr_0, pwr_1;
	int i;

	cfg_0_0 = cfg_0_1 = cfg_1_0 = 0x0;
	cfg_1_1 = 0xffffffff;
	pwr_0 = 0x3;
	pwr_1 = 0x0;	/* leave A1.PLL_0 enabled! */

	switch_cfg = kmalloc(sizeof(long) * 2 * ARRAY_SIZE(clks_base),
			     GFP_ATOMIC);

	if (!switch_cfg)
		return -ENOMEM;

	for (i = 0; i < ARRAY_SIZE(clks_base); ++i) {
		/* Save the original parents */
		switch_cfg[i * 2] = ioread32(clks_base[i] + CLKA_SWITCH_CFG(0));
		switch_cfg[i * 2 + 1] = ioread32(clks_base[i] +
						 CLKA_SWITCH_CFG(1));
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
	iowrite32(cfg_1_1, clks_base[1] + CLKA_SWITCH_CFG(1));
	iowrite32(pwr_1, clks_base[1] + CLKA_POWER_CFG);

	/* Manage the A0 tree */
	iowrite32(cfg_0_0, clks_base[0] + CLKA_SWITCH_CFG(0));
	iowrite32(cfg_0_1, clks_base[0] + CLKA_SWITCH_CFG(1));
	iowrite32(pwr_0, clks_base[0] + CLKA_POWER_CFG);

	pr_debug("stm pm: ClockGens A: saved\n");

	return 0;
}

static void stid127_suspend_post_enter(suspend_state_t state)
{
	int i, j;

	/* turn-on PLLs */
	for (i = 0; i < ARRAY_SIZE(clks_base); ++i)
		iowrite32(0, clks_base[i] + CLKA_POWER_CFG);

	/* for each PLL in the tree */
	for (j = 0; j < 2; ++j) {
		while (!(ioread32(clks_base[0] + CLKA_PLL_LOCK_REG(j)) &
			CLKA_PLL_LOCK_STATUS))
			;
	}

	while (!(ioread32(clks_base[1] + CLKA_PLL_LOCK_REG(1)) &
		CLKA_PLL_LOCK_STATUS))
		;

	/* apply the original parents */
	for (i = 0; i < ARRAY_SIZE(clks_base); ++i) {
		iowrite32(switch_cfg[i * 2], clks_base[i] + CLKA_SWITCH_CFG(0));
		iowrite32(switch_cfg[i * 2 + 1], clks_base[i] +
			  CLKA_SWITCH_CFG(1));
	}

	kfree(switch_cfg);
	switch_cfg = NULL;

	pr_debug("stm pm: ClockGens A: restored\n");
}

static int stid127_get_wake_irq(void)
{
	int irq = 0;
	struct irq_data *d;
	void *gic_cpu = __io_address(STID127_GIC_CPU_BASE);

	irq = readl(gic_cpu + GIC_CPU_INTACK);
	d = irq_get_irq_data(irq);
	writel(d->hwirq, gic_cpu + GIC_CPU_EOI);

	return irq;
}

static struct stm_platform_suspend stid127_suspend = {
	.ops.begin = stid127_suspend_begin,

	.pre_enter = stid127_suspend_pre_enter,
	.post_enter = stid127_suspend_post_enter,

	.eram_iomem = (void *)0xfe240000,
	.get_wake_irq = stid127_get_wake_irq,

};

static int __init stid127_suspend_setup(void)
{
	int i;

	clks_base[0] = ioremap_nocache(0xFEA10000, 0x1000);

	if (!clks_base[0])
		goto err_0;
	clks_base[1] = ioremap_nocache(CLK_A1_BASE, 0x1000);
	if (!clks_base[1])
		goto err_1;

	INIT_LIST_HEAD(&stid127_suspend.mem_tables);

	for (i = 0; i < ARRAY_SIZE(stid127_suspend_tables); ++i)
		list_add_tail(&stid127_suspend_tables[i].node,
			      &stid127_suspend.mem_tables);

	return stm_suspend_register(&stid127_suspend);
err_1:
	iounmap(clks_base[0]);
err_0:
	return -EINVAL;
}
module_init(stid127_suspend_setup);
