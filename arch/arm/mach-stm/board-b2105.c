/*
 * arch/arm/mach-stm/board-b2105.c
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
 * 2. arch/arm/boot/dts/stih416-b2105.dtsp
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
#include <linux/phy.h>
#include <linux/phy_fixed.h>
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
	OF_DEV_AUXDATA("st,sdhci", 0xfe81e000, "sdhci-stm.0", NULL),
	OF_DEV_AUXDATA("snps,dwmac", 0xfe810000, "stmmaceth.0",
		 &ethernet_platform_data),
	OF_DEV_AUXDATA("snps,dwmac", 0xfef08000, "stmmaceth.1",
		 &ethernet_platform_data),
	OF_DEV_AUXDATA("st,coproc-st40", 0, "st,coproc-st40",
		 &mpe41_st40_coproc_data),
	{}
};

static struct fixed_phy_status stmmac1_fixed_phy_status = {
	.link = 1,
	/* The speed is limited to 100 instead of 1000 for alicante/zaragoza
	 * bring-up.
	 */
	.speed = 100,
	.duplex = 1,
};

/* The b2105 the GMAC1 is used for communicating with the b2112 via NIM. */
static void b2105_stmmac1_clk_setting(void)
{
	struct clk *clk, *clk_parent;
	int ret;

	clk_parent = clk_get(NULL, "CLK_S_A0_PLL0LS");

	clk = clk_get(NULL, "CLK_S_ETH1_PHY");
	ret = clk_set_parent(clk, clk_parent);
	if (ret)
		pr_err("%s: error setting the parent clk\n", __func__);
	clk_set_rate(clk, 125000000);

	clk = clk_get(NULL, "CLK_S_MII1_REF_OUT");
	ret = clk_set_parent(clk, clk_parent);
	if (ret)
		pr_err("%s: error setting the parent clk\n", __func__);
	clk_set_rate(clk, 125000000);

	return;
}

static void __init b2105_dt_init(void)
{
	/* GMAC1 uses fixed phy support to communicate with the b2112 board */
	BUG_ON(fixed_phy_add(PHY_POLL, 1, &stmmac1_fixed_phy_status));
	b2105_stmmac1_clk_setting();

	of_platform_populate(NULL, of_default_bus_match_table,
				 stih416_auxdata_lookup, NULL);

	/* Default to HDMI HotPlug */
	if (stm_pad_claim(&stih416_hdmi_hp_pad_config, "HDMI_Hotplug") == NULL)
		printk(KERN_ERR "Failed to claim HDMI-Hotplug pad!\n");

	stih416_configure_audio(&(struct stih416_audio_config) {
			.uni_player_3_spdif_enabled = 1, });

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

static const char *b2105_dt_match[] __initdata = {
	"st,stih416-b2105",
	NULL
};

DT_MACHINE_START(STM_B2105, "STM STiH416 with Flattened Device Tree")
	.map_io		= stih416_map_io,
	.init_early	= core_of_early_device_init,
	.timer		= &stih416_timer,
	.handle_irq	= gic_handle_irq,
	.init_machine	= b2105_dt_init,
	.init_irq	= stm_of_gic_init,
	.restart	= stih416_reset,
	.dt_compat	= b2105_dt_match,
MACHINE_END

#ifdef CONFIG_HIBERNATION_ON_MEMORY

#include <linux/stm/hom.h>

static int b2105_hom_restore(struct stm_wakeup_devices *dev_wk)
{
	return 0;
}

static struct stm_hom_board b2105_hom = {
	.lmi_retention_gpio = stm_gpio(4, 4),
	.restore = b2105_hom_restore,
};

static int __init b2105_hom_init(void)
{
	return stm_hom_stxh416_setup(&b2105_hom);
}

late_initcall(b2105_hom_init);
#endif
