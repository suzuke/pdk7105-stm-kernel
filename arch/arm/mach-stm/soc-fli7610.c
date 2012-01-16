
/*
 * arch/arm/mach-stm/soc-fli7610.c
 *
 * Copyright (C) 2011 STMicroelectronics Limited.
 * Author: Srinivas Kandagatla <srinivas.kandagatla@st.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/sysdev.h>
#include <linux/io.h>
#include <linux/err.h>
#include <linux/stm/clk.h>

#include <asm/irq.h>
#include <asm/mach-types.h>
#include <asm/smp_twd.h>
#include <asm/smp_gt.h>
#include <asm/hardware/gic.h>

#include <asm/mach/map.h>
#include <asm/mach/time.h>

#include <mach/soc-fli7610.h>
#include <mach/hardware.h>
#include <mach/irqs.h>

#include "core.h"

static struct map_desc fli7610_io_desc[] __initdata = {
	{
		.virtual	= IO_ADDRESS(FLI7610_GIC_CPU_BASE),
		.pfn		= __phys_to_pfn(FLI7610_GIC_CPU_BASE),
		.length		= SZ_4K,
		.type		= MT_DEVICE,
	}, {
		.virtual	= IO_ADDRESS(FLI7610_GIC_DIST_BASE),
		.pfn		= __phys_to_pfn(FLI7610_GIC_DIST_BASE),
		.length		= SZ_4K,
		.type		= MT_DEVICE,
	}, {
		.virtual        = IO_ADDRESS(FLI7610_PL310_BASE),
		.pfn            = __phys_to_pfn(FLI7610_PL310_BASE),
		.length         = SZ_16K,
		.type           = MT_DEVICE,
	}, {
		.virtual	= IO_ADDRESS(FLI7610_TAE_ASC0_BASE),
		.pfn		= __phys_to_pfn(FLI7610_TAE_ASC0_BASE),
		.length		= SZ_64K,
		.type		= MT_DEVICE,
	}, {
		.virtual	= IO_ADDRESS(FLI7610_PIO_TAE_SBC_BASE),
		.pfn		= __phys_to_pfn(FLI7610_PIO_TAE_SBC_BASE),
		.length		= SZ_64K,
		.type		= MT_DEVICE,
	}, {
		.virtual	= IO_ADDRESS(FLI7610_PIO_1_TAE_BASE),
		.pfn		= __phys_to_pfn(FLI7610_PIO_1_TAE_BASE),
		.length		= SZ_64K,
		.type		= MT_DEVICE,
	}, {
		.virtual	= IO_ADDRESS(FLI7610_PIO_2_TAE_BASE),
		.pfn		= __phys_to_pfn(FLI7610_PIO_2_TAE_BASE),
		.length		= SZ_64K,
		.type		= MT_DEVICE,

	}, {
		.virtual	= IO_ADDRESS(FLI7610_PIO_3_TAE_BASE),
		.pfn		= __phys_to_pfn(FLI7610_PIO_3_TAE_BASE),
		.length		= SZ_64K,
		.type		= MT_DEVICE,
	}, {
		.virtual	= IO_ADDRESS(FLI7610_PIO_MPE_RIGHT_BASE),
		.pfn		= __phys_to_pfn(FLI7610_PIO_MPE_RIGHT_BASE),
		.length		= SZ_64K,
		.type		= MT_DEVICE,
	}, {
		.virtual	= IO_ADDRESS(FLI7610_PIO_MPE_LEFT_BASE),
		.pfn		= __phys_to_pfn(FLI7610_PIO_MPE_LEFT_BASE),
		.length		= SZ_64K,
		.type		= MT_DEVICE,
	}, {
		.virtual	= IO_ADDRESS(FLI7610_SBC_SYSCONF_BASE),
		.pfn		= __phys_to_pfn(FLI7610_SBC_SYSCONF_BASE),
		.length		= SZ_4K,
		.type		= MT_DEVICE,
	}, {
		.virtual	= IO_ADDRESS(FLI7610_TAE_SYSCONF_BANK1_BASE),
		.pfn		= __phys_to_pfn(FLI7610_TAE_SYSCONF_BANK1_BASE),
		.length		= SZ_4K,
		.type		= MT_DEVICE,
	}, {
		.virtual	= IO_ADDRESS(FLI7610_TAE_SYSCONF_BANK2_BASE),
		.pfn		= __phys_to_pfn(FLI7610_TAE_SYSCONF_BANK2_BASE),
		.length		= SZ_4K,
		.type		= MT_DEVICE,

	}, {
		.virtual	= IO_ADDRESS(FLI7610_TAE_SYSCONF_BANK3_BASE),
		.pfn		= __phys_to_pfn(FLI7610_TAE_SYSCONF_BANK3_BASE),
		.length		= SZ_4K,
		.type		= MT_DEVICE,

	}, {
		.virtual	= IO_ADDRESS(FLI7610_TAE_SYSCONF_BANK4_BASE),
		.pfn		= __phys_to_pfn(FLI7610_TAE_SYSCONF_BANK4_BASE),
		.length		= SZ_4K,
		.type		= MT_DEVICE,
	}, {
		.virtual	= IO_ADDRESS(FLI7610_MPE_LEFT_SYSCONF_BASE),
		.pfn		= __phys_to_pfn(FLI7610_MPE_LEFT_SYSCONF_BASE),
		.length		= SZ_4K,
		.type		= MT_DEVICE,
	}, {
		.virtual	= IO_ADDRESS(FLI7610_MPE_RIGHT_SYSCONF_BASE),
		.pfn		= __phys_to_pfn(FLI7610_MPE_RIGHT_SYSCONF_BASE),
		.length		= SZ_4K,
		.type		= MT_DEVICE,
	}, {
		.virtual	= IO_ADDRESS(FLI7610_MPE_SYSTEM_SYSCONF_BASE),
		.pfn		= __phys_to_pfn(
					FLI7610_MPE_SYSTEM_SYSCONF_BASE),
		.length		= SZ_4K,
		.type		= MT_DEVICE,
	}, {
		.virtual	= IO_ADDRESS(FLI7610_SBC_LPM_CONF_BASE),
		.pfn		= __phys_to_pfn(FLI7610_SBC_LPM_CONF_BASE),
		.length		= SZ_4K,
		.type		= MT_DEVICE,
	}, {
		.virtual	= IO_ADDRESS(FLI7610_SBC_ASC0_BASE),
		.pfn		= __phys_to_pfn(FLI7610_SBC_ASC0_BASE),
		.length		= SZ_64K,
		.type		= MT_DEVICE,
	}
};

void __init fli7610_map_io(void)
{
	iotable_init(fli7610_io_desc, ARRAY_SIZE(fli7610_io_desc));
}

void __init fli7610_gic_init_irq(void)
{
	gic_init(0, 27, __io_address(FLI7610_GIC_DIST_BASE),
			__io_address(FLI7610_GIC_CPU_BASE));
}

static void __init fli7610_timer_init(void)
{

	plat_clk_init();
	plat_clk_alias_init();
#define A9CLK		800000000
#define PERIPHCLK	(A9CLK/2)

#ifdef CONFIG_HAVE_ARM_GT
	global_timer_init(__io_address(FLI7610_GLOBAL_TIMER_BASE),
			  IRQ_GLOBALTIMER, PERIPHCLK);
#endif

#ifdef CONFIG_HAVE_ARM_TWD
	twd_base = __io_address(FLI7610_TWD_BASE);
#endif
}

struct sys_timer fli7610_timer = {
	.init		= fli7610_timer_init,
};
