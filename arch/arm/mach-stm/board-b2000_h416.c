/*
 * arch/arm/mach-stm/board-b2000_h416.c
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

static void __init b2000_init_early(void)
{
	pr_info("STMicroelectronics STiH416 (Orly-2) MBoard initialisation\n");

	stih416_early_device_init();

	stih416_configure_asc(STIH416_ASC(2), &(struct stih416_asc_config) {
			.hw_flow_control = 0,
			.is_console = 1 });
}

#define GMII0_PHY_CLKOUT_NOT_TXCLK_SEL	stih416_gpio(13, 4)
#define GMII1_PHY_CLKOUT_NOT_TXCLK_SEL	stih416_gpio(2, 5)
#define GMII1_PHY_NOT_RESET		stih416_gpio(4, 7)
/*
 * pio[106][2] -> VXIDATA26 -> [U57:74LVC4245] -> PIO_GMII0_notRESET
 */
#define GMII0_PHY_NOT_RESET		stih416_gpio(106, 2)
#define POWER_ON stih416_gpio(4, 3)

#if defined(CONFIG_MACH_STM_B2000_CN23_NONE)
static struct stm_pad_config stih416_hdmi_hp_pad_config = {
		.gpios_num = 1,
		.gpios = (struct stm_pad_gpio []) {
		STIH416_PAD_PIO_IN(2, 5, 1),      /* HDMI Hotplug */
		},
};
#endif

/* Serial FLASH */
static struct stm_plat_spifsm_data b2000_serial_flash =  {
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

#if defined(CONFIG_MACH_STM_B2000_CN22_B2032_GIGA_MODE)
static void b2000_gmac0_txclk_select(int txclk_125_not_25_mhz)
{
	/* When 1000 speed is negotiated we have to set the PIO13[4]. */
	if (txclk_125_not_25_mhz)
		gpio_set_value(GMII0_PHY_CLKOUT_NOT_TXCLK_SEL, 1);
	else
		gpio_set_value(GMII0_PHY_CLKOUT_NOT_TXCLK_SEL, 0);
}
#endif

#if defined(CONFIG_MACH_STM_B2000_CN23_B2032_GIGA_MODE)
/*
 * On B2000B board PIO 2-5 conflicts with the HDMI hot-plug detection pin.
 * As we have a seperate 125clk pin in to the MAC, we might not need
 * tx clk selection.
 * Removing R59 from B2032A will solve the problem and TXCLK is
 * always connected to PHY TXCLK.
 * NOTE: This will be an issue if the B2032 is used with a 7108 as
 * 125clk comes from phy txclk, and so this signal is needed.
 */
/* GMII and RGMII on GMAC1 NOT Yet Supported
 * Noticed Packet drops with GMII &
 * RGMII does not work even for validation.
 */
static void b2000_gmac1_txclk_select(int txclk_125_not_25_mhz)
{
	/* When 1000 speed is negotiated we have to set the PIO2[5]. */
	if (txclk_125_not_25_mhz)
		gpio_set_value(GMII1_PHY_CLKOUT_NOT_TXCLK_SEL, 1);
	else
		gpio_set_value(GMII1_PHY_CLKOUT_NOT_TXCLK_SEL, 0);
}
#endif

#if !((defined(CONFIG_MACH_STM_B2000_CN22_NONE) || \
	defined(CONFIG_MACH_STM_B2000_CN22_B2035)) && \
	defined(CONFIG_MACH_STM_B2000_CN23_NONE))
static void b2000_gmii_gpio_reset(int gpio)
{
	gpio_set_value(gpio, 1);
	gpio_set_value(gpio, 0);
	mdelay(10); /* 10 miliseconds is enough for everyone ;-) */
	gpio_set_value(gpio, 1);
	mdelay(10); /* 10 miliseconds is enough for everyone ;-) */
}
#endif

#if defined(CONFIG_MACH_STM_B2000_CN22_B2035) || \
	defined(CONFIG_MACH_STM_B2000_CN22_B2032)
static int b2000_gmii0_reset(void *bus)
{
	b2000_gmii_gpio_reset(GMII0_PHY_NOT_RESET);
	return 1;
}

static struct stmmac_mdio_bus_data stmmac0_mdio_bus = {
	/* GMII connector CN22 */
	.phy_reset = b2000_gmii0_reset,
	.phy_mask = 0,
};
#endif

#if defined(CONFIG_MACH_STM_B2000_CN23_B2035) || \
	defined(CONFIG_MACH_STM_B2000_CN23_B2032)

#if defined(CONFIG_MACH_STM_B2000_CN23_B2032_RGMII_MODE)
#error "RGMII mode on GMAC1 is not functional"
#endif
static int b2000_gmii1_reset(void *bus)
{
	b2000_gmii_gpio_reset(GMII1_PHY_NOT_RESET);
	return 1;
}

static struct stmmac_mdio_bus_data stmmac1_mdio_bus = {
	/* GMII connector CN23 */
	.phy_reset = b2000_gmii1_reset,
	.phy_mask = 0,
};
#endif

