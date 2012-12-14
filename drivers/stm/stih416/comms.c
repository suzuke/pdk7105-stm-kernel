/*
 * (c) 2012 STMicroelectronics Limited
 *
 * Author: Francesco Virlinzi <francesco.virlinzi@st.com>
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
#include <linux/stm/stih416.h>
#include <linux/stm/sasg2-periphs.h>
#include <linux/clk.h>
#include <asm/irq-ilc.h>

#include <mach/hardware.h>

/* SSC resources ---------------------------------------------------------- */
#define SSC_NUMBER		11
/* Pad configuration for I2C mode */
static struct stm_pad_config stih416_ssc_i2c_pad_configs[SSC_NUMBER] = {
	[0] = {
		.gpios_num = 2,
		/* pad programmed in stih416_configure_ssc_i2c
		 * GPIO [9][2], [9][3],[9][4],
		 * or
		 * GPIO [12][5], [12][6],[12][7],
		 */
	},
	[1] = {
		.gpios_num = 2,
		.gpios = (struct stm_pad_gpio []) {
			STIH416_PAD_PIO_BIDIR_NAMED(12, 0, 1, "SCL"),
			STIH416_PAD_PIO_BIDIR_NAMED(12, 1, 1, "SDA"),
		},
	},
	[2] = {
		.gpios_num = 2,
		/* pad programmed in stih416_configure_ssc_i2c
		 * GPIO [7][6]. [7][7] (only I2C)
		 * or
		 * GPIO [8][6], [8][7], [8][5]
		 */
	},
	[3] = {
		.gpios_num = 2,
		.gpios = (struct stm_pad_gpio []) {
			STIH416_PAD_PIO_BIDIR_NAMED(13, 0, 1, "SCL"),
			STIH416_PAD_PIO_BIDIR_NAMED(13, 1, 1, "SDA"),
		},
	},
	[4] = {
		.gpios_num = 2,
		/* pad programmed in stih416_configure_ssc_i2c
		 * GPIO [10][5], [10][6], [10][7]
		 * or
		 * GPIO [31][4], [31][5], [31][6],
		 */
	},
	[5] = {
		.gpios_num = 2,
		/* pad programmed in stih416_configure_ssc_i2c
		 * GPIO [14][1], [14][2], [14][3]
		 * or
		 * GPIO [14][4], [14][5], [14][6],
		 */
	},
	[6] = {
		.gpios_num = 2,
		.gpios = (struct stm_pad_gpio []) {
			STIH416_PAD_PIO_BIDIR_NAMED(17, 1, 3, "SCL"),
			STIH416_PAD_PIO_BIDIR_NAMED(17, 2, 3, "SDA"),
		},
	},
	[7] = {
		.gpios_num = 2,
		.gpios = (struct stm_pad_gpio []) {
			STIH416_PAD_PIO_BIDIR_NAMED(6, 2, 2, "SCL"),
			STIH416_PAD_PIO_BIDIR_NAMED(6, 3, 2, "SDA"),
		},
	},
	/* SBC SSC0 */
	[8] = {
		.gpios_num = 2,
		.gpios = (struct stm_pad_gpio []) {
			STIH416_PAD_PIO_BIDIR_NAMED(4, 5, 1, "SCL"),
			STIH416_PAD_PIO_BIDIR_NAMED(4, 6, 1, "SDA"),
		},
	},
	/* SBC SSC1 */
	[9] = {
		.gpios_num = 2,
		.gpios = (struct stm_pad_gpio []) {
			STIH416_PAD_PIO_BIDIR_NAMED(3, 2, 2, "SCL"),
			STIH416_PAD_PIO_BIDIR_NAMED(3, 1, 2, "SDA"),
		},
	},
	/* SBC SSC2 */
	[10] = {
		.gpios_num = 2,
		.gpios = (struct stm_pad_gpio []) {
			STIH416_PAD_PIO_BIDIR_NAMED(3, 7, 2, "SCL"),
			STIH416_PAD_PIO_BIDIR_NAMED(3, 6, 2, "SDA"),
		},
	},

};

