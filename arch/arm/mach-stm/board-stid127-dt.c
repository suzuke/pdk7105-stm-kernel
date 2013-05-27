/*
 * Copyright (C) 2013 STMicroelectronics Limited
 *
 * Author(s): Giuseppe Cavallaro <peppe.cavallaro@st.com>
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
 * 2. arch/arm/boot/dts/b2000.dtp
 *	THIS FILECONFIGURES ALL THE DRIVERS WHICH SUPPORT DEVICE TREES.
 *
 * please do not optimize this file or try adding any level of abstraction
 * due to reasons above.
 *****************************************************************************
 */
#include <linux/device.h>
#include <linux/stm/platform.h>
#include <linux/of_platform.h>
#include <linux/stm/soc.h>

#include <linux/stm/core_of.h>
#include <linux/stm/stm_device_of.h>

#include <asm/hardware/gic.h>
#include <asm/mach/time.h>

#include <asm/mach/arch.h>
#include <mach/common-dt.h>
#include <mach/hardware.h>
#include <mach/soc-stid127.h>

struct of_dev_auxdata stid127_auxdata_lookup[] __initdata = {
	OF_DEV_AUXDATA("st,fdma", 0xfe2c0000, "stm-fdma.0", NULL),
	OF_DEV_AUXDATA("st,fdma", 0xfe2e0000, "stm-fdma.1", NULL),
	OF_DEV_AUXDATA("st,fdma", 0xfeb80000, "stm-fdma.2", NULL),
	OF_DEV_AUXDATA("st,usb", 0xfe800000, "stm-usb.0", NULL),
	OF_DEV_AUXDATA("snps,dwmac", 0xfeb00000, "stmmaceth.0",
		 &ethernet_platform_data),
	OF_DEV_AUXDATA("st,miphy-mp", 0xfef24000, "st,miphy-mp.0",
		 &pcie_mp_platform_data),
	OF_DEV_AUXDATA("st,pcie", 0xfef20000, "st,pcie.0",
		 &stm_pcie_config),
	OF_DEV_AUXDATA("st,miphy-mp", 0xfef34000, "st,miphy-mp.1",
		 &pcie_mp_platform_data),
	OF_DEV_AUXDATA("st,pcie", 0xfef30000, "st,pcie.1",
		 &stm_pcie_config),
	{}
};

static void __init stid127_board_dt_init(void)
{
	of_platform_populate(NULL, of_default_bus_match_table,
			     stid127_auxdata_lookup, NULL);
}

/* Setup the Timer */
static void __init stid127_of_timer_init(void)
{
	stid127_plat_clk_init();
	stid127_plat_clk_alias_init();
	stm_of_timer_init();
}

struct sys_timer stid127_of_timer = {
	.init	= stid127_of_timer_init,
};

static const char *stid127_dt_match[] __initdata = {
	"st,stid127",
	NULL
};

DT_MACHINE_START(STM, "StiD127 SoC with Flattened Device Tree")
	.map_io		= stid127_map_io,
	.init_early	= core_of_early_device_init,
	.timer		= &stid127_of_timer,
	.handle_irq	= gic_handle_irq,
	.init_machine	= stid127_board_dt_init,
	.init_irq	= stm_of_gic_init,
	.dt_compat	= stid127_dt_match,
MACHINE_END
