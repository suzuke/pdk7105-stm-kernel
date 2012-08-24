
/*
 * arch/arm/mach-stm/board-fli76xxhdk01.c
 *
 * Copyright (C) 2011 STMicroelectronics Limited.
 * Author: Srinivas Kandagatla <srinivas.kandagatla@st.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/io.h>
#include <linux/gpio.h>
#include <linux/gpio_keys.h>
#include <linux/input.h>
#include <linux/stm/platform.h>
#include <linux/stm/fli7610.h>
#include <linux/mtd/nand.h>
#include <linux/mtd/partitions.h>
#include <sound/stm.h>

#include <asm/mach-types.h>
#include <asm/memory.h>

#include <asm/hardware/gic.h>
#include <mach/soc-fli7610.h>
#include <mach/mpe41.h>
#include <mach/hardware.h>
#define ZB_SEL stm_gpio(3, 0)

#define CONSOLE_PORT	2

static void __init fli76xxhdk01_map_io(void)
{
	fli7610_map_io();
}

static void __init fli76xxhdk01_init_early(void)
{

	printk("STMicroelectronics FLI76XXHDK01 (Newman) initialisation\n");

	fli7610_early_device_init();
	gpio_request(ZB_SEL, "ZB_SEL");
	gpio_direction_output(ZB_SEL, 0);

	fli7610_configure_asc(CONSOLE_PORT, &(struct fli7610_asc_config) {
			.hw_flow_control = 1,
			.is_console = 1	 });
}

#ifdef CONFIG_SND
#define STA333W_REG_COUNT	3
#define STA333W_PWRDN		stm_gpio(12, 1)

static int fli76xxhdk01_sta333w_init(struct i2c_client *client, void *priv)
{
	char cmd[STA333W_REG_COUNT * 2] = {
		0x03, 0x50,
		0x05, 0xde,
		0x07, 0x2f,
	};
	struct i2c_adapter *adapter = client->adapter;
	struct i2c_msg msg[STA333W_REG_COUNT];
	int i;

	for (i = 0 ; i < STA333W_REG_COUNT ; i++) {
		msg[i].addr = client->addr;
		msg[i].flags = client->flags;
		msg[i].len = sizeof(char) * 2;
		msg[i].buf = &cmd[i*2];
	}

	if (i2c_transfer(adapter, &msg[0], STA333W_REG_COUNT) < 0)
		return -EFAULT;

	return 0;
}

static struct i2c_board_info fli76xxhdk01_snd_conv_i2c_0 = {
	I2C_BOARD_INFO("snd_conv_i2c", 0x1d),
	.platform_data = &(struct snd_stm_conv_i2c_info) {
		.group = "speakers",
		.source_bus_id = "snd_uni_player.0",
		.channel_from = 0,
		.channel_to = 1,
		.format = SND_STM_FORMAT__I2S |
				SND_STM_FORMAT__SUBFRAME_32_BITS,
		.oversampling = 256,
		.init = fli76xxhdk01_sta333w_init,
	},
};
#endif

/* NAND Flash */
static struct stm_nand_bank_data fli76xxhdk01_nand_flash = {
	.csn            = 0,
	.options        = NAND_NO_AUTOINCR,
	.bbt_options	= NAND_BBT_USE_FLASH,
	.nr_partitions  = 3,
	.partitions     = (struct mtd_partition []) {
		{
			.name   = "NAND Flash 1",
			.offset = 0,
			.size   = 0x00800000
		}, {
			.name   = "NAND Flash 2",
			.offset = MTDPART_OFS_NXTBLK,
			.size   = 0x01000000
		}, {
			.name   = "NAND Flash 3",
			.offset = MTDPART_OFS_NXTBLK,
			.size   = MTDPART_SIZ_FULL
		},
	},
	.timing_data = &(struct stm_nand_timing_data) {
		.sig_setup      = 10,		/* times in ns */
		.sig_hold       = 10,
		.CE_deassert    = 0,
		.WE_to_RBn      = 100,
		.wr_on          = 10,
		.wr_off         = 30,
		.rd_on          = 10,
		.rd_off         = 30,
		.chip_delay     = 30,		/* in us */
	},
};

