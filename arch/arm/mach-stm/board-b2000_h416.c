/*
 * arch/arm/mach-stm/board-b2000_h416.c
 *
 * Copyright (C) 2012 STMicroelectronics Limited.
 * Author: Francesco Virlinzi <francesco.virlinzi@st.com>
 *		Srinivas Kandagatla <srinivas.kandagatla@st.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 *****************************************************************************
 * NOTE: THIS FILEIS AN INTERMEDIATE TRANSISSION FROM NON-DEVICE TREES
 * TO DEVICE TREES. IDEALLY THIS FILESHOULD NOT EXIST IN FULL DEVICE TREE
 * SUPPORTED KERNEL.
 *
 * WITH THE ASSUMPTION THAT SDK2 WILL MOVE TO FULL DEVICE TREES AT
 * SOME POINT, AT WHICH THIS FILEIS NOT REQUIRED ANYMORE
 *
 * ALSO BOARD SUPPORT WITH THIS APPROCH IS IS DONE IN TWO PLACES
 * 1. IN THIS FILE
 * 2. arch/arm/boot/dts/b2000_h416.dtp
 *	THIS FILECONFIGURES ALL THE DRIVERS WHICH SUPPORT DEVICE TREES.
 *
 * please do not optimize this file or try adding any level of abstraction
 * due to reasons above.
 *****************************************************************************
 */

#include <linux/init.h>
#include <linux/io.h>
#include <linux/input.h>
#include <linux/gpio.h>
#include <linux/stm/platform.h>
#include <linux/stm/stih416.h>
#include <linux/of_platform.h>
#include <linux/stm/core_of.h>
#include <linux/of_gpio.h>
#include <linux/stm/mpe41_of_devices.h>
#include <linux/stm/stm_device_of.h>

#include <asm/hardware/gic.h>
#include <asm/memory.h>
#include <asm/mach/time.h>
#include <mach/common-dt.h>
#include <mach/soc-stih416.h>
#include <mach/hardware.h>

struct of_dev_auxdata stih416_auxdata_lookup[] __initdata = {
	OF_DEV_AUXDATA("st,fdma", 0xfd600000, "stm-fdma.0", NULL),
	OF_DEV_AUXDATA("st,fdma", 0xfd620000, "stm-fdma.1", NULL),
	OF_DEV_AUXDATA("st,fdma", 0xfd640000, "stm-fdma.2", NULL),
	OF_DEV_AUXDATA("st,fdma", 0xfea00000, "stm-fdma.3", NULL),
	OF_DEV_AUXDATA("st,fdma", 0xfea20000, "stm-fdma.4", NULL),
	OF_DEV_AUXDATA("st,sdhci", 0xfe81e000, "sdhci-stm.0",
		 &mmc_platform_data),
	OF_DEV_AUXDATA("st,sdhci", 0xfe81f000, "sdhci-stm.1",
		 &mmc_platform_data),
	OF_DEV_AUXDATA("snps,dwmac", 0xfe810000, "stmmaceth.0",
		 &ethernet_platform_data),
	OF_DEV_AUXDATA("snps,dwmac", 0xfef08000, "stmmaceth.1",
		 &ethernet_platform_data),
	OF_DEV_AUXDATA("st,coproc-st40", 0, "st,coproc-st40",
		 &mpe41_st40_coproc_data),
	{}
};

#if defined(CONFIG_MACH_STM_B2000_CN23_NONE)
static struct stm_pad_config stih416_hdmi_hp_pad_config = {
		.gpios_num = 1,
		.gpios = (struct stm_pad_gpio []) {
		STIH416_PAD_PIO_IN(2, 5, 1),      /* HDMI Hotplug */
		},
};
#endif

static void __init b2000_dt_init(void)
{
	int power_on_gpio;
	struct device_node *np = of_find_node_by_path("/soc");
	if (np) {
		power_on_gpio = of_get_named_gpio(np, "power-on-gpio", 0);
		if (power_on_gpio > 0) {
			gpio_request(power_on_gpio, "POWER_PIO");
			gpio_direction_output(power_on_gpio, 1);
		}
		of_node_put(np);
	}
	of_platform_populate(NULL, of_default_bus_match_table,
				 stih416_auxdata_lookup, NULL);

#if defined(CONFIG_MACH_STM_B2000_CN23_NONE)
	/* Default to HDMI HotPlug */
	if (stm_pad_claim(&stih416_hdmi_hp_pad_config, "HDMI_Hotplug") == NULL)
		printk(KERN_ERR "Failed to claim HDMI-Hotplug pad!\n");
#endif

	stih416_configure_audio(&(struct stih416_audio_config) {
			.uni_player_3_spdif_enabled = 1, });

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

	stih416_configure_miphy(&(struct stih416_miphy_config) {
			       .id = 1,
			       .mode = PCIE_MODE,
			       .iface = UPORT_IF,});

	stih416_configure_pcie(&(struct stih416_pcie_config) {
			      .port = 1,
			      .reset_gpio = -1 /* stih416_gpio(106,5) */});
	return;
}

/* Setup the Timer */
static void __init stih416_timer_init(void)
{
	stih416_plat_clk_init();
	stih416_plat_clk_alias_init();

	stm_of_timer_init();
}

struct sys_timer stih416_timer = {
	.init	= stih416_timer_init,
};


static const char *b2000_dt_match[] __initdata = {
	"st,stih416-b2000",
	NULL
};


DT_MACHINE_START(STM_B2000, "STM STiH416 with Flattened Device Tree")
	.map_io		= stih416_map_io,
	.init_early	= core_of_early_device_init,
	.timer		= &stih416_timer,
	.handle_irq	= gic_handle_irq,
	.init_machine	= b2000_dt_init,
	.init_irq	= stm_of_gic_init,
	.restart	= stih416_reset,
	.dt_compat	= b2000_dt_match,
MACHINE_END

#ifdef CONFIG_HIBERNATION_ON_MEMORY

#include <linux/stm/hom.h>

static int b2000_hom_restore(struct stm_wakeup_devices *dev_wk)
{
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
