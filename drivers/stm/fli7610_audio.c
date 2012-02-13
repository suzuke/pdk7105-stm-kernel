/*
 * Copyright (c) 2011 STMicroelectronics Limited
 *
 * Author: John Boddie <john.boddie@st.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */



#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/irq.h>
#include <linux/stm/platform.h>
#include <linux/stm/fli7610.h>
#include <linux/stm/fli7610-periphs.h>
#include <sound/stm.h>
#include <asm/irq-ilc.h>


/* Audio subsystem resources ---------------------------------------------- */

/* Audio mute converters (mute/unmute output) */

static struct platform_device fli7610_snd_conv_gpio_0 = {
	.name = "snd_conv_gpio",
	.id = 0,
	.dev.platform_data = &(struct snd_stm_conv_gpio_info) {
		.group = "speakers",

		.source_bus_id = "snd_uniperif_player.0", /* LS */
		.channel_from = 0,
		.channel_to = 1,
		.format = SND_STM_FORMAT__I2S |
				SND_STM_FORMAT__SUBFRAME_32_BITS,
		.oversampling = 256,

		.mute_supported = 1,
		.mute_gpio = stm_gpio(12, 1), /* Audio mute */
		.mute_value = 0,
	},
};

/* Audio HP mute control */

static struct platform_device fli7610_snd_conv_gpio_1 = {
	.name = "snd_conv_gpio",
	.id = 1,
	.dev.platform_data = &(struct snd_stm_conv_gpio_info) {
		.group = "headphones",

		.source_bus_id = "snd_uniperif_player.2", /* HP */
		.channel_from = 0,
		.channel_to = 1,
		.format = SND_STM_FORMAT__I2S |
				SND_STM_FORMAT__SUBFRAME_32_BITS,
		.oversampling = 256,

		.mute_supported = 1,
		.mute_gpio = stm_gpio(12, 3), /* Audio head phones mute */
		.mute_value = 0,
	},
};

/* Audio dummy converters (change format/oversampling) */

static struct platform_device fli7610_snd_conv_dummy_0 = {
	.name = "snd_conv_dummy",
	.id = 1,
	.dev.platform_data = &(struct snd_stm_conv_dummy_info) {
		.group = "dummy",

		.source_bus_id = "snd_uniperif_player.1", /* AUX */
		.channel_from = 0,
		.channel_to = 1,
		.format = SND_STM_FORMAT__I2S |
				SND_STM_FORMAT__SUBFRAME_32_BITS,
		.oversampling = 256,
	},
};

static struct platform_device fli7610_snd_conv_dummy_1 = {
	.name = "snd_conv_dummy",
	.id = 2,
	.dev.platform_data = &(struct snd_stm_conv_dummy_info) {
		.group = "dummy",

		.source_bus_id = "snd_uniperif_player.3", /* AV Out */
		.channel_from = 0,
		.channel_to = 1,
		.format = SND_STM_FORMAT__I2S |
				SND_STM_FORMAT__SUBFRAME_32_BITS,
		.oversampling = 256,
	},
};

/* Bi-phase converter (outputs SPDIF) */

static struct platform_device stih415_conv_biphase = {
	.name = "snd_conv_biphase",
	.id = -1,
	.dev.platform_data = &(struct snd_stm_conv_biphase_info) {
		.source_bus_id = "snd_uniperif_player.4", /* SPDIF */
		.channel_from = 0,
		.channel_to = 1,
		.enable = { TAE_SYSCONF(161), 31, 31},
	},
};

/* SPDIF RX converter (converts SPDIF into I2S) */

static struct platform_device fli7610_conv_spdif_rx = {
	.name = "snd_conv_spdif_rx",
	.id = -1,
	.num_resources = 2,
	.resource = (struct resource []) {
		STM_PLAT_RESOURCE_MEM(0xfe012000, 0x5c),
		FLI7610_RESOURCE_IRQ(175),
	},
	.dev.platform_data = &(struct snd_stm_conv_spdif_rx_info) {
		.source_bus_id = "snd_uniperif_reader.5", /* SPDIF RX */
		.channel_from = 0,
		.channel_to = 1,
		.clock_name = "CLK_SPDIF_RX",
		.clock_rate = 48000,
	},
};

/* Uniperipheral players */

