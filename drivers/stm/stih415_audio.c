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
#include <linux/stm/stih415.h>
#include <linux/stm/stih415-periphs.h>
#include <sound/stm.h>
#include <asm/irq-ilc.h>


/* Audio subsystem resources ---------------------------------------------- */

/* Internal DAC */

static struct platform_device stih415_conv_dac = {
	.name = "snd_conv_dac_sc",
	.id = -1,
	.dev.platform_data = &(struct snd_stm_conv_dac_sc_info) {
		.source_bus_id = "snd_uniperif_player.2", /* DAC */
		.channel_from = 0,
		.channel_to = 1,
		.nrst = { SYSCONF(329), 0, 0 },
		.mode = { SYSCONF(329), 1, 2 },
		.nsb = { SYSCONF(329), 3, 3 },
		.softmute = { SYSCONF(329), 4, 4 },
		.pdana = { SYSCONF(329), 5, 5 },
		.pndbg = { SYSCONF(329), 6, 6 },
	},
};

/* Bi-phase converter (outputs SPDIF) */

static struct platform_device stih415_conv_biphase = {
	.name = "snd_conv_biphase",
	.id = -1,
	.dev.platform_data = &(struct snd_stm_conv_biphase_info) {
		.source_bus_id = "snd_uniperif_player.3",
		.channel_from = 0,
		.channel_to = 1,
		.enable = { SYSCONF(331), 6, 6 },
	},
};

/* Uniperipheral players */

static struct platform_device stih415_uni_player_0 = {
	.name = "snd_uniperif_player",
	.id = 0,
	.num_resources = 2,
	.resource = (struct resource []) {
		STM_PLAT_RESOURCE_MEM(0xfe002000, 0x150),
		STIH415_RESOURCE_IRQ(140),
	},
	.dev.platform_data = &(struct snd_stm_uniperif_player_info) {
		.name = "Uni Player #0 (HDMI)",
		.ver = 1,
		.card_device = 0,
		.player_type = SND_STM_UNIPERIF_PLAYER_TYPE_HDMI,
		.clock_name = "CLKS_B_PCM_FSYN0",
		.channels = 8,
		.fdma_name = "fdma_dmac.3",
		.fdma_initiator = 0,
		.fdma_request_line = 23,
	},
};

static struct snd_stm_uniperif_player_info stih415_uni_player_1_info = {
	.name = "Uni Player #1 (PIO)",
	.ver = 1,
	.card_device = 1,
	.player_type = SND_STM_UNIPERIF_PLAYER_TYPE_PCM,
	.clock_name = "CLKS_B_PCM_FSYN1",
	.channels = 8,
	.fdma_name = "fdma_dmac.3",
	.fdma_initiator = 0,
	.fdma_request_line = 24,
	/* .pad_config set by stih415_configure_audio() */
};

static struct stm_pad_config stih415_uni_player_1_pad_config = {
	.gpios_num = 7,
	.gpios = (struct stm_pad_gpio []) {
		/* Pads shared with i2c-stm */
		STM_PAD_PIO_OUT(13, 0, 6),	/* MCLK */
		STM_PAD_PIO_OUT(13, 1, 6),	/* LRCLK */
		STM_PAD_PIO_OUT(13, 2, 6),	/* SCLK */
		STM_PAD_PIO_OUT(13, 3, 6),	/* DATA0 */
		/* Pads not shared */
		STM_PAD_PIO_OUT(15, 2, 6),	/* DATA1 */
		/* Pads shared with stasc */
		STM_PAD_PIO_OUT(17, 4, 6),	/* DATA2 */
		STM_PAD_PIO_OUT(17, 5, 6),	/* DATA3 */
	},
};

static struct platform_device stih415_uni_player_1 = {
	.name = "snd_uniperif_player",
	.id = 1,
	.num_resources = 2,
	.resource = (struct resource []) {
		STM_PLAT_RESOURCE_MEM(0xfe003000, 0x150),
		STIH415_RESOURCE_IRQ(141),
	},
	.dev.platform_data = &stih415_uni_player_1_info,
};

static struct platform_device stih415_uni_player_2 = {
	.name = "snd_uniperif_player",
	.id = 2,
	.num_resources = 2,
	.resource = (struct resource []) {
		STM_PLAT_RESOURCE_MEM(0xfe004000, 0x150),
		STIH415_RESOURCE_IRQ(142),
	},
	.dev.platform_data = &(struct snd_stm_uniperif_player_info) {
		.name = "Uni Player #2 (DAC)",
		.ver = 1,
		.card_device = 2,
		.player_type = SND_STM_UNIPERIF_PLAYER_TYPE_PCM,
		.clock_name = "CLKS_B_PCM_FSYN2",
		.channels = 2,
		.fdma_name = "fdma_dmac.3",
		.fdma_initiator = 0,
		.fdma_request_line = 25,
	},
};

