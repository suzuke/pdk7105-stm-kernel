/*
 * arch/arm/mach-stm/board-b2000.c
 *
 * Copyright (C) 2011 STMicroelectronics Limited.
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
#include <linux/stm/stih415.h>

#include <asm/mach-types.h>
#include <asm/memory.h>

#include <mach/soc-stih415.h>
#include <mach/hardware.h>

int ok_to_print = 0;

static void __init stih415_veloce_map_io(void)
{

	stih415_map_io();
	ok_to_print = 1;

	printk("STMicroelectronics STiH415 (Orly) MBoard initialisation\n");

	stih415_early_device_init();

	stih415_configure_asc(2, &(struct stih415_asc_config) {
			.hw_flow_control = 1,
			.is_console = 1 });
}

static void __init b2000_init(void)
{
}

MACHINE_START(STM_B2000, "STMicroelectronics B2000 - STiH415 MBoard")
	.phys_io	= STIH415_ASC0_BASE,
	.io_pg_offst	= (IO_ADDRESS(STIH415_ASC0_BASE) >> 18) & 0xfffc,
	.boot_params	= PHYS_OFFSET + 0x00000100,
	.map_io		= stih415_veloce_map_io,
	.init_irq	= stih415_gic_init_irq,
	.timer		= &stih415_timer,
	.init_machine	= b2000_init,
MACHINE_END
