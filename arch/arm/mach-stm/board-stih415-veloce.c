/*
 * arch/arm/mach-stm/board-stih415-veloce.c
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
#include <linux/stm/nice.h>

#include <asm/mach-types.h>
#include <asm/memory.h>

#include <mach/soc-nice.h>
#include <mach/hardware.h>

int ok_to_print = 0;

static void __init stih415_veloce_map_io(void)
{

	nice_map_io();
	ok_to_print = 1;

	printk("STMicroelectronics STiH415 (Orly) Veloce initialisation\n");

	nice_early_device_init();

	nice_configure_asc(4, &(struct nice_asc_config) {
			.routing.asc0 = nice_asc0_pio0,
			.hw_flow_control = 0,
			.is_console = 1,
			.force_m1 = 1, });
}

static void __init stih415_veloce_init(void)
{
}

MACHINE_START(STIH415_VELOCE, "STMicroelectronics STiH415 Veloce")
	.phys_io	= NICE_SBC_ASC0_BASE,
	.io_pg_offst	= (IO_ADDRESS(NICE_SBC_ASC0_BASE) >> 18) & 0xfffc,
	.boot_params	= PHYS_OFFSET + 0x00000100,
	.map_io		= stih415_veloce_map_io,
	.init_irq	= nice_gic_init_irq,
	.timer		= &nice_timer,
	.init_machine	= stih415_veloce_init,
MACHINE_END