/* Pad configuration for SPI mode */
static struct stm_pad_config stih416_ssc_spi_pad_configs[SSC_NUMBER] = {
	[0] = {
		.gpios_num = 3,
		/* pad programmed in stih416_configure_ssc_spi:
		 * GPIO [9][2], [9][3],[9][4],
		 * or
		 * GPIO [12][5], [12][6],[12][7],
		 */
	},
	[1] = {
		.gpios_num = 3,
		.gpios = (struct stm_pad_gpio []) {
			STIH416_PAD_PIO_OUT(12, 0, 1),	/* SCK */
			STIH416_PAD_PIO_OUT(12, 1, 1),	/* MOSI */
			STIH416_PAD_PIO_IN(11, 7, 1),	/* MISO */
		},
	},
	[2] = {
		.gpios_num = 3,
		/* pad programmed in stih416_configure_ssc_spi:
		 * GPIO [7][6]. [7][7] (only I2C)
		 * or
		 * GPIO [8][6], [8][7], [8][5]
		 */
	},
	[3] = {
		.gpios_num = 3,
		.gpios = (struct stm_pad_gpio []) {
			STIH416_PAD_PIO_OUT(13, 0, 1),	/* SCK */
			STIH416_PAD_PIO_OUT(13, 1, 1),	/* MOSI */
			STIH416_PAD_PIO_IN(13, 2, 1),	/* MISO */
		},
	},
	[4] = {
		.gpios_num = 3,
		/* pad programmed in stih416_configure_ssc_spi:
		 * GPIO [10][5], [10][6], [10][7]
		 * or
		 * GPIO [31][4], [31][5], [31][6],
		 */
	},
	[5] = {
		.gpios_num = 3,
		/* pad programmed in stih416_configure_ssc_spi:
		 * GPIO [14][1], [14][2], [14][3]
		 * or
		 * GPIO [14][4], [14][5], [14][6],
		 */
	},
	[6] = {
		.gpios_num = 3,
		.gpios = (struct stm_pad_gpio []) {
			STIH416_PAD_PIO_OUT(17, 1, 3),	/* SCK */
			STIH416_PAD_PIO_OUT(17, 2, 3),	/* MOSI */
			STIH416_PAD_PIO_IN(17, 3, 3),	/* MISO */
		},
	},
	[7] = {
		/*
		 * SSC7 can be only i2c
		 */
	},
	/* SBC SSC0 */
	[8] = {
		.gpios_num = 3,
		.gpios = (struct stm_pad_gpio []) {
			STIH416_PAD_PIO_OUT(4, 5, 1),	/* SCK */
			STIH416_PAD_PIO_OUT(4, 6, 1),	/* MOSI */
			STIH416_PAD_PIO_IN(4, 7, 1),	/* MISO */
		},
	},
	/* SBC SSC1 */
	[9] = {
		.gpios_num = 3,
		.gpios = (struct stm_pad_gpio []) {
			STIH416_PAD_PIO_OUT(3, 2, 2),	/* SCK */
			STIH416_PAD_PIO_OUT(3, 1, 2),	/* MOSI */
			STIH416_PAD_PIO_IN(3, 0, 2),	/* MISO */
		},
	},
	/* SBC SSC2 */
	[10] = {
		.gpios_num = 3,
		.gpios = (struct stm_pad_gpio []) {
			STIH416_PAD_PIO_OUT(3, 7, 2),	/* SCK */
			STIH416_PAD_PIO_OUT(3, 6, 2),	/* MOSI */
			STIH416_PAD_PIO_IN(3, 4, 2),	/* MISO */
		},
	},

};

#define SSC_DEVICE(id, base, irq)					\
	[id] = {							\
		.num_resources = 2,					\
		.resource = (struct resource[]) {			\
			STM_PLAT_RESOURCE_MEM(base, 0x110),		\
			STIH416_RESOURCE_IRQ(irq),			\
		},							\
		.dev.platform_data = &(struct stm_plat_ssc_data) {	\
		},							\
	}

