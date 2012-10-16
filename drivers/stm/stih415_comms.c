/*
 * (c) 2011 STMicroelectronics Limited
 *
 * Author: Srinivas Kandagatla <srinivas.kandagatla@st.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */



#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/i2c.h>
#include <linux/stm/pad.h>
#include <linux/stm/emi.h>
#include <linux/stm/platform.h>
#include <linux/stm/stih415.h>
#include <linux/stm/stih415-periphs.h>
#include <linux/clk.h>
#include <asm/irq-ilc.h>
#ifdef CONFIG_ARM
#include <mach/hardware.h>
#endif

/* SSC resources ---------------------------------------------------------- */

/* Pad configuration for I2C mode */
static struct stm_pad_config stih415_ssc_i2c_pad_configs[] = {
	[0] = {
		.gpios_num = 2,
		.gpios = (struct stm_pad_gpio []) {
			STM_PAD_PIO_BIDIR_NAMED(9, 2, 1, "SCL"),
			STM_PAD_PIO_BIDIR_NAMED(9, 3, 1, "SDA"),
		},
	},
	[1] = {
		.gpios_num = 2,
		.gpios = (struct stm_pad_gpio []) {
			STM_PAD_PIO_BIDIR_NAMED(12, 0, 1, "SCL"),
			STM_PAD_PIO_BIDIR_NAMED(12, 1, 1, "SDA"),
		},
	},
	[2] = {
		.gpios_num = 2,
		.gpios = (struct stm_pad_gpio []) {
			STM_PAD_PIO_BIDIR_NAMED(8, 6, 5, "SCL"),
			STM_PAD_PIO_BIDIR_NAMED(8, 7, 5, "SDA"),
		},
	},
	[3] = {
		.gpios_num = 2,
		.gpios = (struct stm_pad_gpio []) {
			STM_PAD_PIO_BIDIR_NAMED(13, 0, 1, "SCL"),
			STM_PAD_PIO_BIDIR_NAMED(13, 1, 1, "SDA"),
		},
	},
	[4] = {
		.gpios_num = 2,
		.gpios = (struct stm_pad_gpio []) {
			STM_PAD_PIO_BIDIR_NAMED(10, 5, 4, "SCL"),
			STM_PAD_PIO_BIDIR_NAMED(10, 6, 4, "SDA"),
		},
	},
	[5] = {
		.gpios_num = 2,
		.gpios = (struct stm_pad_gpio []) {
			STM_PAD_PIO_BIDIR_NAMED(14, 4, 3, "SCL"),
			STM_PAD_PIO_BIDIR_NAMED(14, 5, 3, "SDA"),
		},
	},
	[6] = {
		.gpios_num = 2,
		.gpios = (struct stm_pad_gpio []) {
			STM_PAD_PIO_BIDIR_NAMED(17, 1, 3, "SCL"),
			STM_PAD_PIO_BIDIR_NAMED(17, 2, 3, "SDA"),
		},
	},
	/* SBC SSC0 */
	[7] = {
		.gpios_num = 2,
		.gpios = (struct stm_pad_gpio []) {
			STM_PAD_PIO_BIDIR_NAMED(4, 5, 1, "SCL"),
			STM_PAD_PIO_BIDIR_NAMED(4, 6, 1, "SDA"),
		},
	},
	/* SBC SSC1 */
	[8] = {
		.gpios_num = 2,
		.gpios = (struct stm_pad_gpio []) {
			STM_PAD_PIO_BIDIR_NAMED(3, 2, 2, "SCL"),
			STM_PAD_PIO_BIDIR_NAMED(3, 1, 2, "SDA"),
		},
	},
	/* SBC SSC2 */
	[9] = {
		.gpios_num = 2,
		.gpios = (struct stm_pad_gpio []) {
			STM_PAD_PIO_BIDIR_NAMED(3, 7, 2, "SCL"),
			STM_PAD_PIO_BIDIR_NAMED(3, 6, 2, "SDA"),
		},
	},

};

