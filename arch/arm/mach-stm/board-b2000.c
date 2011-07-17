/*
 * arch/arm/mach-stm/board-b2000.c
 *
 * Copyright (C) 2011 STMicroelectronics Limited.
 * Author: Stuart Menefy <stuart.menefy@st.com>
 * 	   Srinivas Kandagatla <srinivas.kandagatla@st.com>
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
#include <linux/phy.h>
#include <linux/leds.h>
#include <linux/stm/platform.h>
#include <linux/stm/stih415.h>

#include <asm/mach-types.h>
#include <asm/memory.h>
#include <asm/delay.h>

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

#define GMII0_PHY_NOT_RESET stm_gpio(25 /*106 -100 + 18 */, 2)
#define GMII1_PHY_NOT_RESET stm_gpio(4, 7)
#define GMII1_PHY_CLKOUT_NOT_TXCLK_SEL stm_gpio(2, 5)
#define GMII0_PHY_CLKOUT_NOT_TXCLK_SEL stm_gpio(13, 4)


static int b2000_gmii0_reset(void *bus)
{
	gpio_set_value(GMII0_PHY_NOT_RESET, 1);
	gpio_set_value(GMII0_PHY_NOT_RESET, 0);
	udelay(1000); /* 10 miliseconds is enough for everyone ;-) */
	gpio_set_value(GMII0_PHY_NOT_RESET, 1);
	udelay(1000); /* 10 miliseconds is enough for everyone ;-) */

	return 1;
}

static int b2000_gmii1_reset(void *bus)
{
	gpio_set_value(GMII1_PHY_NOT_RESET, 1);
	gpio_set_value(GMII1_PHY_NOT_RESET, 0);
	udelay(1000); /* 10 miliseconds is enough for everyone ;-) */
	gpio_set_value(GMII1_PHY_NOT_RESET, 1);
	udelay(1000); /* 10 miliseconds is enough for everyone ;-) */

	return 1;
}


#ifdef CONFIG_SH_ST_B2000_GMAC1_GMII_MODE
static void b2000_gmac1_txclk_select(int txclk_250_not_25_mhz)
{
	/* When 1000 speed is negotiated we have to set the PIO2[5]. */
	if (txclk_250_not_25_mhz)
		gpio_set_value(GMII1_PHY_CLKOUT_NOT_TXCLK_SEL, 1);
	else
		gpio_set_value(GMII1_PHY_CLKOUT_NOT_TXCLK_SEL, 0);
}
#endif
#ifdef CONFIG_SH_ST_B2000_GMAC0_GMII_MODE
static void b2000_gmac0_txclk_select(int txclk_250_not_25_mhz)
{
	/* When 1000 speed is negotiated we have to set the PIO2[5]. */
	if (txclk_250_not_25_mhz)
		gpio_set_value(GMII0_PHY_CLKOUT_NOT_TXCLK_SEL, 1);
	else
		gpio_set_value(GMII0_PHY_CLKOUT_NOT_TXCLK_SEL, 0);
}
#endif
static struct platform_device b2000_phy_devices[] = {
	{
		/* GMII connector CN22 */
		.name = "stmmacphy",
		.id = 0,
		.dev.platform_data = &(struct plat_stmmacphy_data) {
			.bus_id = 0,
			.phy_addr = -1,
			.phy_mask = 0,
			.interface = PHY_INTERFACE_MODE_GMII,
			.phy_reset = &b2000_gmii0_reset,
		},
	}, {
		/* GMII connector CN23 */
		.name = "stmmacphy",
		.id = 1,
		.dev.platform_data = &(struct plat_stmmacphy_data) {
			.bus_id = 1,
			.phy_addr = -1,
			.phy_mask = 0,
			.interface = PHY_INTERFACE_MODE_GMII,
			.phy_reset = &b2000_gmii1_reset,
		},
	},
};

static struct platform_device b2000_leds = {
	.name = "leds-gpio",
	.id = -1,
	.dev.platform_data = &(struct gpio_led_platform_data) {
		.num_leds = 1,
		.leds = (struct gpio_led[]) {
			{
				.name = "FP_LED",
				.default_trigger = "heartbeat",
				.gpio = stm_gpio(STIH415_PIO(105), 7),
			}
		},
	},
};

static struct platform_device *b2000_devices[] __initdata = {
	&b2000_leds,
};
static void __init b2000_init(void)
{
	/* Reset */
	gpio_request(GMII0_PHY_NOT_RESET, "GMII0_PHY_NOT_RESET");
	gpio_direction_output(GMII0_PHY_NOT_RESET, 0);

	gpio_request(GMII1_PHY_NOT_RESET, "GMII1_PHY_NOT_RESET");
	gpio_direction_output(GMII1_PHY_NOT_RESET, 0);

	/* Default to 100 Mbps */
	gpio_request(GMII1_PHY_CLKOUT_NOT_TXCLK_SEL, "GMII1_TXCLK_SEL");
	gpio_direction_output(GMII1_PHY_CLKOUT_NOT_TXCLK_SEL, 0);

	/* Default to 100 Mbps */
	gpio_request(GMII0_PHY_CLKOUT_NOT_TXCLK_SEL, "GMII0_TXCLK_SEL");
	gpio_direction_output(GMII0_PHY_CLKOUT_NOT_TXCLK_SEL, 0);

/* GMII Mode on B2032A needs R26 to be fitted with 51R */
	stih415_configure_ethernet(0, &(struct stih415_ethernet_config) {
#ifndef CONFIG_SH_ST_B2000_GMAC0_GMII_MODE
			.mode = stih415_ethernet_mode_mii,
#else
			.mode = stih415_ethernet_mode_gmii_gtx,
			.txclk_select = b2000_gmac0_txclk_select,
#endif
			.ext_clk = 1,
			.phy_bus = 0, });

	stih415_configure_ethernet(1, &(struct stih415_ethernet_config) {
#ifndef CONFIG_SH_ST_B2000_GMAC0_GMII_MODE
			.mode = stih415_ethernet_mode_mii,
#else
			.mode = stih415_ethernet_mode_gmii_gtx,
			.txclk_select = b2000_gmac1_txclk_select,
#endif
			.ext_clk = 1,
			.phy_bus = 1, });

	stih415_configure_usb(0);
	stih415_configure_usb(1);
	stih415_configure_usb(2);

	platform_add_devices(b2000_devices,
		ARRAY_SIZE(b2000_devices));
	return;
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
