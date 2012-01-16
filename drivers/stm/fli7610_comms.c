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
#include <linux/stm/fli7610.h>
#include <linux/stm/fli7610-periphs.h>
#include <linux/clk.h>
#include <asm/irq-ilc.h>
#ifdef CONFIG_ARM
#include <mach/hardware.h>
#endif

/* SSC resources ---------------------------------------------------------- */

/* Pad configuration for I2C mode */
static struct stm_pad_config fli7610_ssc_i2c_pad_configs[] = {
	[0] = {
		.gpios_num = 2,
		.gpios = (struct stm_pad_gpio []) {
			STM_PAD_PIO_BIDIR_NAMED(14, 3, 1, "SCL"),
			STM_PAD_PIO_BIDIR_NAMED(14, 4, 1, "SDA"),
		},
	},
	[1] = {
		.gpios_num = 2,
		.gpios = (struct stm_pad_gpio []) {
			STM_PAD_PIO_BIDIR_NAMED(14, 5, 1, "SCL"),
			STM_PAD_PIO_BIDIR_NAMED(14, 6, 1, "SDA"),
		},
	},
	[2] = {
		.gpios_num = 2,
		.gpios = (struct stm_pad_gpio []) {
			STM_PAD_PIO_BIDIR_NAMED(15, 0, 1, "SCL"),
			STM_PAD_PIO_BIDIR_NAMED(14, 7, 1, "SDA"),
		},
	},
	/* I2C-LPM */
	[3] = {
		.gpios_num = 2,
		.gpios = (struct stm_pad_gpio []) {
			STM_PAD_PIO_BIDIR_NAMED(6, 1, 1, "SCL"),
			STM_PAD_PIO_BIDIR_NAMED(6, 2, 1, "SDA"),
		},
	},

};

/* Pad configuration for SPI mode */
static struct stm_pad_config fli7610_ssc_spi_pad_configs[] = {
	[0] = { /* TAE SSC2 */
		.gpios_num = 3,
		.gpios = (struct stm_pad_gpio []) {
			STM_PAD_PIO_OUT(14, 7, 2),	/* SCK */
			STM_PAD_PIO_OUT(15, 0, 2),	/* MOSI */
			STM_PAD_PIO_IN(12, 5, 1),	/* MISO */
		},
	},
	/* LPM SSC0 */
	[1] = {
		.gpios_num = 3,
		.gpios = (struct stm_pad_gpio []) {
			STM_PAD_PIO_OUT(6, 3, 2),	/* SCK */
			STM_PAD_PIO_OUT(6, 4, 2),	/* MOSI */
			STM_PAD_PIO_IN(6, 6, 2),	/* MISO */
		},
	},

};

static struct platform_device fli7610_ssc_devices[] = {
	/* SSC0 is I2C only */
	[0] = {
		/* .name & .id set in fli7610_configure_ssc_*() */
		.num_resources = 2,
		.resource = (struct resource[]) {
			STM_PLAT_RESOURCE_MEM(FLI7610_TAE_SSC0_BASE, 0x110),
			FLI7610_RESOURCE_IRQ(187),
		},
		.dev.platform_data = &(struct stm_plat_ssc_data) {
			/* .pad_config_* set in fli7610_configure_ssc_*() */
		},
	},
	/* SSC1 shares I2C pads with the Int_Demod I2C interface */
	[1] = {
		/* .name & .id set in fli7610_configure_ssc_*() */
		.num_resources = 2,
		.resource = (struct resource[]) {
			STM_PLAT_RESOURCE_MEM(FLI7610_TAE_SSC1_BASE, 0x110),
			FLI7610_RESOURCE_IRQ(188),
		},
		.dev.platform_data = &(struct stm_plat_ssc_data) {
			/* .pad_config_* set in fli7610_configure_ssc_*() */
		},
	},
	/* SSC2 can be configured to operate in both I2C and SPI mode */
	[2] = {
		/* .name & .id set in fli7610_configure_ssc_*() */
		.num_resources = 2,
		.resource = (struct resource[]) {
			STM_PLAT_RESOURCE_MEM(FLI7610_TAE_SSC2_BASE, 0x110),
			FLI7610_RESOURCE_IRQ(189),
		},
		.dev.platform_data = &(struct stm_plat_ssc_data) {
			/* .pad_config_* set in fli7610_configure_ssc_*() */
		},
	},
	/* SBC SSC0 I2C and SPI mode */
	[3] = {
		/* .name & .id set in fli7610_configure_ssc_*() */
		.num_resources = 2,
		.resource = (struct resource[]) {
			STM_PLAT_RESOURCE_MEM(FLI7610_SBC_SSC0_BASE, 0x110),
			FLI7610_RESOURCE_IRQ(206),
		},
		.dev.platform_data = &(struct stm_plat_ssc_data) {
			/* .pad_config_* set in fli7610_configure_ssc_*() */
		},
	},
	/* SBC SSC1 Only SPI mode */
	[4] = {
		/* .name & .id set in fli7610_configure_ssc_*() */
		.num_resources = 2,
		.resource = (struct resource[]) {
			STM_PLAT_RESOURCE_MEM(FLI7610_SBC_SSC1_BASE, 0x110),
			FLI7610_RESOURCE_IRQ(207),
		},
		.dev.platform_data = &(struct stm_plat_ssc_data) {
			/* .pad_config_* set in fli7610_configure_ssc_*() */
		},
	}
};

