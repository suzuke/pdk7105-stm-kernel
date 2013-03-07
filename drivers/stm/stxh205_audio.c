/*
 * Copyright (c) 2013 STMicroelectronics Limited
 *
 * Author: John Boddie <john.boddie@st.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <asm/irq-ilc.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/irq.h>
#include <linux/stm/platform.h>
#include <linux/stm/stxh205.h>
#include <sound/stm.h>


/* Audio subsystem resources ---------------------------------------------- */

/* Internal DAC */

static struct platform_device stxh205_conv_dac = {
	.name = "snd_conv_dac_sc",
	.id = -1,
	.dev.platform_data = &(struct snd_stm_conv_dac_sc_info) {
		.source_bus_id = "snd_pcm_player.1",
		.channel_from = 0,
		.channel_to = 1,
		.nrst = { SYSCONF(441), 0, 0 },
		.sb = { SYSCONF(441), 3, 3 },
		.softmute = { SYSCONF(441), 4, 4 },
		.npdana = { SYSCONF(441), 5, 5 },
	},
};

/* PCM players  */

static struct platform_device stxh205_pcm_player_0 = {
	.name = "snd_pcm_player",
	.id = 0,
	.num_resources = 2,
	.resource = (struct resource []) {
		STM_PLAT_RESOURCE_MEM(0xfd000d00, 0x28),
		STM_PLAT_RESOURCE_IRQ(ILC_IRQ(105), -1),
	},
	.dev.platform_data = &(struct snd_stm_pcm_player_info) {
		.name = "PCM player #0 (HDMI)",
		.ver = 6,
		.card_device = 0,
		.channels = 8,
		.fdma_initiator = 0,
		.fdma_request_line = 23,
	},
};

static struct platform_device stxh205_pcm_player_1 = {
	.name = "snd_pcm_player",
	.id = 1,
	.num_resources = 2,
	.resource = (struct resource []) {
		STM_PLAT_RESOURCE_MEM(0xfd002000, 0x28),
		STM_PLAT_RESOURCE_IRQ(ILC_IRQ(70), -1),
	},
	.dev.platform_data = &(struct snd_stm_pcm_player_info) {
		.name = "PCM player #1 (DAC)",
		.ver = 6,
		.card_device = 1,
		.channels = 2,
		.fdma_initiator = 0,
		.fdma_request_line = 24,
	},
};

static struct snd_stm_pcm_player_info stxh205_pcm_player_2_info = {
	.name = "PCM player #2 (PIO)",
	.ver = 6,
	.card_device = 2,
	.channels = 2,
	.fdma_initiator = 0,
	.fdma_request_line = 25,
	/* .pad_config set by stxh205_configure_audio() */
};

static struct stm_pad_config stxh205_pcm_player_2_pad_config = {
	.gpios_num = 4,
	.gpios = (struct stm_pad_gpio []) {
		STM_PAD_PIO_OUT(15, 3, 3),	/* LRCLK (conflict ASC2) */
		STM_PAD_PIO_OUT(15, 2, 3),	/* SCLK  (conflict SSC3/ASC2) */
		STM_PAD_PIO_OUT(15, 1, 3),	/* MCLK  (conflict SSC3/ASC2) */
		STM_PAD_PIO_OUT(15, 0, 3),	/* DATA0 (conflict SSC3/ASC2) */
	},
};


static struct platform_device stxh205_pcm_player_2 = {
	.name = "snd_pcm_player",
	.id = 2,
	.num_resources = 2,
	.resource = (struct resource []) {
		STM_PLAT_RESOURCE_MEM(0xfd003000, 0x28),
		STM_PLAT_RESOURCE_IRQ(ILC_IRQ(71), -1),
	},
	.dev.platform_data = &stxh205_pcm_player_2_info,
};

/* SPDIF player */

static struct snd_stm_spdif_player_info stxh205_spdif_player_info = {
	.name = "SPDIF player",
	.ver = 4,
	.card_device = 3,
	.fdma_initiator = 0,
	.fdma_request_line = 27,
	/* .pad_config set by stxh205_configure_audio() */
};

static struct stm_pad_config stxh205_spdif_player_pad_config = {
	.gpios_num = 1,
	.gpios = (struct stm_pad_gpio []) {
		STM_PAD_PIO_OUT(4, 1, 1),
	},
};

static struct platform_device stxh205_spdif_player = {
	.name = "snd_spdif_player",
	.id = -1,
	.num_resources = 2,
	.resource = (struct resource []) {
		STM_PLAT_RESOURCE_MEM(0xfd000c00, 0x44),
		STM_PLAT_RESOURCE_IRQ(ILC_IRQ(104), -1),
	},
	.dev.platform_data = &stxh205_spdif_player_info,
};

/* I2S to SPDIF converters */

static struct platform_device stxh205_conv_i2sspdif_0 = {
	.name = "snd_conv_i2sspdif",
	.id = 0,
	.num_resources = 2,
	.resource = (struct resource []) {
		STM_PLAT_RESOURCE_MEM(0xfd001000, 0x224),
		STM_PLAT_RESOURCE_IRQ(ILC_IRQ(100), -1),
	},
	.dev.platform_data = &(struct snd_stm_conv_i2sspdif_info) {
		.ver = 4,
		.source_bus_id = "snd_pcm_player.0",
		.channel_from = 0,
		.channel_to = 1,
	},
};

