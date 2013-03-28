/*
 * Copyright (C) 2012 STMicroelectronics Limited
 *
 * Author: Francesco Virlinzi <francesco.virlinzist.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/i2c.h>
#include <linux/stm/pad.h>
#include <linux/stm/platform.h>
#include <linux/stm/stig125.h>
#include <linux/stm/stig125-periphs.h>
#include <linux/clk.h>
#include <asm/irq-ilc.h>
#ifdef CONFIG_ARM
#include <mach/hardware.h>
#endif

#ifndef CONFIG_OF
/* All the Drivers are now configured using device trees so,
 * Please start using device trees */
#warning  "This code will disappear soon, you should use device trees"
/*
 * - 3 SSCs in the SBC
 * - 2 SSCs standalone @ 0xFE2A8000
 * - 8 SSCs in Comms block
 * - 1 SSC in TelSS block
 */
#define SSC_NUMBER		14
/* Pad configuration for I2C mode */
#define I2C_PAD(_id, scl_port, scl_pin, scl_alt,			\
	sda_port, sda_pin, sda_alt)					\
	[_id] = {							\
		.gpios_num = 2,						\
		.gpios = (struct stm_pad_gpio []) {			\
			STM_PAD_PIO_BIDIR_NAMED(scl_port, scl_pin,	\
				scl_alt, "SCL"),			\
			STM_PAD_PIO_BIDIR_NAMED(sda_port, sda_pin,	\
				sda_alt, "SDA"),			\
		},							\
	}

static struct stm_pad_config stig125_ssc_i2c_pad_configs[SSC_NUMBER] = {
	I2C_PAD(0, 5, 5, 3, 5, 4, 3),
	I2C_PAD(1, 6, 3, 2, 6, 4, 2),
	I2C_PAD(2, 7, 0, 1, 7, 1, 1),
	I2C_PAD(3, 6, 1, 3, 6, 0, 3),
	I2C_PAD(4, 2, 3, 3, 2, 2, 3),
	I2C_PAD(5, 19, 0, 1, 19, 1, 1),
	I2C_PAD(6, 19, 2, 1, 19, 3, 1),
	I2C_PAD(7, 13, 0, 2, 13, 1, 2),
	I2C_PAD(8, 13, 3, 2, 13, 4, 2),
	I2C_PAD(9, 14, 0, 2, 14, 1, 2),
	/* SSC_on SBC */
	I2C_PAD(10, 26, 0, 1, 26, 1, 1),
	I2C_PAD(11, 24, 5, 2, 24, 4, 2),
	I2C_PAD(12, 25, 2, 2, 25, 1, 2),
	/* SSC in TELSS */
	I2C_PAD(STIG125_TELSS_SSC, 16, 0, 1, 16, 1, 1),
};

/* Pad configuration for SPI mode */
#define SPI_PAD(_id, scl_port, scl_pin, scl_alt,			\
	sda_port, sda_pin, sda_alt, sdo_port, sdo_pin, sdo_alt)		\
	[_id] = {							\
		.gpios_num = 3,						\
		.gpios = (struct stm_pad_gpio []) {			\
			STM_PAD_PIO_BIDIR_NAMED(scl_port, scl_pin,	\
				scl_alt, "SCL"),			\
			STM_PAD_PIO_BIDIR_NAMED(sda_port, sda_pin,	\
				sda_alt, "SDA"),			\
			STM_PAD_PIO_BIDIR_NAMED(sdo_port, sdo_pin,	\
				sdo_alt, "SDO"),			\
		},							\
	}

/*
 * Pad configuration for TELSS SPI mode.
 * - Bi-directional ports are unable to supply required signal strength so
 *   uni-directional ports are used.
 */
