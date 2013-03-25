/*
 * Copyright (c) 2012,2013 STMicroelectronics Limited
 *
 * Author: John Boddie <john.boddie@st.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */


#include <linux/stm/soc.h>
#include <linux/stm/stig125.h>
#include <linux/stm/platform.h>
#include <sound/stm.h>


/*
 * Common telss word position info.
 */
static struct snd_stm_telss_timeslot_info stig125_telss_timeslot_info = {
	.word_num = 8,
	.word_pos = (struct snd_stm_telss_word_pos_info []) {
		{0x00, 0x02},			/* Word 1 MSB, LSB */
		{0x04, 0x06},			/* Word 2 MSB, LSB */
		{0x08, 0x0a},			/* Word 3 MSB, LSB */
		{0x0c, 0x0e},			/* Word 4 MSB, LSB */
		{0x10, 0x12},			/* Word 5 MSB, LSB */
		{0x14, 0x16},			/* Word 6 MSB, LSB */
		{0x18, 0x1a},			/* Word 7 MSB, LSB */
		{0x1c, 0x1e},			/* Word 8 MSB, LSB */
	},
};


/*
 * Common telss handset info (Use case 8: Application note for TDM interface).
 * See file include/sound/stm.h for macro.
 */

static struct snd_stm_telss_handset_info stig125_telss_handset_info[] = {
	SND_STM_TELSS_HANDSET_INFO(8000, 0, 2, 0, 0, 0, 1, 1, 0, 1),
	SND_STM_TELSS_HANDSET_INFO(8000, 4, 6, 0, 0, 0, 1, 1, 0, 1),
};


/*
 * Uniperipheral TDM player
 */

static struct snd_stm_uniperif_tdm_info stig125_uniperif_tdm_player_info = {
	.name = "Uniperif TDM #0 (Player)",	/* Device name */
	.ver = 0,				/* Currently unused */
	.card_device = -1,			/* Automatically assigned */

	.fdma_name =  "stm-fdma.2",		/* Name of FDMA to use */
	.fdma_channel = 1,			/* TELSS FDMA DOS v1.2 */
	.fdma_initiator = 1,			/* TELSS FDMA DOS v1.2 */
	.fdma_direction = DMA_MEM_TO_DEV,	/* DMA_MEM_TO_DEV = Player */
	.fdma_direct_conn = 1,			/* TELSS Functional Spec */
	.fdma_request_line = 30,		/* TELSS Functional Spec */

	.pad_config = NULL,			/* Set by configure function */

	.rising_edge = 1,			/* Tx is on the rising edge */
	.clk_rate = 49152000,			/* Clock rate */
	.pclk_rate = 512000,			/* PCLK: 8kHz * 8 slots * 8 */
	.fs01_rate = 8000,			/* 8kHz Fsync */
	.timeslots = 8,				/* Time slots per Fsync */
	.fs02_rate = 8000,			/* 8kHz FSync */
	.fs02_delay_clock = 6,			/* Fsync PCM clocks delay */
	.fs02_delay_timeslot = 0,		/* Delay timeslot */
	.msbit_start = 0,			/* Timeslot start position */
	.timeslot_info = &stig125_telss_timeslot_info,

	.frame_size = 2,			/* 2 words */
	.frame_count = 40,			/* 5ms * FSync / 1000 */
	.handset_count = 2,			/* Either 2/4/6/8/10 */
	.handset_info = stig125_telss_handset_info,
};

static struct stm_pad_config stig125_uniperif_tdm_player_pad_config = {
	.gpios_num = 3,
	.gpios = (struct stm_pad_gpio []) {
		STM_PAD_PIO_OUT(15, 4, 1),	/* TEL_TDM_PCLK */
		STM_PAD_PIO_OUT(15, 5, 1),	/* TEL_TDM_DTX */
		STM_PAD_PIO_OUT(16, 7, 2),	/* TEL_CODED_SLIC_FSYNC */
	},
};

static struct platform_device stig125_uniperif_tdm_player = {
	.name = "snd_uniperif_tdm",
	.id = 0,
	.num_resources = 2,
	.resource = (struct resource []) {
		STM_PLAT_RESOURCE_MEM(0xfeba4000, 0x150),
		STIG125_RESOURCE_IRQ(65),
	},
	.dev.platform_data = &stig125_uniperif_tdm_player_info,
};