static struct platform_device b2000_leds = {
	.name = "leds-gpio",
	.id = -1,
	.dev.platform_data = &(struct gpio_led_platform_data) {
		.num_leds = 1,
		.leds = (struct gpio_led[]) {
			{
				.name = "FP_LED",
				.default_trigger = "heartbeat",
				.gpio = stih416_gpio(105, 7),
			}
		},
	},
};

static struct platform_device *b2000_devices[] __initdata = {
	&b2000_leds,
};


static void b2000_ethphy_gpio_init(int cold_boot)
{
	/* Reset */
	if (cold_boot) {
		gpio_request(GMII0_PHY_NOT_RESET, "GMII0_PHY_NOT_RESET");
		gpio_request(GMII1_PHY_NOT_RESET, "GMII1_PHY_NOT_RESET");
	}
	gpio_direction_output(GMII0_PHY_NOT_RESET, 0);
	gpio_direction_output(GMII1_PHY_NOT_RESET, 0);

#if !defined(CONFIG_MACH_STM_B2000_CN23_NONE)
	/* Default to 100 Mbps */
	gpio_request(GMII1_PHY_CLKOUT_NOT_TXCLK_SEL, "GMII1_TXCLK_SEL");
#if defined(CONFIG_MACH_STM_B2000_CN23_B2032_GIGA_MODE)
	gpio_direction_output(GMII1_PHY_CLKOUT_NOT_TXCLK_SEL, 1);
#else
	gpio_direction_output(GMII1_PHY_CLKOUT_NOT_TXCLK_SEL, 0);
#endif
	gpio_free(GMII1_PHY_CLKOUT_NOT_TXCLK_SEL);
#endif

#if defined(CONFIG_MACH_STM_B2000_CN22_B2032) || \
	defined(CONFIG_MACH_STM_B2000_CN22_B2032)
	/* Default to 100 Mbps */
	gpio_request(GMII0_PHY_CLKOUT_NOT_TXCLK_SEL, "GMII0_TXCLK_SEL");
	/* Can be ignored for RGMII on this PHY */
#if defined(CONFIG_MACH_STM_B2000_CN22_B2032_GIGA_MODE)
	gpio_direction_output(GMII0_PHY_CLKOUT_NOT_TXCLK_SEL, 1);
#else
	gpio_direction_output(GMII0_PHY_CLKOUT_NOT_TXCLK_SEL, 0);
#endif
	gpio_free(GMII0_PHY_CLKOUT_NOT_TXCLK_SEL);
#endif


#if defined(CONFIG_MACH_STM_B2000_CN22_B2035) || \
	defined(CONFIG_MACH_STM_B2000_CN22_B2032)
	b2000_gmii0_reset(NULL);
#endif

#if defined(CONFIG_MACH_STM_B2000_CN23_B2035) || \
	defined(CONFIG_MACH_STM_B2000_CN23_B2032)
	b2000_gmii1_reset(NULL);
#endif
}