static struct platform_device stih416_ssc_devices[SSC_NUMBER] = {
	SSC_DEVICE(0, SASG2_SSC0_BASE, 187),
	SSC_DEVICE(1, SASG2_SSC1_BASE, 188),
	SSC_DEVICE(2, SASG2_SSC2_BASE, 189),
	SSC_DEVICE(3, SASG2_SSC3_BASE, 190),
	SSC_DEVICE(4, SASG2_SSC4_BASE, 191),
	SSC_DEVICE(5, SASG2_SSC5_BASE, 192),
	SSC_DEVICE(6, SASG2_SSC6_BASE, 193),
	SSC_DEVICE(7, SASG2_SSC7_BASE, 194),
	/* SSC_on_SBC */
	SSC_DEVICE(8, SASG2_SBC_SSC0_BASE, 206),
	SSC_DEVICE(9, SASG2_SBC_SSC1_BASE, 207),
	SSC_DEVICE(10, SASG2_SBC_SSC2_BASE, 208),
};

static int __initdata stih416_ssc_configured[ARRAY_SIZE(stih416_ssc_devices)];

static int stih416_configure_ssc_routing(int ssc, int is_spi,
	struct stm_pad_config *pad_config, struct stih416_ssc_config *config)
{
	int ret = 0;
	if (ssc != 0 && ssc != 2 && ssc != 4 && ssc != 5)
		return -EINVAL;

