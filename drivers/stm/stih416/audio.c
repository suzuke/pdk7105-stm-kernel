/*
 * Copyright (c) 2012 STMicroelectronics Limited
 *
 * Author: Francesco Virlinzi <francesco.virlinzi@st.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */



#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/irq.h>
#include <linux/stm/platform.h>
#include <linux/stm/soc.h>
#include <linux/stm/stih416.h>
#include <linux/stm/sasg2-periphs.h>
#include <sound/stm.h>
#include <asm/irq-ilc.h>


/* Audio subsystem resources ---------------------------------------------- */

/* Internal DAC */

static struct platform_device stih416_conv_dac = {
	.name = "snd_conv_dac_sc",
	.id = -1,
	.dev.platform_data = &(struct snd_stm_conv_dac_sc_info) {
		.source_bus_id = "snd_uni_player.2", /* DAC */
		.channel_from = 0,
		.channel_to = 1,
/*
 *		Orly-2 datasheet reports sysconf 2517[0] as reserved
 *		SOC designer said that to reset the IP
 *		the standard 'Reset Generator control' has to be used.
 *		In case of audio.DAC:
 *			SYSCONF(2553).[14]
 *			which isn't described in the datasheet...
 */
		.nrst = { SYSCONF(2553), 14, 14 },
		.mode = { SYSCONF(2517), 1, 2 },
		.nsb = { SYSCONF(2517), 3, 3 },
		.softmute = { SYSCONF(2517), 4, 4 },
		.pdana = { SYSCONF(2517), 5, 5 },
		.pndbg = { SYSCONF(2517), 6, 6 },
	},
};

/* Bi-phase converter (outputs SPDIF) */

static struct platform_device stih416_conv_biphase = {
	.name = "snd_conv_biphase",
	.id = -1,
	.dev.platform_data = &(struct snd_stm_conv_biphase_info) {
		.source_bus_id = "snd_uni_player.3",
		.channel_from = 0,
		.channel_to = 1,
		.enable = { SYSCONF(2519), 6, 6 },
	},
};

/* Uniperipheral players */

static struct platform_device stih416_uni_player_0 = {
	.name = "snd_uni_player",
	.id = 0,
	.num_resources = 2,
	.resource = (struct resource []) {
		STM_PLAT_RESOURCE_MEM(SASG2_PCMPLAYER_0_BASE, 0x150),
		STIH416_RESOURCE_IRQ(140),
	},
	.dev.platform_data = &(struct snd_stm_uniperif_player_info) {
		.name = "Uni Player #0 (HDMI)",
		.ver = 1,
		.card_device = 0,
		.player_type = SND_STM_UNIPERIF_PLAYER_TYPE_HDMI,
		.channels = 8,
		.parking_enabled = 1,
		.fdma_name = "stm-fdma.3",
		.fdma_initiator = 0,
		.fdma_request_line = 23,
	},
};

static struct snd_stm_uniperif_player_info stih416_uni_player_1_info = {
	.name = "Uni Player #1 (PIO)",
	.ver = 1,
	.card_device = 1,
	.player_type = SND_STM_UNIPERIF_PLAYER_TYPE_PCM,
	.channels = 8,
	.fdma_name = "stm-fdma.3",
	.fdma_initiator = 0,
	.fdma_request_line = 24,
	/* .pad_config set by stih416_configure_audio() */
};

static struct stm_pad_config stih416_uni_player_1_pad_config = {
	.gpios_num = 7,
	.gpios = (struct stm_pad_gpio []) {
		/* Pads shared with i2c-stm */
		STIH416_PAD_PIO_OUT(13, 0, 6),	/* MCLK */
		STIH416_PAD_PIO_OUT(13, 1, 6),	/* LRCLK */
		STIH416_PAD_PIO_OUT(13, 2, 6),	/* SCLK */
		STIH416_PAD_PIO_OUT(13, 3, 6),	/* DATA0 */
		/* Pads not shared */
		STIH416_PAD_PIO_OUT(15, 2, 6),	/* DATA1 */
		/* Pads shared with stasc */
		STIH416_PAD_PIO_OUT(17, 4, 6),	/* DATA2 */
		STIH416_PAD_PIO_OUT(17, 5, 6),	/* DATA3 */
	},
};

static struct platform_device stih416_uni_player_1 = {
	.name = "snd_uni_player",
	.id = 1,
	.num_resources = 2,
	.resource = (struct resource []) {
		STM_PLAT_RESOURCE_MEM(SASG2_PCMPLAYER_1_BASE, 0x150),
		STIH416_RESOURCE_IRQ(141),
	},
	.dev.platform_data = &stih416_uni_player_1_info,
};

static struct platform_device stih416_uni_player_2 = {
	.name = "snd_uni_player",
	.id = 2,
	.num_resources = 2,
	.resource = (struct resource []) {
		STM_PLAT_RESOURCE_MEM(SASG2_PCMPLAYER_2_BASE, 0x150),
		STIH416_RESOURCE_IRQ(142),
	},
	.dev.platform_data = &(struct snd_stm_uniperif_player_info) {
		.name = "Uni Player #2 (DAC)",
		.ver = 1,
		.card_device = 2,
		.player_type = SND_STM_UNIPERIF_PLAYER_TYPE_PCM,
		.channels = 2,
		.fdma_name = "stm-fdma.3",
		.fdma_initiator = 0,
		.fdma_request_line = 25,
	},
};

