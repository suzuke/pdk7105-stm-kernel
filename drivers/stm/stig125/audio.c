/*
 * Copyright (c) 2012 STMicroelectronics Limited
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


/* Audio subsystem resources ---------------------------------------------- */

/* Internal DAC */

static struct platform_device stig125_conv_dac = {
	.name = "snd_conv_dac_sc",
	.id = -1,
	.dev.platform_data = &(struct snd_stm_conv_dac_sc_info) {
		.source_bus_id = "snd_uni_player.0", /* DAC */
		.channel_from = 0,
		.channel_to = 1,
		.nrst = { SYSCONF(917), 0, 0 },
		.sb  = { SYSCONF(917), 1, 1 },
		.mute_l = { SYSCONF(917), 2, 2 },
		.mute_r = { SYSCONF(917), 2, 3 },
	},
};

/* Bi-phase converter (outputs SPDIF) */

static struct platform_device stig125_conv_biphase = {
	.name = "snd_conv_biphase",
	.id = -1,
	.dev.platform_data = &(struct snd_stm_conv_biphase_info) {
		.source_bus_id = "snd_uni_player.4",
		.channel_from = 0,
		.channel_to = 1,
		.enable = { SYSCONF(918), 7, 7 },
	},
};

/* Uniperipheral players */

static struct platform_device stig125_uni_player_0 = {
	.name = "snd_uni_player",
	.id = 0,
	.num_resources = 2,
	.resource = (struct resource []) {
		STM_PLAT_RESOURCE_MEM(0xfe285000, 0x150),
		STIG125_RESOURCE_IRQ(67),
	},
	.dev.platform_data = &(struct snd_stm_uniperif_player_info) {
		.name = "Uni Player #0 (DAC)",
		.ver = 1,
		.card_device = 0,
		.player_type = SND_STM_UNIPERIF_PLAYER_TYPE_PCM,
		.channels = 2,
		.fdma_name =  "stm-fdma.1",
		.fdma_initiator = 0,
		.fdma_request_line = 37,
	},
};

static struct snd_stm_uniperif_player_info stig125_uni_player_1_info = {
	.name = "Uni Player #1 (PIO)",
	.ver = 1,
	.card_device = 1,
	.player_type = SND_STM_UNIPERIF_PLAYER_TYPE_PCM,
	.channels = 8,
	.fdma_name =  "stm-fdma.1",
	.fdma_initiator = 0,
	.fdma_request_line = 38,
	/* .pad_config set by stih415_configure_audio() */
};

static struct stm_pad_config stig125_uni_player_1_pad_config = {
	.gpios_num = 7,
	.gpios = (struct stm_pad_gpio []) {
		STM_PAD_PIO_OUT(17, 2, 1),	/* MCLK */
		STM_PAD_PIO_OUT(17, 3, 1),	/* LRCLK */
		STM_PAD_PIO_OUT(17, 4, 1),	/* SCLK */
		STM_PAD_PIO_OUT(17, 5, 1),	/* DATA0 */
		STM_PAD_PIO_OUT(17, 6, 1),	/* DATA1 */
		STM_PAD_PIO_OUT(17, 7, 1),	/* DATA2 */
		STM_PAD_PIO_OUT(18, 0, 1),	/* DATA3 */
	},
};

static struct platform_device stig125_uni_player_1 = {
	.name = "snd_uni_player",
	.id = 1,
	.num_resources = 2,
	.resource = (struct resource []) {
		STM_PLAT_RESOURCE_MEM(0xfe286000, 0x150),
		STIG125_RESOURCE_IRQ(68),
	},
	.dev.platform_data = &stig125_uni_player_1_info,
};

static struct platform_device stig125_uni_player_2 = {
	.name = "snd_uni_player",
	.id = 2,
	.num_resources = 2,
	.resource = (struct resource []) {
		STM_PLAT_RESOURCE_MEM(0xfe287000, 0x150),
		STIG125_RESOURCE_IRQ(69),
	},
	.dev.platform_data = &(struct snd_stm_uniperif_player_info) {
		.name = "Uni Player #2 (HDMI)",
		.ver = 1,
		.card_device = 2,
		.player_type = SND_STM_UNIPERIF_PLAYER_TYPE_HDMI,
		.channels = 8,
		.parking_enabled = 1,
		.fdma_name =  "stm-fdma.1",
		.fdma_initiator = 0,
		.fdma_request_line = 39,
	},
};

static struct snd_stm_uniperif_player_info stig125_uni_player_3_info = {
	.name = "Uni Player #3 (CH3/4MOD)",
	.ver = 1,
	.card_device = 3,
	.player_type = SND_STM_UNIPERIF_PLAYER_TYPE_PCM,
	.channels = 2,
	.fdma_name = "stm-fdma.1",
	.fdma_initiator = 0,
	.fdma_request_line = 40,
	/* .pad_config set by stih415_configure_audio() */
};

