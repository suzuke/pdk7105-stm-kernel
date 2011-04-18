/*
 * arch/arm/mach-stm/nice.c
 *
 * Copyright (C) 2010 STMicroelectronics Limited.
 * Author: Stuart Menefy <stuart.menefy@st.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/sysdev.h>
#include <linux/io.h>

#include <asm/irq.h>
#include <asm/leds.h>
#include <asm/mach-types.h>
#include <asm/smp_twd.h>
#include <asm/smp_gt.h>
#include <asm/hardware/gic.h>

#include <asm/mach/map.h>
#include <asm/mach/time.h>

#include <mach/soc-nice.h>
#include <mach/hardware.h>
#include <mach/irqs.h>

#include "core.h"

static struct map_desc nice_io_desc[] __initdata = {
	{
		.virtual	= IO_ADDRESS(NICE_GIC_CPU_BASE),
		.pfn		= __phys_to_pfn(NICE_GIC_CPU_BASE),
		.length		= SZ_4K,
		.type		= MT_DEVICE,
	}, {
		.virtual	= IO_ADDRESS(NICE_GIC_DIST_BASE),
		.pfn		= __phys_to_pfn(NICE_GIC_DIST_BASE),
		.length		= SZ_4K,
		.type		= MT_DEVICE,
	}, {
		.virtual        = IO_ADDRESS(NICE_PL310_BASE),
		.pfn            = __phys_to_pfn(NICE_PL310_BASE),
		.length         = SZ_16K,
		.type           = MT_DEVICE,
	}, {
		.virtual	= IO_ADDRESS(NICE_ASC0_BASE),
		.pfn		= __phys_to_pfn(NICE_ASC0_BASE),
		.length		= SZ_4K,
		.type		= MT_DEVICE,
	}, {
		.virtual	= IO_ADDRESS(NICE_PIO_MPE_BASE),
		.pfn		= __phys_to_pfn(NICE_PIO_MPE_BASE),
		.length		= SZ_64K,
		.type		= MT_DEVICE,
	}, {
		.virtual	= IO_ADDRESS(NICE_PIO_SAS_REAR_BASE),
		.pfn		= __phys_to_pfn(NICE_PIO_SAS_REAR_BASE),
		.length		= SZ_64K,
		.type		= MT_DEVICE,
	}, {
		.virtual	= IO_ADDRESS(NICE_PIO_SAS_FRONT_BASE),
		.pfn		= __phys_to_pfn(NICE_PIO_SAS_FRONT_BASE),
		.length		= SZ_64K,
		.type		= MT_DEVICE,
	}, {
		.virtual	= IO_ADDRESS(NICE_SBC_SYSCONF_BASE),
		.pfn		= __phys_to_pfn(NICE_SBC_SYSCONF_BASE),
		.length		= SZ_4K,
		.type		= MT_DEVICE,
	}, {
		.virtual	= IO_ADDRESS(NICE_SBC_ASC0_BASE),
		.pfn		= __phys_to_pfn(NICE_SBC_ASC0_BASE),
		.length		= SZ_4K,
		.type		= MT_DEVICE,
	}	
};

void __init nice_map_io(void)
{
	iotable_init(nice_io_desc, ARRAY_SIZE(nice_io_desc));
}

void __init nice_gic_init_irq(void)
{
	gic_cpu_base_addr = __io_address(NICE_GIC_CPU_BASE);
	gic_dist_init(0, __io_address(NICE_GIC_DIST_BASE),
		      27);
	gic_cpu_init(0, __io_address(NICE_GIC_CPU_BASE));
}

static void __init nice_timer_init(void)
{
#ifdef CONFIG_HAVE_ARM_GT
	global_timer_init(__io_address(NICE_GLOBAL_TIMER_BASE),
			  IRQ_GLOBALTIMER);
#endif

#ifdef CONFIG_HAVE_ARM_TWD
	twd_base = __io_address(NICE_TWD_BASE);
#endif
}

struct sys_timer nice_timer = {
	.init		= nice_timer_init,
};
