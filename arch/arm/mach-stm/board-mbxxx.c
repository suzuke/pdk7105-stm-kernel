/*
 * arch/arm/mach-stm/board-mbxxx.c
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
#include <linux/delay.h>
#include <linux/io.h>
#include <linux/gpio.h>
#include <linux/gpio_keys.h>
#include <linux/input.h>
#include <linux/stm/platform.h>
#include <linux/stm/nice.h>

#include <asm/mach-types.h>
#include <asm/memory.h>

#include <mach/soc-nice.h>
#include <mach/hardware.h>

int ok_to_print = 0;

static void __init mbxxx_map_io(void)
{

	nice_map_io();
	ok_to_print = 1;

	printk("STMicroelectronics MBxxx initialisation\n");

	nice_early_device_init();

	nice_configure_asc(0, &(struct nice_asc_config) {
			.routing.asc0 = nice_asc0_pio0,
			.hw_flow_control = 0,
			.is_console = 1, });
}

static void __init mbxxx_init(void)
{
}

MACHINE_START(STM_MBXXX, "STMicroelectronics MBxxx")
	.phys_io	= NICE_ASC0_BASE,
	.io_pg_offst	= (IO_ADDRESS(NICE_ASC0_BASE) >> 18) & 0xfffc,
	.boot_params	= PHYS_OFFSET + 0x00000100,
	.map_io		= mbxxx_map_io,
	.init_irq	= nice_gic_init_irq,
	.timer		= &nice_timer,
	.init_machine	= mbxxx_init,
MACHINE_END
