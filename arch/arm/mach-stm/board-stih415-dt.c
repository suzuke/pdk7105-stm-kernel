
 /*
 * Copyright (C) 2013 STMicroelectronics Limited.
 *  Author: Srinivas Kandagatla <srinivas.kandagatla@st.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include <linux/of_platform.h>
#include <linux/of_platform.h>
#include <linux/of_gpio.h>
#include <linux/stm/stih415.h>
#include <linux/stm/mpe41_of_devices.h>
#include <linux/stm/stm_device_of.h>
#include <linux/stm/soc.h>
#include <linux/stm/core_of.h>
#include <asm/hardware/gic.h>
#include <asm/mach/time.h>
#include <mach/common-dt.h>
#include <mach/soc-stih415.h>
#include <mach/mpe41.h>

struct of_dev_auxdata stih415_auxdata_lookup[] __initdata = {
	OF_DEV_AUXDATA("st,fdma", 0xfd600000, "stm-fdma.0", NULL),
	OF_DEV_AUXDATA("st,fdma", 0xfd620000, "stm-fdma.1", NULL),
	OF_DEV_AUXDATA("st,fdma", 0xfd640000, "stm-fdma.2", NULL),
	OF_DEV_AUXDATA("st,fdma", 0xfea00000, "stm-fdma.3", NULL),
	OF_DEV_AUXDATA("st,fdma", 0xfea20000, "stm-fdma.4", NULL),
	OF_DEV_AUXDATA("st,usb", 0xfe100000, "stm-usb.0", NULL),
	OF_DEV_AUXDATA("st,usb", 0xfe200000, "stm-usb.1", NULL),
	OF_DEV_AUXDATA("st,usb", 0xfe300000, "stm-usb.2", NULL),
	OF_DEV_AUXDATA("st,sdhci", 0xfe81e000, "sdhci-stm.0",
		 &mmc_platform_data),
	OF_DEV_AUXDATA("snps,dwmac", 0xfe810000, "stmmaceth.0",
		 &ethernet_platform_data),
	OF_DEV_AUXDATA("snps,dwmac", 0xfef08000, "stmmaceth.1",
		 &ethernet_platform_data),
	OF_DEV_AUXDATA("st,miphy-mp", 0xfe804000, "st,miphy-mp",
		 &pcie_mp_platform_data),
	OF_DEV_AUXDATA("st,pcie", 0xfe800000, "st,pcie",
		 &stm_pcie_config),
	OF_DEV_AUXDATA("st,coproc-st40", 0, "st,coproc-st40",
		 &mpe41_st40_coproc_data),
	{}
};

/* Setup the Timer */
static void __init stih415_timer_init(void)
{
	stih415_plat_clk_init();
	stih415_plat_clk_alias_init();
	stm_of_timer_init();
}

struct sys_timer stih415_timer = {
	.init	= stih415_timer_init,
};

void __init stih415_dt_init(void)
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
				 stih415_auxdata_lookup, NULL);

	return;
}

static int __init stih415_late_devices_setup(void)
{
	struct device_node *np = of_find_compatible_node(
					NULL, NULL, "virtual,mdio-gpio");
	if (np)
		of_platform_device_create(np, "mdio-gpio", NULL);

	return 0;
}

late_initcall(stih415_late_devices_setup);

static const char *stih415_dt_match[] __initdata = {
	"st,stih415",
	NULL
};

DT_MACHINE_START(STM, "StiH415 SoC with Flattened Device Tree")
	.map_io		= stih415_map_io,
	.init_early	= core_of_early_device_init,
	.timer		= &stih415_timer,
	.handle_irq	= gic_handle_irq,
	.init_machine	= stih415_dt_init,
	.init_irq	= stm_of_gic_init,
	.restart	= stih415_reset,
	.dt_compat	= stih415_dt_match,
MACHINE_END
