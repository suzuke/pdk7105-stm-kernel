/*
 * arch/arm/mach-stm/board-b2092.c
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
#include <linux/delay.h>
#include <linux/io.h>
#include <linux/gpio.h>
#include <linux/gpio_keys.h>
#include <linux/mdio-gpio.h>
#include <linux/input.h>
#include <linux/phy.h>
#include <linux/leds.h>
#include <linux/stm/platform.h>
#include <linux/stm/stih416.h>
#include <linux/mtd/nand.h>
#include <linux/mtd/partitions.h>

#include <asm/hardware/gic.h>
#include <asm/mach-types.h>
#include <asm/memory.h>
#include <asm/delay.h>

#include <mach/soc-stih416.h>
#include <mach/mpe42.h>
#include <mach/hardware.h>

/*
 * To use GPIO_POWER_ENABLE J33 has to be 1-2
 */
#define GPIO_POWER_ENABLE stih416_gpio(4, 3)
/*
 * To use GPIO_GMII0_PHY_NOT_RESET:
 * gpio[106][2]-> U57 -> PIO_GMII0_notRESET
 */
#define GPIO_GMII0_PHY_NOT_RESET stih416_gpio(106, 2)
/* To use GPIO_GMII1_PHY_NOT_RESET J39 has to be 1-2 */
#define GPIO_GMII1_PHY_NOT_RESET stih416_gpio(4, 7)

/*
 * In case of ETH Giga Mode the:
 * OSC_clk -> stih416.pll_and_GMac -> Pio_out (125MHz)
 */
#define GPIO_GMII0_PHY_CLKOUT_NOT_TXCLK_SEL	stih416_gpio(13, 4)
#define GPIO_GMII1_PHY_CLKOUT_NOT_TXCLK_SEL	stih416_gpio(2, 5)
/*
 * GPIO_FP_LED
 * gpio[105][7] -> U47 -> FP_LED
 */
#define GPIO_FP_LED		stih416_gpio(105, 7)

/* Serial FLASH */
static struct stm_plat_spifsm_data b2092_serial_flash =  {
	.name		= "n25q128",
	.nr_parts	= 2,
	.parts = (struct mtd_partition []) {
		{
			.name = "Serial Flash 1",
			.size = 0x00200000,
			.offset = 0,
		}, {
			.name = "Serial Flash 2",
			.size = MTDPART_SIZ_FULL,
			.offset = MTDPART_OFS_NXTBLK,
		},
	},
	.capabilities = {
		/* Capabilities may be overriden by SoC configuration */
		.dual_mode = 1,
		.quad_mode = 1,
	},
};


static struct platform_device b2092_leds = {
	.name = "leds-gpio",
	.id = -1,
	.dev.platform_data = &(struct gpio_led_platform_data) {
		.num_leds = 1,
		.leds = (struct gpio_led[]) {
			{
				.name = "FP_LED",
				.default_trigger = "heartbeat",
				.gpio = GPIO_FP_LED,
			}
		},
	},
};

static struct platform_device *b2092_devices[] __initdata = {
	&b2092_leds,
};


static inline void b2092_gmii_gpio_reset(int gpio)
{

	gpio_set_value(gpio, 1);
	gpio_set_value(gpio, 0);
	mdelay(10); /* 10 miliseconds is enough for everyone ;-) */
	gpio_set_value(gpio, 1);
	mdelay(10); /* 10 miliseconds is enough for everyone ;-) */
}

#if defined(CONFIG_MACH_STM_B2092_CN22_B2032) ||	\
	defined(CONFIG_MACH_STM_B2092_CN22_B2035)
static int b2092_gmii0_reset(void *priv)
{
	b2092_gmii_gpio_reset(GPIO_GMII0_PHY_NOT_RESET);
	return 1;
}

static struct stmmac_mdio_bus_data stmmac0_mdio_bus = {
	.phy_reset = b2092_gmii0_reset,
	.phy_mask = 0,
};
#endif /* CONFIG_MACH_STM_B2092_CN22_B2032 */