static struct platform_device stxh205_conv_i2sspdif_1 = {
	.name = "snd_conv_i2sspdif",
	.id = 1,
	.num_resources = 2,
	.resource = (struct resource []) {
		STM_PLAT_RESOURCE_MEM(0xfd001400, 0x224),
		STM_PLAT_RESOURCE_IRQ(ILC_IRQ(101), -1),
	},
	.dev.platform_data = &(struct snd_stm_conv_i2sspdif_info) {
		.ver = 4,
		.source_bus_id = "snd_pcm_player.0",
		.channel_from = 2,
		.channel_to = 3,
	},
};

static struct platform_device stxh205_conv_i2sspdif_2 = {
	.name = "snd_conv_i2sspdif",
	.id = 2,
	.num_resources = 2,
	.resource = (struct resource []) {
		STM_PLAT_RESOURCE_MEM(0xfd001800, 0x224),
		STM_PLAT_RESOURCE_IRQ(ILC_IRQ(102), -1),
	},
	.dev.platform_data = &(struct snd_stm_conv_i2sspdif_info) {
		.ver = 4,
		.source_bus_id = "snd_pcm_player.0",
		.channel_from = 4,
		.channel_to = 5,
	},
};

static struct platform_device stxh205_conv_i2sspdif_3 = {
	.name = "snd_conv_i2sspdif",
	.id = 3,
	.num_resources = 2,
	.resource = (struct resource []) {
		STM_PLAT_RESOURCE_MEM(0xfd001c00, 0x224),
		STM_PLAT_RESOURCE_IRQ(ILC_IRQ(103), -1),
	},
	.dev.platform_data = &(struct snd_stm_conv_i2sspdif_info) {
		.ver = 4,
		.source_bus_id = "snd_pcm_player.0",
		.channel_from = 6,
		.channel_to = 7,
	},
};

/* PCM reader */

static struct snd_stm_pcm_reader_info stxh205_pcm_reader_info = {
	.name = "PCM Reader",
	.ver = 5,
	.card_device = 4,
	.channels = 2,
	.fdma_initiator = 0,
	.fdma_request_line = 26,
	/* .pad_config set by stxh205_configure_audio() */
};

static struct stm_pad_config stxh205_pcm_reader_pad_config = {
	.gpios_num = 4,
	.gpios = (struct stm_pad_gpio []) {
		STM_PAD_PIO_IN(15, 7, 6),	/* LRCLK (conflict SSC3/ASC2) */
		STM_PAD_PIO_IN(15, 6, 6),	/* SCLK  (conflict SSC3/ASC2) */
		STM_PAD_PIO_OUT(15, 5, 6),	/* MCLK  (conflict SSC3/ASC2) */
		STM_PAD_PIO_IN(15, 4, 6),	/* DATA0 (conflict ASC2) */
	},
};

static struct platform_device stxh205_pcm_reader = {
	.name = "snd_pcm_reader",
	.id = -1,
	.num_resources = 2,
	.resource = (struct resource []) {
		STM_PLAT_RESOURCE_MEM(0xfd004000, 0x28),
		STM_PLAT_RESOURCE_IRQ(ILC_IRQ(72), -1),
	},
	.dev.platform_data = &stxh205_pcm_reader_info,
};

static struct platform_device *stxh205_audio_devices[] __initdata = {
	&stxh205_conv_dac,
	&stxh205_pcm_player_0,
	&stxh205_pcm_player_1,
	&stxh205_pcm_player_2,
	&stxh205_spdif_player,
	&stxh205_conv_i2sspdif_0,
	&stxh205_conv_i2sspdif_1,
	&stxh205_conv_i2sspdif_2,
	&stxh205_conv_i2sspdif_3,
	&stxh205_pcm_reader,
};

static int __init stxh205_audio_devices_setup(void)
{
	/*
	 * Ugly but quick hack to have SPDIF player and I2S to SPDIF converters
	 * enabled without loading STMFB.
	 *
	 * We basically write to the HDMI_GPOUT register of the HDMI frame
	 * formatter in the TVOUT susbsytem to:
	 * - Select SPDIF from I2S2SPDIF+PCM path
	 * - Select data from external PCM player
	 */

	void *hdmi_gpout = ioremap(0xfd000020, 4);
	writel(readl(hdmi_gpout) | 0x3, hdmi_gpout);
	iounmap(hdmi_gpout);

	return platform_add_devices(stxh205_audio_devices,
			ARRAY_SIZE(stxh205_audio_devices));
}
device_initcall(stxh205_audio_devices_setup);

/* Configuration */

void __init stxh205_configure_audio(struct stxh205_audio_config *config)
{
	static int configured;

	BUG_ON(configured);
	configured = 1;

	if (config->pcm_player_2_output_enabled)
		stxh205_pcm_player_2_info.pad_config =
				&stxh205_pcm_player_2_pad_config;

	if (config->spdif_player_output_enabled)
		stxh205_spdif_player_info.pad_config =
				&stxh205_spdif_player_pad_config;

	if (config->pcm_reader_input_enabled)
		stxh205_pcm_reader_info.pad_config =
				&stxh205_pcm_reader_pad_config;
}