	switch (ssc) {
	case 0:	/* SSC-0 */
		/* SSC-0.SCL */
		switch (config->routing.ssc0.sclk) {
		case stih416_ssc0_sclk_pio9_2:
			stm_pad_config_add_pio_bidir_named(pad_config,
				STIH416_GPIO(9), 2, 1, "SCL");
			break;
		case stih416_ssc0_sclk_pio12_5:
			stm_pad_config_add_pio_bidir_named(pad_config,
				STIH416_GPIO(12), 5, 2, "SCL");
			break;
		default:
			ret = -EINVAL;
			BUG();
			break;
		}
		/* SSC-0.SDA */
		switch (config->routing.ssc0.mtsr) {
		case stih416_ssc0_mtsr_pio9_3:
			stm_pad_config_add_pio_bidir_named(pad_config,
				STIH416_GPIO(9), 3, 1, "SDA");
			break;
		case stih416_ssc0_mtsr_pio12_6:
			stm_pad_config_add_pio_bidir_named(pad_config,
				STIH416_GPIO(12), 6, 2, "SDA");
			break;
		default:
			ret = -EINVAL;
			BUG();
			break;
		}
		/* SSC-0.SD0 */
		if (!is_spi)
			break;
		switch (config->routing.ssc0.mrst) {
		case stih416_ssc0_mrst_pio9_6:
			stm_pad_config_add_pio_bidir_named(pad_config,
				STIH416_GPIO(9), 6, 1, "SDO");
			break;
		case stih416_ssc0_mrst_pio12_7:
			stm_pad_config_add_pio_bidir_named(pad_config,
				STIH416_GPIO(12), 7, 2, "SDO");
			break;
		break;
		}
		break;
	case 2:	/* SSC-2 */
		/* SSC-2.SCL */
		switch (config->routing.ssc2.sclk) {
		case stih416_ssc2_sclk_pio7_6:
			stm_pad_config_add_pio_bidir_named(pad_config,
				STIH416_GPIO(7), 6, 2, "SCL");
			break;
		case stih416_ssc2_sclk_pio8_6:
			stm_pad_config_add_pio_bidir_named(pad_config,
				STIH416_GPIO(8), 6, 5, "SCL");
			break;
		default:
			BUG();
			break;
		}
		/* SSC-2.SDA */
		switch (config->routing.ssc2.mtsr) {
		case stih416_ssc2_mtsr_pio7_7:
			stm_pad_config_add_pio_bidir_named(pad_config,
				STIH416_GPIO(7), 7, 2, "SDA");
			break;
		case stih416_ssc2_mtsr_pio8_7:
			stm_pad_config_add_pio_bidir_named(pad_config,
				STIH416_GPIO(8), 7, 5, "SDA");
			break;
		default:
			ret = -EINVAL;
			BUG();
			break;
		}

		/* SSC-2.SD0 */
		if (!is_spi)
			break;
		switch (config->routing.ssc2.mrst) {
			/* there is no spi mode supported on GPIO 7 */
		case stih416_ssc2_mrst_pio8_5:
			stm_pad_config_add_pio_bidir_named(pad_config,
				STIH416_GPIO(8), 5, 5, "SDO");
			break;
		default:
			ret = -EINVAL;
			BUG();
			break;
		}
		break;
	case 4:	/* SSC-4 */
		/* SSC-4.SCL */
		switch (config->routing.ssc4.sclk) {
		case stih416_ssc4_sclk_pio10_5:
			stm_pad_config_add_pio_bidir_named(pad_config,
				STIH416_GPIO(10), 5, 4, "SCL");
			break;
		case stih416_ssc4_sclk_pio31_4:
			stm_pad_config_add_pio_bidir_named(pad_config,
				STIH416_GPIO(31), 4, 2, "SCL");
			break;
		default:
			ret = -EINVAL;
			BUG();
			break;
		}
		/* SSC-4.SDA */
		switch (config->routing.ssc4.mtsr) {
		case stih416_ssc4_mtsr_pio10_6:
			stm_pad_config_add_pio_bidir_named(pad_config,
				STIH416_GPIO(10), 6, 4, "SDA");
			break;
		case stih416_ssc4_mtsr_pio31_5:
			stm_pad_config_add_pio_bidir_named(pad_config,
				STIH416_GPIO(31), 5, 2, "SDA");
		default:
			ret = -EINVAL;
			BUG();
			break;
		}
		/* SSC-4.SD0 */
		if (!is_spi)
			break;
		switch (config->routing.ssc4.mrst) {
		case stih416_ssc4_mrst_pio10_7:
			stm_pad_config_add_pio_bidir_named(pad_config,
				STIH416_GPIO(10), 7, 4, "SDO");
			break;
		case stih416_ssc4_mrst_pio31_6:
			stm_pad_config_add_pio_bidir_named(pad_config,
				STIH416_GPIO(31), 6, 2, "SDO");
			break;
		default:
			ret = -EINVAL;
			BUG();
			break;
		}
		break;
	case 5:	/* SSC-5 */
		/* SSC-5.SCL */
		switch (config->routing.ssc5.sclk) {
		case stih416_ssc5_sclk_pio14_1:
			stm_pad_config_add_pio_bidir_named(pad_config,
				STIH416_GPIO(14), 1, 4, "SCL");
			break;
		case stih416_ssc5_sclk_pio14_4:
			stm_pad_config_add_pio_bidir_named(pad_config,
				STIH416_GPIO(14), 4, 3, "SCL");
			break;
		default:
			BUG();
			break;
		}
		/* SSC-5.SDA */
		switch (config->routing.ssc5.mtsr) {
		case stih416_ssc5_mtsr_pio14_2:
			stm_pad_config_add_pio_bidir_named(pad_config,
				STIH416_GPIO(14), 2, 4, "SDA");
			break;
		case stih416_ssc5_mtsr_pio14_5:
			stm_pad_config_add_pio_bidir_named(pad_config,
				STIH416_GPIO(14), 5, 3, "SDA");
			break;
		default:
			ret = -EINVAL;
			BUG();
			break;
		}

		/* SSC-5.SD0 */
		if (!is_spi)
			break;
		switch (config->routing.ssc5.mrst) {
		case stih416_ssc5_mrst_pio14_3:
			stm_pad_config_add_pio_bidir_named(pad_config,
				STIH416_GPIO(14), 3, 4, "SDO");
			break;
		case stih416_ssc5_mrst_pio14_6:
			stm_pad_config_add_pio_bidir_named(pad_config,
				STIH416_GPIO(14), 6, 3, "SDO");
			break;
		default:
			ret = -EINVAL;
			BUG();
			break;
		}
		break;

	}
	return ret;
}

