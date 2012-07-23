
/*
 *  linux/arch/arm/mach-stm/mpe41.c
 *
 * Copyright (C) 2012 STMicroelectronics Limited.
 * Author: Srinivas Kandagatla <srinivas.kandagatla@st.com>
 *
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include <linux/init.h>
#include <linux/io.h>
#include <linux/err.h>
#include <linux/stm/clk.h>
#include <linux/clockchips.h>

#include <asm/irq.h>
#include <asm/smp_twd.h>
#include <asm/localtimer.h>
#include <asm/smp_twd.h>
#include <asm/smp_gt.h>
#include <asm/hardware/gic.h>

#include <asm/mach/map.h>
#include <asm/mach/time.h>
#include <mach/mpe41.h>
#include <mach/hardware.h>
#include <mach/io.h>
#include <mach/irqs.h>

/* Setup th GIC */
void __init mpe41_gic_init_irq(void)
{
	gic_init(0, 27, __io_address(MPE41_GIC_DIST_BASE),
			__io_address(MPE41_GIC_CPU_BASE));
}

/* Setup the Global Timer */
static void __init mpe41_timer_init(void)
{

	struct clk *a9_clk;
	plat_clk_init();
	plat_clk_alias_init();

	a9_clk = clk_get(NULL, "CLKM_A9");
	if (IS_ERR(a9_clk))
		panic("Unable to determine Cortex A9 clock frequency\n");

#ifdef CONFIG_HAVE_ARM_GT
	global_timer_init(__io_address(MPE41_GLOBAL_TIMER_BASE),
			IRQ_GLOBALTIMER, clk_get_rate(a9_clk)/2);
#endif

}

struct sys_timer mpe41_timer = {
	.init		= mpe41_timer_init,
};


/*
 * Setup the localtimer.
 */
int __cpuinit local_timer_setup(struct clock_event_device *evt)
{
	twd_base = __io_address(MPE41_TWD_BASE);
	evt->irq = IRQ_LOCALTIMER;
	twd_timer_setup(evt);
	return 0;
}

#ifdef CONFIG_SMP
void __iomem *scu_base_addr = ((void __iomem *) IO_ADDRESS(MPE41_SCU_BASE));
#endif