/* Serial FLASH */
static struct stm_plat_spifsm_data fli76xxhdk01_serial_flash =  {
	.name		= "n25q128",
	.nr_parts	= 3,
	.parts = (struct mtd_partition []) {
		{
			.name = "Serial Flash 1",
			.size = 0x00200000,
			.offset = 0,
		}, {
			.name = "Serial Flash 2",
			.size = 0x00400000,
			.offset = MTDPART_OFS_NXTBLK,
		}, {
			.name = "Serial Flash 3",
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


static struct platform_device fli76xxhdk01_leds = {
	.name = "leds-gpio",
	.id = -1,
	.dev.platform_data = &(struct gpio_led_platform_data) {
		.num_leds = 2,
		.leds = (struct gpio_led[]) {
			{
				.name = "LED_RED",
				.default_trigger = "heartbeat",
				.gpio = stm_gpio(8, 4),
			}, {
				.name = "LED_GREEN",
				.gpio = stm_gpio(8, 5),
			}
		},
	},
};

static struct platform_device *fli76xxhdk01_devices[] __initdata = {
	&fli76xxhdk01_leds,
};

static void __init fli76xxhdk01_init(void)
{
	fli7610_configure_usb(0);
	fli7610_configure_usb(1);
	fli7610_configure_usb(2);

	/* SCL1_3V, SDA1_3V */
	fli7610_configure_ssc_i2c(FLI7610_I2C1,
			&(struct fli7610_ssc_config) {.i2c_fastmode = 0,});

	/* SCL2_3V, SDA2_3V */
	fli7610_configure_ssc_i2c(FLI7610_I2C2,
			&(struct fli7610_ssc_config) {.i2c_fastmode = 0,});

	/* SCL3_3V, SDA3_3V */
	fli7610_configure_ssc_i2c(FLI7610_I2C3,
			&(struct fli7610_ssc_config) {.i2c_fastmode = 0,});

	/* SCL_LPM_3V3, SDA_LPM_3V3 */
	fli7610_configure_ssc_i2c(FLI7610_I2C1_LPM,
			&(struct fli7610_ssc_config) {.i2c_fastmode = 0,});

	fli7610_configure_lirc();

	fli7610_configure_pwm(&(struct fli7610_pwm_config) {
			.pwm = fli7610_sbc_pwm,
			.enabled[0] = 1,
			.enabled[1] = 1,
			.enabled[2] = 1, });

#ifdef CONFIG_SND
	/* Power up the STA333W */
	gpio_request(STA333W_PWRDN, "STA333W_PWRDN");
	gpio_direction_output(STA333W_PWRDN, 1);

	/* Add a new STA333W I2C device */
	i2c_register_board_info(0, &fli76xxhdk01_snd_conv_i2c_0, 1);

	fli7610_configure_audio(&(struct fli7610_audio_config) {
			.uni_player_0_pcm_mode =
					fli7610_uni_player_0_pcm_8_channels,
			.uni_player_1_pcm_mode =
					fli7610_uni_player_1_pcm_2_channels,
			.uni_player_4_spdif_enabled = 1,
			.uni_reader_0_spdif_enabled = 1, });
#endif

	fli7610_configure_nand(&(struct stm_nand_config) {
			.driver = stm_nand_bch,
			.nr_banks = 1,
			.banks = &fli76xxhdk01_nand_flash,
			.rbn.flex_connected = 1,});

	fli7610_configure_spifsm(&fli76xxhdk01_serial_flash);

	fli7610_configure_mmc(0);

	platform_add_devices(fli76xxhdk01_devices,
		ARRAY_SIZE(fli76xxhdk01_devices));
}

MACHINE_START(STM_NMHDK_FLI7610, "STMicroelectronics Newman FLI76XXHDK01")
	.atag_offset	= 0x100,
	.map_io		= fli76xxhdk01_map_io,
#ifdef CONFIG_SPARSE_IRQ
	.nr_irqs	= NR_IRQS_LEGACY,
#endif
	.init_irq	= mpe41_gic_init_irq,
	.timer		= &mpe41_timer,
	.init_machine	= fli76xxhdk01_init,
	.init_early     = fli76xxhdk01_init_early,
	.handle_irq	= gic_handle_irq,
	.restart	= fli7610_reset,
MACHINE_END