#define SPI_PAD_TELSS(_id, scl_port, scl_pin, scl_alt,			\
	sda_port, sda_pin, sda_alt, sdo_port, sdo_pin, sdo_alt)		\
	[_id] = {							\
		.gpios_num = 3,						\
		.gpios = (struct stm_pad_gpio []) {			\
			STM_PAD_PIO_OUT_NAMED(scl_port, scl_pin,	\
				scl_alt, "SCL"),			\
			STM_PAD_PIO_OUT_NAMED(sda_port, sda_pin,	\
				sda_alt, "SDA"),			\
			STM_PAD_PIO_IN_NAMED(sdo_port, sdo_pin,	\
				sdo_alt, "SDO"),			\
		},							\
	}

static struct stm_pad_config stig125_ssc_spi_pad_configs[SSC_NUMBER] = {
	SPI_PAD(0, 5, 5, 3, 5, 4, 3, 5, 3, 3),
	SPI_PAD(1, 6, 3, 2, 6, 4, 2, 6, 5, 3),
	SPI_PAD(2, 7, 0, 1, 7, 1, 1, 7, 2, 1),
	SPI_PAD(3, 6, 1, 3, 6, 0, 3, 5, 7, 3),
	SPI_PAD(4, 2, 3, 3, 2, 2, 3, 2, 1, 3),
	SPI_PAD(5, 19, 0, 1, 19, 1, 1, 20, 0, 2),
	SPI_PAD(6, 19, 2, 1, 19, 3, 1, 20, 1, 2),
	SPI_PAD(7, 13, 0, 2, 13, 1, 2, 13, 2, 2),
	SPI_PAD(8, 13, 3, 2, 13, 4, 2, 13, 5, 2),
	SPI_PAD(9, 14, 0, 2, 14, 1, 2, 14, 2, 2),
	/* SSC_on SBC */
	SPI_PAD(10, 26, 0, 1, 26, 1, 1, 26, 2, 1),
	SPI_PAD(11, 24, 5, 2, 24, 4, 2, 24, 3, 2),
	SPI_PAD(12, 25, 2, 2, 25, 1, 2, 24, 7, 2),
	/* NO SPI mode on SSC_12 */
	SPI_PAD_TELSS(STIG125_TELSS_SSC, 16, 0, 1, 16, 1, 1, 16, 2, 1),
};

#define SSC_DEVICE(id, base, irq)					\
	[id] = {							\
		.num_resources = 2,					\
		.resource = (struct resource[]) {			\
			STM_PLAT_RESOURCE_MEM(base, 0x110),		\
			STIG125_RESOURCE_IRQ(irq),			\
		},							\
		.dev.platform_data = &(struct stm_plat_ssc_data) {	\
			.i2c_speed = 100,				\
		},							\
	}

static struct platform_device stig125_ssc_devices[SSC_NUMBER] = {
	/* SSCs in Comms block */
	SSC_DEVICE(0, STIG125_SSC0_BASE, 106),
	SSC_DEVICE(1, STIG125_SSC1_BASE, 107),
	SSC_DEVICE(2, STIG125_SSC2_BASE, 108),
	SSC_DEVICE(3, STIG125_SSC3_BASE, 109),
	SSC_DEVICE(4, STIG125_SSC4_BASE, 110),
	SSC_DEVICE(5, STIG125_SSC5_BASE, 111),
	SSC_DEVICE(6, STIG125_SSC6_BASE, 112),
	SSC_DEVICE(7, STIG125_SSC7_BASE, 113),
	/* SSCs stand-alone */
	SSC_DEVICE(8, STIG125_SSC8_BASE, 122),
	SSC_DEVICE(9, STIG125_SSC9_BASE, 123),
	/* SSCs on SBC */
	SSC_DEVICE(10, STIG125_SBC_SSC0_BASE, 139),
	SSC_DEVICE(11, STIG125_SBC_SSC1_BASE, 140),
	SSC_DEVICE(12, STIG125_SBC_SSC2_BASE, 141),
	/* TELSS SSC */
	[STIG125_TELSS_SSC] = {
		.num_resources = 2,
		.resource = (struct resource[]) {
			STM_PLAT_RESOURCE_MEM(STIG125_TELSS_SSC_BASE, 0x110),
			{
				.start = STIG125_IRQMUX(63),
				.end = STIG125_IRQMUX(63),
				.flags = IORESOURCE_IRQ,
			},
		},
		.dev.platform_data = &(struct stm_plat_ssc_data) {
		},
	}
};

