/*
 * Copyright (C) 2012 STMicroelectronics Limited
 *
 * Author(s): Carmelo Amoroso <carmelo.amoroso@st.com>
 *            Francesco Virlinzi <francesco.virlinzi@st.com>
 *	Srinivas Kandagatla <srinivas.kandagatla@st.com>
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

#include <linux/of_platform.h>
#include <linux/stm/stig125.h>
#include <linux/stm/soc.h>

#include <linux/stm/core_of.h>
#include <linux/stm/stm_device_of.h>

#include <asm/hardware/gic.h>
#include <asm/mach/time.h>

#include <mach/common-dt.h>
#include <mach/hardware.h>
#include <mach/soc-stig125.h>


/* SPI support for TELSS */
static unsigned int spi_core_data;

static struct spi_board_info spi_core[] =  {
	{
		.modalias = "spicore",
		.bus_num = 0,
		.chip_select = 0,
		.max_speed_hz = 4000000,
		.platform_data = &spi_core_data,
		.mode = SPI_MODE_3,
	},
};

static struct platform_device b2044_leds = {
	.name = "leds-gpio",
	.id = -1,
	.dev.platform_data = &(struct gpio_led_platform_data) {
		.num_leds = 1,
		.leds = (struct gpio_led[]) {
			{
				.name = "FP_LED",
				.default_trigger = "heartbeat",
				.gpio = stm_gpio(STIG125_SBC_PIO(2), 2),
			}
		},
	},
};

static struct platform_device *b2044_devices[] __initdata = {
	&b2044_leds,
};

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

static void __init b2044_dt_init(void)
{

	of_platform_populate(NULL, of_default_bus_match_table,
				 stig125_auxdata_lookup, NULL);

	stig125_configure_fp();

	stig125_configure_miphy(&(struct stig125_miphy_config){
			.id = 2,
			.mode = PCIE_MODE,
			.iface = UPORT_IF,
			});
	stig125_configure_pcie(1);
	stig125_configure_pcie(2);

	stig125_configure_audio(&(struct stig125_audio_config) {
			.uni_player_1_pcm_mode =
				stig125_uni_player_1_pcm_8_channels,
			.uni_player_4_spdif_enabled = 1,
			.uni_reader_0_pcm_mode =
				stig125_uni_reader_0_pcm_2_channels });

#ifdef CONFIG_SND_STM_TELSS
	stig125_configure_telss(&(struct stig125_telss_config) {
			.uniperif_tdm_player_enabled = 1,
			.uniperif_tdm_reader_enabled = 1,});
#endif
}

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


static const char *stig125_dt_match[] __initdata = {
	"st,stig125-b2044",
	NULL
};

DT_MACHINE_START(STM, "StiG125 SoC with Flattened Device Tree")
	.map_io		= stig125_map_io,
	.init_early	= core_of_early_device_init,
	.timer		= &stig125_of_timer,
	.handle_irq	= gic_handle_irq,
	.init_machine	= b2044_dt_init,
	.init_irq	= stm_of_gic_init,
	.dt_compat	= stig125_dt_match,
MACHINE_END