static int __initdata fli7610_ssc_configured[ARRAY_SIZE(fli7610_ssc_devices)];

int __init fli7610_configure_ssc_i2c(int ssc)
{
	static int i2c_busnum;
	struct stm_plat_ssc_data *plat_data;
	struct stm_pad_config *pad_config;

	BUG_ON(ssc < 0 || ssc >= ARRAY_SIZE(fli7610_ssc_i2c_pad_configs));

	BUG_ON(fli7610_ssc_configured[ssc]);
	fli7610_ssc_configured[ssc] = 1;


	fli7610_ssc_devices[ssc].name = "i2c-stm";
	fli7610_ssc_devices[ssc].id = i2c_busnum;

	plat_data = fli7610_ssc_devices[ssc].dev.platform_data;

	pad_config = &fli7610_ssc_i2c_pad_configs[ssc];

	plat_data->pad_config = pad_config;

	/* I2C bus number reservation (to prevent any hot-plug device
	 * from using it) */
	i2c_register_board_info(i2c_busnum, NULL, 0);
	if (ssc > 2)
		clk_add_alias_platform_device(NULL, &fli7610_ssc_devices[ssc],
					"sbc_comms_clk", NULL);

	platform_device_register(&fli7610_ssc_devices[ssc]);

	return i2c_busnum++;
}

/* NOT TESTED  */
int __init fli7610_configure_ssc_spi(int ssc, struct fli7610_ssc_config *config)
{
	static int spi_busnum;
	struct fli7610_ssc_config default_config = {};
	struct stm_plat_ssc_data *plat_data;
	struct stm_pad_config *pad_config;
	BUG_ON(ssc < 0 || ssc >= ARRAY_SIZE(fli7610_ssc_devices));
	/* FLI7610 has only two instances of SPI,
	 * One in LPM at 0 and other in NON_LPM at 2 */
	BUG_ON(ssc > 3 || ssc < 2);

	BUG_ON(fli7610_ssc_configured[ssc]);
	fli7610_ssc_configured[ssc] = 1;

	if (!config)
		config = &default_config;

	fli7610_ssc_devices[ssc].name = "spi-stm";
	fli7610_ssc_devices[ssc].id = spi_busnum;

	plat_data = fli7610_ssc_devices[ssc].dev.platform_data;

	pad_config = &fli7610_ssc_spi_pad_configs[ssc - 2];

	plat_data->spi_chipselect = config->spi_chipselect;
	plat_data->pad_config = pad_config;

	if (ssc > 2)
		clk_add_alias_platform_device(NULL, &fli7610_ssc_devices[ssc]
					, "sbc_comms_clk", NULL);

	platform_device_register(&fli7610_ssc_devices[ssc]);

	return spi_busnum++;
}


/* LiRC resources --------------------------------------------------------- */