/*
 * Uniperipheral TDM reader
 */

static struct snd_stm_uniperif_tdm_info stig125_uniperif_tdm_reader_info = {
	.name = "Uniperif TDM #1 (Reader)",	/* Device name */
	.ver = 0,				/* Currently unused */
	.card_device = -1,			/* Automatically assigned */

	.fdma_name = "stm-fdma.2",		/* Name of FDMA to use */
	.fdma_channel = 2,			/* TELSS FDMA DOS v1.2 */
	.fdma_initiator = 1,			/* TELSS FDMA DOS v1.2 */
	.fdma_direction = DMA_DEV_TO_MEM,	/* DMA_DEV_TO_MEM = Reader */
	.fdma_direct_conn = 1,			/* TELSS Functional Spec */
	.fdma_request_line = 29,		/* TELSS Functional Spec */

	.pad_config = NULL,			/* Set by configure function */

	.rising_edge = 0,			/* Rx is on the falling edge */
	.timeslots = 8,				/* Time slots per Fsync */
	.msbit_start = 0,			/* Timeslot start position */
	.timeslot_info = &stig125_telss_timeslot_info,

	.frame_size = 2,			/* 2 words */
	.frame_count = 40,			/* 5ms * FSync / 1000 */
	.handset_count = 2,			/* Either 2/4/6/8/10 */
	.handset_info = stig125_telss_handset_info,
};

static struct stm_pad_config stig125_uniperif_tdm_reader_pad_config = {
	.gpios_num = 1,
	.gpios = (struct stm_pad_gpio []) {
		STM_PAD_PIO_IN(15, 6, 1),	/* TEL_TDM_DRX */
	},
};

static struct platform_device stig125_uniperif_tdm_reader = {
	.name = "snd_uniperif_tdm",
	.id = 1,
	.num_resources = 2,
	.resource = (struct resource []) {
		STM_PLAT_RESOURCE_MEM(0xfeba5000, 0x150),
		STIG125_RESOURCE_IRQ(66),
	},
	.dev.platform_data = &stig125_uniperif_tdm_reader_info,
};


/*
 * TELSS Glue setup
 */

static struct snd_stm_telss_glue_info stig125_telss_glue_info = {
	.name = "TELSS Glue",			/* Device name */
	.ver = 0,				/* Currently unused */
	.mode = SND_STM_TELSS_GLUE_MODE_LANTIQ,	/* Lantiq mode */
	.loopback = 0,				/* Disable loopback mode */
};

static struct platform_device stig125_telss_glue = {
	.name = "snd_telss_glue",
	.id = -1,
	.num_resources = 1,
	.resource = (struct resource []) {
		STM_PLAT_RESOURCE_MEM(0xfeba8000, 0x1000),
	},
	.dev.platform_data = &stig125_telss_glue_info,
};


/*
 * Devices
 */

static struct platform_device *stig125_telss_devices[] __initdata = {
	&stig125_uniperif_tdm_player,
	&stig125_uniperif_tdm_reader,
	&stig125_telss_glue,
};

static int __init stig125_telss_devices_setup(void)
{
	if (!stm_soc_is_stig125()) {
		BUG();
		return -ENODEV;
	}

	return platform_add_devices(stig125_telss_devices,
			ARRAY_SIZE(stig125_telss_devices));
}
device_initcall(stig125_telss_devices_setup);


/*
 * Configuration
 */

void __init stig125_configure_telss(struct stig125_telss_config *config)
{
	static int configured;

	BUG_ON(configured);
	configured = 1;

	if (config->uniperif_tdm_player_enabled) {
		/* Setup the GPIO pins */
		stig125_uniperif_tdm_player_info.pad_config =
				&stig125_uniperif_tdm_player_pad_config;
	}

	if (config->uniperif_tdm_reader_enabled) {
		/* Setup the GPIO pins */
		stig125_uniperif_tdm_reader_info.pad_config =
				&stig125_uniperif_tdm_reader_pad_config;
	}
}