static void __init stig125_ssc_set_clk(int ssc)
{
	switch (ssc) {
	case 8 ... 9:
		/* SSC-8 and SSC-9 are using a dedicate ssc_comms_clk */
		clk_add_alias_platform_device(NULL, &stig125_ssc_devices[ssc],
			"ssc_comms_clk", NULL);
		break;
	case STIG125_SBC_SSC(0) ... STIG125_SBC_SSC(2):
		clk_add_alias_platform_device(NULL, &stig125_ssc_devices[ssc],
			"sbc_comms_clk", NULL);
		break;
	case STIG125_TELSS_SSC:
		/* TELSS-SSC is using a dedicate telss_comms_clk */
		clk_add_alias_platform_device(NULL, &stig125_ssc_devices[ssc],
			"telss_comms_clk", NULL);
		break;
	default:
		break;
	}
}

static int __initdata stig125_ssc_configured[ARRAY_SIZE(stig125_ssc_devices)];

int __init stig125_configure_ssc_i2c(int ssc, unsigned i2c_bus_speed)
{
	int ret;
	static int i2c_busnum;
	struct stm_plat_ssc_data *plat_data;

	BUG_ON(ssc < 0 || ssc >= ARRAY_SIZE(stig125_ssc_devices));
	BUG_ON(stig125_ssc_configured[ssc]);

	ret = i2c_register_board_info(i2c_busnum, NULL, 0);
	if (ret)
		return ret;

	stig125_ssc_devices[ssc].name = "i2c-stm";
	stig125_ssc_devices[ssc].id = i2c_busnum++;
	plat_data = stig125_ssc_devices[ssc].dev.platform_data;
	plat_data->pad_config = &stig125_ssc_i2c_pad_configs[ssc];
	plat_data->i2c_speed = i2c_bus_speed;

	stig125_ssc_set_clk(ssc);

	ret = platform_device_register(&stig125_ssc_devices[ssc]);
	return ret;
}

int __init stig125_configure_ssc_spi(int ssc, struct stig125_ssc_config *config)
{
	int ret = 0;
	static int spi_busnum;
	struct stig125_ssc_config default_config = {};
	struct stm_plat_ssc_data *plat_data;

	BUG_ON(ssc < 0 || ssc >= ARRAY_SIZE(stig125_ssc_devices));
	BUG_ON(stig125_ssc_configured[ssc]);


	if (!config)
		config = &default_config;

	stig125_ssc_devices[ssc].name = "spi-stm";
	stig125_ssc_devices[ssc].id = spi_busnum++;
	plat_data = stig125_ssc_devices[ssc].dev.platform_data;
	plat_data->pad_config = &stig125_ssc_spi_pad_configs[ssc];
	plat_data->spi_chipselect = config->spi_chipselect;

	stig125_ssc_set_clk(ssc);

	ret = platform_device_register(&stig125_ssc_devices[ssc]);
	return ret;
}

/* LiRC resources --------------------------------------------------------- */

static struct platform_device stig125_lirc_device = {
	.name = "lirc-stm",
	.id = -1,
	.num_resources = 2,
	.resource = (struct resource []) {
		STM_PLAT_RESOURCE_MEM(STIG125_SBC_IRB_BASE, 0x234),
		STIG125_RESOURCE_IRQ(136),
	},
	.dev.platform_data = &(struct stm_plat_lirc_data) {
		/* The clock settings will be calculated by
		 * the driver from the system clock */
		.irbclock       = 0, /* use current_cpu data */
		.irbclkdiv      = 0, /* automatically calculate */
		.irbperiodmult  = 0,
		.irbperioddiv   = 0,
		.irbontimemult  = 0,
		.irbontimediv   = 0,
		.irbrxmaxperiod = 0x5000,
		.sysclkdiv      = 1,
		.rxpolarity     = 1,
	},
};