static struct platform_device stig125_uni_player_3 = {
	.name = "snd_uni_player",
	.id = 3,
	.num_resources = 2,
	.resource = (struct resource []) {
		STM_PLAT_RESOURCE_MEM(0xfe289000, 0x1000),
		STIG125_RESOURCE_IRQ(70),
	},
	.dev.platform_data = &stig125_uni_player_3_info,
};


static struct snd_stm_uniperif_player_info stig125_uni_player_4_info = {
	.name = "Uni Player #4 (SPDIF)",
	.ver = 1,
	.card_device = 4,
	.player_type = SND_STM_UNIPERIF_PLAYER_TYPE_SPDIF,
	.channels = 2,
	.fdma_name =  "stm-fdma.1",
	.fdma_initiator = 0,
	.fdma_request_line = 41,
	/* .pad_config set by stih415_configure_audio() */
};

static struct stm_pad_config stig125_uni_player_4_pad_config = {
	.gpios_num = 1,
	.gpios = (struct stm_pad_gpio []) {
		STM_PAD_PIO_OUT(18, 1, 1),	/* SPDIF out */
	},
};

static struct platform_device stig125_uni_player_4 = {
	.name = "snd_uni_player",
	.id = 4,
	.num_resources = 2,
	.resource = (struct resource []) {
		STM_PLAT_RESOURCE_MEM(0xfe28A000, 0x150),
		STIG125_RESOURCE_IRQ(71),
	},
	.dev.platform_data = &stig125_uni_player_4_info,
};

/* Uniperipheral reader */

static struct snd_stm_pcm_reader_info stig125_uni_reader_0_info = {
	.name = "Uni Reader #0 (PIO)",
	.ver = 1,
	.card_device = 5,
	.channels = 2,
	.fdma_name = "stm-fdma.1",
	.fdma_initiator = 0,
	.fdma_request_line = 42,
	/* .pad_config set by stig125_configure_audio() */
};

static struct stm_pad_config stig125_uni_reader_0_pad_config = {
	.gpios_num = 6,
	.gpios = (struct stm_pad_gpio []) {
		STM_PAD_PIO_IN(18, 2, 1),	/* LRCLK */
		STM_PAD_PIO_IN(18, 3, 1),	/* SCLK */
		STM_PAD_PIO_IN(18, 4, 1),	/* DATA0 */
		STM_PAD_PIO_IN(18, 5, 1),	/* DATA1 */
		STM_PAD_PIO_IN(18, 6, 1),	/* DATA2 */
		STM_PAD_PIO_IN(18, 7, 1),	/* DATA3 */
	},
};

static struct platform_device stig125_uni_reader_0 = {
	.name = "snd_uni_reader",
	.id = 5,
	.num_resources = 2,
	.resource = (struct resource []) {
		STM_PLAT_RESOURCE_MEM(0xfe288000, 0x150),
		STIG125_RESOURCE_IRQ(72),
	},
	.dev.platform_data = &stig125_uni_reader_0_info,
};

/* Devices */

static struct platform_device *stig125_audio_devices[] __initdata = {
	&stig125_conv_dac,
	&stig125_conv_biphase,
	&stig125_uni_player_0,  /* DAC */
	&stig125_uni_player_1,  /* PIO */
	&stig125_uni_player_2,  /* HDMI */
	&stig125_uni_player_3,  /* CH3/4MOD */
	&stig125_uni_player_4,  /* SPDIF */
	&stig125_uni_reader_0,  /* Reader */
};

static int __init stig125_audio_devices_setup(void)
{
	if (!stm_soc_is_stig125()) {
		BUG();
		return -ENODEV;
	}

	return platform_add_devices(stig125_audio_devices,
					ARRAY_SIZE(stig125_audio_devices));
}
device_initcall(stig125_audio_devices_setup);

/* Configuration */

void __init stig125_configure_audio(struct stig125_audio_config *config)
{
	static int configured;

	BUG_ON(configured);
	configured = 1;

	if (config->uni_player_1_pcm_mode > stig125_uni_player_1_pcm_disabled) {
		int unused = 4 - config->uni_player_1_pcm_mode;

		stig125_uni_player_1_info.pad_config =
				&stig125_uni_player_1_pad_config;

		stig125_uni_player_1_pad_config.gpios_num -= unused;
	}

	if (config->uni_player_4_spdif_enabled) {
		stig125_uni_player_4_info.pad_config =
				&stig125_uni_player_4_pad_config;
	}

	if (config->uni_reader_0_pcm_mode > stig125_uni_reader_0_pcm_disabled) {
		int unused = 4 - config->uni_reader_0_pcm_mode;

		stig125_uni_reader_0_info.pad_config =
				&stig125_uni_reader_0_pad_config;

		stig125_uni_reader_0_pad_config.gpios_num -= unused;
	}
}