int __init stih416_configure_ssc_i2c(int ssc, struct stih416_ssc_config *config)
{
	static int i2c_busnum;
	struct stm_plat_ssc_data *plat_data;
	struct stm_pad_config *pad_config;

	BUG_ON(ssc < 0 || ssc >= ARRAY_SIZE(stih416_ssc_devices));

	BUG_ON(stih416_ssc_configured[ssc]);
	stih416_ssc_configured[ssc] = 1;


	stih416_ssc_devices[ssc].name = "i2c-stm";
	stih416_ssc_devices[ssc].id = i2c_busnum;

	plat_data = stih416_ssc_devices[ssc].dev.platform_data;

	if (ssc == 0 || ssc == 2 || ssc == 4 || ssc == 5) {
		pad_config = stm_pad_config_alloc(2, 0);
		stih416_configure_ssc_routing(ssc, 0, pad_config, config);
	} else
		pad_config = &stih416_ssc_i2c_pad_configs[ssc];

	plat_data->pad_config = pad_config;

	if (config)
		plat_data->i2c_speed = config->i2c_speed;
	/* I2C bus number reservation (to prevent any hot-plug device
	 * from using it) */
	i2c_register_board_info(i2c_busnum, NULL, 0);
	if (ssc > 7)
		clk_add_alias_platform_device(NULL, &stih416_ssc_devices[ssc],
			"sbc_comms_clk", NULL);

	platform_device_register(&stih416_ssc_devices[ssc]);

	return i2c_busnum++;
}


int __init stih416_configure_ssc_spi(int ssc, struct stih416_ssc_config *config)
{
	static int spi_busnum;
	struct stih416_ssc_config default_config = {};
	struct stm_plat_ssc_data *plat_data;
	struct stm_pad_config *pad_config;

	BUG_ON(ssc < 0 || ssc >= ARRAY_SIZE(stih416_ssc_devices));

	BUG_ON(stih416_ssc_configured[ssc]);
	BUG_ON(ssc == 7); /* SSC 7 can be only I2C */

	stih416_ssc_configured[ssc] = 1;

	if (!config)
		config = &default_config;

	stih416_ssc_devices[ssc].name = "spi-stm";
	stih416_ssc_devices[ssc].id = spi_busnum;

	plat_data = stih416_ssc_devices[ssc].dev.platform_data;

	if (ssc == 0 || ssc == 2 || ssc == 4 || ssc == 5) {
		pad_config = stm_pad_config_alloc(3, 0);
		stih416_configure_ssc_routing(ssc, 1, pad_config, config);
	} else
		pad_config = &stih416_ssc_spi_pad_configs[ssc];

	plat_data->spi_chipselect = config->spi_chipselect;
	plat_data->pad_config = pad_config;

	if (ssc > 7)
		clk_add_alias_platform_device(NULL, &stih416_ssc_devices[ssc],
			"sbc_comms_clk", NULL);

	platform_device_register(&stih416_ssc_devices[ssc]);

	return spi_busnum++;
}


/* LiRC resources --------------------------------------------------------- */


