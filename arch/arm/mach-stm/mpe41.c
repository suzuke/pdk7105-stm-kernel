
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
#include <asm/smp_gt.h>
#include <asm/hardware/gic.h>

#include <asm/mach/map.h>
#include <asm/mach/time.h>
#include <mach/mpe41.h>
#include <mach/hardware.h>
#include <mach/irqs.h>

/* Setup th GIC */
void __init mpe41_gic_init_irq(void)
{
	gic_init(0, 27, __io_address(MPE41_GIC_DIST_BASE),
			__io_address(MPE41_GIC_CPU_BASE));
}


#ifdef CONFIG_HAVE_ARM_TWD
static DEFINE_TWD_LOCAL_TIMER(mpe41_local_timer,
			      MPE41_TWD_BASE, IRQ_LOCALTIMER);

static void __init mpe41_twd_init(void)
{
	int err;
	err = twd_local_timer_register(&mpe41_local_timer);
	if (err)
		pr_err("twd_local_timer_register failed %d\n", err);
}
#else
#define mpe41_twd_init() do { } while 0
#endif

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

	mpe41_twd_init();
}

struct sys_timer mpe41_timer = {
	.init		= mpe41_timer_init,
};

#ifdef CONFIG_SMP
void __iomem *scu_base_addr = ((void __iomem *) IO_ADDRESS(MPE41_SCU_BASE));
#endif