static struct platform_device fli7610_lirc_device = {
	.name = "lirc-stm",
	.id = -1,
	.num_resources = 2,
	.resource = (struct resource []) {
		STM_PLAT_RESOURCE_MEM(FLI7610_SBC_IRB_BASE, 0x234),
		FLI7610_RESOURCE_IRQ(203),
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
		.pads = &(struct stm_pad_config) {
			.gpios_num = 1,
			.gpios = (struct stm_pad_gpio []) {
				STM_PAD_PIO_IN(8, 2, 1),
			},
			.sysconfs_num = 2,
			.sysconfs = (struct stm_pad_sysconf []) {
				STM_PAD_SYSCONF(LPM_SYSCONF(1), 6, 6, 1),
				STM_PAD_SYSCONF(LPM_SYSCONF(1), 7, 7, 0),
			},
		},
		.rxuhfmode = 0,
	},
};

void __init fli7610_configure_lirc(void)
{

	static int configured;

	BUG_ON(configured++);

	platform_device_register(&fli7610_lirc_device);
}

/* PWM resources ---------------------------------------------------------- */
static struct stm_plat_pwm_data fli7610_pwm_platform_data[] =  {
	/* SAS PWM Module  */
	[0] = {
		.channel_pad_config = {
			[0] = &(struct stm_pad_config) {
				.gpios_num = 1,
				.gpios = (struct stm_pad_gpio []) {
					STM_PAD_PIO_OUT(15, 7, 1),
				},
			},
			[1] = &(struct stm_pad_config) {
				.gpios_num = 1,
				.gpios = (struct stm_pad_gpio []) {
					STM_PAD_PIO_OUT(16, 5, 1),
				},
			},
			[2] = &(struct stm_pad_config) {
				.gpios_num = 1,
				.gpios = (struct stm_pad_gpio []) {
					STM_PAD_PIO_OUT(16, 6, 1),
				},
			}
		},
	},
	/* SBC PWM Module */
	[1] = {
		.channel_pad_config = {
			[0] = &(struct stm_pad_config) {
				.gpios_num = 1,
				.gpios = (struct stm_pad_gpio []) {
					STM_PAD_PIO_OUT(8, 5, 1),
				},
			},
			[1] = &(struct stm_pad_config) {
				.gpios_num = 1,
				.gpios = (struct stm_pad_gpio []) {
					STM_PAD_PIO_OUT(8, 4, 1),
				},
			},
			[2] = &(struct stm_pad_config) {
				.gpios_num = 1,
				.gpios = (struct stm_pad_gpio []) {
					STM_PAD_PIO_OUT(3, 2, 2),
				},
			},
			[3] = &(struct stm_pad_config) {
				.gpios_num = 1,
				.gpios = (struct stm_pad_gpio []) {
					STM_PAD_PIO_OUT(3, 1, 2),
				},
			}
		},
	},
};

static struct platform_device fli7610_pwm_devices[] =  {
	[0] = {
		.name = "stm-pwm",
		.id = 0,
		.num_resources = 2,
		.resource = (struct resource[]) {
			STM_PLAT_RESOURCE_MEM(FLI7610_TAE_PWM_BASE, 0x68),
			FLI7610_RESOURCE_IRQ(200),
		},
		.dev.platform_data = &fli7610_pwm_platform_data[0],
	},
	[1] = {
		.name = "stm-pwm",
		.id = 1,
		.num_resources = 2,
		.resource = (struct resource[]) {
			STM_PLAT_RESOURCE_MEM(FLI7610_SBC_PWM_BASE, 0x68),
			FLI7610_RESOURCE_IRQ(202),
		},
		.dev.platform_data = &fli7610_pwm_platform_data[1],
	},
};

static int __initdata fli7610_pwm_configured[ARRAY_SIZE(fli7610_pwm_devices)];
void __init fli7610_configure_pwm(struct fli7610_pwm_config *config)
{
	int pwm;
	int i;
	BUG_ON(!config);
	pwm = config->pwm;
	BUG_ON(pwm < 0 || pwm >= ARRAY_SIZE(fli7610_pwm_devices));

	BUG_ON(fli7610_pwm_configured[pwm]);

	fli7610_pwm_configured[pwm] = 1;
	for (i = 0; i < STM_PLAT_PWM_NUM_CHANNELS; i++)
		fli7610_pwm_platform_data[pwm].channel_enabled[i] =
					config->enabled[i];

	platform_device_register(&fli7610_pwm_devices[pwm]);
}
