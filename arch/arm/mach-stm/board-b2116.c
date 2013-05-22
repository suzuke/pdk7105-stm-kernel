/*
 * arch/arm/mach-stm/board-b2116.c
 *
 * Copyright (C) 2013 STMicroelectronics Limited.
 * Author: Giuseppe Cavallaro <peppe.cavallaro@st.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 *****************************************************************************
 * NOTE: THIS FILE IS AN INTERMEDIATE TRANSISSION FROM NON-DEVICE TREES
 * TO DEVICE TREES. IDEALLY THIS FILE SHOULD NOT EXIST IN FULL DEVICE TREE
 * SUPPORTED KERNEL.
 *
 * WITH THE ASSUMPTION THAT SDK2 WILL MOVE TO FULL DEVICE TREES AT
 * SOME POINT, AT WHICH THIS FILEIS NOT REQUIRED ANYMORE.
 *
 * ALSO BOARD SUPPORT WITH THIS APPROCH IS IS DONE IN TWO PLACES
 * 1. IN THIS FILE
 * 2. arch/arm/boot/dts/stih315-b2116.dtsp
 *	THIS FILE CONFIGURES ALL THE DRIVERS WHICH SUPPORT DEVICE TREES.
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
#include <linux/clk.h>

#include <asm/hardware/gic.h>
#include <asm/memory.h>
#include <asm/mach/time.h>
#include <mach/common-dt.h>
#include <mach/soc-stih416.h>
#include <mach/hardware.h>

static struct stm_pad_config stih416_hdmi_hp_pad_config = {
	.gpios_num = 1,
	.gpios = (struct stm_pad_gpio []) {
		STM_PAD_PIO_IN(2, 5, 1),	/* HDMI Hotplug */
	},
};

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
	OF_DEV_AUXDATA("snps,dwmac", 0xfef08000, "stmmaceth.1",
		 &ethernet_platform_data),
	OF_DEV_AUXDATA("st,coproc-st40", 0, "st,coproc-st40",
		 &mpe41_st40_coproc_data),
	{}
};

static void __init b2116_dt_init(void)
{
	of_platform_populate(NULL, of_default_bus_match_table,
				 stih416_auxdata_lookup, NULL);

	/* Default to HDMI HotPlug */
	if (stm_pad_claim(&stih416_hdmi_hp_pad_config, "HDMI_Hotplug") == NULL)
		pr_err("Failed to claim HDMI-Hotplug pad!\n");

	stih416_configure_audio(&(struct stih416_audio_config) {
					.uni_player_3_spdif_enabled = 1, });

	/* 1 SATA */
	stih416_configure_miphy(&(struct stih416_miphy_config) {
		.id = 0,
		.mode = SATA_MODE,});
	stih416_configure_sata(0);
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

static const char *b2116_dt_match[] __initdata = {
	"st,stih315-b2116",
	NULL
};

DT_MACHINE_START(STM_B2116, "STM STiH315 with Flattened Device Tree")
	.map_io		= stih416_map_io,
	.init_early	= core_of_early_device_init,
	.timer		= &stih416_timer,
	.handle_irq	= gic_handle_irq,
	.init_machine	= b2116_dt_init,
	.init_irq	= stm_of_gic_init,
	.restart	= stih416_reset,
	.dt_compat	= b2116_dt_match,
MACHINE_END

#ifdef CONFIG_HIBERNATION_ON_MEMORY

#include <linux/stm/hom.h>

static int b2116_hom_restore(struct stm_wakeup_devices *dev_wk)
{
	return 0;
}

static struct stm_hom_board b2116_hom = {
	.restore = b2116_hom_restore,
};

static int __init b2116_hom_init(void)
{
	return stm_hom_stxh416_setup(&b2116_hom);
}

late_initcall(b2116_hom_init);
#endif