/* Pad configuration for SPI mode */
static struct stm_pad_config stih415_ssc_spi_pad_configs[] = {
	[0] = {
		.gpios_num = 3,
		.gpios = (struct stm_pad_gpio []) {
			STM_PAD_PIO_OUT(9, 2, 1),	/* SCK */
			STM_PAD_PIO_OUT(9, 3, 1),	/* MOSI */
			STM_PAD_PIO_IN(9, 6, 1),	/* MISO */
		},
	},
	[1] = {
		.gpios_num = 3,
		.gpios = (struct stm_pad_gpio []) {
			STM_PAD_PIO_OUT(12, 0, 1),	/* SCK */
			STM_PAD_PIO_OUT(12, 1, 1),	/* MOSI */
			STM_PAD_PIO_IN(11, 7, 1),	/* MISO */
		},
	},
	[2] = {
		.gpios_num = 3,
		.gpios = (struct stm_pad_gpio []) {
			STM_PAD_PIO_OUT(8, 6, 5),	/* SCK */
			STM_PAD_PIO_OUT(8, 7, 5),	/* MOSI */
			STM_PAD_PIO_IN(8, 5, 5),	/* MISO */
		},
	},
	[3] = {
		.gpios_num = 3,
		.gpios = (struct stm_pad_gpio []) {
			STM_PAD_PIO_OUT(13, 0, 1),	/* SCK */
			STM_PAD_PIO_OUT(13, 1, 1),	/* MOSI */
			STM_PAD_PIO_IN(13, 2, 1),	/* MISO */
		},
	},
	[4] = {
		.gpios_num = 3,
		.gpios = (struct stm_pad_gpio []) {
			STM_PAD_PIO_OUT(10, 5, 4),	/* SCK */
			STM_PAD_PIO_OUT(10, 6, 4),	/* MOSI */
			STM_PAD_PIO_IN(10, 7, 4),	/* MISO */
		},
	},
	[5] = {
		.gpios_num = 3,
		.gpios = (struct stm_pad_gpio []) {
			STM_PAD_PIO_OUT(14, 4, 3),	/* SCK */
			STM_PAD_PIO_OUT(14, 5, 3),	/* MOSI */
			STM_PAD_PIO_IN(14, 6, 3),	/* MISO */
		},
	},
	[6] = {
		.gpios_num = 3,
		.gpios = (struct stm_pad_gpio []) {
			STM_PAD_PIO_OUT(17, 1, 3),	/* SCK */
			STM_PAD_PIO_OUT(17, 2, 3),	/* MOSI */
			STM_PAD_PIO_IN(17, 3, 3),	/* MISO */
		},
	},
	/* SBC SSC0 */
	[7] = {
		.gpios_num = 3,
		.gpios = (struct stm_pad_gpio []) {
			STM_PAD_PIO_OUT(4, 7, 1),	/* SCK */
			STM_PAD_PIO_OUT(4, 6, 1),	/* MOSI */
			STM_PAD_PIO_IN(4, 5, 1),	/* MISO */
		},
	},
	/* SBC SSC1 */
	[8] = {
		.gpios_num = 3,
		.gpios = (struct stm_pad_gpio []) {
			STM_PAD_PIO_OUT(3, 2, 2),	/* SCK */
			STM_PAD_PIO_OUT(3, 1, 2),	/* MOSI */
			STM_PAD_PIO_IN(3, 0, 2),	/* MISO */
		},
	},
	/* SBC SSC2 */
	[9] = {
		.gpios_num = 3,
		.gpios = (struct stm_pad_gpio []) {
			STM_PAD_PIO_OUT(3, 7, 2),	/* SCK */
			STM_PAD_PIO_OUT(3, 6, 2),	/* MOSI */
			STM_PAD_PIO_IN(3, 4, 2),	/* MISO */
		},
	},

};