void __init stig125_configure_lirc(struct stig125_lirc_config *config)
{
	static int configured;
	struct stig125_lirc_config default_config = {};
	struct stm_plat_lirc_data *plat_data =
			stig125_lirc_device.dev.platform_data;
	struct stm_pad_config *pad_config;

	BUG_ON(configured);
	configured = 1;

	if (!config)
		config = &default_config;

	pad_config = stm_pad_config_alloc(3, 2);
	BUG_ON(!pad_config);

	plat_data->txenabled = config->tx_enabled || config->tx_od_enabled;
	plat_data->dev_config = kzalloc(sizeof(struct stm_device_config),
					 GFP_KERNEL);
	plat_data->dev_config->pad_config = pad_config;

	/* IRB Enabled */
	stm_pad_config_add_sysconf(pad_config, LPM_SYSCONF_BANK,
						1, 6, 6, 1);

	/* IRB in Normal mode */
	stm_pad_config_add_sysconf(pad_config, LPM_SYSCONF_BANK,
						1, 7, 7, 0);
	switch (config->rx_mode) {
	case stig125_lirc_rx_disabled:
		/* Nothing to do */
		break;
	case stig125_lirc_rx_mode_ir:
		plat_data->rxuhfmode = 0;
		stm_pad_config_add_pio_in(pad_config, 25, 3, 2);
		break;
	case stig125_lirc_rx_mode_uhf:
		plat_data->rxuhfmode = 1;
		stm_pad_config_add_pio_in(pad_config, 25, 4, 2);
		break;
	default:
		BUG();
		break;
	}
	if (config->tx_enabled)
		stm_pad_config_add_pio_out(pad_config, 25, 5, 2);

	if (config->tx_od_enabled)
		stm_pad_config_add_pio_out(pad_config, 25, 6, 2);

	clk_add_alias_platform_device(NULL, &stig125_lirc_device,
		"sbc_comms_clk", NULL);

	platform_device_register(&stig125_lirc_device);
}

/* PWM resources ---------------------------------------------------------- */
static struct stm_plat_pwm_data stig125_pwm_platform_data =  {
	/* SBC PWM Module  */
	.channel_pad_config = {
		[0] = &(struct stm_pad_config) {
			.gpios_num = 1,
			.gpios = (struct stm_pad_gpio []) {
				STM_PAD_PIO_OUT(STIG125_SBC_PIO(1), 3, 1),
			},
		},
		[1] = &(struct stm_pad_config) {
			.gpios_num = 1,
			.gpios = (struct stm_pad_gpio []) {
				STM_PAD_PIO_OUT(STIG125_SBC_PIO(2), 7, 1),
			},
		},
	},
};

static struct platform_device stig125_pwm_devices =  {
	.name = "stm-pwm",
	.id = 0,
	.num_resources = 2,
	.resource = (struct resource[]) {
		STM_PLAT_RESOURCE_MEM(STIG125_SBC_PWM_BASE, 0x68),
		STIG125_RESOURCE_IRQ(137),
	},
		.dev.platform_data = &stig125_pwm_platform_data,
};

void __init stig125_configure_pwm(struct stig125_pwm_config *config)
{
	BUG_ON(!config);


	stig125_pwm_platform_data.channel_enabled[0] =
			config->out0_enabled;
	stig125_pwm_platform_data.channel_enabled[1] =
				config->out1_enabled;

	platform_device_register(&stig125_pwm_devices);
}

#endif /* CONFIG_OF */