#ifdef CONFIG_MACH_STM_B2092_CN22_B2032_GIGA_MODE
static void b2092_gmac0_txclk_select(int txclk_125_not_25_mhz)
{
	gpio_set_value(GPIO_GMII0_PHY_CLKOUT_NOT_TXCLK_SEL,
		txclk_125_not_25_mhz ? 1 : 0);
}
#endif /* CONFIG_MACH_STM_B2092_CN22_B2032_GIGA_MODE */

#ifndef CONFIG_MACH_STM_B2092_CN23_NONE
static int b2092_gmii1_reset(void *data)
{
	b2092_gmii_gpio_reset(GPIO_GMII1_PHY_NOT_RESET);
	return 1;
}

static struct stmmac_mdio_bus_data stmmac1_mdio_bus = {
	/* GMII connector CN23 */
	.phy_reset = b2092_gmii1_reset,
	.phy_mask = 0,
};

#ifdef CONFIG_MACH_STM_B2092_CN23_B2032_GIGA_MODE
static void b2092_gmac1_txclk_select(int txclk_125_not_25_mhz)
{
	gpio_set_value(GPIO_GMII1_PHY_CLKOUT_NOT_TXCLK_SEL,
		txclk_125_not_25_mhz ? 1 : 0);
}
#endif /* CONFIG_MACH_STM_B2092_CN23_B2032_GIGA_MODE */
#endif /* CONFIG_MACH_STM_B2092_CN23_NONE */

static void __init b2092_init_early(void)
{
	pr_info("STMicroelectronics STiH416 (Orly-2) MBoard initialisation\n");

	stih416_early_device_init();

	stih416_configure_asc(STIH416_ASC(2), &(struct stih416_asc_config) {
			.hw_flow_control = 0,
			.is_console = 1 });
}

static void b2092_gmac0phy_gpio_init(int cold_boot)
{
#ifndef CONFIG_MACH_STM_B2092_CN22_NONE
	if (cold_boot)
		gpio_request(GPIO_GMII0_PHY_NOT_RESET, "gmii0_phy_not_reset");

	gpio_direction_output(GPIO_GMII0_PHY_NOT_RESET, 0);

	gpio_request(GPIO_GMII0_PHY_CLKOUT_NOT_TXCLK_SEL, "gmii0_txclk_sel");
#ifdef CONFIG_MACH_STM_B2092_CN22_B2032_GIGA_MODE
	gpio_direction_output(GPIO_GMII0_PHY_CLKOUT_NOT_TXCLK_SEL, 1);
#else
	gpio_direction_output(GPIO_GMII0_PHY_CLKOUT_NOT_TXCLK_SEL, 0);
#endif /* CONFIG_MACH_STM_B2092_CN22_B2032_GIGA_MODE */
	gpio_free(GPIO_GMII0_PHY_CLKOUT_NOT_TXCLK_SEL);

#ifdef CONFIG_MACH_STM_B2092_CN22_B2032
	b2092_gmii0_reset(NULL);
#endif /* CONFIG_MACH_STM_B2092_CN22_B2032 */
#endif /* !CONFIG_MACH_STM_B2092_CN22_NONE */
}

static void b2092_gmac1phy_gpio_init(int cold_boot)
{
#ifndef CONFIG_MACH_STM_B2092_CN23_NONE
	if (cold_boot)
		gpio_request(GPIO_GMII1_PHY_NOT_RESET, "gmii1_phy_not_reset");

	gpio_direction_output(GPIO_GMII1_PHY_NOT_RESET, 0);

	gpio_request(GPIO_GMII1_PHY_CLKOUT_NOT_TXCLK_SEL, "gmii1_txclk_sel");
#ifdef CONFIG_MACH_STM_B2092_CN23_B2032_GIGA_MODE
	gpio_direction_output(GPIO_GMII1_PHY_CLKOUT_NOT_TXCLK_SEL, 1);
#else
	gpio_direction_output(GPIO_GMII1_PHY_CLKOUT_NOT_TXCLK_SEL, 0);
#endif /* CONFIG_MACH_STM_B2092_CN23_B2032_GIGA_MODE */
	gpio_free(GPIO_GMII1_PHY_CLKOUT_NOT_TXCLK_SEL);

#ifdef CONFIG_MACH_STM_B2092_CN23_B2032
	b2092_gmii1_reset(NULL);
#endif /* CONFIG_MACH_STM_B2092_CN23_B2032 */
#endif /* !CONFIG_MACH_STM_B2092_CN23_NONE */
}