static struct platform_device stih415_ssc_devices[] = {
	[0] = {
		/* .name & .id set in stih415_configure_ssc_*() */
		.num_resources = 2,
		.resource = (struct resource[]) {
			STM_PLAT_RESOURCE_MEM(STIH415_SSC0_BASE, 0x110),
			STIH415_RESOURCE_IRQ(187),
		},
		.dev.platform_data = &(struct stm_plat_ssc_data) {
			/* .pad_config_* set in stih415_configure_ssc_*() */
		},
	},
	[1] = {
		/* .name & .id set in stih415_configure_ssc_*() */
		.num_resources = 2,
		.resource = (struct resource[]) {
			STM_PLAT_RESOURCE_MEM(STIH415_SSC1_BASE, 0x110),
			STIH415_RESOURCE_IRQ(188),
		},
		.dev.platform_data = &(struct stm_plat_ssc_data) {
			/* .pad_config_* set in stih415_configure_ssc_*() */
		},
	},
	[2] = {
		/* .name & .id set in stih415_configure_ssc_*() */
		.num_resources = 2,
		.resource = (struct resource[]) {
			STM_PLAT_RESOURCE_MEM(STIH415_SSC2_BASE, 0x110),
			STIH415_RESOURCE_IRQ(189),
		},
		.dev.platform_data = &(struct stm_plat_ssc_data) {
			/* .pad_config_* set in stih415_configure_ssc_*() */
		},
	},
	[3] = {
		/* .name & .id set in stih415_configure_ssc_*() */
		.num_resources = 2,
		.resource = (struct resource[]) {
			STM_PLAT_RESOURCE_MEM(STIH415_SSC3_BASE, 0x110),
			STIH415_RESOURCE_IRQ(190),
		},
		.dev.platform_data = &(struct stm_plat_ssc_data) {
			/* .pad_config_* set in stih415_configure_ssc_*() */
		},
	},
	[4] = {
		/* .name & .id set in stih415_configure_ssc_*() */
		.num_resources = 2,
		.resource = (struct resource[]) {
			STM_PLAT_RESOURCE_MEM(STIH415_SSC4_BASE, 0x110),
			STIH415_RESOURCE_IRQ(191),
		},
		.dev.platform_data = &(struct stm_plat_ssc_data) {
			/* .pad_config_* set in stih415_configure_ssc_*() */
		},
	},
	[5] = {
		/* .name & .id set in stih415_configure_ssc_*() */
		.num_resources = 2,
		.resource = (struct resource[]) {
			STM_PLAT_RESOURCE_MEM(STIH415_SSC5_BASE, 0x110),
			STIH415_RESOURCE_IRQ(192),
		},
		.dev.platform_data = &(struct stm_plat_ssc_data) {
			/* .pad_config_* set in stih415_configure_ssc_*() */
		},
	},
	[6] = {
		/* .name & .id set in stih415_configure_ssc_*() */
		.num_resources = 2,
		.resource = (struct resource[]) {
			STM_PLAT_RESOURCE_MEM(STIH415_SSC6_BASE, 0x110),
			STIH415_RESOURCE_IRQ(193),
		},
		.dev.platform_data = &(struct stm_plat_ssc_data) {
			/* .pad_config_* set in stih415_configure_ssc_*() */
		},
	},
	/* SBC SSC0 */
	[7] = {
		/* .name & .id set in stih415_configure_ssc_*() */
		.num_resources = 2,
		.resource = (struct resource[]) {
			STM_PLAT_RESOURCE_MEM(STIH415_SBC_SSC0_BASE, 0x110),
			STIH415_RESOURCE_IRQ(206),
		},
		.dev.platform_data = &(struct stm_plat_ssc_data) {
			/* .pad_config_* set in stih415_configure_ssc_*() */
		},
	},
	/* SBC SSC1 */
	[8] = {
		/* .name & .id set in stih415_configure_ssc_*() */
		.num_resources = 2,
		.resource = (struct resource[]) {
			STM_PLAT_RESOURCE_MEM(STIH415_SBC_SSC1_BASE, 0x110),
			STIH415_RESOURCE_IRQ(207),
		},
		.dev.platform_data = &(struct stm_plat_ssc_data) {
			/* .pad_config_* set in stih415_configure_ssc_*() */
		},
	},
	/* SBC SSC2 */
	[9] = {
		/* .name & .id set in stih415_configure_ssc_*() */
		.num_resources = 2,
		.resource = (struct resource[]) {
			STM_PLAT_RESOURCE_MEM(STIH415_SBC_SSC2_BASE, 0x110),
			STIH415_RESOURCE_IRQ(208),
		},
		.dev.platform_data = &(struct stm_plat_ssc_data) {
			/* .pad_config_* set in stih415_configure_ssc_*() */
		},
	},
};