static struct snd_stm_uniperif_player_info stih415_uni_player_3_info = {
	.name = "Uni Player #3 (SPDIF)",
	.ver = 1,
	.card_device = 3,
	.clock_name = "CLKS_B_PCM_FSYN3",
	.player_type = SND_STM_UNIPERIF_PLAYER_TYPE_SPDIF,
	.channels = 2,
	.fdma_name = "fdma_dmac.3",
	.fdma_initiator = 0,
	.fdma_request_line = 27,
	/* .pad_config set by stih415_configure_audio() */
};

static struct stm_pad_config stih415_uni_player_3_pad_config = {
	.gpios_num = 1,
	.gpios = (struct stm_pad_gpio []) {
		STM_PAD_PIO_OUT(9, 7, 1),
	},
};

static struct platform_device stih415_uni_player_3 = {
	.name = "snd_uniperif_player",
	.id = 3,
	.num_resources = 2,
	.resource = (struct resource []) {
		STM_PLAT_RESOURCE_MEM(0xfe006000, 0x150),
		STIH415_RESOURCE_IRQ(144),
	},
	.dev.platform_data = &stih415_uni_player_3_info,
};

/* Uniperipheral reader */

static struct snd_stm_pcm_reader_info stih415_uni_reader_0_info = {
	.name = "Uni Reader #0 (SPDIF)",
	.ver = 1,
	.card_device = 4,
	.channels = 2,
	.fdma_name = "fdma_dmac.3",
	.fdma_initiator = 0,
	.fdma_request_line = 26,
	/* .pad_config set by stih415_configure_audio() */
};

static struct stm_pad_config stih415_uni_reader_0_pad_config = {
	.gpios_num = 4,
	.gpios = (struct stm_pad_gpio []) {
		STM_PAD_PIO_OUT(13, 0, 7),	/* MCLK */
		STM_PAD_PIO_IN(13, 1, 7),	/* LRCLK */
		STM_PAD_PIO_IN(13, 2, 7),	/* SCLK */
		STM_PAD_PIO_IN(17, 5, 7),	/* DATA0 */
	},
};

static struct platform_device stih415_uni_reader_0 = {
	.name = "snd_uniperif_reader",
	.id = 4,
	.num_resources = 2,
	.resource = (struct resource []) {
		STM_PLAT_RESOURCE_MEM(0xfe005000, 0x14c),
		STIH415_RESOURCE_IRQ(143),
	},
	.dev.platform_data = &stih415_uni_reader_0_info,
};

/* Devices */

static struct platform_device *stih415_audio_devices[] __initdata = {
	&stih415_conv_dac,
	&stih415_conv_biphase,
	&stih415_uni_player_0,  /* HDMI */
	&stih415_uni_player_1,  /* PIO */
	&stih415_uni_player_2,  /* DAC */
	&stih415_uni_player_3,  /* SPDIF */

	&stih415_uni_reader_0,  /* SPDIF */
};

static int __init stih415_audio_devices_setup(void)
{
	if (stm_soc_type() != CPU_STIH415) {
		BUG();
		return -ENODEV;
	}

	return platform_add_devices(stih415_audio_devices,
					ARRAY_SIZE(stih415_audio_devices));
}
device_initcall(stih415_audio_devices_setup);

/* Configuration */

void __init stih415_configure_audio(struct stih415_audio_config *config)
{
	static int configured;

	BUG_ON(configured);
	configured = 1;

	if (config->uni_player_1_pcm_mode >
			stih415_uni_player_1_pcm_disabled) {
		int unused = 4 - config->uni_player_1_pcm_mode;

		stih415_uni_player_1_info.pad_config =
				&stih415_uni_player_1_pad_config;

		stih415_uni_player_1_pad_config.gpios_num -= unused;
	}

	if (config->uni_player_3_spdif_enabled) {
		stih415_uni_player_3_info.pad_config =
				&stih415_uni_player_3_pad_config;
	}

	if (config->uni_reader_0_spdif_enabled) {
		stih415_uni_reader_0_info.pad_config =
				&stih415_uni_reader_0_pad_config;
	}
}
