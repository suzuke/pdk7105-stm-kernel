/*
 * Copyright (C) 2012 STMicroelectronics Limited.
 *
 * Author(s): Carmelo Amoroso <carmelo.amoroso@st.com>
 *            Nunzio Raciti <nunzio.raciti@st.com>
 *
 * (Based on 2.6.32 port by Stuart Menefy <stuart.menefy@st.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/err.h>
#include <linux/stm/clk.h>
#include <linux/stm/stig125.h>

#include <asm/smp_twd.h>
#include <asm/smp_gt.h>
#include <asm/hardware/gic.h>

#include <asm/page.h>
#include <asm/mach/map.h>
#include <asm/mach/time.h>

#include <mach/soc-stig125.h>
#include <mach/hardware.h>
#include <mach/irqs.h>

static struct map_desc stig125_io_desc[] __initdata = {
	{
		.virtual	= IO_ADDRESS(STIG125_SCU_BASE),
		.pfn		= __phys_to_pfn(STIG125_SCU_BASE),
		.length		= SZ_4K,
		.type		= MT_DEVICE,
	}, {
		.virtual	= IO_ADDRESS(STIG125_GIC_DIST_BASE),
		.pfn		= __phys_to_pfn(STIG125_GIC_DIST_BASE),
		.length		= SZ_4K,
		.type		= MT_DEVICE,
	}, {
		.virtual        = IO_ADDRESS(STIG125_PL310_BASE),
		.pfn            = __phys_to_pfn(STIG125_PL310_BASE),
		.length         = SZ_16K,
		.type           = MT_DEVICE,
	}, {
		.virtual	= IO_ADDRESS(STIG125_ASC0_BASE),
		.pfn		= __phys_to_pfn(STIG125_ASC0_BASE),
		.length		= SZ_4K,
		.type		= MT_DEVICE,
	}, {
		.virtual	= IO_ADDRESS(STIG125_ASC1_BASE),
		.pfn		= __phys_to_pfn(STIG125_ASC1_BASE),
		.length		= SZ_4K,
		.type		= MT_DEVICE,
	}, {
		.virtual	= IO_ADDRESS(STIG125_ASC2_BASE),
		.pfn		= __phys_to_pfn(STIG125_ASC2_BASE),
		.length		= SZ_4K,
		.type		= MT_DEVICE,
	}, {
		.virtual	= IO_ADDRESS(STIG125_ASC3_BASE),
		.pfn		= __phys_to_pfn(STIG125_ASC3_BASE),
		.length		= SZ_4K,
		.type		= MT_DEVICE,
	}, {
		.virtual	= IO_ADDRESS(STIG125_PIO_NORTH_BASE),
		.pfn		= __phys_to_pfn(STIG125_PIO_NORTH_BASE),
		.length		= SZ_64K,
		.type		= MT_DEVICE,
	}, {
		.virtual	= IO_ADDRESS(STIG125_PIO_SBC_BASE),
		.pfn		= __phys_to_pfn(STIG125_PIO_SBC_BASE),
		.length		= SZ_64K,
		.type		= MT_DEVICE,
	}, {
		.virtual	= IO_ADDRESS(STIG125_PIO_WEST_BASE),
		.pfn		= __phys_to_pfn(STIG125_PIO_WEST_BASE),
		.length		= SZ_64K,
		.type		= MT_DEVICE,
	}, {
		.virtual	= IO_ADDRESS(STIG125_PIO_SOUTH_BASE),
		.pfn		= __phys_to_pfn(STIG125_PIO_SOUTH_BASE),
		.length		= SZ_64K,
		.type		= MT_DEVICE,
	}, {
		.virtual	= IO_ADDRESS(STIG125_SYSCONF_HD_BASE),
		.pfn		= __phys_to_pfn(STIG125_SYSCONF_HD_BASE),
		.length		= SZ_64K,
		.type		= MT_DEVICE,
	}, {
		.virtual	= IO_ADDRESS(STIG125_SYSCONF_CPU_BASE),
		.pfn		= __phys_to_pfn(STIG125_SYSCONF_CPU_BASE),
		.length		= SZ_64K,
		.type		= MT_DEVICE,
	}, {
		.virtual	= IO_ADDRESS(STIG125_SYSCONF_DDR_BASE),
		.pfn		= __phys_to_pfn(STIG125_SYSCONF_DDR_BASE),
		.length		= SZ_64K,
		.type		= MT_DEVICE,
	}, {
		.virtual	= IO_ADDRESS(STIG125_SYSCONF_NORTH_BASE),
		.pfn		= __phys_to_pfn(STIG125_SYSCONF_NORTH_BASE),
		.length		= SZ_64K,
		.type		= MT_DEVICE,
	}, {
		.virtual	= IO_ADDRESS(STIG125_SYSCONF_SBC_BASE),
		.pfn		= __phys_to_pfn(STIG125_SYSCONF_SBC_BASE),
		.length		= SZ_64K,
		.type		= MT_DEVICE,
	}, {
		.virtual	= IO_ADDRESS(STIG125_SYSCONF_WEST_BASE),
		.pfn		= __phys_to_pfn(STIG125_SYSCONF_WEST_BASE),
		.length		= SZ_64K,
		.type		= MT_DEVICE,
	}, {
		.virtual	= IO_ADDRESS(STIG125_SYSCONF_DOCSIS_BASE),
		.pfn		= __phys_to_pfn(STIG125_SYSCONF_DOCSIS_BASE),
		.length		= SZ_64K,
		.type		= MT_DEVICE,
	}, {
		.virtual	= IO_ADDRESS(STIG125_SYSCONF_SOUTH_BASE),
		.pfn		= __phys_to_pfn(STIG125_SYSCONF_SOUTH_BASE),
		.length		= SZ_64K,
		.type		= MT_DEVICE,
	} , {
		.virtual	= IO_ADDRESS(STIG125_SBC_LPM_CONF_BASE),
		.pfn		= __phys_to_pfn(STIG125_SBC_LPM_CONF_BASE),
		.length		= SZ_4K,
		.type		= MT_DEVICE,
	}, {
		.virtual	= IO_ADDRESS(STIG125_SBC_ASC0_BASE),
		.pfn		= __phys_to_pfn(STIG125_SBC_ASC0_BASE),
		.length		= SZ_4K,
		.type		= MT_DEVICE,
	}, {
		.virtual	= IO_ADDRESS(STIG125_SBC_ASC1_BASE),
		.pfn		= __phys_to_pfn(STIG125_SBC_ASC1_BASE),
		.length		= SZ_4K,
		.type		= MT_DEVICE,
	}
};

void __init stig125_map_io(void)
{
	iotable_init(stig125_io_desc, ARRAY_SIZE(stig125_io_desc));
}

void __init stig125_gic_init_irq(void)
{
	gic_init(0, 27, __io_address(STIG125_GIC_DIST_BASE),
			__io_address(STIG125_GIC_CPU_BASE));
}

#ifdef CONFIG_HAVE_ARM_TWD
static DEFINE_TWD_LOCAL_TIMER(stig125_local_timer,
			      STIG125_TWD_BASE, IRQ_LOCALTIMER);

static void __init stig125_twd_init(void)
{
	int err;
	err = twd_local_timer_register(&stig125_local_timer);
	if (err)
		pr_err("twd_local_timer_register failed %d\n", err);
}
#else
#define stig125_twd_init() do { } while (0)
#endif

static void __init stig125_timer_init(void)
{

	stig125_plat_clk_init();
	stig125_plat_clk_alias_init();

#ifdef CONFIG_HAVE_ARM_GT
	global_timer_init(__io_address(STIG125_GLOBAL_TIMER_BASE),
			  IRQ_GLOBALTIMER);
#endif

	stig125_twd_init();
}

struct sys_timer stig125_timer = {
	.init	= stig125_timer_init,
};

#ifdef CONFIG_SMP
void __iomem *scu_base_addr = ((void __iomem *) IO_ADDRESS(STIG125_SCU_BASE));
#endif
