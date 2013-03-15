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
#include <linux/stm/stih415-periphs.h>


#include <asm/irq.h>
#include <asm/leds.h>
#include <asm/mach-types.h>

#include <asm/mach/map.h>
#include <asm/mach/time.h>

#include <mach/mpe41.h>
#include <mach/soc-stih415.h>
#include <mach/hardware.h>
#include <mach/irqs.h>

#include "core.h"

static struct map_desc stih415_io_desc[] __initdata = {
	{
		.virtual	= IO_ADDRESS(MPE41_SCU_BASE),
		.pfn		= __phys_to_pfn(MPE41_SCU_BASE),
		.length		= SZ_4K,
		.type		= MT_DEVICE,
	}, {
		.virtual	= IO_ADDRESS(MPE41_GIC_DIST_BASE),
		.pfn		= __phys_to_pfn(MPE41_GIC_DIST_BASE),
		.length		= SZ_4K,
		.type		= MT_DEVICE,
	}, {
		.virtual        = IO_ADDRESS(MPE41_PL310_BASE),
		.pfn            = __phys_to_pfn(MPE41_PL310_BASE),
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
		.virtual	= IO_ADDRESS(MPE41_PIO_RIGHT_BASE),
		.pfn		= __phys_to_pfn(MPE41_PIO_RIGHT_BASE),
		.length		= SZ_64K,
		.type		= MT_DEVICE,
	}, {
		.virtual	= IO_ADDRESS(MPE41_PIO_LEFT_BASE),
		.pfn		= __phys_to_pfn(MPE41_PIO_LEFT_BASE),
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
		.virtual	= IO_ADDRESS(MPE41_LEFT_SYSCONF_BASE),
		.pfn		= __phys_to_pfn(MPE41_LEFT_SYSCONF_BASE),
		.length		= SZ_4K,
		.type		= MT_DEVICE,
	}, {
		.virtual	= IO_ADDRESS(MPE41_RIGHT_SYSCONF_BASE),
		.pfn		= __phys_to_pfn(MPE41_RIGHT_SYSCONF_BASE),
		.length		= SZ_4K,
		.type		= MT_DEVICE,
	}, {
		.virtual	= IO_ADDRESS(MPE41_SYSTEM_SYSCONF_BASE),
		.pfn		= __phys_to_pfn(MPE41_SYSTEM_SYSCONF_BASE),
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
#ifdef CONFIG_SMP
	scu_base_addr = ((void __iomem *) IO_ADDRESS(MPE41_SCU_BASE));
#endif
	iotable_init(stih415_io_desc, ARRAY_SIZE(stih415_io_desc));
}
