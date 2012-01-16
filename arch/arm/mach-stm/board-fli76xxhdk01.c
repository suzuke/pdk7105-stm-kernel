/*
 * arch/arm/mach-stm/board-fli76xxhdk01.c
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
#include <linux/delay.h>
#include <linux/io.h>
#include <linux/gpio.h>
#include <linux/gpio_keys.h>
#include <linux/input.h>
#include <linux/stm/platform.h>
#include <linux/stm/fli7610.h>

#include <asm/mach-types.h>
#include <asm/memory.h>

#include <mach/soc-fli7610.h>
#include <mach/hardware.h>
#define ZB_SEL stm_gpio(3, 0)

#define CONSOLE_PORT	2

static void __init fli76xxhdk01_map_io(void)
{
	fli7610_map_io();
}

static void __init fli76xxhdk01_init_early(void)
{

	printk("STMicroelectronics FLI76XXHDK01 (Newman) initialisation\n");

	fli7610_early_device_init();
	gpio_request(ZB_SEL, "ZB_SEL");
	gpio_direction_output(ZB_SEL, 0);

	fli7610_configure_asc(CONSOLE_PORT, &(struct fli7610_asc_config) {
			.hw_flow_control = 1,
			.is_console = 1	 });
}

static void __init fli76xxhdk01_init(void)
{
	fli7610_configure_usb(0);
	fli7610_configure_usb(1);
	fli7610_configure_usb(2);

	fli7610_configure_ssc_i2c(FLI7610_SSC(0));
	fli7610_configure_ssc_i2c(FLI7610_SSC(1));
	/* SBC I2C */
	fli7610_configure_ssc_i2c(FLI7610_SBC_SSC(0));

	fli7610_configure_lirc();

	fli7610_configure_pwm(&(struct fli7610_pwm_config) {
			.pwm = fli7610_sbc_pwm,
			.enabled[0] = 1,
			.enabled[1] = 1,
			.enabled[2] = 1, });

}

MACHINE_START(STM_NMHDK_FLI7610, "STMicroelectronics Newman FLI76XXHDK01")
	.boot_params	= PHYS_OFFSET + 0x00000100,
	.map_io		= fli76xxhdk01_map_io,
	.init_irq	= fli7610_gic_init_irq,
	.timer		= &fli7610_timer,
	.init_machine	= fli76xxhdk01_init,
	.init_early     = fli76xxhdk01_init_early,
MACHINE_END