static struct snd_stm_uniperif_player_info fli7610_uni_player_0_info = {
	.name = "Uni Player #0 (LS)",
	.ver = 1,
	.card_device = 0,
	.player_type = SND_STM_UNIPERIF_PLAYER_TYPE_PCM,
	.clock_name = "CLK_256FS_FREE_RUN",
	.channels = 8,
	.fdma_name = "fdma_dmac.3",
	.fdma_initiator = 0,
	.fdma_request_line = 41,
	/* .pad_config set by fli7610_configure_audio() */
};

static struct stm_pad_config fli7610_uni_player_0_pad_config = {
	.gpios_num = 7,
	.gpios = (struct stm_pad_gpio []) {
		STM_PAD_PIO_OUT(10, 4, 1),  /* I2SA_OUT_MCLK */
		STM_PAD_PIO_OUT(10, 5, 1),  /* I2SA_OUT_LRCLK */
		STM_PAD_PIO_OUT(10, 6, 1),  /* I2SA_OUT_SCLK */
		STM_PAD_PIO_OUT(10, 0, 1),  /* I2SA_OUT_D0 */
		STM_PAD_PIO_OUT(10, 1, 1),  /* I2SA_OUT_D1 */
		STM_PAD_PIO_OUT(10, 2, 1),  /* I2SA_OUT_D2 */
		STM_PAD_PIO_OUT(10, 3, 1),  /* I2SA_OUT_D3 */
	},
};

static struct platform_device fli7610_uni_player_0 = {
	.name = "snd_uniperif_player",
	.id = 0,
	.num_resources = 2,
	.resource = (struct resource []) {
		STM_PLAT_RESOURCE_MEM(0xfe002000, 0x150),
		FLI7610_RESOURCE_IRQ(140),
	},
	.dev.platform_data = &fli7610_uni_player_0_info,
};

static struct snd_stm_uniperif_player_info fli7610_uni_player_1_info = {
	.name = "Uni Player #1 (AUX)",
	.ver = 1,
	.card_device = 1,
	.player_type = SND_STM_UNIPERIF_PLAYER_TYPE_PCM,
	.clock_name = "CLK_256FS_FREE_RUN",
	.channels = 2,
	.fdma_name = "fdma_dmac.3",
	.fdma_initiator = 0,
	.fdma_request_line = 42,
	/* .pad_config set by fli7610_configure_audio() */
};

static struct stm_pad_config fli7610_uni_player_1_pad_config = {
	.gpios_num = 4,
	.gpios = (struct stm_pad_gpio []) {
		STM_PAD_PIO_OUT(11, 1, 1),  /* I2SB_OUT_MCLK */
		STM_PAD_PIO_OUT(11, 2, 1),  /* I2SB_OUT_LRCLK */
		STM_PAD_PIO_OUT(11, 3, 1),  /* I2SB_OUT_SCLK */
		STM_PAD_PIO_OUT(11, 0, 1),  /* I2SB_OUT_D */
	},
};

static struct platform_device fli7610_uni_player_1 = {
	.name = "snd_uniperif_player",
	.id = 1,
	.num_resources = 2,
	.resource = (struct resource []) {
		STM_PLAT_RESOURCE_MEM(0xfe003000, 0x150),
		FLI7610_RESOURCE_IRQ(141),
	},
	.dev.platform_data = &fli7610_uni_player_1_info,
};

static struct platform_device fli7610_uni_player_2 = {
	.name = "snd_uniperif_player",
	.id = 2,
	.num_resources = 2,
	.resource = (struct resource []) {
		STM_PLAT_RESOURCE_MEM(0xfe004000, 0x150),
		FLI7610_RESOURCE_IRQ(142),
	},
	.dev.platform_data = &(struct snd_stm_uniperif_player_info) {
		.name = "Uni Player #2 (HP)",
		.ver = 1,
		.card_device = 2,
		.player_type = SND_STM_UNIPERIF_PLAYER_TYPE_PCM,
		.clock_name = "CLK_256FS_FREE_RUN",
		.channels = 2,
		.fdma_name = "fdma_dmac.3",
		.fdma_initiator = 0,
		.fdma_request_line = 43,
	},
};

static struct platform_device fli7610_uni_player_3 = {
	.name = "snd_uniperif_player",
	.id = 3,
	.num_resources = 2,
	.resource = (struct resource []) {
		STM_PLAT_RESOURCE_MEM(0xfe00a000, 0x150),
		FLI7610_RESOURCE_IRQ(143),
	},
	.dev.platform_data = &(struct snd_stm_uniperif_player_info) {
		.name = "Uni Player #3 (AV Out)",
		.ver = 1,
		.card_device = 3,
		.player_type = SND_STM_UNIPERIF_PLAYER_TYPE_PCM,
		.clock_name = "CLK_256FS_FREE_RUN",
		.channels = 2,
		.fdma_name = "fdma_dmac.3",
		.fdma_initiator = 0,
		.fdma_request_line = 44,
	},
};