static int __initdata stih415_ssc_configured[ARRAY_SIZE(stih415_ssc_devices)];

int __init stih415_configure_ssc_i2c(int ssc, struct stih415_ssc_config *config)
{
	static int i2c_busnum;
	struct stm_plat_ssc_data *plat_data;
	struct stm_pad_config *pad_config;

	BUG_ON(ssc < 0 || ssc >= ARRAY_SIZE(stih415_ssc_devices));

	BUG_ON(stih415_ssc_configured[ssc]);
	stih415_ssc_configured[ssc] = 1;


	stih415_ssc_devices[ssc].name = "i2c-stm";
	stih415_ssc_devices[ssc].id = i2c_busnum;

	plat_data = stih415_ssc_devices[ssc].dev.platform_data;

	pad_config = &stih415_ssc_i2c_pad_configs[ssc];

	plat_data->pad_config = pad_config;
	if (config)
		plat_data->i2c_speed = config->i2c_speed;

	/* I2C bus number reservation (to prevent any hot-plug device
	 * from using it) */
	i2c_register_board_info(i2c_busnum, NULL, 0);
	if (ssc > 5)
		clk_add_alias_platform_device(NULL, &stih415_ssc_devices[ssc],
			"sbc_comms_clk", NULL);

	platform_device_register(&stih415_ssc_devices[ssc]);

	return i2c_busnum++;
}

/* NOT TESTED  */
int __init stih415_configure_ssc_spi(int ssc, struct stih415_ssc_config *config)
{
	static int spi_busnum;
	struct stih415_ssc_config default_config = {};
	struct stm_plat_ssc_data *plat_data;
	struct stm_pad_config *pad_config;

	BUG_ON(ssc < 0 || ssc >= ARRAY_SIZE(stih415_ssc_devices));

	BUG_ON(stih415_ssc_configured[ssc]);
	stih415_ssc_configured[ssc] = 1;

	if (!config)
		config = &default_config;

	stih415_ssc_devices[ssc].name = "spi-stm";
	stih415_ssc_devices[ssc].id = spi_busnum;

	plat_data = stih415_ssc_devices[ssc].dev.platform_data;

	pad_config = &stih415_ssc_spi_pad_configs[ssc];

	plat_data->spi_chipselect = config->spi_chipselect;
	plat_data->pad_config = pad_config;

	if (ssc > 5)
		clk_add_alias_platform_device(NULL, &stih415_ssc_devices[ssc],
			"sbc_comms_clk", NULL);

	platform_device_register(&stih415_ssc_devices[ssc]);

	return spi_busnum++;
}


/* LiRC resources --------------------------------------------------------- */

static struct platform_device stih415_lirc_device = {
	.name = "lirc-stm",
	.id = -1,
	.num_resources = 2,
	.resource = (struct resource []) {
		STM_PLAT_RESOURCE_MEM(STIH415_SBC_IRB_BASE, 0x234),
		STIH415_RESOURCE_IRQ(203),
	},
	.dev.platform_data = &(struct stm_plat_lirc_data) {
		/* The clock settings will be calculated by
		 * the driver from the system clock */
		.irbclock	= 0, /* use current_cpu data */
		.irbclkdiv	= 0, /* automatically calculate */
		.irbperiodmult	= 0,
		.irbperioddiv	= 0,
		.irbontimemult	= 0,
		.irbontimediv	= 0,
		.irbrxmaxperiod = 0x5000,
		.sysclkdiv	= 1,
		.rxpolarity	= 1,
	},
};