static struct platform_device stih416_lirc_device = {
	.name = "lirc-stm",
	.id = -1,
	.num_resources = 2,
	.resource = (struct resource []) {
		STM_PLAT_RESOURCE_MEM(SASG2_SBC_IRB_BASE, 0x234),
		STIH416_RESOURCE_IRQ(203),
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

void __init stih416_configure_lirc(struct stih416_lirc_config *config)
{
	static int configured;
	struct stih416_lirc_config default_config = {};
	struct stm_plat_lirc_data *plat_data =
			stih416_lirc_device.dev.platform_data;
	struct stm_pad_config *pad_config;

	BUG_ON(configured);
	configured = 1;

	if (!config)
		config = &default_config;

	pad_config = stm_pad_config_alloc(3, 2);
	BUG_ON(!pad_config);

	plat_data->txenabled = config->tx_enabled || config->tx_od_enabled;
	plat_data->pads = pad_config;

	/* IRB Enabled */
	stm_pad_config_add_sysconf(pad_config, LPM_SYSCONF_BANK,
						1, 6, 6, 1);

	/* IRB in Normal mode */
	stm_pad_config_add_sysconf(pad_config, LPM_SYSCONF_BANK,
						1, 7, 7, 0);
	switch (config->rx_mode) {
	case stih416_lirc_rx_disabled:
		/* Nothing to do */
		break;
	case stih416_lirc_rx_mode_ir:
		plat_data->rxuhfmode = 0;
		stm_pad_config_add_pio_in(pad_config, STIH416_GPIO(4), 0, 2);
		break;
	case stih416_lirc_rx_mode_uhf:
		plat_data->rxuhfmode = 1;
		stm_pad_config_add_pio_in(pad_config, STIH416_GPIO(4), 1, 2);
		break;
	default:
		BUG();
		break;
	}

	if (config->tx_enabled)
		stm_pad_config_add_pio_out(pad_config, STIH416_GPIO(4), 2, 2);

	if (config->tx_od_enabled)
		stm_pad_config_add_pio_out(pad_config, STIH416_GPIO(4), 3, 2);

	clk_add_alias_platform_device(NULL, &stih416_lirc_device,
		"sbc_comms_clk", NULL);

	platform_device_register(&stih416_lirc_device);
}

/* PWM resources ---------------------------------------------------------- */
static struct stm_plat_pwm_data stih416_pwm_platform_data[] =  {
	/* SAS PWM Module  */
	[0] = {
		.channel_pad_config = {
			[0] = &(struct stm_pad_config) {
				.gpios_num = 1,
				.gpios = (struct stm_pad_gpio []) {
					STIH416_PAD_PIO_OUT(9, 7, 2),
				},
			},
			[1] = &(struct stm_pad_config) {
				.gpios_num = 1,
				.gpios = (struct stm_pad_gpio []) {
					STIH416_PAD_PIO_OUT(13, 2, 2),
				},
			},
			[2] = &(struct stm_pad_config) {
				.gpios_num = 1,
				.gpios = (struct stm_pad_gpio []) {
					STIH416_PAD_PIO_OUT(15, 2, 4),
				},
			},
			[3] = &(struct stm_pad_config) {
				.gpios_num = 1,
				.gpios = (struct stm_pad_gpio []) {
					STIH416_PAD_PIO_OUT(17, 4, 1),
				},
			},
		},
	},
	/* SBC PWM-0 Module */
	[1] = {
		.channel_pad_config = {
			[0] = &(struct stm_pad_config) {
				.gpios_num = 1,
				.gpios = (struct stm_pad_gpio []) {
					STIH416_PAD_PIO_OUT(3, 0, 1),
				},
			},
			[1] = &(struct stm_pad_config) {
				.gpios_num = 1,
				.gpios = (struct stm_pad_gpio []) {
					STIH416_PAD_PIO_OUT(4, 4, 1),
				},
			},
			[2] = &(struct stm_pad_config) {
				.gpios_num = 1,
				.gpios = (struct stm_pad_gpio []) {
					STIH416_PAD_PIO_OUT(4, 6, 3),
				},
			},
			[3] = &(struct stm_pad_config) {
				.gpios_num = 1,
				.gpios = (struct stm_pad_gpio []) {
					STIH416_PAD_PIO_OUT(4, 7, 3),
				},
			},
		},
	},
};

#define PWM_DEVICE(_id, _base, _irq)					\
	[_id] = {							\
		.name = "stm-pwm",					\
		.id = _id,						\
		.num_resources = 2,					\
		.resource = (struct resource[]) {			\
			STM_PLAT_RESOURCE_MEM(_base, 0x68),		\
			STIH416_RESOURCE_IRQ(_irq),			\
		},							\
		.dev.platform_data = &stih416_pwm_platform_data[_id],	\
	}

static struct platform_device stih416_pwm_devices[] =  {
	/* SAS PWM */
	PWM_DEVICE(0, SASG2_PWM_BASE, 200),
	/* SBC PWMs */
	PWM_DEVICE(1, SASG2_SBC_PWM_BASE, 202),
};

static int __initdata stih416_pwm_configured[ARRAY_SIZE(stih416_pwm_devices)];
void __init stih416_configure_pwm(struct stih416_pwm_config *config)
{
	int pwm;

	BUG_ON(!config);
	pwm = config->pwm;
	BUG_ON(pwm < 0 || pwm >= ARRAY_SIZE(stih416_pwm_devices));

	BUG_ON(stih416_pwm_configured[pwm]);

	stih416_pwm_configured[pwm] = 1;

	stih416_pwm_platform_data[pwm].channel_enabled[0] =
			config->out0_enabled;
	stih416_pwm_platform_data[pwm].channel_enabled[1] =
				config->out1_enabled;
	stih416_pwm_platform_data[pwm].channel_enabled[2] =
				config->out2_enabled;
	stih416_pwm_platform_data[pwm].channel_enabled[3] =
				config->out3_enabled;

	platform_device_register(&stih416_pwm_devices[pwm]);
}

/* Keyscan resources -------------------------------------------------------*/
static struct stm_pad_config stih416_keyscan_pad_config = {
	.gpios_num = 8,
	.gpios = (struct stm_pad_gpio []) {
		STIH416_PAD_PIO_IN(0, 2, 2),  /* KEYSCAN_IN[0] */
		STIH416_PAD_PIO_IN(0, 3, 2),  /* KEYSCAN_IN[1] */
		STIH416_PAD_PIO_IN(0, 4, 2),  /* KEYSCAN_IN[2] */
		STIH416_PAD_PIO_IN(2, 6, 2),  /* KEYSCAN_IN[3] */

		STIH416_PAD_PIO_OUT(1, 6, 2), /* KEYSCAN_OUT[0] */
		STIH416_PAD_PIO_OUT(1, 7, 2), /* KEYSCAN_OUT[1] */
		STIH416_PAD_PIO_OUT(0, 6, 2), /* KEYSCAN_OUT[2] */
		STIH416_PAD_PIO_OUT(2, 7, 2), /* KEYSCAN_OUT[3] */
		},
	.sysconfs_num = 1,
	.sysconfs = (struct stm_pad_sysconf []) {
		/* KEYSCAN_POWERDOWN_REQ */
		STM_PAD_SYSCONF(LPM_CONFIG(1), 8, 8, 0),
	}
};

static struct platform_device stih416_keyscan_device = {
	.name = "stm-keyscan",
	.id = -1,
	.num_resources = 2,
	.resource = (struct resource []) {
		STM_PLAT_RESOURCE_MEM(SASG2_SBC_KEYSCAN_BASE, 0x2000),
		STIH416_RESOURCE_IRQ(212),
	},
	.dev.platform_data = &(struct stm_plat_keyscan_data) {
		.pad_config = &stih416_keyscan_pad_config,
	},
};

void stih416_configure_keyscan(const struct stm_keyscan_config *config)
{
	struct stm_plat_keyscan_data *plat_data;
	int i;

	plat_data = stih416_keyscan_device.dev.platform_data;
	plat_data->keyscan_config = *config;    /* struct copy */

	for (i = config->num_in_pads; i < 4; i++)
		stih416_keyscan_pad_config.gpios[i].direction =
			stm_pad_gpio_direction_ignored;

	for (i = config->num_out_pads+4; i < 8; i++)
		stih416_keyscan_pad_config.gpios[i].direction =
			stm_pad_gpio_direction_ignored;

	clk_add_alias_platform_device(NULL, &stih416_keyscan_device,
		"sbc_comms_clk", NULL);

	platform_device_register(&stih416_keyscan_device);
}

