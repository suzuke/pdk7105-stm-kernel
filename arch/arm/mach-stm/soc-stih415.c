/*
 * arch/arm/mach-stm/soc-stih415.c
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
#include <linux/io.h>
#include <linux/err.h>
#include <linux/stm/clk.h>

#include <asm/irq.h>
#include <asm/leds.h>
#include <asm/mach-types.h>
#include <asm/smp_twd.h>
#include <asm/smp_gt.h>
#include <asm/hardware/gic.h>

#include <asm/mach/map.h>
#include <asm/mach/time.h>

#include <mach/soc-stih415.h>
#include <mach/hardware.h>
#include <mach/irqs.h>

#include "core.h"

static struct map_desc stih415_io_desc[] __initdata = {
	{
		.virtual	= IO_ADDRESS(STIH415_SCU_BASE),
		.pfn		= __phys_to_pfn(STIH415_SCU_BASE),
		.length		= SZ_4K,
		.type		= MT_DEVICE,
	}, {
		.virtual	= IO_ADDRESS(STIH415_GIC_DIST_BASE),
		.pfn		= __phys_to_pfn(STIH415_GIC_DIST_BASE),
		.length		= SZ_4K,
		.type		= MT_DEVICE,
	}, {
		.virtual        = IO_ADDRESS(STIH415_PL310_BASE),
		.pfn            = __phys_to_pfn(STIH415_PL310_BASE),
		.length         = SZ_16K,
		.type           = MT_DEVICE,
	}, {
		.virtual	= IO_ADDRESS(STIH415_ASC0_BASE),
		.pfn		= __phys_to_pfn(STIH415_ASC0_BASE),
		.length		= SZ_64K,
		.type		= MT_DEVICE,
	}, {
		.virtual	= IO_ADDRESS(STIH415_PIO_SAS_SBC_BASE),
		.pfn		= __phys_to_pfn(STIH415_PIO_SAS_SBC_BASE),
		.length		= SZ_64K,
		.type		= MT_DEVICE,
	}, {
		.virtual	= IO_ADDRESS(STIH415_PIO_SAS_FRONT_BASE),
		.pfn		= __phys_to_pfn(STIH415_PIO_SAS_FRONT_BASE),
		.length		= SZ_64K,
		.type		= MT_DEVICE,
	}, {
		.virtual	= IO_ADDRESS(STIH415_PIO_SAS_REAR_BASE),
		.pfn		= __phys_to_pfn(STIH415_PIO_SAS_REAR_BASE),
		.length		= SZ_64K,
		.type		= MT_DEVICE,
	}, {
		.virtual	= IO_ADDRESS(STIH415_PIO_MPE_RIGHT_BASE),
		.pfn		= __phys_to_pfn(STIH415_PIO_MPE_RIGHT_BASE),
		.length		= SZ_64K,
		.type		= MT_DEVICE,
	}, {
		.virtual	= IO_ADDRESS(STIH415_PIO_MPE_LEFT_BASE),
		.pfn		= __phys_to_pfn(STIH415_PIO_MPE_LEFT_BASE),
		.length		= SZ_64K,
		.type		= MT_DEVICE,
	}, {
		.virtual	= IO_ADDRESS(STIH415_SBC_SYSCONF_BASE),
		.pfn		= __phys_to_pfn(STIH415_SBC_SYSCONF_BASE),
		.length		= SZ_4K,
		.type		= MT_DEVICE,
	}, {
		.virtual	= IO_ADDRESS(STIH415_SAS_FRONT_SYSCONF_BASE),
		.pfn		= __phys_to_pfn(STIH415_SAS_FRONT_SYSCONF_BASE),
		.length		= SZ_4K,
		.type		= MT_DEVICE,
	}, {
		.virtual	= IO_ADDRESS(STIH415_SAS_REAR_SYSCONF_BASE),
		.pfn		= __phys_to_pfn(STIH415_SAS_REAR_SYSCONF_BASE),
		.length		= SZ_4K,
		.type		= MT_DEVICE,
	}, {
		.virtual	= IO_ADDRESS(STIH415_MPE_LEFT_SYSCONF_BASE),
		.pfn		= __phys_to_pfn(STIH415_MPE_LEFT_SYSCONF_BASE),
		.length		= SZ_4K,
		.type		= MT_DEVICE,
	}, {
		.virtual	= IO_ADDRESS(STIH415_MPE_RIGHT_SYSCONF_BASE),
		.pfn		= __phys_to_pfn(STIH415_MPE_RIGHT_SYSCONF_BASE),
		.length		= SZ_4K,
		.type		= MT_DEVICE,
	}, {
		.virtual	= IO_ADDRESS(STIH415_MPE_SYSTEM_SYSCONF_BASE),
		.pfn		= __phys_to_pfn(STIH415_MPE_SYSTEM_SYSCONF_BASE),
		.length		= SZ_4K,
		.type		= MT_DEVICE,
	}, {
		.virtual	= IO_ADDRESS(STIH415_SBC_LPM_CONF_BASE),
		.pfn		= __phys_to_pfn(STIH415_SBC_LPM_CONF_BASE),
		.length		= SZ_4K,
		.type		= MT_DEVICE,
	}, {
		.virtual	= IO_ADDRESS(STIH415_SBC_ASC0_BASE),
		.pfn		= __phys_to_pfn(STIH415_SBC_ASC0_BASE),
		.length		= SZ_64K,
		.type		= MT_DEVICE,
	}
};

void __init stih415_map_io(void)
{
	iotable_init(stih415_io_desc, ARRAY_SIZE(stih415_io_desc));
}

void __init stih415_gic_init_irq(void)
{
	gic_init(0, 27, __io_address(STIH415_GIC_DIST_BASE),
			__io_address(STIH415_GIC_CPU_BASE));
}

static void __init stih415_timer_init(void)
{
	struct clk* a9_clk;

	plat_clk_init();
	plat_clk_alias_init();

	a9_clk = clk_get(NULL, "CLKM_A9");
	if (IS_ERR(a9_clk))
		panic("Unable to determine Cortex A9 clock frequency\n");

#ifdef CONFIG_HAVE_ARM_GT
	global_timer_init(__io_address(STIH415_GLOBAL_TIMER_BASE),
			  IRQ_GLOBALTIMER, clk_get_rate(a9_clk)/2);
#endif

#ifdef CONFIG_HAVE_ARM_TWD
	twd_base = __io_address(STIH415_TWD_BASE);
#endif
}

struct sys_timer stih415_timer = {
	.init		= stih415_timer_init,
};

#ifdef CONFIG_SMP
void __iomem *scu_base_addr = ((void __iomem *) IO_ADDRESS(STIH415_SCU_BASE));
#endif
