 /*
 * Copyright (C) 2013 STMicroelectronics Limited.
 *  Author: Srinivas Kandagatla <srinivas.kandagatla@st.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/of_platform.h>
#include <linux/stm/stig125.h>
#include <linux/stm/soc.h>
#include <linux/stm/core_of.h>
#include <linux/stm/stm_device_of.h>
#include <asm/hardware/gic.h>
#include <asm/mach/time.h>
#include <asm/mach/map.h>
#include <mach/common-dt.h>
#include <mach/hardware.h>
#include <mach/soc-stig125.h>

struct of_dev_auxdata stig125_auxdata_lookup[] __initdata = {
	OF_DEV_AUXDATA("st,fdma", 0xfe2c0000, "stm-fdma.0", NULL),
	OF_DEV_AUXDATA("st,fdma", 0xfe2e0000, "stm-fdma.1", NULL),
	OF_DEV_AUXDATA("st,sdhci", 0xfe96c000, "sdhci-stm.0",
		 &mmc_platform_data),
	OF_DEV_AUXDATA("st,miphy-mp", 0xfefb2000, "st,miphy-mp.0",
		 &pcie_mp_platform_data),
	OF_DEV_AUXDATA("st,miphy-mp", 0xfefb6000, "st,miphy-mp.1",
		 &pcie_mp_platform_data),
	{}
};

/* Setup the Timer */
static void __init stig125_of_timer_init(void)
{
	stig125_plat_clk_init();
	stig125_plat_clk_alias_init();
	stm_of_timer_init();
}

struct sys_timer stig125_of_timer = {
	.init	= stig125_of_timer_init,
};

void __init stig125_dt_init(void)
{
	of_platform_populate(NULL, of_default_bus_match_table,
				 stig125_auxdata_lookup, NULL);
	return;
}

static const char *stig125_dt_match[] __initdata = {
	"st,stig125",
	NULL
};

DT_MACHINE_START(STM, "StiG125 SoC with Flattened Device Tree")
	.map_io		= stig125_map_io,
	.init_early	= core_of_early_device_init,
	.timer		= &stig125_of_timer,
	.handle_irq	= gic_handle_irq,
	.init_machine	= stig125_dt_init,
	.init_irq	= stm_of_gic_init,
	.dt_compat	= stig125_dt_match,
MACHINE_END
