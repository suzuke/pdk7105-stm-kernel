/*
 * Copyright (C) 2012 STMicroelectronics Limited
 *
 * Author: Nunzio Raciti <nunzio.raciti@st.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/stm/platform.h>
#include <linux/stm/stig125.h>

#include <asm/mach-types.h>
#include <asm/hardware/gic.h>
#include <asm/hardware/cache-l2x0.h>
#include <mtd/mtd-abi.h>
#include <linux/mtd/partitions.h>

#include <mach/soc-stig125.h>
#include <mach/hardware.h>

static struct platform_device b2078_leds = {
	.name = "leds-gpio",
	.id = -1,
	.dev.platform_data = &(struct gpio_led_platform_data) {
		.num_leds = 5,
		.leds = (struct gpio_led[]) {
			{
				.name = "LED0_RED",
				.default_trigger = "none",
				.active_low = true,
				.gpio = stm_gpio(4, 0),
			}, {
				.name = "LED1_RED",
				.default_trigger = "none",
				.active_low = true,
				.gpio = stm_gpio(4, 1),
			}, {
				.name = "LED2_YELLOW",
				.default_trigger = "heartbeat",
				.active_low = true,
				.gpio = stm_gpio(4, 2),
			}, {
				.name = "LED3_GREEN",
				.default_trigger = "default-on",
				.active_low = true,
				.gpio = stm_gpio(4, 3),
			}, {
				.name = "LED4_RED",
				.default_trigger = "none",
				.active_low = true,
				.gpio = stm_gpio(4, 4),
			}
		},
	},
};


static struct platform_device *b2078_devices[] __initdata = {
	&b2078_leds,
};

/* Serial FLASH */
static struct stm_plat_spifsm_data b2078_serial_flash =  {
	.name		= "n25q256",
	.nr_parts	= 6,
	.parts = (struct mtd_partition []) {
		{
			/* The boot partition is read-only */
			.name = "Boot",
			.size = 0x00080000,
			.offset = 0,
			.mask_flags = MTD_WRITEABLE,
		}, {
			.name = "NVM",
			.size = 0x00030000,
			.offset = MTDPART_OFS_NXTBLK,
		}, {
			.name = "eCM",
			.size = 0x00200000,
			.offset = MTDPART_OFS_NXTBLK,
		}, {
			.name = "Kernel",
			.size = 0x00280000,
			.offset = MTDPART_OFS_NXTBLK,
		}, {
			.name = "Root FS",
			.size = 0x00380000,
			.offset = MTDPART_OFS_NXTBLK,
		}, {
			.name = "Firmwares FS",
			.size = MTDPART_SIZ_FULL,
			.offset = MTDPART_OFS_NXTBLK,
		},
	},
	.capabilities = {
		/* Capabilities may be overriden by SoC configuration */
		.dual_mode = 1,
		.quad_mode = 1,
	},
};

static void stig125_spi_fst_cs(struct spi_device *spi, int value)
{
	gpio_set_value(stm_gpio(23, 3),
		(spi->mode & SPI_CS_HIGH) ? value : !value);
}

static void stig125_spi_tel_cs(struct spi_device *spi, int value)
{
	gpio_set_value(stm_gpio(16, 3),
		(spi->mode & SPI_CS_HIGH) ? value : !value);
}

static void __init b2078_init(void)
{
	platform_add_devices(b2078_devices, ARRAY_SIZE(b2078_devices));

	stig125_configure_usb(0);
	stig125_configure_fp();

	/* see schematic: i2c-tel */
	stig125_configure_ssc_i2c(STIG125_SSC(1), 100);
	/* see schematic: i2c-aux */
	stig125_configure_ssc_i2c(STIG125_SSC(4), 100);
	/* see schematic: i2c-fst - connected with the B2095 (TDA18265) @0x62 */
	stig125_configure_ssc_i2c(STIG125_SSC(5), 100);
	/* see schematic: i2c-wb */
	stig125_configure_ssc_i2c(STIG125_SSC(6), 100);


	/* see schematic: SPI_FST - connected with the B2095 (TDA18265) */
	stig125_configure_ssc_spi(STIG125_SSC(11), &(struct stig125_ssc_config){
				.spi_chipselect = stig125_spi_fst_cs });

	/* see schematic: SPI_TEL  */
	stig125_configure_ssc_spi(STIG125_TELSS_SSC,
				&(struct stig125_ssc_config){
					.spi_chipselect = stig125_spi_tel_cs });

	stig125_configure_spifsm(&b2078_serial_flash);
}

static void __init b2078_init_early(void)
{
	pr_info("STMicroelectronics STiG125 (Docsis3) MBoard initialisation\n");

	stig125_early_device_init();

	stig125_configure_asc(1, &(struct stig125_asc_config) {
			.hw_flow_control = 0,
			.is_console = 1,
			.force_m1 = 1, });
}

MACHINE_START(STM_B2078, "STMicroelectronics B2078 - STiG125 board")
	.atag_offset    = 0x100,
	.map_io		= stig125_map_io,
#ifdef CONFIG_SPARSE_IRQ
	.nr_irqs        = NR_IRQS_LEGACY,
#endif
	.init_early	= b2078_init_early,
	.init_irq       = stig125_gic_init_irq,
	.timer		= &stig125_timer,
	.handle_irq	= gic_handle_irq,
	.init_machine	= b2078_init,
	.restart        = stig125_reset,
MACHINE_END