static void __init b2092_init(void)
{
	/* gpio-power on board power supplies */
	gpio_request(GPIO_POWER_ENABLE, "gpio_power_enable");
	gpio_direction_output(GPIO_POWER_ENABLE, 1);

	/* Reset GMAC 0/1 PHY attached devices */
	b2092_gmac0phy_gpio_init(1);
	b2092_gmac1phy_gpio_init(1);

#if defined(CONFIG_MACH_STM_B2092_CN22_B2032) ||	\
	defined(CONFIG_MACH_STM_B2092_CN22_B2035)
	stih416_configure_ethernet(0, &(struct stih416_ethernet_config) {
#ifdef CONFIG_MACH_STM_B2092_CN22_B2035
	.interface = PHY_INTERFACE_MODE_RMII,
		.ext_clk = 0,
		.phy_addr = 9,
#endif /* CONFIG_MACH_STM_B2092_CN22_B2035 */

#ifdef CONFIG_MACH_STM_B2092_CN22_B2032
		/* B2032 modified to support GMII */
#ifdef CONFIG_MACH_STM_B2092_CN22_B2032_GIGA_MODE
		.txclk_select = b2092_gmac0_txclk_select,
#ifdef CONFIG_MACH_STM_B2000_CN22_B2092_GMII_MODE
		.interface = PHY_INTERFACE_MODE_GMII,
#else
		.interface = PHY_INTERFACE_MODE_RGMII_ID,
#endif /* CONFIG_MACH_STM_B2092_CN22_B2032_GMII_MODE */
#else
		.interface = PHY_INTERFACE_MODE_MII,
#endif /* CONFIG_MACH_STM_B2092_CN22_B2032_GIGA_MODE */
		.ext_clk = 1,
		.phy_addr = 1,
#endif /* CONFIG_MACH_STM_B2092_CN22_B2032 */

		.phy_bus = 1,
		.mdio_bus_data = &stmmac0_mdio_bus,});
#endif /* CONFIG_MACH_STM_B2092_CN22_B2032 || CONFIG_MACH_STM_B2092_CN22_B2035*/

/* GMAC1 */
#ifndef CONFIG_MACH_STM_B2092_CN23_NONE
	stih416_configure_ethernet(1, &(struct stih416_ethernet_config) {
#ifdef CONFIG_MACH_STM_B2092_CN23_B2035
		.interface = PHY_INTERFACE_MODE_RMII,
		.ext_clk = 0,
		.phy_addr = 9,
#endif /* CONFIG_MACH_STM_B2092_CN23_B2035 */

/* RGMII on GMAC1 is not validated yet */
#ifdef CONFIG_MACH_STM_B2092_CN23_B2032
#ifdef CONFIG_MACH_STM_B2092_CN23_B2032_GIGA_MODE
		.txclk_select = b2092_gmac1_txclk_select,
#ifdef CONFIG_MACH_STM_B2092_CN23_B2032_GMII_MODE
		.interface = PHY_INTERFACE_MODE_GMII,
#else
		.interface = PHY_INTERFACE_MODE_RGMII_ID,
#endif /* CONFIG_MACH_STM_B2092_CN23_B2032_GMII_MODE */
#else
		.interface = PHY_INTERFACE_MODE_MII,
#endif /* CONFIG_MACH_STM_B2092_CN23_B2032 */
		.ext_clk = 1,
		.phy_addr = 1,
#endif /* CONFIG_MACH_STM_B2092_CN23_B2032 */
		.phy_bus = 1,
		.mdio_bus_data = &stmmac1_mdio_bus,});
#endif /* CONFIG_MACH_STM_B2092_CN23_NONE */

	stih416_configure_usb(0);
	stih416_configure_usb(1);
	stih416_configure_usb(2);
	stih416_configure_usb(3);

#if defined(CONFIG_MACH_STM_B2092_CN22_B2035)	  ||	\
	defined(CONFIG_MACH_STM_B2092_CN22_B2048) ||	\
	defined(CONFIG_MACH_STM_B2092_CN22_B2107)

#ifdef CONFIG_MACH_STM_B2092_B2107_DUAL_EMMC
	/* Dual eMMC on board*/
	stih416_configure_mmc(0, 1);
	stih416_configure_mmc(1, 1);
#endif
#ifdef CONFIG_MACH_STM_B2092_CN22_B2048
	/* eMMC on board */
	stih416_configure_mmc(0, 1);
#endif
#ifdef CONFIG_MACH_STM_B2092_CN22_B2035
	/*
	 * In case of B2035 or B2048A without eMMC, we actually have the
	 * MMC/SD slot on the daughter board.
	 */
	stih416_configure_mmc(0, 0);
#endif
#endif

	 /* HDMI */
	stih416_configure_ssc_i2c(STIH416_SSC(1),
		&(struct stih416_ssc_config) {.i2c_speed = 100,});
	/* Frontend I2C, make sure J17-J20 are configured accordingly */
	stih416_configure_ssc_i2c(STIH416_SSC(3),
		&(struct stih416_ssc_config) {.i2c_speed = 100,});
	/* Backend I2C, make sure J50, J51 are configured accordingly */
	stih416_configure_ssc_i2c(STIH416_SBC_SSC(0),
		&(struct stih416_ssc_config) {.i2c_speed = 100,});

	stih416_configure_spifsm(&b2092_serial_flash);

	stih416_configure_keyscan(&(struct stm_keyscan_config) {
		.num_out_pads = 4,
		.num_in_pads = 4,
		.debounce_us = 5000,
		.keycodes = {
			/* in0 ,   in1  ,   in2 ,  in3  */
			KEY_F13, KEY_F9,  KEY_F5, KEY_F1, /* out0 */
			KEY_F14, KEY_F10, KEY_F6, KEY_F2, /* out1 */
			KEY_F15, KEY_F11, KEY_F7, KEY_F3, /* out2 */
			KEY_F16, KEY_F12, KEY_F8, KEY_F4  /* out3 */
			},
		});

	platform_add_devices(b2092_devices,
		ARRAY_SIZE(b2092_devices));

	return;
}

MACHINE_START(STM_B2092, "STMicroelectronics B2092 - STiH416 MBoard")
	.atag_offset	= 0x100,
	.map_io		= stih416_map_io,
#ifdef CONFIG_SPARSE_IRQ
	.nr_irqs	= NR_IRQS_LEGACY,
#endif
	.init_early	= b2092_init_early,
	.init_irq	= mpe42_gic_init_irq,
	.timer		= &mpe42_timer,
	.handle_irq	= gic_handle_irq,
	.init_machine	= b2092_init,
	.restart	= stih416_reset,
MACHINE_END

#ifdef CONFIG_HIBERNATION_ON_MEMORY

#include <linux/stm/hom.h>

static int b2092_hom_restore(struct stm_wakeup_devices *dev_wk)
{
	b2092_gmac0phy_gpio_init(0);
	b2092_gmac1phy_gpio_init(0);

	return 0;
}

static struct stm_hom_board b2092_hom = {
	.restore = b2092_hom_restore,
};

static int __init b2092_hom_init(void)
{
	return stm_hom_stxh416_setup(&b2092_hom);
}

module_init(b2092_hom_init);
#endif
