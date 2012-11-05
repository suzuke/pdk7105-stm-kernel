
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
#include <linux/clk.h>
#include <linux/stm/clk.h>
#include <linux/clockchips.h>

#include <asm/irq.h>
#include <asm/smp_twd.h>
#include <asm/smp_gt.h>
#include <asm/hardware/gic.h>
#include <asm/pmu.h>

#ifdef CONFIG_CPU_SUBTYPE_STIH415
#include <linux/stm/stih415.h>
#endif
#ifdef CONFIG_CPU_SUBTYPE_FLI7610
#include <linux/stm/fli7610.h>
#endif

#include <asm/mach/map.h>
#include <asm/mach/time.h>
#include <mach/mpe41.h>
#include <mach/hardware.h>
#include <mach/irqs.h>

#ifdef CONFIG_CPU_SUBTYPE_STIH415
#include <linux/stm/stih415.h>
#include <linux/stm/stih415-periphs.h>
#endif

#ifdef CONFIG_CPU_SUBTYPE_FLI7610
#include <linux/stm/fli7610.h>
#include <linux/stm/fli7610-periphs.h>
#endif

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
#define mpe41_twd_init() do { } while (0)
#endif

/* Setup the Global Timer */
static void __init mpe41_timer_init(void)
{
	struct clk *a9_clk;

#ifdef CONFIG_CPU_SUBTYPE_FLI7610
	fli7610_plat_clk_init();
	fli7610_plat_clk_alias_init();
#endif
#ifdef CONFIG_CPU_SUBTYPE_STIH415
	stih415_plat_clk_init();
	stih415_plat_clk_alias_init();
#endif
	a9_clk = clk_get(NULL, "CLKM_A9");
	if (IS_ERR(a9_clk))
		panic("Unable to determine Cortex A9 clock frequency\n");

#ifdef CONFIG_HAVE_ARM_GT
	global_timer_init(__io_address(MPE41_GLOBAL_TIMER_BASE),
			IRQ_GLOBALTIMER);
#endif

	mpe41_twd_init();
}

struct sys_timer mpe41_timer = {
	.init		= mpe41_timer_init,
};

#ifdef CONFIG_SMP
void __iomem *scu_base_addr = ((void __iomem *) IO_ADDRESS(MPE41_SCU_BASE));
#endif

#ifdef CONFIG_HW_PERF_EVENTS

static struct resource pmu_resource = {
	.start          = 31,
	.end            = 31,
	.flags          = IORESOURCE_IRQ,
};

static struct platform_device mpe41_pmu_device = {
	.name                   = "arm-pmu",
	.id                     = ARM_PMU_DEVICE_CPU,
	.resource               = &pmu_resource,
	.num_resources          = 1,
};

static int __init mpe41_configure_pmu(void)
{
	struct sysconf_field *sc_a9_irq_en =
			sysconf_claim(SYSCONF(642), 0, 5, "pmu");
	struct sysconf_field *sc_irq0_n_sel =
			sysconf_claim(SYSCONF(642), 14, 16, "pmu");
	struct sysconf_field *sc_irq1_n_sel =
			sysconf_claim(SYSCONF(642), 17, 19, "pmu");

	/* The PMU interrupts are muxed onto PPI 31 with various
	 * other things, selected via this sysconf */
	sysconf_write(sc_a9_irq_en, 0xc);
	sysconf_write(sc_irq0_n_sel, 0x5);
	sysconf_write(sc_irq1_n_sel, 0x6);

	sysconf_release(sc_a9_irq_en);
	sysconf_release(sc_irq0_n_sel);
	sysconf_release(sc_irq1_n_sel);

	return platform_device_register(&mpe41_pmu_device);
}
device_initcall(mpe41_configure_pmu);

#endif /* CONFIG_HW_PERF_EVENTS */
