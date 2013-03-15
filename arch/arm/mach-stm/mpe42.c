/*
 * linux/arch/arm/mach-stm/mpe42.c
 *
 * Copyright (C) 2012 STMicroelectronics Limited.
 * Author: Francesco Virlinzi <francesco.virlinzi@st.com>
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
#include <asm/pmu.h>

#include <asm/mach/map.h>
#include <asm/mach/time.h>
#include <mach/mpe42.h>
#include <mach/hardware.h>

#include <linux/stm/stih416.h>
#include <linux/stm/mpe42-periphs.h>

static struct map_desc mpe42_io_desc[] __initdata = {
	{
		.virtual	= IO_ADDRESS(MPE42_SCU_BASE),
		.pfn		= __phys_to_pfn(MPE42_SCU_BASE),
		.length		= SZ_4K,
		.type		= MT_DEVICE,
	}, {
		.virtual	= IO_ADDRESS(MPE42_GIC_DIST_BASE),
		.pfn		= __phys_to_pfn(MPE42_GIC_DIST_BASE),
		.length		= SZ_4K,
		.type		= MT_DEVICE,
	}, {
		.virtual        = IO_ADDRESS(MPE42_PL310_BASE),
		.pfn            = __phys_to_pfn(MPE42_PL310_BASE),
		.length         = SZ_16K,
		.type           = MT_DEVICE,
	}, {
		.virtual	= IO_ADDRESS(MPE42_PIO_10_BASE),
		.pfn		= __phys_to_pfn(MPE42_PIO_10_BASE),
		.length		= SZ_64K,
		.type		= MT_DEVICE,
	}, {
		.virtual	= IO_ADDRESS(MPE42_PIO_11_BASE),
		.pfn		= __phys_to_pfn(MPE42_PIO_11_BASE),
		.length		= SZ_64K,
		.type		= MT_DEVICE,
	}, {
		.virtual	= IO_ADDRESS(MPE42_FVDP_FE_SYSCON_BASE),
		.pfn		= __phys_to_pfn(MPE42_FVDP_FE_SYSCON_BASE),
		.length		= SZ_4K,
		.type		= MT_DEVICE,
	}, {
		.virtual	= IO_ADDRESS(MPE42_FVDP_LITE_SYSCONF_BASE),
		.pfn		= __phys_to_pfn(MPE42_FVDP_LITE_SYSCONF_BASE),
		.length		= SZ_4K,
		.type		= MT_DEVICE,
	}, {
		.virtual	= IO_ADDRESS(MPE42_CPU_SYSCONF_BASE),
		.pfn		= __phys_to_pfn(MPE42_CPU_SYSCONF_BASE),
		.length		= SZ_4K,
		.type		= MT_DEVICE,
	}, {
		.virtual	= IO_ADDRESS(MPE42_COMPO_SYSCONF_BASE),
		.pfn		= __phys_to_pfn(MPE42_COMPO_SYSCONF_BASE),
		.length		= SZ_4K,
		.type		= MT_DEVICE,
	}, {
		.virtual	= IO_ADDRESS(MPE42_TRANSPORT_SYSCONF_BASE),
		.pfn		= __phys_to_pfn(MPE42_TRANSPORT_SYSCONF_BASE),
		.length		= SZ_4K,
		.type		= MT_DEVICE,
	}
};

void __init mpe42_map_io(void)
{
	iotable_init(mpe42_io_desc, ARRAY_SIZE(mpe42_io_desc));
}

#ifndef CONFIG_OF
/* Setup th GIC */
void __init mpe42_gic_init_irq(void)
{
	gic_init(0, 27, __io_address(MPE42_GIC_DIST_BASE),
			__io_address(MPE42_GIC_CPU_BASE));
}


#ifdef CONFIG_HAVE_ARM_TWD
static DEFINE_TWD_LOCAL_TIMER(mpe42_local_timer,
			      MPE42_TWD_BASE, IRQ_LOCALTIMER);

static void __init mpe42_twd_init(void)
{
	int err;
	err = twd_local_timer_register(&mpe42_local_timer);
	if (err)
		pr_err("twd_local_timer_register failed %d\n", err);
}
#else
#define mpe42_twd_init() do { } while (0)
#endif

/* Setup the Global Timer */
static void __init mpe42_timer_init(void)
{
	stih416_plat_clk_init();
	stih416_plat_clk_alias_init();

#ifdef CONFIG_HAVE_ARM_GT
	global_timer_init(__io_address(MPE42_GLOBAL_TIMER_BASE),
			IRQ_GLOBALTIMER);
#endif

	mpe42_twd_init();
}

struct sys_timer mpe42_timer = {
	.init		= mpe42_timer_init,
};

#ifdef CONFIG_HW_PERF_EVENTS

static struct resource pmu_resource = {
	.start		= 31,
	.end		= 31,
	.flags		= IORESOURCE_IRQ,
};

static struct platform_device mpe42_pmu_device = {
	.name			= "arm-pmu",
	.id			= ARM_PMU_DEVICE_CPU,
	.resource		= &pmu_resource,
	.num_resources		= 1,
};

static int __init mpe42_configure_pmu(void)
{
	struct sysconf_field *sc_a9_irq_en =
			sysconf_claim(SYSCONF(7543), 0, 5, "pmu");
	struct sysconf_field *sc_irq0_n_sel =
			sysconf_claim(SYSCONF(7543), 14, 16, "pmu");
	struct sysconf_field *sc_irq1_n_sel =
			sysconf_claim(SYSCONF(7543), 17, 19, "pmu");

	/* The PMU interrupts are muxed onto PPI 31 with various
	 * other things, selected via this sysconf
	 */
	sysconf_write(sc_a9_irq_en, 0xc);
	sysconf_write(sc_irq0_n_sel, 0x5);
	sysconf_write(sc_irq1_n_sel, 0x6);

	sysconf_release(sc_a9_irq_en);
	sysconf_release(sc_irq0_n_sel);
	sysconf_release(sc_irq1_n_sel);

	return platform_device_register(&mpe42_pmu_device);
}
device_initcall(mpe42_configure_pmu);

#endif /* CONFIG_HW_PERF_EVENTS */
#endif /* CONFIG_OF */
