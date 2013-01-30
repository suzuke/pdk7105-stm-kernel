/*
 * arch/arm/mach-stm/board-b2020_h416.c
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

static void __init b2020_init_early(void)
{
	pr_info("STMicroelectronics STiH416 (Orly-2) B2020 ADI initialisation\n");

	stih416_early_device_init();
	/* for UART11 (aka SBC_ASC_1) via J26 use device 6 for console
	 * for UART10 (aka SBC_ASC_0) via J35 use device 5 for console
	 */
	stih416_configure_asc(STIH416_SBC_ASC(1), &(struct stih416_asc_config) {
			.hw_flow_control = 0,
			.is_console = 1 });
}

#define B2020_GMII1_PHY_NOT_RESET stih416_gpio(3, 0)

static struct stm_pad_config stih416_hdmi_hp_pad_config = {
	.gpios_num = 1,
	.gpios = (struct stm_pad_gpio []) {
		STIH416_PAD_PIO_IN(2, 5, 1),	/* HDMI Hotplug */
	},
};
/* NAND Flash */
static struct stm_nand_bank_data b2020_nand_flash = {
	.csn		= 0,
	.options	= NAND_NO_AUTOINCR,
	.bbt_options	= NAND_BBT_USE_FLASH,
	.nr_partitions	= 2,
	.partitions	= (struct mtd_partition []) {
		{
			.name	= "NAND Flash 1",
			.offset = 0,
			.size	= 0x00800000
		}, {
			.name	= "NAND Flash 2",
			.offset = MTDPART_OFS_NXTBLK,
			.size	= MTDPART_SIZ_FULL
		},
	},
	.timing_data = &(struct stm_nand_timing_data) {
		.sig_setup	= 10,		/* times in ns */
		.sig_hold	= 10,
		.CE_deassert	= 0,
		.WE_to_RBn	= 100,
		.wr_on		= 10,
		.wr_off		= 30,
		.rd_on		= 10,
		.rd_off		= 30,
		.chip_delay	= 30,		/* in us */
	},
};

/* Serial FLASH */
static struct stm_plat_spifsm_data b2020_serial_flash = {
	.name		= "s25fl128s1",
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

static int b2020_gmii1_reset(void *bus)
{

	gpio_set_value(B2020_GMII1_PHY_NOT_RESET, 1);
	gpio_set_value(B2020_GMII1_PHY_NOT_RESET, 0);
	mdelay(10); /* 10 miliseconds is enough for everyone ;-) */
	gpio_set_value(B2020_GMII1_PHY_NOT_RESET, 1);
	mdelay(10); /* 10 miliseconds is enough for everyone ;-) */

	return 1;
}

static struct stmmac_mdio_bus_data stmmac1_mdio_bus = {
	.phy_reset = &b2020_gmii1_reset,
	.phy_mask = 0,
};

static struct platform_device b2020_leds = {
	.name = "leds-gpio",
	.id = -1,
	.dev.platform_data = &(struct gpio_led_platform_data) {
		.num_leds = 1,
		.leds = (struct gpio_led[]) {
			{
				.name = "FP_LED",
				.default_trigger = "heartbeat",
				.gpio = stih416_gpio(4, 1),
			}
		},
	},
};

static struct platform_device *b2020_devices[] __initdata = {
	&b2020_leds,
};


static void b2020_ethphy_gpio_init(int cold_boot)
{
	/* Reset */
	if (cold_boot) {
		gpio_request(B2020_GMII1_PHY_NOT_RESET,
			     "B2020_GMII1_PHY_NOT_RESET");
	}


	gpio_direction_output(B2020_GMII1_PHY_NOT_RESET, 0);

	b2020_gmii1_reset(NULL);
}

static void __init b2020_init(void)
{
	b2020_ethphy_gpio_init(1);

	/* Default to HDMI HotPlug */
	if (stm_pad_claim(&stih416_hdmi_hp_pad_config, "HDMI_Hotplug") == NULL)
		pr_err("Failed to claim HDMI-Hotplug pad!\n");

	stih416_configure_ethernet(1, &(struct stih416_ethernet_config) {
			.interface = PHY_INTERFACE_MODE_RGMII_ID,
			.ext_clk = 1,
			.phy_addr = 1,
			.phy_bus = 1,
			.mdio_bus_data = &stmmac1_mdio_bus,});

	stih416_configure_usb(0);
	stih416_configure_usb(1);
	stih416_configure_usb(2);
	stih416_configure_usb(3);

	/* HDMI */
	stih416_configure_ssc_i2c(STIH416_SSC(1),
			&(struct stih416_ssc_config) {.i2c_speed = 100,});

	/* Frontend I2C, make sure J17-J20 are configured accordingly */
	stih416_configure_ssc_i2c(STIH416_SSC(3),
			&(struct stih416_ssc_config) {.i2c_speed = 100,});

	/* Backend I2C, make sure J50, J51 are configured accordingly */
	stih416_configure_ssc_i2c(STIH416_SBC_SSC(0),
			&(struct stih416_ssc_config) {.i2c_speed = 100,});

	/* IR_IN */
	stih416_configure_lirc(&(struct stih416_lirc_config) {
			.rx_mode = stih416_lirc_rx_mode_ir, });

	stih416_configure_pwm(&(struct stih416_pwm_config) {
			.pwm = stih416_sbc_pwm,
			.out0_enabled = 1, });

	stih416_configure_mmc(0, 0);
	stih416_configure_mmc(1, 1);

	stih416_configure_nand(&(struct stm_nand_config) {
			.driver = stm_nand_bch,
			.nr_banks = 1,
			.banks = &b2020_nand_flash,
			.rbn.flex_connected = 1,});

	stih416_configure_spifsm(&b2020_serial_flash);

	stih416_configure_audio(&(struct stih416_audio_config) {
			.uni_player_3_spdif_enabled = 1, });

	platform_add_devices(b2020_devices,
		ARRAY_SIZE(b2020_devices));

	/* 1 SATA + 1 PCIe */
	stih416_configure_miphy(&(struct stih416_miphy_config) {
		.id = 0,
		.mode = SATA_MODE,});
	stih416_configure_sata(0);

	return;
}

MACHINE_START(STM_B2020, "STMicroelectronics B2020 - STiH416 MBoard")
	.atag_offset	= 0x100,
	.map_io		= stih416_map_io,
#ifdef CONFIG_SPARSE_IRQ
	.nr_irqs	= NR_IRQS_LEGACY,
#endif
	.init_early	= b2020_init_early,
	.init_irq	= mpe42_gic_init_irq,
	.timer		= &mpe42_timer,
	.handle_irq	= gic_handle_irq,
	.init_machine	= b2020_init,
	.restart	= stih416_reset,
MACHINE_END

#ifdef CONFIG_HIBERNATION_ON_MEMORY

#include <linux/stm/hom.h>

static int b2020_hom_restore(struct stm_wakeup_devices *dev_wk)
{
	b2020_ethphy_gpio_init(0);
	return 0;
}

static struct stm_hom_board b2020_hom = {
	.lmi_retention_gpio = stih416_gpio(4, 4),
	.restore = b2020_hom_restore,
};

static int __init b2020_hom_init(void)
{
	return stm_hom_stxh416_setup(&b2020_hom);
}

module_init(b2020_hom_init);
#endif