static struct snd_stm_uniperif_player_info stih416_uni_player_3_info = {
	.name = "Uni Player #3 (SPDIF)",
	.ver = 1,
	.card_device = 3,
	.player_type = SND_STM_UNIPERIF_PLAYER_TYPE_SPDIF,
	.channels = 2,
	.fdma_name = "stm-fdma.3",
	.fdma_initiator = 0,
	.fdma_request_line = 27,
	/* .pad_config set by stih416_configure_audio() */
};

static struct stm_pad_config stih416_uni_player_3_pad_config = {
	.gpios_num = 1,
	.gpios = (struct stm_pad_gpio []) {
		STIH416_PAD_PIO_OUT(9, 7, 1),
	},
};

static struct platform_device stih416_uni_player_3 = {
	.name = "snd_uni_player",
	.id = 3,
	.num_resources = 2,
	.resource = (struct resource []) {
		STM_PLAT_RESOURCE_MEM(SASG2_SPDIFPLAYER_BASE, 0x150),
		STIH416_RESOURCE_IRQ(144),
	},
	.dev.platform_data = &stih416_uni_player_3_info,
};

/* Uniperipheral readers */

static struct snd_stm_pcm_reader_info stih416_uni_reader_0_info = {
	.name = "Uni Reader #0 (SPDIF)",
	.ver = 1,
	.card_device = 4,
	.channels = 2,
	.fdma_name = "stm-fdma.3",
	.fdma_initiator = 0,
	.fdma_request_line = 26,
	/* .pad_config set by stih416_configure_audio() */
};

static struct stm_pad_config stih416_uni_reader_0_pad_config = {
	.gpios_num = 4,
	.gpios = (struct stm_pad_gpio []) {
		STIH416_PAD_PIO_OUT(13, 0, 7),	/* MCLK */
		STIH416_PAD_PIO_IN(13, 1, 7),	/* LRCLK */
		STIH416_PAD_PIO_IN(13, 2, 7),	/* SCLK */
		STIH416_PAD_PIO_IN(17, 5, 7),	/* DATA0 */
	},
};

static struct platform_device stih416_uni_reader_0 = {
	.name = "snd_uni_reader",
	.id = 4,
	.num_resources = 2,
	.resource = (struct resource []) {
		STM_PLAT_RESOURCE_MEM(SASG2_PCMREADER_0_BASE, 0x14c),
		STIH416_RESOURCE_IRQ(143),
	},
	.dev.platform_data = &stih416_uni_reader_0_info,
};

static struct snd_stm_pcm_reader_info stih416_uni_reader_1_info = {
	.name = "Uni Reader #1 (HDMI)",
	.ver = 1,
	.card_device = 5,
	.channels = 2,
	.fdma_name = "stm-fdma.3",
	.fdma_initiator = 0,
	.fdma_request_line = 22,
	/* .pad_config set by stih416_configure_audio() */
};

static struct platform_device stih416_uni_reader_1 = {
	.name = "snd_uni_reader",
	.id = 5,
	.num_resources = 2,
	.resource = (struct resource []) {
		STM_PLAT_RESOURCE_MEM(SASG2_PCMREADER_1_BASE, 0x14c),
		STIH416_RESOURCE_IRQ(145),
	},
	.dev.platform_data = &stih416_uni_reader_1_info,
};

/* Devices */

static struct platform_device *stih416_audio_devices[] __initdata = {
	&stih416_conv_dac,
	&stih416_conv_biphase,
	&stih416_uni_player_0,  /* HDMI */
	&stih416_uni_player_1,  /* PIO */
	&stih416_uni_player_2,  /* DAC */
	&stih416_uni_player_3,  /* SPDIF */

	&stih416_uni_reader_0,  /* SPDIF */
	&stih416_uni_reader_1,  /* HDMI */
};

static int __init stih416_audio_devices_setup(void)
{
	if (!stm_soc_is_stih416()) {
		BUG();
		return -ENODEV;
	}

	return platform_add_devices(stih416_audio_devices,
					ARRAY_SIZE(stih416_audio_devices));
}
device_initcall(stih416_audio_devices_setup);

/* Configuration */

void __init stih416_configure_audio(struct stih416_audio_config *config)
{
	static int configured;

	BUG_ON(configured);
	configured = 1;

	if (config->uni_player_1_pcm_mode >
			stih416_uni_player_1_pcm_disabled) {
		int unused = 4 - config->uni_player_1_pcm_mode;

		stih416_uni_player_1_info.pad_config =
				&stih416_uni_player_1_pad_config;

		stih416_uni_player_1_pad_config.gpios_num -= unused;
	}

	if (config->uni_player_3_spdif_enabled) {
		stih416_uni_player_3_info.pad_config =
				&stih416_uni_player_3_pad_config;
	}

	if (config->uni_reader_0_spdif_enabled) {
		stih416_uni_reader_0_info.pad_config =
				&stih416_uni_reader_0_pad_config;
	}
}