void __init stih415_configure_lirc(struct stih415_lirc_config *config)
{
	static int configured;
	struct stih415_lirc_config default_config = {};
	struct stm_plat_lirc_data *plat_data =
			stih415_lirc_device.dev.platform_data;
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
	case stih415_lirc_rx_disabled:
		/* Nothing to do */
		break;
	case stih415_lirc_rx_mode_ir:
		plat_data->rxuhfmode = 0;
		stm_pad_config_add_pio_in(pad_config, 4, 0, 2);
		break;
	case stih415_lirc_rx_mode_uhf:
		plat_data->rxuhfmode = 1;
		stm_pad_config_add_pio_in(pad_config, 4, 1, 2);
		break;
	default:
		BUG();
		break;
	}

	if (config->tx_enabled)
		stm_pad_config_add_pio_out(pad_config, 4, 2, 2);

	if (config->tx_od_enabled)
		stm_pad_config_add_pio_out(pad_config, 4, 3, 2);

	clk_add_alias_platform_device(NULL, &stih415_lirc_device,
		"sbc_comms_clk", NULL);

	platform_device_register(&stih415_lirc_device);
}


/* PWM resources ---------------------------------------------------------- */

static struct stm_plat_pwm_data stih415_pwm_platform_data[] =  {
	/* SAS PWM Module  */
	[0] = {
		.channel_pad_config = {
			[0] = &(struct stm_pad_config) {
				.gpios_num = 1,
				.gpios = (struct stm_pad_gpio []) {
					STM_PAD_PIO_OUT(9, 7, 2),
				},
			},
			[1] = &(struct stm_pad_config) {
				.gpios_num = 1,
				.gpios = (struct stm_pad_gpio []) {
					STM_PAD_PIO_OUT(13, 2, 2),
				},
			},
		},
	},
	/* SBC PWM Module */
	[1] = {
		.channel_pad_config = {
			[0] = &(struct stm_pad_config) {
				.gpios_num = 1,
				.gpios = (struct stm_pad_gpio []) {
					STM_PAD_PIO_OUT(3, 0, 1),
				},
			},
			[1] = &(struct stm_pad_config) {
				.gpios_num = 1,
				.gpios = (struct stm_pad_gpio []) {
					STM_PAD_PIO_OUT(4, 4, 1),
				},
			},
		},
	},
};

static struct platform_device stih415_pwm_devices[] =  {
	[0] = {
		.name = "stm-pwm",
		.id = 0,
		.num_resources = 2,
		.resource = (struct resource[]) {
			STM_PLAT_RESOURCE_MEM(STIH415_PWM_BASE, 0x68),
			STIH415_RESOURCE_IRQ(200),
		},
		.dev.platform_data = &stih415_pwm_platform_data[0],
	},
	[1] = {
		.name = "stm-pwm",
		.id = 1,
		.num_resources = 2,
		.resource = (struct resource[]) {
			STM_PLAT_RESOURCE_MEM(STIH415_SBC_PWM_BASE, 0x68),
			STIH415_RESOURCE_IRQ(202),
		},
		.dev.platform_data = &stih415_pwm_platform_data[1],
	},
};

static int __initdata stih415_pwm_configured[ARRAY_SIZE(stih415_pwm_devices)];
void __init stih415_configure_pwm(struct stih415_pwm_config *config)
{
	int pwm;

	BUG_ON(!config);
	pwm = config->pwm;
	BUG_ON(pwm < 0 || pwm >= ARRAY_SIZE(stih415_pwm_devices));

	BUG_ON(stih415_pwm_configured[pwm]);

	stih415_pwm_configured[pwm] = 1;

	stih415_pwm_platform_data[pwm].channel_enabled[0] =
			config->out0_enabled;
	stih415_pwm_platform_data[pwm].channel_enabled[1] =
				config->out1_enabled;

	platform_device_register(&stih415_pwm_devices[pwm]);
}
