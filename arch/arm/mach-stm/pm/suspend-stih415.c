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

#include <linux/stm/stih415.h>
#include <linux/stm/stih415-periphs.h>
#include <linux/stm/sysconf.h>
#include <linux/stm/clk.h>
#include <linux/stm/wakeup_devices.h>

#include "../suspend.h"
#include <mach/hardware.h>
#include <mach/mpe41.h>
#include <mach/soc-stih415.h>
#include <asm/hardware/gic.h>	/* gic offset and struct gic_chip_data */

#include <linux/stm/poke_table.h>
#include <linux/stm/synopsys_dwc_ddr32.h>

static const long stih415_ddr0_enter[] = {
synopsys_ddr32_in_self_refresh(STIH415_MPE_DDR0_PCTL_BASE),
synopsys_ddr32_phy_standby_enter(STIH415_MPE_DDR0_PCTL_BASE),
};

static const long stih415_ddr1_enter[] = {
synopsys_ddr32_in_self_refresh(STIH415_MPE_DDR1_PCTL_BASE),
synopsys_ddr32_phy_standby_enter(STIH415_MPE_DDR1_PCTL_BASE),
};

static const long stih415_ddr0_exit[] = {
synopsys_ddr32_phy_standby_exit(STIH415_MPE_DDR0_PCTL_BASE),
synopsys_ddr32_out_of_self_refresh(STIH415_MPE_DDR0_PCTL_BASE),
};

static const long stih415_ddr1_exit[] = {
synopsys_ddr32_phy_standby_exit(STIH415_MPE_DDR1_PCTL_BASE),
synopsys_ddr32_out_of_self_refresh(STIH415_MPE_DDR1_PCTL_BASE),
};

#define SUSPEND_TBL(_enter, _exit) {			\
	.enter = _enter,				\
	.enter_size = ARRAY_SIZE(_enter) * sizeof(long),\
	.exit = _exit,					\
	.exit_size = ARRAY_SIZE(_exit) * sizeof(long),	\
}

static struct stm_suspend_table stih415_suspend_tables[] = {
	SUSPEND_TBL(stih415_ddr0_enter, stih415_ddr0_exit),
	SUSPEND_TBL(stih415_ddr1_enter, stih415_ddr1_exit),
};

struct stm_wakeup_devices stih415_wkd;
static struct stm_mcm_suspend *main_mcm;
static struct stm_mcm_suspend *peripheral_mcm;
struct stm_mcm_suspend *stx_mpe41_suspend_setup(void);
struct stm_mcm_suspend *stx_sasg1_suspend_setup(void);
static suspend_state_t target_state;

static int stih415_suspend_begin(suspend_state_t state)
{
	int ret = 0;

	pr_info("[STM][PM] Analyzing the wakeup devices\n");

	stm_check_wakeup_devices(&stih415_wkd);
	target_state = state;

	ret = stm_suspend_mcm_begin(main_mcm, state, &stih415_wkd);
	if (ret)
		return ret;

	ret = stm_suspend_mcm_begin(peripheral_mcm, state, &stih415_wkd);
	if (ret)
		stm_suspend_mcm_end(main_mcm, state);


	return ret;
}

static int stih415_suspend_pre_enter(suspend_state_t state)
{
	int ret = 0;

	ret = stm_suspend_mcm_pre_enter(main_mcm, state, &stih415_wkd);

	if (ret)
		return ret;

	ret = stm_suspend_mcm_pre_enter(peripheral_mcm, state, &stih415_wkd);
	if (ret)
		stm_suspend_mcm_post_enter(main_mcm, state);

	return ret;
}

static void stih415_suspend_post_enter(suspend_state_t state)
{
	stm_suspend_mcm_post_enter(peripheral_mcm, state);

	stm_suspend_mcm_post_enter(main_mcm, state);
}

static void stih415_suspend_end(void)
{
	stm_suspend_mcm_end(peripheral_mcm, target_state);
	stm_suspend_mcm_end(main_mcm, target_state);
}

static int stih415_get_wake_irq(void)
{
	int irq = 0;
	struct irq_data *d;
	void *gic_cpu = __io_address(MPE41_GIC_CPU_BASE);

	irq = readl(gic_cpu + GIC_CPU_INTACK);
	d = irq_get_irq_data(irq);
	writel(d->hwirq, gic_cpu + GIC_CPU_EOI);

	return irq;
}

static struct stm_platform_suspend stih415_suspend = {
	.ops.begin = stih415_suspend_begin,
	.ops.end = stih415_suspend_end,

	.pre_enter = stih415_suspend_pre_enter,
	.post_enter = stih415_suspend_post_enter,

	.eram_iomem = (void *)0xc00a0000,
	.get_wake_irq = stih415_get_wake_irq,
};

static int __init stih415_suspend_setup(void)
{
	int i;

	INIT_LIST_HEAD(&stih415_suspend.mem_tables);

	main_mcm = stx_mpe41_suspend_setup();

	peripheral_mcm = stx_sasg1_suspend_setup();

	if (!main_mcm || !peripheral_mcm) {
		pr_err("stm: Error on mcm registration\n");
		return -ENOSYS;
	}

	for (i = 0; i < ARRAY_SIZE(stih415_suspend_tables); ++i)
		list_add_tail(&stih415_suspend_tables[i].node,
			&stih415_suspend.mem_tables);

	return stm_suspend_register(&stih415_suspend);
}

module_init(stih415_suspend_setup);
