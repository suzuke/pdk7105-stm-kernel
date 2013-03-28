/*
 * arch/arm/mach-stm/soc-stih416.c
 *
 * Copyright (C) 2012 STMicroelectronics Limited.
 * Author: Francesco Virlinzi <francesco.virlinzi@st.com>
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

#include <linux/stm/sasg2-periphs.h>

#include <asm/irq.h>
#include <asm/leds.h>
#include <asm/mach-types.h>
#include <asm/smp_twd.h>
#include <asm/smp_gt.h>
#include <asm/hardware/gic.h>

#include <asm/mach/map.h>
#include <asm/mach/time.h>

#include <mach/soc-stih416.h>
#include <mach/mpe42.h>
#include <mach/hardware.h>
#include <mach/irqs.h>

#include "core.h"

static struct map_desc stih416_io_desc[] __initdata = {
	{
		.virtual	= IO_ADDRESS(SASG2_ASC0_BASE),
		.pfn		= __phys_to_pfn(SASG2_ASC0_BASE),
		.length		= SZ_64K,
		.type		= MT_DEVICE,
	}, {
		.virtual	= IO_ADDRESS(SASG2_SBC_PIO_BASE),
		.pfn		= __phys_to_pfn(SASG2_SBC_PIO_BASE),
		.length		= SZ_64K,
		.type		= MT_DEVICE,
	}, {
		.virtual	= IO_ADDRESS(SASG2_PIO_REAR_BASE),
		.pfn		= __phys_to_pfn(SASG2_PIO_REAR_BASE),
		.length		= SZ_64K,
		.type		= MT_DEVICE,
	}, {
		.virtual	= IO_ADDRESS(SASG2_PIO_FRONT_BASE),
		.pfn		= __phys_to_pfn(SASG2_PIO_FRONT_BASE),
		.length		= SZ_64K,
		.type		= MT_DEVICE,
	}, {
		.virtual	= IO_ADDRESS(SASG2_SBC_SYSCONF_BASE),
		.pfn		= __phys_to_pfn(SASG2_SBC_SYSCONF_BASE),
		.length		= SZ_4K,
		.type		= MT_DEVICE,
	}, {
		.virtual	= IO_ADDRESS(SASG2_FRONT_SYSCONF_BASE),
		.pfn		= __phys_to_pfn(SASG2_FRONT_SYSCONF_BASE),
		.length		= SZ_4K,
		.type		= MT_DEVICE,
	}, {
		.virtual	= IO_ADDRESS(SASG2_REAR_SYSCONF_BASE),
		.pfn		= __phys_to_pfn(SASG2_REAR_SYSCONF_BASE),
		.length		= SZ_4K,
		.type		= MT_DEVICE,
	}, {
		.virtual	= IO_ADDRESS(SASG2_SBC_LPM_CONF_BASE),
		.pfn		= __phys_to_pfn(SASG2_SBC_LPM_CONF_BASE),
		.length		= SZ_4K,
		.type		= MT_DEVICE,
	}, {
		.virtual	= IO_ADDRESS(SASG2_SBC_ASC0_BASE),
		.pfn		= __phys_to_pfn(SASG2_SBC_ASC0_BASE),
		.length		= SZ_64K,
		.type		= MT_DEVICE,
	}
};

void __init stih416_map_io(void)
{
	iotable_init(stih416_io_desc, ARRAY_SIZE(stih416_io_desc));
	mpe42_map_io();
#ifdef CONFIG_SMP
	scu_base_addr = ((void __iomem *) IO_ADDRESS(MPE42_SCU_BASE));
#endif
}
