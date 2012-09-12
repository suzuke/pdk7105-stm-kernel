/*
 * arch/arm/mach-stm/board-b2020.c
 *
 * Copyright (C) 2012 STMicroelectronics Limited.
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
#include <linux/input.h>
#include <linux/phy.h>
#include <linux/leds.h>
#include <linux/stm/platform.h>
#include <linux/stm/stih415.h>
#include <linux/mtd/nand.h>
#include <linux/mtd/partitions.h>

#include <asm/hardware/gic.h>
#include <asm/mach-types.h>
#include <asm/memory.h>
#include <asm/delay.h>

#include <mach/soc-stih415.h>
#include <mach/mpe41.h>
#include <mach/hardware.h>

static void __init b2020_init_early(void)
{
	printk(KERN_INFO
	       "STMicroelectronics STiH415 (Orly) B2020 ADI initialisation\n");

	stih415_early_device_init();
	/* for UART11 via J26 use device 5 for console
	 * for UART10 via J35 use device 4 for console
	 */
	stih415_configure_asc(5, &(struct stih415_asc_config) {
			.hw_flow_control = 0,
			.is_console = 1 });
}

#define B2020_GMII1_PHY_NOT_RESET stm_gpio(3, 0)

static struct stm_pad_config stih415_hdmi_hp_pad_config = {
	.gpios_num = 1,
	.gpios = (struct stm_pad_gpio []) {
		STM_PAD_PIO_IN(2, 5, 1),	/* HDMI Hotplug */
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
	.name		= "m25p128",
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
				.gpio = stm_gpio(4, 1),
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
	if (stm_pad_claim(&stih415_hdmi_hp_pad_config, "HDMI_Hotplug") == NULL)
		printk(KERN_ERR "Failed to claim HDMI-Hotplug pad!\n");

	stih415_configure_ethernet(1, &(struct stih415_ethernet_config) {
			.interface = PHY_INTERFACE_MODE_RGMII_ID,
			.ext_clk = 1,
			.phy_addr = 1,
			.phy_bus = 1,
			.mdio_bus_data = &stmmac1_mdio_bus,});

	stih415_configure_usb(0);
	stih415_configure_usb(1);
	stih415_configure_usb(2);

	/* HDMI */
	stih415_configure_ssc_i2c(STIH415_SSC(1),
			&(struct stih415_ssc_config) {.i2c_speed = 100,});

	/* Frontend I2C, make sure J17-J20 are configured accordingly */
	stih415_configure_ssc_i2c(STIH415_SSC(3),
			&(struct stih415_ssc_config) {.i2c_speed = 100,});

	/* Backend I2C, make sure J50, J51 are configured accordingly */
	stih415_configure_ssc_i2c(STIH415_SBC_SSC(0),
			&(struct stih415_ssc_config) {.i2c_speed = 100,});

	/* IR_IN */
	stih415_configure_lirc(&(struct stih415_lirc_config) {
			.rx_mode = stih415_lirc_rx_mode_ir, });

	stih415_configure_pwm(&(struct stih415_pwm_config) {
			.pwm = stih415_sbc_pwm,
			.out0_enabled = 1, });

	stih415_configure_mmc(0);

	stih415_configure_nand(&(struct stm_nand_config) {
			.driver = stm_nand_bch,
			.nr_banks = 1,
			.banks = &b2020_nand_flash,
			.rbn.flex_connected = 1,});

	stih415_configure_spifsm(&b2020_serial_flash);

	stih415_configure_audio(&(struct stih415_audio_config) {
			.uni_player_3_spdif_enabled = 1, });

	platform_add_devices(b2020_devices,
		ARRAY_SIZE(b2020_devices));

	/* 1 SATA + 1 PCIe */
	stih415_configure_miphy(&(struct stih415_miphy_config) {
		/* You need a valid clock to the PCIe block in order to talk to
		 * the miphy, but the PCIe clock on Rev A,B board is set to
		 * 200MHz, it needs to be changed to 100MHz in order to get
		 * PCIe working: remove resistor R51 and connect a 10k resistor
		 * on R56. Rev C boards have the option resistors set correcty.
		 *
		 * The UNUSED is to improve resiliance to unmodified boards: If
		 * you have an unmodified board, disabling CONFIG_PCI will at
		 * least get you working SATA
		 */
		.modes = (enum miphy_mode[2]) { SATA_MODE,
		IS_ENABLED(CONFIG_PCI) ?  PCIE_MODE : UNUSED_MODE }, });
	stih415_configure_sata(0, &(struct stih415_sata_config) { });

	stih415_configure_pcie(&(struct stih415_pcie_config) {
				.reset_gpio = -1, /* No Reset */
				});

	return;
}

MACHINE_START(STM_B2020, "STMicroelectronics B2020 - STiH415 MBoard")
	.atag_offset	= 0x100,
	.map_io		= stih415_map_io,
#ifdef CONFIG_SPARSE_IRQ
	.nr_irqs	= NR_IRQS_LEGACY,
#endif
	.init_early	= b2020_init_early,
	.init_irq	= mpe41_gic_init_irq,
	.timer		= &mpe41_timer,
	.handle_irq	= gic_handle_irq,
	.init_machine	= b2020_init,
	.restart	= stih415_reset,
MACHINE_END

#ifdef CONFIG_HIBERNATION_ON_MEMORY

#include <linux/stm/hom.h>

static int b2020_hom_restore(void)
{
	b2020_ethphy_gpio_init(0);
	return 0;
}

static struct stm_hom_board b2020_hom = {
	.restore = b2020_hom_restore,
};

static int __init b2020_hom_init(void)
{
	return stm_hom_board_register(&b2020_hom);
}

late_initcall(b2020_hom_init);
#endif