static struct snd_stm_uniperif_player_info fli7610_uni_player_4_info = {
	.name = "Uni Player #4 (SPDIF)",
	.ver = 1,
	.card_device = 4,
	.clock_name = "CLK_256FS_FREE_RUN",
	.player_type = SND_STM_UNIPERIF_PLAYER_TYPE_SPDIF,
	.channels = 2,
	.fdma_name = "fdma_dmac.3",
	.fdma_initiator = 0,
	.fdma_request_line = 40,
	/* .pad_config set by fli7610_configure_audio() */
};

static struct stm_pad_config fli7610_uni_player_4_pad_config = {
	.gpios_num = 1,
	.gpios = (struct stm_pad_gpio []) {
		STM_PAD_PIO_OUT(10, 7, 1),
	},
};

static struct platform_device fli7610_uni_player_4 = {
	.name = "snd_uniperif_player",
	.id = 4,
	.num_resources = 2,
	.resource = (struct resource []) {
		STM_PLAT_RESOURCE_MEM(0xfe006000, 0x150),
		FLI7610_RESOURCE_IRQ(144),
	},
	.dev.platform_data = &fli7610_uni_player_4_info,
};

/* Uniperipheral readers */

static struct snd_stm_pcm_reader_info fli7610_uni_reader_0_info = {
	.name = "Uni Reader #0 (SPDIF)",
	.ver = 1,
	.card_device = 5,
	.channels = 2,
	.fdma_name = "fdma_dmac.3",
	.fdma_initiator = 0,
	.fdma_request_line = 46,
	/* .pad_config set by fli7610_configure_audio() */
};

static struct stm_pad_config fli7610_uni_reader_0_pad_config = {
	.gpios_num = 1,
	.gpios = (struct stm_pad_gpio []) {
		STM_PAD_PIO_IN(12, 0, 1),
	},
};

static struct platform_device fli7610_uni_reader_0 = {
	.name = "snd_uniperif_reader",
	.id = 5,
	.num_resources = 2,
	.resource = (struct resource []) {
		STM_PLAT_RESOURCE_MEM(0xfe005000, 0x150),
		FLI7610_RESOURCE_IRQ(190),
	},
	.dev.platform_data = &fli7610_uni_reader_0_info,
};

static struct platform_device fli7610_uni_reader_1 = {
	.name = "snd_uniperif_reader",
	.id = 6,
	.num_resources = 2,
	.resource = (struct resource []) {
		STM_PLAT_RESOURCE_MEM(0xfe00b000, 0x150),
		FLI7610_RESOURCE_IRQ(191),
	},
	.dev.platform_data = &(struct snd_stm_pcm_reader_info) {
		.name = "Uni Reader #1 (HDMI1)",
		.ver = 1,
		.card_device = 6,
		.channels = 8,
		.fdma_name = "fdma_dmac.3",
		.fdma_initiator = 0,
		.fdma_request_line = 47,
	},
};

static struct platform_device fli7610_uni_reader_2 = {
	.name = "snd_uniperif_reader",
	.id = 7,
	.num_resources = 2,
	.resource = (struct resource []) {
		STM_PLAT_RESOURCE_MEM(0xfe00c000, 0x150),
		FLI7610_RESOURCE_IRQ(192),
	},
	.dev.platform_data = &(struct snd_stm_pcm_reader_info) {
		.name = "Uni Reader #2 (I2S)",
		.ver = 1,
		.card_device = 7,
		.channels = 6,
		.fdma_name = "fdma_dmac.3",
		.fdma_initiator = 0,
		.fdma_request_line = 48,
	},
};

static struct platform_device fli7610_uni_reader_3 = {
	.name = "snd_uniperif_reader",
	.id = 8,
	.num_resources = 2,
	.resource = (struct resource []) {
		STM_PLAT_RESOURCE_MEM(0xfe00d000, 0x150),
		FLI7610_RESOURCE_IRQ(193),
	},
	.dev.platform_data = &(struct snd_stm_pcm_reader_info) {
		.name = "Uni Reader #3 (SIF)",
		.ver = 1,
		.card_device = 8,
		.channels = 2,
		.fdma_name = "fdma_dmac.3",
		.fdma_initiator = 0,
		.fdma_request_line = 49,
	},
};