static void __init b2000_init(void)
{
	/* gpio-power on board power supplies */
	gpio_request(POWER_ON, "POWER_PIO");
	gpio_direction_output(POWER_ON, 1);

	b2000_ethphy_gpio_init(1);
#if defined(CONFIG_MACH_STM_B2000_CN23_NONE)
	/* Default to HDMI HotPlug */
	if (stm_pad_claim(&stih416_hdmi_hp_pad_config, "HDMI_Hotplug") == NULL)
		printk(KERN_ERR "Failed to claim HDMI-Hotplug pad!\n");
#endif
/*
 * B2032A (MII or GMII) Ethernet card
 * GMII Mode on B2032A needs R26 to be fitted with 51R
 * On B2000B board, to get GMAC0 working make sure that jumper
 * on PIN 9-10 on CN35 and CN36 are removed.
 *
 * For RGMII: Remove the J1 Jumper.
 * 100/10 Link: TXCLK_SELECTION can be ignored for this PHY.
 */

/*
 * B2035A (RMII + MMC(on CN22)) Ethernet + MMC card
 * B2035A board has IP101ALF PHY connected in RMII mode
 * and an MMC card
 * It is designed to be conneted to GMAC0 (CN22) to get MMC working,
 * however we can connect it to GMAC1 for RMII testing.
 */

/* GMAC0 */
#if defined(CONFIG_MACH_STM_B2000_CN22_B2035) || \
	defined(CONFIG_MACH_STM_B2000_CN22_B2032)
	stih416_configure_ethernet(0, &(struct stih416_ethernet_config) {
#ifdef CONFIG_MACH_STM_B2000_CN22_B2035
			.interface = PHY_INTERFACE_MODE_RMII,
			.ext_clk = 0,
			.phy_addr = 9,
#endif /* CONFIG_MACH_STM_B2000_CN22_B2035 */

#ifdef CONFIG_MACH_STM_B2000_CN22_B2032
/* B2032 modified to support GMII */
#if defined(CONFIG_MACH_STM_B2000_CN22_B2032_GIGA_MODE)
			.txclk_select = b2000_gmac0_txclk_select,
#ifdef CONFIG_MACH_STM_B2000_CN22_B2032_GMII_MODE
			.interface = PHY_INTERFACE_MODE_GMII,
#else
			.interface = PHY_INTERFACE_MODE_RGMII_ID,
#endif /* CONFIG_MACH_STM_B2000_CN22_B2032_GMII_MODE */
#else
			.interface = PHY_INTERFACE_MODE_MII,
#endif /* CONFIG_MACH_STM_B2000_CN22_B2032_GIGA_MODE */
			.ext_clk = 1,
			.phy_addr = 1,
#endif /* CONFIG_MACH_STM_B2000_CN22_B2032 */

			.phy_bus = 0,
			.mdio_bus_data = &stmmac0_mdio_bus,});

#endif

/* GMAC1 */
#if !defined(CONFIG_MACH_STM_B2000_CN23_NONE)
	stih416_configure_ethernet(1, &(struct stih416_ethernet_config) {
#ifdef CONFIG_MACH_STM_B2000_CN23_B2035
			.interface = PHY_INTERFACE_MODE_RMII,
			.ext_clk = 0,
			.phy_addr = 9,
#endif /* CONFIG_MACH_STM_B2000_CN23_B2035 */

/* RGMII on GMAC1 has problems with TX side
 */
#ifdef CONFIG_MACH_STM_B2000_CN23_B2032
#if defined(CONFIG_MACH_STM_B2000_CN23_B2032_GIGA_MODE)
			.txclk_select = b2000_gmac1_txclk_select,
#ifdef CONFIG_MACH_STM_B2000_CN23_B2032_GMII_MODE
			.interface = PHY_INTERFACE_MODE_GMII,
#else
			.interface = PHY_INTERFACE_MODE_RGMII_ID,
#endif /* CONFIG_MACH_STM_B2000_CN23_B2032_GMII_MODE */
#else
			.interface = PHY_INTERFACE_MODE_MII,
#endif /* CONFIG_MACH_STM_B2000_CN23_B2032 */
			.ext_clk = 1,
			.phy_addr = 1,
#endif /* CONFIG_MACH_STM_B2000_CN23_B2032 */

			.phy_bus = 1,
			.mdio_bus_data = &stmmac1_mdio_bus,});
#endif /* CONFIG_MACH_STM_B2000_CN23_NONE */

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
			.tx_enabled = IS_ENABLED(CONFIG_LIRC_STM_TX),
			.rx_mode = stih416_lirc_rx_mode_ir, });

	stih416_configure_pwm(&(struct stih416_pwm_config) {
			.pwm = stih416_sbc_pwm,
			.out0_enabled = 1, });

#if defined(CONFIG_MACH_STM_B2000_CN22_B2048)
# if defined(CONFIG_MACH_STM_B2000_B2048_EMMC)
	/* eMMC on board */
	stih416_configure_mmc(0, 1);
# else
	/*
	 * In case of B2035 or B2048A without eMMC, we actually have the
	 * MMC/SD slot on the daughter board.
	 */
	stih416_configure_mmc(0, 0);
# endif
#endif

	stih416_configure_spifsm(&b2000_serial_flash);

	stih416_configure_keyscan(&(struct stm_keyscan_config) {
			.num_out_pads = 4,
			.num_in_pads = 4,
			.debounce_us = 5000,
			.keycodes = {
				/* in0 ,   in1  ,   in2 ,  in3  */
				KEY_F13, KEY_F9,  KEY_F5, KEY_F1,  /* out0 */
				KEY_F14, KEY_F10, KEY_F6, KEY_F2,  /* out1 */
				KEY_F15, KEY_F11, KEY_F7, KEY_F3,  /* out2 */
				KEY_F16, KEY_F12, KEY_F8, KEY_F4   /* out3 */
			},
		});

	/* 1 SATA + 1 SATA */
	stih416_configure_miphy(&(struct stih416_miphy_config) {
		.id = 0,
		.mode = SATA_MODE,
		.iface = UPORT_IF,
		});
	stih416_configure_sata(0);

	platform_add_devices(b2000_devices,
		ARRAY_SIZE(b2000_devices));

	return;
}

MACHINE_START(STM_B2000, "STMicroelectronics B2000 - STiH416 MBoard")
	.atag_offset	= 0x100,
	.map_io		= stih416_map_io,
#ifdef CONFIG_SPARSE_IRQ
	.nr_irqs	= NR_IRQS_LEGACY,
#endif
	.init_early	= b2000_init_early,
	.init_irq	= mpe42_gic_init_irq,
	.timer		= &mpe42_timer,
	.handle_irq	= gic_handle_irq,
	.init_machine	= b2000_init,
	.restart	= stih416_reset,
MACHINE_END

#ifdef CONFIG_HIBERNATION_ON_MEMORY

#include <linux/stm/hom.h>

static int b2000_hom_restore(struct stm_wakeup_devices *dev_wk)
{
	b2000_ethphy_gpio_init(0);
	return 0;
}

static struct stm_hom_board b2000_hom = {
	.lmi_retention_gpio = stih416_gpio(4, 4),
	.restore = b2000_hom_restore,
};

static int __init b2000_hom_init(void)
{
	return stm_hom_stxh416_setup(&b2000_hom);
}

module_init(b2000_hom_init);
#endif
