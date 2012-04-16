
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

#include <asm/hardware/cache-l2x0.h>
#include <asm/mach-types.h>
#include <asm/memory.h>

#include <asm/hardware/gic.h>
#include <mach/soc-fli7610.h>
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


static void __init fli76xxhdk01_init(void)
{
#ifdef CONFIG_CACHE_L2X0

	l2x0_init(__io_address(FLI7610_PL310_BASE), 0x1<<22, 0xffbfffff);
#endif
	fli7610_configure_usb(0);
	fli7610_configure_usb(1);
	fli7610_configure_usb(2);

	fli7610_configure_ssc_i2c(FLI7610_SSC(0));
	fli7610_configure_ssc_i2c(FLI7610_SSC(1));
	/* SBC I2C */
	fli7610_configure_ssc_i2c(FLI7610_SBC_SSC(0));

	fli7610_configure_lirc();

	fli7610_configure_pwm(&(struct fli7610_pwm_config) {
			.pwm = fli7610_sbc_pwm,
			.enabled[0] = 1,
			.enabled[1] = 1,
			.enabled[2] = 1, });

	fli7610_configure_audio(&(struct fli7610_audio_config) {
			.uni_player_0_pcm_mode =
					fli7610_uni_player_0_pcm_8_channels,
			.uni_player_1_pcm_mode =
					fli7610_uni_player_1_pcm_2_channels,
			.uni_player_4_spdif_enabled = 1,
			.uni_reader_0_spdif_enabled = 1, });

	/* reset */
	stm_board_reset = fli7610_reset;

	fli7610_configure_nand(&(struct stm_nand_config) {
			.driver = stm_nand_bch,
			.nr_banks = 1,
			.banks = &fli76xxhdk01_nand_flash,
			.rbn.flex_connected = 1,});

	fli7610_configure_spifsm(&fli76xxhdk01_serial_flash);

}

MACHINE_START(STM_NMHDK_FLI7610, "STMicroelectronics Newman FLI76XXHDK01")
	.atag_offset	= 0x100,
	.map_io		= fli76xxhdk01_map_io,
#ifdef CONFIG_SPARSE_IRQ
	.nr_irqs	= NR_IRQS_LEGACY,
#endif
	.init_irq	= fli7610_gic_init_irq,
	.timer		= &fli7610_timer,
	.init_machine	= fli76xxhdk01_init,
	.init_early     = fli76xxhdk01_init_early,
	.handle_irq	= gic_handle_irq,
MACHINE_END