static struct platform_device fli7610_uni_reader_4 = {
	.name = "snd_uniperif_reader",
	.id = 9,
	.num_resources = 2,
	.resource = (struct resource []) {
		STM_PLAT_RESOURCE_MEM(0xfe00e000, 0x150),
		FLI7610_RESOURCE_IRQ(194),
	},
	.dev.platform_data = &(struct snd_stm_pcm_reader_info) {
		.name = "Uni Reader #4 (AV In)",
		.ver = 1,
		.card_device = 9,
		.channels = 2,
		.fdma_name = "fdma_dmac.3",
		.fdma_initiator = 0,
		.fdma_request_line = 50,
	},
};

static struct platform_device fli7610_uni_reader_5 = {
	.name = "snd_uniperif_reader",
	.id = 10,
	.num_resources = 2,
	.resource = (struct resource []) {
		STM_PLAT_RESOURCE_MEM(0xfe00f000, 0x150),
		FLI7610_RESOURCE_IRQ(197),
	},
	.dev.platform_data = &(struct snd_stm_pcm_reader_info) {
		.name = "Uni Reader #5 (HDMI2)",
		.ver = 1,
		.card_device = 10,
		.channels = 8,
		.fdma_name = "fdma_dmac.3",
		.fdma_initiator = 0,
		.fdma_request_line = 45,
	},
};

static struct platform_device fli7610_uni_reader_6 = {
	.name = "snd_uniperif_reader",
	.id = 11,
	.num_resources = 2,
	.resource = (struct resource []) {
		STM_PLAT_RESOURCE_MEM(0xfe010000, 0x150),
		FLI7610_RESOURCE_IRQ(198),
	},
	.dev.platform_data = &(struct snd_stm_pcm_reader_info) {
		.name = "Uni Reader #6 (DP1)",
		.ver = 1,
		.card_device = 11,
		.channels = 8,
		.fdma_name = "fdma_dmac.3",
		.fdma_initiator = 0,
		.fdma_request_line = 51,
	},
};

/* Devices */

static struct platform_device *fli7610_audio_devices[] __initdata = {
	&fli7610_snd_conv_gpio_0,
	&fli7610_snd_conv_gpio_1,
	&fli7610_snd_conv_dummy_0,
	&fli7610_snd_conv_dummy_1,
	&stih415_conv_biphase,
	&fli7610_uni_player_0,  /* LS */
	&fli7610_uni_player_1,  /* AUX */
	&fli7610_uni_player_2,  /* HP */
	&fli7610_uni_player_3,  /* AV Out */
	&fli7610_uni_player_4,  /* SPDIF */

	&fli7610_conv_spdif_rx,
	&fli7610_uni_reader_0,  /* SPDIF */
	&fli7610_uni_reader_1,  /* HDMI1 */
	&fli7610_uni_reader_2,  /* I2S */
	&fli7610_uni_reader_3,  /* SIF */
	&fli7610_uni_reader_4,  /* AV In */
	&fli7610_uni_reader_5,  /* HDMI2 */
	&fli7610_uni_reader_6,  /* DP1 */
};

static int __init fli7610_audio_devices_setup(void)
{
	if (stm_soc_type() != CPU_FLI7610) {
		BUG();
		return -ENODEV;
	}

	return platform_add_devices(fli7610_audio_devices,
					ARRAY_SIZE(fli7610_audio_devices));
}
device_initcall(fli7610_audio_devices_setup);

/* Configuration */

void __init fli7610_configure_audio(struct fli7610_audio_config *config)
{
	static int configured;

	BUG_ON(configured);
	configured = 1;

	if (config->uni_player_0_pcm_mode >
			fli7610_uni_player_0_pcm_disabled) {
		int unused = 4 - config->uni_player_0_pcm_mode;

		fli7610_uni_player_0_info.pad_config =
				&fli7610_uni_player_0_pad_config;

		fli7610_uni_player_0_pad_config.gpios_num -= unused;

		fli7610_uni_player_0_info.channels =
				config->uni_player_0_pcm_mode * 2;
	}

	if (config->uni_player_1_pcm_mode >
			fli7610_uni_player_1_pcm_disabled) {
		fli7610_uni_player_0_info.pad_config =
				&fli7610_uni_player_1_pad_config;
	}

	if (config->uni_player_4_spdif_enabled) {
		fli7610_uni_player_4_info.pad_config =
				&fli7610_uni_player_4_pad_config;
	}

	if (config->uni_reader_0_spdif_enabled) {
		fli7610_uni_reader_0_info.pad_config =
				&fli7610_uni_reader_0_pad_config;
	}
}
