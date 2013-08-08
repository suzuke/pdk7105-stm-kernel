/*
 * Copyright (C) 2013 STMicroelectronics Limited.
 *
 * Author: Francesco Virlinzi <francesco.virlinzi@st.com>
 * Author: Giuseppe Cavallaro <pepp.cavallaro@st.com>
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

#include <mach/soc-stid127.h>
#include <mach/mpe42.h>
#include <mach/hardware.h>
#include <mach/irqs.h>

#include "core.h"

static struct map_desc stid127_io_desc[] __initdata = {
	{
		.virtual	= IO_ADDRESS(STID127_SCU_BASE),
		.pfn		= __phys_to_pfn(STID127_SCU_BASE),
		.length		= SZ_4K,
		.type		= MT_DEVICE,
	}, {
		.virtual	= IO_ADDRESS(STID127_GIC_DIST_BASE),
		.pfn		= __phys_to_pfn(STID127_GIC_DIST_BASE),
		.length		= SZ_4K,
		.type		= MT_DEVICE,
	}, {
		.virtual        = IO_ADDRESS(STID127_PL310_BASE),
		.pfn            = __phys_to_pfn(STID127_PL310_BASE),
		.length         = SZ_16K,
		.type           = MT_DEVICE,
	}, {
		.virtual	= IO_ADDRESS(STID127_ASC0_BASE),
		.pfn		= __phys_to_pfn(STID127_ASC0_BASE),
		.length		= SZ_8K,
		.type		= MT_DEVICE,
	}, {
		.virtual	= IO_ADDRESS(STID127_ASC2_BASE),
		.pfn		= __phys_to_pfn(STID127_ASC2_BASE),
		.length		= SZ_8K,
		.type		= MT_DEVICE,
	}, {
		.virtual	= IO_ADDRESS(STID127_PIO_PEAST0_BASE),
		.pfn		= __phys_to_pfn(STID127_PIO_PEAST0_BASE),
		.length		= SZ_64K,
		.type		= MT_DEVICE,
	}, {
		.virtual	= IO_ADDRESS(STID127_PIO_WEST0_BASE),
		.pfn		= __phys_to_pfn(STID127_PIO_WEST0_BASE),
		.length		= SZ_64K,
		.type		= MT_DEVICE,
	}, {
		.virtual	= IO_ADDRESS(STID127_PIO_SOUTH0_BASE),
		.pfn		= __phys_to_pfn(STID127_PIO_SOUTH0_BASE),
		.length		= SZ_64K,
		.type		= MT_DEVICE,
	}, {
		.virtual	= IO_ADDRESS(STID127_SYSCONF_HD_BASE),
		.pfn		= __phys_to_pfn(STID127_SYSCONF_HD_BASE),
		.length		= SZ_4K,
		.type		= MT_DEVICE,
	}, {
		.virtual	= IO_ADDRESS(STID127_SYSCONF_CPU_BASE),
		.pfn		= __phys_to_pfn(STID127_SYSCONF_CPU_BASE),
		.length		= SZ_4K,
		.type		= MT_DEVICE,
	}, {
		.virtual	= IO_ADDRESS(STID127_SYSCONF_WEST_BASE),
		.pfn		= __phys_to_pfn(STID127_SYSCONF_WEST_BASE),
		.length		= SZ_4K,
		.type		= MT_DEVICE,
	}, {
		.virtual	= IO_ADDRESS(STID127_SYSCONF_DOCSIS_BASE),
		.pfn		= __phys_to_pfn(STID127_SYSCONF_DOCSIS_BASE),
		.length		= SZ_4K,
		.type		= MT_DEVICE,
	}, {
		.virtual	= IO_ADDRESS(STID127_SYSCONF_SOUTH_BASE),
		.pfn		= __phys_to_pfn(STID127_SYSCONF_SOUTH_BASE),
		.length		= SZ_4K,
		.type		= MT_DEVICE,
	},
	{
		.virtual	= IO_ADDRESS(STID127_SYSCONF_PWEST_BASE),
		.pfn		= __phys_to_pfn(STID127_SYSCONF_PWEST_BASE),
		.length		= SZ_4K,
		.type		= MT_DEVICE,
	}, {
		.virtual	= IO_ADDRESS(STID127_SYSCONF_PSOUTH_BASE),
		.pfn		= __phys_to_pfn(STID127_SYSCONF_PSOUTH_BASE),
		.length		= SZ_4K,
		.type		= MT_DEVICE,
	}, {
		.virtual	= IO_ADDRESS(STID127_SYSCONF_PEAST_BASE),
		.pfn		= __phys_to_pfn(STID127_SYSCONF_PEAST_BASE),
		.length		= SZ_4K,
		.type		= MT_DEVICE,
	}
};

void __init stid127_map_io(void)
{
	iotable_init(stid127_io_desc, ARRAY_SIZE(stid127_io_desc));
#ifdef CONFIG_SMP
	scu_base_addr = ((void __iomem *) IO_ADDRESS(STID127_SCU_BASE));
#endif
}
