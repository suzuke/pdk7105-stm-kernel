/*
 *   STMicroelectronics System-on-Chips' Uniperipheral player driver
 *
 *   Copyright (c) 2005-2011 STMicroelectronics Limited
 *
 *   Author: John Boddie <john.boddie@st.com>
 *           Sevanand Singh <sevanand.singh@st.com>
 *           Mark Glaisher
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the Free Software
 *   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
 *
 */
#include <asm/cacheflush.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/io.h>
#include <linux/platform_device.h>
#include <linux/interrupt.h>
#include <linux/semaphore.h>
#include <linux/delay.h>
#include <linux/stm/clk.h>
#include <linux/stm/pad.h>
#include <linux/stm/dma.h>
#include <linux/pm_runtime.h>
#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/control.h>
#include <sound/info.h>
#include <sound/pcm_params.h>

#include "common.h"
#include "reg_aud_uniperif.h"


static int snd_stm_debug_level;
module_param_named(debug, snd_stm_debug_level, int, S_IRUGO | S_IWUSR);


/*
 * Some hardware-related definitions
 */

#define DEFAULT_FORMAT (SND_STM_FORMAT__I2S | \
		SND_STM_FORMAT__SUBFRAME_32_BITS)
#define DEFAULT_OVERSAMPLING 128 /* make all ip's running at same rate*/

/* The sample count field (NSAMPLES in CTRL register) is 19 bits wide */
#define MAX_SAMPLES_PER_PERIOD ((1 << 20) - 1)

#define MIN_IEC958_SAMPLE_RATE	32000

#define PARKING_SUBBLOCKS	4
#define PARKING_NBFRAMES	4
#define PARKING_BUFFER_SIZE	128	/* Optimal FDMA transfer is 128-bytes */

#define SPDIF_PREAMBLE_BYTES 8
enum snd_stm_uniperif_spdif_input_mode {
	SNDRV_STM_UNIPERIF_SPDIF_INPUT_MODE_NORMAL,
	SNDRV_STM_UNIPERIF_SPDIF_INPUT_MODE_RAW
};
enum snd_stm_uniperif_spdif_encoding_mode {
	SNDRV_STM_UNIPERIF_SPDIF_ENCODING_MODE_PCM,
	SNDRV_STM_UNIPERIF_SPDIF_ENCODING_MODE_ENCODED
};

struct snd_stm_uniperif_spdif_settings {
	enum snd_stm_uniperif_spdif_input_mode input_mode;
	enum snd_stm_uniperif_spdif_encoding_mode encoding_mode;
	struct snd_aes_iec958 iec958;
	unsigned char iec61937_preamble[SPDIF_PREAMBLE_BYTES]; /* Used in */
	unsigned int iec61937_audio_repetition;          /* encoded */
	unsigned int iec61937_pause_repetition;          /* mode */
};


/*
 * Uniperipheral player instance definition
 */

struct snd_stm_uniperif_player {
	/* System information */
	struct snd_stm_uniperif_player_info *info;
	struct device *device;
	struct snd_pcm *pcm;
	int ver; /* IP version, used by register access macros */

	/* Resources */
	struct resource *mem_region;
	void *base;
	unsigned long fifo_phys_address;
	unsigned int irq;
	int fdma_channel;

	/* Environment settings */
	struct clk *clock;
	struct snd_pcm_hw_constraint_list channels_constraint;
	struct snd_stm_conv_source *conv_source;

	/* Runtime data */
	struct snd_stm_conv_group *conv_group;
	struct snd_stm_buffer *buffer;
	struct snd_info_entry *proc_entry;
	struct snd_pcm_substream *substream;
	struct stm_pad_state *pads;

	struct snd_pcm_hardware hardware;

	/* Specific to spdif player */
	struct snd_stm_uniperif_spdif_settings default_settings;
	spinlock_t default_settings_lock; /* Protects default_settings */
	struct snd_stm_uniperif_spdif_settings stream_settings;

	int stream_iec958_pa_pb_sync_lost;
	int stream_iec958_status_cnt;
	int stream_iec958_subcode_cnt;

	int dma_max_transfer_size;
	struct stm_dma_audio_config dma_config;
	struct dma_chan *dma_channel;
	struct dma_async_tx_descriptor *dma_descriptor;
	dma_cookie_t dma_cookie;
	struct stm_dma_park_config dma_park_config;

	int buffer_bytes;
	int period_bytes;

	/* Configuration */
	unsigned int current_rate;
	unsigned int current_format;
	unsigned int current_channels;
	unsigned char current_fmda_cnt;

	snd_stm_magic_field;
};

static struct snd_pcm_hardware snd_stm_uniperif_player_pcm_hw = {
	.info		= (SNDRV_PCM_INFO_MMAP |
				SNDRV_PCM_INFO_MMAP_VALID |
				SNDRV_PCM_INFO_INTERLEAVED |
				SNDRV_PCM_INFO_BLOCK_TRANSFER |
				SNDRV_PCM_INFO_PAUSE |
				SNDRV_PCM_INFO_RESUME),
	.formats	= (SNDRV_PCM_FMTBIT_S32_LE |
				SNDRV_PCM_FMTBIT_S16_LE),

	.rates		= SNDRV_PCM_RATE_CONTINUOUS,
	.rate_min	= 8000,
	.rate_max	= 192000,

	.channels_min	= 2,
	.channels_max	= 10,

	.periods_min	= 2,
	.periods_max	= 1024,  /* TODO: sample, work out this somehow... */

	/* Values below were worked out mostly basing on ST media player
	 * requirements. They should, however, fit most "normal" cases...
	 * Note 1: that these value must be also calculated not to exceed
	 * NSAMPLE interrupt counter size (19 bits) - MAX_SAMPLES_PER_PERIOD.
	 * Note 2: for 16/16-bits data this counter is a "frames counter",
	 * not "samples counter" (two channels are read as one word).
	 * Note 3: period_bytes_min defines minimum time between period
	 * (NSAMPLE) interrupts... Keep it large enough not to kill
	 * the system... */
	.period_bytes_min = 4096, /* 1024 frames @ 32kHz, 16 bits, 2 ch. */
	.period_bytes_max = 81920, /* 2048 frames @ 192kHz, 32 bits, 10 ch. */
	.buffer_bytes_max = 81920 * 3, /* 3 worst-case-periods */
};

static struct snd_pcm_hardware snd_stm_uniperif_player_raw_hw = {
	.info		= (SNDRV_PCM_INFO_MMAP |
				SNDRV_PCM_INFO_MMAP_VALID |
				SNDRV_PCM_INFO_INTERLEAVED |
				SNDRV_PCM_INFO_BLOCK_TRANSFER |
				SNDRV_PCM_INFO_PAUSE |
				SNDRV_PCM_INFO_RESUME),
	.formats	= (SNDRV_PCM_FMTBIT_S32_LE),

	.rates		= SNDRV_PCM_RATE_CONTINUOUS,
	.rate_min	= 32000,
	.rate_max	= 192000,

	.channels_min	= 2,
	.channels_max	= 2,

	.periods_min	= 2,
	.periods_max	= 1024,  /* TODO: sample, work out this somehow... */

	/* See above... */
	.period_bytes_min = 4096, /* 1024 frames @ 32kHz, 16 bits, 2 ch. */
	.period_bytes_max = 81920, /* 2048 frames @ 192kHz, 32 bits, 10 ch. */
	.buffer_bytes_max = 81920 * 3, /* 3 worst-case-periods */
};


/*
 * Uniperipheral player functions
 */

static int snd_stm_uniperif_player_halt(struct snd_pcm_substream *substream);
static int snd_stm_uniperif_player_stop(struct snd_pcm_substream *substream);


/*
 * Uniperipheral player implementation
 */

static irqreturn_t snd_stm_uniperif_player_irq_handler(int irq, void *dev_id)
{
	irqreturn_t result = IRQ_NONE;
	struct snd_stm_uniperif_player *player = dev_id;
	unsigned int status;

	snd_stm_printd(2, "%s(irq=%d, dev_id=0x%p)\n", __func__, irq, dev_id);

	BUG_ON(!player);
	BUG_ON(!snd_stm_magic_valid(player));

	/* Get interrupt status & clear them immediately */
	preempt_disable();
	status = get__AUD_UNIPERIF_ITS(player);
	set__AUD_UNIPERIF_ITS_BCLR(player, status);
	preempt_enable();

	/* FIFO error? */
	if (unlikely(status & mask__AUD_UNIPERIF_ITS__FIFO_ERROR(player))) {
		snd_stm_printe("Player '%s' FIFO error detected!\n",
				dev_name(player->device));

		/* Disable interrupt so doesn't continually fire */
		set__AUD_UNIPERIF_ITM_BCLR__FIFO_ERROR(player);

		/* Indicate xrun and stop the player */
		snd_pcm_stop(player->substream, SNDRV_PCM_STATE_XRUN);

		result = IRQ_HANDLED;

	} else if (unlikely(status &
			    mask__AUD_UNIPERIF_ITS__PA_PB_SYNC_LOST(player))) {
		snd_stm_printe("PA PB sync loss detected in player '%s'!\n",
			       dev_name(player->device));
		player->stream_iec958_pa_pb_sync_lost = 1;
		/* for pa pb sync loss we may handle later on */
		/*snd_pcm_stop(player->substream, SNDRV_PCM_STATE_XRUN);*/

		result = IRQ_HANDLED;

	} else if (unlikely(status &
			    mask__AUD_UNIPERIF_ITS__DMA_ERROR(player))) {
		snd_stm_printe("Player '%s' DMA error detected!\n",
				dev_name(player->device));

		/* Disable interrupt so doesn't continually fire */
		set__AUD_UNIPERIF_ITM_BCLR__DMA_ERROR(player);

		/* Indicate xrun and stop the player */
		snd_pcm_stop(player->substream, SNDRV_PCM_STATE_XRUN);

		result = IRQ_HANDLED;
	}

	/* Some alien interrupt??? */
	BUG_ON(result != IRQ_HANDLED);

	return result;
}

static bool snd_stm_uniperif_player_dma_filter_fn(struct dma_chan *chan,
		void *fn_param)
{
	struct snd_stm_uniperif_player *player = fn_param;
	struct stm_dma_audio_config *config = &player->dma_config;

	BUG_ON(!player);

	/* If FDMA name has been specified, attempt to match channel to it */
	if (player->info->fdma_name)
		if (!stm_dma_is_fdma_name(chan, player->info->fdma_name))
			return false;

	/* Setup this channel for audio operation */
	config->type = STM_DMA_TYPE_AUDIO;
	config->dma_addr = player->fifo_phys_address;
	config->dreq_config.request_line = player->info->fdma_request_line;
	config->dreq_config.initiator = player->info->fdma_initiator;
	config->dreq_config.increment = 0;
	config->dreq_config.hold_off = 0;
	config->dreq_config.maxburst = 1;
	config->dreq_config.buswidth = DMA_SLAVE_BUSWIDTH_4_BYTES;
	config->dreq_config.direction = DMA_MEM_TO_DEV;

	/* Set the default parking configuration */
	config->park_config.sub_periods = PARKING_SUBBLOCKS;
	config->park_config.buffer_size = PARKING_BUFFER_SIZE;

	/* Save the channel config inside the channel structure */
	chan->private = config;

	snd_stm_printd(0, "Uniperipheral player '%s' using fdma '%s' channel "
			"%d\n", player->info->name, dev_name(chan->device->dev),
			chan->chan_id);
	return true;
}

static int snd_stm_uniperif_player_open(struct snd_pcm_substream *substream)
{
	struct snd_stm_uniperif_player *player =
			snd_pcm_substream_chip(substream);
	struct snd_pcm_runtime *runtime = substream->runtime;
	int result;

	snd_stm_printd(1, "%s(substream=0x%p)\n", __func__, substream);

	BUG_ON(!player);
	BUG_ON(!snd_stm_magic_valid(player));
	BUG_ON(!runtime);

	snd_pcm_set_sync(substream);  /* TODO: ??? */

	if (player->dma_channel == NULL) {
		dma_cap_mask_t mask;

		/* Set the dma channel capabilities we want */
		dma_cap_zero(mask);
		dma_cap_set(DMA_SLAVE, mask);
		dma_cap_set(DMA_CYCLIC, mask);

		/* Request a matching dma channel */
		player->dma_channel = dma_request_channel(mask,
				snd_stm_uniperif_player_dma_filter_fn, player);

		if (!player->dma_channel) {
			snd_stm_printe("Failed to request dma channel\n");
			return -ENODEV;
		}

		/* Get handle to any attached converter */
		player->conv_group = snd_stm_conv_request_group(
				player->conv_source);
	}

	if (player->conv_group)
		snd_stm_printd(1, "'%s' is attached to '%s' converter(s)...\n",
				dev_name(player->device),
				snd_stm_conv_get_name(player->conv_group));
	else
		snd_stm_printd(1, "Warning! No converter attached to '%s'!\n",
				dev_name(player->device));

	/* Get default data */

	spin_lock(&player->default_settings_lock);
	player->stream_settings = player->default_settings;
	spin_unlock(&player->default_settings_lock);

	/* Set up channel constraints and inform ALSA */

	result = snd_pcm_hw_constraint_list(runtime, 0,
				SNDRV_PCM_HW_PARAM_CHANNELS,
				&player->channels_constraint);
	if (result < 0) {
		snd_stm_printe("Can't set channels constraint!\n");
		goto error;
	}

	/* It is better when buffer size is an integer multiple of period
	 * size... Such thing will ensure this :-O */
	result = snd_pcm_hw_constraint_integer(runtime,
			SNDRV_PCM_HW_PARAM_PERIODS);
	if (result < 0) {
		snd_stm_printe("Can't set periods constraint!\n");
		goto error;
	}

	/* Make the period (so buffer as well) length (in bytes) a multiply
	 * of a FDMA transfer bytes (which varies depending on channels
	 * number and sample bytes) */
	result = snd_stm_pcm_hw_constraint_transfer_bytes(runtime,
			player->dma_max_transfer_size * 4);
	if (result < 0) {
		snd_stm_printe("Can't set buffer bytes constraint!\n");
		goto error;
	}

	runtime->hw = player->hardware;

	/* Interrupt handler will need the substream pointer... */
	player->substream = substream;

	return 0;

error:
	/* We should always have a dma channel if we get here */
	BUG_ON(!player->dma_channel);

	/* Tear everything down if not parked */
	if (!dma_audio_is_parking_active(player->dma_channel)) {
		/* Release the dma channel */
		dma_release_channel(player->dma_channel);
		player->dma_channel = NULL;

		/* Release any converter */
		if (player->conv_group) {
			snd_stm_conv_release_group(player->conv_group);
			player->conv_group = NULL;
		}
	}
	return result;
}

static int snd_stm_uniperif_player_close(struct snd_pcm_substream *substream)
{
	struct snd_stm_uniperif_player *player =
			snd_pcm_substream_chip(substream);

	snd_stm_printd(1, "%s(substream=0x%p)\n", __func__, substream);

	BUG_ON(!player);
	BUG_ON(!snd_stm_magic_valid(player));

	/* We should always have a dma channel if we get here */
	BUG_ON(!player->dma_channel);

	/* Tear everything down if not parked */
	if (!dma_audio_is_parking_active(player->dma_channel)) {
		/* Release the dma channel */
		dma_release_channel(player->dma_channel);
		player->dma_channel = NULL;

		/* Release any converter */
		if (player->conv_group) {
			snd_stm_conv_release_group(player->conv_group);
			player->conv_group = NULL;
		}

		player->substream = NULL;
	}

	return 0;
}

static int snd_stm_uniperif_player_hw_free(struct snd_pcm_substream *substream)
{
	struct snd_stm_uniperif_player *player =
			snd_pcm_substream_chip(substream);
	struct snd_pcm_runtime *runtime = substream->runtime;

	snd_stm_printd(1, "%s(substream=0x%p)\n", __func__, substream);

	BUG_ON(!player);
	BUG_ON(!snd_stm_magic_valid(player));
	BUG_ON(!runtime);

	/* This callback may be called more than once... */

	if (snd_stm_buffer_is_allocated(player->buffer)) {
		/* Terminate all DMA transfers (if not parked) */
		if (!dma_audio_is_parking_active(player->dma_channel))
			dmaengine_terminate_all(player->dma_channel);

		/* Free buffer */
		snd_stm_buffer_free(player->buffer);
	}

	return 0;
}

static void snd_stm_uniperif_player_comp_cb(void *param)
{
	struct snd_stm_uniperif_player *player = param;

	BUG_ON(!player);
	BUG_ON(!snd_stm_magic_valid(player));

	snd_pcm_period_elapsed(player->substream);
}

static int snd_stm_uniperif_player_hw_params(
		struct snd_pcm_substream *substream,
		struct snd_pcm_hw_params *hw_params)
{
	struct snd_stm_uniperif_player *player =
			snd_pcm_substream_chip(substream);
	struct snd_pcm_runtime *runtime = substream->runtime;
	int buffer_bytes, frame_bytes, transfer_bytes, period_bytes, periods;
	unsigned int transfer_size;
	struct dma_slave_config slave_config;
	int result;

	snd_stm_printd(1, "%s(substream=0x%p, hw_params=0x%p)\n",
		       __func__, substream, hw_params);

	BUG_ON(!player);
	BUG_ON(!snd_stm_magic_valid(player));
	BUG_ON(!runtime);

	/* This function may be called many times, so let's be prepared... */
	if (snd_stm_buffer_is_allocated(player->buffer))
		snd_stm_uniperif_player_hw_free(substream);

	/* Allocate buffer */

	buffer_bytes = params_buffer_bytes(hw_params);
	periods = params_periods(hw_params);
	period_bytes = buffer_bytes / periods;
	BUG_ON(periods * period_bytes != buffer_bytes);

	result = snd_stm_buffer_alloc(player->buffer, substream,
			buffer_bytes);
	if (result != 0) {
		snd_stm_printe("Can't allocate %d bytes buffer for '%s'!\n",
				buffer_bytes, dev_name(player->device));
		result = -ENOMEM;
		goto error_buf_alloc;
	}

	/* Set FDMA transfer size (number of opcodes generated
	 * after request line assertion) */

	frame_bytes = snd_pcm_format_physical_width(params_format(hw_params)) *
			params_channels(hw_params) / 8;
	transfer_bytes = snd_stm_pcm_transfer_bytes(frame_bytes,
			player->dma_max_transfer_size * 4);
	transfer_size = transfer_bytes / 4;

	snd_stm_printd(1, "FDMA request trigger limit and transfer size set "
			"to %d.\n", transfer_size);

	BUG_ON(buffer_bytes % transfer_bytes != 0);
	BUG_ON(transfer_size > player->dma_max_transfer_size);

	BUG_ON(transfer_size != 1 && transfer_size % 2 != 0);
	BUG_ON(transfer_size >
			mask__AUD_UNIPERIF_CONFIG__FDMA_TRIGGER_LIMIT(player));

	set__AUD_UNIPERIF_CONFIG__FDMA_TRIGGER_LIMIT(player, transfer_size);

	/* Save the buffer bytes and period bytes for when start dma */
	player->buffer_bytes = buffer_bytes;
	player->period_bytes = period_bytes;

	/* Setup the dma configuration */
	slave_config.direction = DMA_MEM_TO_DEV;
	slave_config.dst_addr = player->fifo_phys_address;
	slave_config.dst_addr_width = DMA_SLAVE_BUSWIDTH_4_BYTES;
	slave_config.dst_maxburst = transfer_size;

	result = dmaengine_slave_config(player->dma_channel, &slave_config);
	if (result) {
		snd_stm_printe("Failed to configure dma channel\n");
		goto error_dma_config;
	}

	/* Set the parking configuration (actually set in 'prepare' fn) */
	player->dma_park_config.sub_periods = PARKING_SUBBLOCKS;
	player->dma_park_config.buffer_size = transfer_bytes + (frame_bytes-1);
	player->dma_park_config.buffer_size /= frame_bytes;
	player->dma_park_config.buffer_size *= frame_bytes;

	return 0;

error_dma_config:
	snd_stm_buffer_free(player->buffer);
error_buf_alloc:
	return result;
}

static unsigned int snd_stm_uniperif_player_samples_per_period(
		struct snd_pcm_runtime *runtime)
{
	unsigned int frames, samples;
	int bits_per_sample;

	/* Extract the required information from the runtime structure */
	frames = runtime->period_size * runtime->channels;
	bits_per_sample = snd_pcm_format_physical_width(runtime->format);
	BUG_ON(bits_per_sample < 16);

	/* uniperipheral samples are not the same as ALSA samples so we don't
	 * use the ALSA framework's library functions for this conversion.
	 */
	samples = (frames * (bits_per_sample / 16)) / 2;

	/* Account for out-by-one hardware period timer bug (RnDHV00031683) */
	samples -= 1 * (bits_per_sample / 16);

	return samples;
}

static int snd_stm_uniperif_player_prepare_pcm(
		struct snd_stm_uniperif_player *player,
		struct snd_pcm_runtime *runtime)
{
	int bits_in_output_frame;
	int oversampling;
	int format;
	int lr_pol;
	int result;

	snd_stm_printd(1, "%s(player=0x%p)\n", __func__, player);

	/* Get format & oversampling value from connected converter */
	if (player->conv_group) {
		format = snd_stm_conv_get_format(player->conv_group);
		oversampling =
			snd_stm_conv_get_oversampling(player->conv_group);
		if (oversampling == 0)
			oversampling = DEFAULT_OVERSAMPLING;
	} else {
		format = DEFAULT_FORMAT;
		oversampling = DEFAULT_OVERSAMPLING;
	}

	snd_stm_printd(1, "Player %s: sampling frequency %d, oversampling %d\n",
			dev_name(player->device), runtime->rate,
			oversampling);

	BUG_ON(oversampling < 0);

	/* For 32 bits subframe oversampling must be a multiple of 128,
	 * for 16 bits - of 64 */
	BUG_ON((format & SND_STM_FORMAT__SUBFRAME_32_BITS) &&
			(oversampling % 128 != 0));
	BUG_ON((format & SND_STM_FORMAT__SUBFRAME_16_BITS) &&
			(oversampling % 64 != 0));

	/* Set up player hardware */

	snd_stm_printd(1, "Player %s format configuration:\n",
			dev_name(player->device));

	/* Number of bits per subframe (which is one channel sample)
	 * on output - it determines serial clock frequency, which is
	 * 64 times sampling rate for 32 bits subframe (2 channels 32
	 * bits each means 64 bits per frame) and 32 times sampling
	 * rate for 16 bits subframe
	 * (you know why, don't you? :-) */
	switch (format & SND_STM_FORMAT__SUBFRAME_MASK) {
	case SND_STM_FORMAT__SUBFRAME_32_BITS:
		snd_stm_printd(1, "- 32 bits per subframe\n");
		set__AUD_UNIPERIF_I2S_FMT__NBIT_32(player);
		set__AUD_UNIPERIF_I2S_FMT__DATA_SIZE_32(player);
		bits_in_output_frame = 64; /* frame = 2 * subframe */
		break;
	case SND_STM_FORMAT__SUBFRAME_16_BITS:
		snd_stm_printd(1, "- 16 bits per subframe\n");
		set__AUD_UNIPERIF_I2S_FMT__NBIT_16(player);
		set__AUD_UNIPERIF_I2S_FMT__DATA_SIZE_16(player);
		bits_in_output_frame = 32; /* frame = 2 * subframe */
		break;
	default:
		snd_BUG();
		return -EINVAL;
	}

	switch (format & SND_STM_FORMAT__MASK) {
	case SND_STM_FORMAT__I2S:
		snd_stm_printd(1, "- I2S\n");
		set__AUD_UNIPERIF_I2S_FMT__ALIGN_LEFT(player);
		set__AUD_UNIPERIF_I2S_FMT__PADDING_I2S_MODE(player);
		lr_pol = value__AUD_UNIPERIF_I2S_FMT__LR_POL_LOW(player);
		break;
	case SND_STM_FORMAT__LEFT_JUSTIFIED:
		snd_stm_printd(1, "- left justified\n");
		set__AUD_UNIPERIF_I2S_FMT__ALIGN_LEFT(player);
		set__AUD_UNIPERIF_I2S_FMT__PADDING_SONY_MODE(player);
		lr_pol = value__AUD_UNIPERIF_I2S_FMT__LR_POL_HIG(player);
		break;
	case SND_STM_FORMAT__RIGHT_JUSTIFIED:
		snd_stm_printd(1, "- right justified\n");
		set__AUD_UNIPERIF_I2S_FMT__ALIGN_RIGHT(player);
		set__AUD_UNIPERIF_I2S_FMT__PADDING_SONY_MODE(player);
		lr_pol = value__AUD_UNIPERIF_I2S_FMT__LR_POL_HIG(player);
		break;
	default:
		snd_BUG();
		return -EINVAL;
	}

	/* Configure data memory format */

	switch (runtime->format) {
	case SNDRV_PCM_FORMAT_S16_LE:
		/* One data word contains two samples */
		set__AUD_UNIPERIF_CONFIG__MEM_FMT_16_16(player);

		/* Workaround for a problem with L/R channels swap in case of
		 * 16/16 memory model: PCM player expects left channel data in
		 * word's upper two bytes, but due to little endianess
		 * character of our memory there is right channel data there;
		 * the workaround is to invert L/R signal, however it is
		 * cheating, because in such case channel phases are shifted
		 * by one sample...
		 * (ask me for more details if above is not clear ;-)
		 * TODO this somehow better... */
		if (lr_pol)
			set__AUD_UNIPERIF_I2S_FMT__LR_POL_LOW(player);
		else
			set__AUD_UNIPERIF_I2S_FMT__LR_POL_HIG(player);
		break;

	case SNDRV_PCM_FORMAT_S32_LE:
		/* Actually "16 bits/0 bits" means "32/28/24/20/18/16 bits
		 * on the left than zeros (if less than 32 bites)"... ;-) */
		set__AUD_UNIPERIF_CONFIG__MEM_FMT_16_0(player);

		/* In x/0 bits memory mode there is no problem with
		 * L/R polarity */
		if (lr_pol)
			set__AUD_UNIPERIF_I2S_FMT__LR_POL_HIG(player);
		else
			set__AUD_UNIPERIF_I2S_FMT__LR_POL_LOW(player);
		break;

	default:
		snd_BUG();
		return -EINVAL;
	}

	/* Set the number of samples to read */
	set__AUD_UNIPERIF_I2S_FMT__NO_OF_SAMPLES_TO_READ(player,
			snd_stm_uniperif_player_samples_per_period(runtime));

	/* Set up frequency synthesizer */
	result = clk_enable(player->clock);
	if (result != 0) {
		snd_stm_printe("Can't enable clock for player '%s'!\n",
				dev_name(player->device));
		return result;
	}

	result = clk_set_rate(player->clock, runtime->rate * oversampling);
	if (result != 0) {
		snd_stm_printe("Can't configure clock for player '%s'!\n",
				dev_name(player->device));
		clk_disable(player->clock);
		return result;
	}

	/* Configure PCM player frequency divider
	 *
	 *             Fdacclk             Fs * oversampling
	 * divider = ----------- = ------------------------------- =
	 *            2 * Fsclk     2 * Fs * bits_in_output_frame
	 *
	 *                  oversampling
	 *         = --------------------------
	 *            2 * bits_in_output_frame
	 * where:
	 *   - Fdacclk - frequency of DAC clock signal, known also as PCMCLK,
	 *               MCLK (master clock), "system clock" etc.
	 *   - Fsclk - frequency of SCLK (serial clock) aka BICK (bit clock)
	 *   - Fs - sampling rate (frequency)
	 *   - bits_in_output_frame - number of bits in output signal _frame_
	 *                (32 or 64, depending on NBIT field of FMT register)
	 */

	/* Set rounding to off */
	set__AUD_UNIPERIF_CTRL__ROUNDING_OFF(player);

	/* Set clock divisor */
	set__AUD_UNIPERIF_CTRL__DIVIDER(player,
				oversampling / (2 * bits_in_output_frame));

	/* Number of channels... */

	BUG_ON(runtime->channels % 2 != 0);
	BUG_ON(runtime->channels < 2);
	BUG_ON(runtime->channels > 10);

	set__AUD_UNIPERIF_I2S_FMT__NUM_CH(player, runtime->channels / 2);

	/* Set 1-bit audio format to disabled */
	set__AUD_UNIPERIF_CONFIG__ONE_BIT_AUD_DISABLE(player);

	set__AUD_UNIPERIF_I2S_FMT__ORDER_MSB(player);
	set__AUD_UNIPERIF_I2S_FMT__SCLK_EDGE_FALLING(player);

	/* No spdif formatting as outputting to DAC  */
	set__AUD_UNIPERIF_CTRL__SPDIF_FMT_OFF(player);

	return 0;
}

static void snd_stm_uniperif_player_set_channel_status(
		struct snd_stm_uniperif_player *player,
		struct snd_pcm_runtime *runtime)
{
	int n;
	unsigned int status;

	snd_stm_printd(1, "%s(player=0x%p)\n", __func__, player);

	BUG_ON(!player);
	BUG_ON(!snd_stm_magic_valid(player));

	/*
	 * Some AVRs and TVs require the channel status to contain a correct
	 * sampling frequency. If no sample rate is already specified, then
	 * set one.
	 */

	if (runtime && (player->stream_settings.iec958.status[3] == 0x01)) {
		switch (runtime->rate) {
		case 22050:
			player->stream_settings.iec958.status[3] = 0x04;
			break;
		case 44100:
			player->stream_settings.iec958.status[3] = 0x00;
			break;
		case 88200:
			player->stream_settings.iec958.status[3] = 0x08;
			break;
		case 176400:
			player->stream_settings.iec958.status[3] = 0x0c;
			break;
		case 24000:
			player->stream_settings.iec958.status[3] = 0x06;
			break;
		case 48000:
			player->stream_settings.iec958.status[3] = 0x02;
			break;
		case 96000:
			player->stream_settings.iec958.status[3] = 0x0a;
			break;
		case 192000:
			player->stream_settings.iec958.status[3] = 0x0e;
			break;
		case 32000:
			player->stream_settings.iec958.status[3] = 0x03;
			break;
		case 768000:
			player->stream_settings.iec958.status[3] = 0x09;
			break;
		default:
			/* Mark as sampling frequency not indicated */
			player->stream_settings.iec958.status[3] = 0x01;
			break;
		}
	}

	/* Program the new channel status */
	for (n = 0; n < 6; ++n) {
		status  = player->stream_settings.iec958.status[0+(n*4)] & 0xf;
		status |= player->stream_settings.iec958.status[1+(n*4)] << 8;
		status |= player->stream_settings.iec958.status[2+(n*4)] << 16;
		status |= player->stream_settings.iec958.status[3+(n*4)] << 24;
		snd_stm_printd(1, "- Channel Status Register %d: %08x\n",
				n, status);
		set__AUD_UNIPERIF_CHANNEL_STA_REGn(player, n, status);
	}

	/* Update the channel status */
	set__AUD_UNIPERIF_CONFIG__CHL_STS_UPDATE(player);
}

static int snd_stm_uniperif_player_prepare_iec958(
		struct snd_stm_uniperif_player *player,
		struct snd_pcm_runtime *runtime)
{
	int oversampling;
	int result;

	snd_stm_printd(1, "%s(player=0x%p)\n", __func__, player);

	BUG_ON(!player);
	BUG_ON(!snd_stm_magic_valid(player));
	BUG_ON(!runtime);

	/* Get format & oversampling value from connected converter */
	if (player->conv_group) {
		if (player->info->player_type ==
			SND_STM_UNIPERIF_PLAYER_TYPE_SPDIF) {
			int format = snd_stm_conv_get_format(
					player->conv_group);

			BUG_ON((format & SND_STM_FORMAT__MASK) !=
				SND_STM_FORMAT__SPDIF);
		}

		oversampling =
			snd_stm_conv_get_oversampling(player->conv_group);
		if (oversampling == 0)
			oversampling = DEFAULT_OVERSAMPLING;
	} else {
		oversampling = DEFAULT_OVERSAMPLING;
	}

	snd_stm_printd(1, "Player %s: sampling frequency %d, oversampling %d\n",
			dev_name(player->device), runtime->rate,
			oversampling);

	/* Oversampling must be multiple of 128 as spdif frame is 32-bits */
	BUG_ON(oversampling <= 0);
	BUG_ON(oversampling % 128 != 0);

	/* No sample rates below 32kHz are supported for iec958 */
	if (runtime->rate < MIN_IEC958_SAMPLE_RATE) {
		snd_stm_printe("Player %s: Invalid sample rate (%d)\n",
			       dev_name(player->device), runtime->rate);
		return -EINVAL;
	}

	if (player->stream_settings.input_mode ==
			SNDRV_STM_UNIPERIF_SPDIF_INPUT_MODE_NORMAL) {

		snd_stm_printd(1, "- Normal input mode\n");

		switch (runtime->format) {
		case SNDRV_PCM_FORMAT_S16_LE:
			snd_stm_printd(1, "- 16 bits per subframe\n");
			/* 16/16 memory format */
			set__AUD_UNIPERIF_CONFIG__MEM_FMT_16_16(player);
			/* 16-bits per sub-frame */
			set__AUD_UNIPERIF_I2S_FMT__NBIT_32(player);
			/* Set 16-bit sample precision */
			set__AUD_UNIPERIF_I2S_FMT__DATA_SIZE_16(player);
			break;
		case SNDRV_PCM_FORMAT_S32_LE:
			snd_stm_printd(1, "- 32 bits per subframe\n");
			/* 16/0 memory format */
			set__AUD_UNIPERIF_CONFIG__MEM_FMT_16_0(player);
			/* 32-bits per sub-frame */
			set__AUD_UNIPERIF_I2S_FMT__NBIT_32(player);
			/* Set 24-bit sample precision */
			set__AUD_UNIPERIF_I2S_FMT__DATA_SIZE_24(player);
			break;
		default:
			snd_BUG();
			return -EINVAL;
		}

		/* Set parity to be calculated by the hardware */
		set__AUD_UNIPERIF_CONFIG__PARITY_CNTR_BY_HW(player);

		/* Set channel status bits to be inserted by the hardware */
		set__AUD_UNIPERIF_CONFIG__CHANNEL_STA_CNTR_BY_HW(player);

		/* Set user data bits to be inserted by the hardware */
		set__AUD_UNIPERIF_CONFIG__USER_DAT_CNTR_BY_HW(player);

		/* Set validity bits to be inserted by the hardware */
		set__AUD_UNIPERIF_CONFIG__VALIDITY_DAT_CNTR_BY_HW(player);

		/* Set full software control to disabled */
		set__AUD_UNIPERIF_CONFIG__SPDIF_SW_CTRL_DISABLE(player);

		set__AUD_UNIPERIF_CTRL__ZERO_STUFF_HW(player);

		if (player->stream_settings.encoding_mode ==
				SNDRV_STM_UNIPERIF_SPDIF_ENCODING_MODE_PCM) {

			snd_stm_printd(1, "- Linear PCM mode\n");

			/* Set 24-bit max word size (Player2 should do this) */
			player->stream_settings.iec958.status[4] = 0x0b;

			/* Clear user validity bits */
			set__AUD_UNIPERIF_USER_VALIDITY__VALIDITY_LEFT(player,
				0);
			set__AUD_UNIPERIF_USER_VALIDITY__VALIDITY_RIGHT(player,
				0);
		} else {
			struct snd_stm_uniperif_spdif_settings *settings =
				&player->stream_settings;

			snd_stm_printd(1, "- Encoded mode\n");

			/* Configure number of frames for data/pause burst */
			set__AUD_UNIPERIF_SPDIF_FRAMELEN_BURST__DAT_BURST(
					player,
					settings->iec61937_audio_repetition);
			set__AUD_UNIPERIF_SPDIF_FRAMELEN_BURST__PAUSE_BURST(
					player,
					settings->iec61937_pause_repetition);

			/* Select in bits */
			set__AUD_UNIPERIF_CONFIG__PD_FMT_IN_BIT(player);

			/* Configure iec61937 preamble */
			set__AUD_UNIPERIF_SPDIF_PA_PB__PA(player,
					settings->iec61937_preamble[0] |
					settings->iec61937_preamble[1] << 8);
			set__AUD_UNIPERIF_SPDIF_PA_PB__PB(player,
					settings->iec61937_preamble[2] |
					settings->iec61937_preamble[3] << 8);
			set__AUD_UNIPERIF_SPDIF_PC_PD__PC(player,
					settings->iec61937_preamble[4] |
					settings->iec61937_preamble[5] << 8);
			set__AUD_UNIPERIF_SPDIF_PC_PD__PD(player,
					settings->iec61937_preamble[6] |
					settings->iec61937_preamble[7] << 8);

			/* Set user validity bits */
			set__AUD_UNIPERIF_USER_VALIDITY__VALIDITY_LEFT(player,
				1);
			set__AUD_UNIPERIF_USER_VALIDITY__VALIDITY_RIGHT(player,
				1);
		}

		/* Reset iec958 software formatting counters */
		player->stream_iec958_status_cnt = 0;
		player->stream_iec958_subcode_cnt = 0;

		/* Update the channel status */
		snd_stm_uniperif_player_set_channel_status(player, runtime);

		/* Clear the user validity user bits */
		set__AUD_UNIPERIF_USER_VALIDITY__USER_LEFT(player, 0);
		set__AUD_UNIPERIF_USER_VALIDITY__USER_RIGHT(player, 0);
	} else {

		snd_stm_printd(1, "- Raw input mode\n");

		/* 16/0 memory format */
		set__AUD_UNIPERIF_CONFIG__MEM_FMT_16_0(player);

		/* 32-bits per sub-frame */
		set__AUD_UNIPERIF_I2S_FMT__NBIT_32(player);

		/* Set 24-bit sample precision */
		set__AUD_UNIPERIF_I2S_FMT__DATA_SIZE_32(player);

		/* Set parity to be calculated by the hardware */
		set__AUD_UNIPERIF_CONFIG__PARITY_CNTR_BY_HW(player);

		/* Set channel status bits to be inserted by the software */
		set__AUD_UNIPERIF_CONFIG__CHANNEL_STA_CNTR_BY_SW(player);

		/* Set user data bits to be inserted by the software */
		set__AUD_UNIPERIF_CONFIG__USER_DAT_CNTR_BY_SW(player);

		/* Set validity bits to be inserted by the software */
		set__AUD_UNIPERIF_CONFIG__VALIDITY_DAT_CNTR_BY_SW(player);

		/* Set full software control to enabled */
		set__AUD_UNIPERIF_CONFIG__SPDIF_SW_CTRL_ENABLE(player);

		/* Set zero stuff by hardware to stop glitch at end of audio */
		set__AUD_UNIPERIF_CTRL__ZERO_STUFF_HW(player);
	}

	/* Disable one-bit audio mode */
	set__AUD_UNIPERIF_CONFIG__ONE_BIT_AUD_DISABLE(player);

	/* Enable consecutive frames repetition of Z preamble (not for HBRA) */
	set__AUD_UNIPERIF_CONFIG__REPEAT_CHL_STS_ENABLE(player);

	/* Change to SUF0_SUBF1 and left/right channels swap! */
	set__AUD_UNIPERIF_CONFIG__SUBFRAME_SEL_SUBF1_SUBF0(player);

	/* Set left-right clock polarity depending on player type */
	switch (player->info->player_type) {
	case SND_STM_UNIPERIF_PLAYER_TYPE_HDMI:
		set__AUD_UNIPERIF_I2S_FMT__LR_POL_HIG(player);
		break;
	case SND_STM_UNIPERIF_PLAYER_TYPE_SPDIF:
		set__AUD_UNIPERIF_I2S_FMT__LR_POL_LOW(player);
		break;
	default:
		snd_BUG();
		return -EINVAL;
	}

	/* Set data output on rising edge */
	set__AUD_UNIPERIF_I2S_FMT__SCLK_EDGE_RISING(player);

	/* Set Sony padding mode (data is not delayed by one bit-clock) */
	set__AUD_UNIPERIF_I2S_FMT__PADDING_SONY_MODE(player);

	/* Set data aligned to left with respect to left-right clock polarity */
	set__AUD_UNIPERIF_I2S_FMT__ALIGN_LEFT(player);

	/* Set data output as MSB first */
	set__AUD_UNIPERIF_I2S_FMT__ORDER_MSB(player);

	/* Set the number of channels (maximum supported by spdif is 2) */
	if (player->info->player_type == SND_STM_UNIPERIF_PLAYER_TYPE_SPDIF)
		BUG_ON(runtime->channels != 2);

	set__AUD_UNIPERIF_I2S_FMT__NUM_CH(player, runtime->channels / 2);

	/* Set the number of samples to read */
	set__AUD_UNIPERIF_I2S_FMT__NO_OF_SAMPLES_TO_READ(player,
			snd_stm_uniperif_player_samples_per_period(runtime));

	/* Set rounding to off */
	set__AUD_UNIPERIF_CTRL__ROUNDING_OFF(player);

	/* Set clock divisor */
	set__AUD_UNIPERIF_CTRL__DIVIDER(player, oversampling / 128);

	/* Set the spdif latency to not wait before starting player */
	set__AUD_UNIPERIF_CTRL__SPDIF_LAT_OFF(player);

	/*
	 * Ensure spdif iec-60958 formatting is off. It will be enabled
	 * in function snd_stm_uniperif_player_start at the same time as
	 * the operation mode is set to work around a silicon issue.
	 */
	set__AUD_UNIPERIF_CTRL__SPDIF_FMT_OFF(player);

	/* Enable clock */
	result = clk_enable(player->clock);
	if (result != 0) {
		snd_stm_printe("Can't enable clock for player '%s'!\n",
				dev_name(player->device));
		return result;
	}

	/* Set clock rate */
	result = clk_set_rate(player->clock, runtime->rate * oversampling);
	if (result != 0) {
		snd_stm_printe("Can't configure clock for player '%s'!\n",
				dev_name(player->device));
		clk_disable(player->clock);
		return result;
	}

	return 0;
}

static int snd_stm_uniperif_player_prepare(struct snd_pcm_substream *substream)
{
	struct snd_stm_uniperif_player *player =
			snd_pcm_substream_chip(substream);
	struct snd_pcm_runtime *runtime = substream->runtime;
	int changed;
	int result;

	snd_stm_printd(1, "%s(substream=0x%p)\n", __func__, substream);

	BUG_ON(!player);
	BUG_ON(!snd_stm_magic_valid(player));
	BUG_ON(!runtime);
	BUG_ON(runtime->period_size * runtime->channels >=
			MAX_SAMPLES_PER_PERIOD);

	if (dma_audio_is_parking_active(player->dma_channel)) {
		changed  = (player->current_rate != runtime->rate);
		changed |= (player->current_format != runtime->format);
		changed |= (player->current_channels != runtime->channels);

		/* Return if new configuration is the same as current */
		if (changed == 0)
			return 0;

		/* Ensure uniperif is stopped and parking not active */
		snd_stm_uniperif_player_halt(substream);
	}

	/* We know dma stopped and not parked - configure parking */
	result = dma_audio_parking_config(player->dma_channel,
			&player->dma_park_config);
	if (result) {
		snd_stm_printe("Failed to reconfigure dma parking\n");
		return result;
	}

	/* Store new configuration */
	player->current_rate = runtime->rate;
	player->current_format = runtime->format;
	player->current_channels = runtime->channels;

	/* Uniperipheral setup is dependent on player type */
	switch (player->info->player_type) {
	case SND_STM_UNIPERIF_PLAYER_TYPE_HDMI:
		return snd_stm_uniperif_player_prepare_iec958(player, runtime);
	case SND_STM_UNIPERIF_PLAYER_TYPE_PCM:
		return snd_stm_uniperif_player_prepare_pcm(player, runtime);
	case SND_STM_UNIPERIF_PLAYER_TYPE_SPDIF:
		return snd_stm_uniperif_player_prepare_iec958(player, runtime);
	default:
		snd_BUG();
		return -EINVAL;
	}
}

static int snd_stm_uniperif_player_start(struct snd_pcm_substream *substream)
{
	struct snd_stm_uniperif_player *player =
			snd_pcm_substream_chip(substream);
	unsigned int ctrl;

	snd_stm_printd(1, "%s(substream=0x%p)\n", __func__, substream);

	BUG_ON(!player);
	BUG_ON(!snd_stm_magic_valid(player));

	/* Prepare the dma descriptor */
	player->dma_descriptor = dma_audio_prep_tx_cyclic(player->dma_channel,
			substream->runtime->dma_addr, player->buffer_bytes,
			player->period_bytes);
	if (!player->dma_descriptor) {
		snd_stm_printe("Failed to prepare dma descriptor\n");
		return -ENOMEM;
	}

	/* Set the dma callback */
	player->dma_descriptor->callback = snd_stm_uniperif_player_comp_cb;
	player->dma_descriptor->callback_param = player;


	/* If parking is enabled, just submit the next decsriptor and return */
	if (dma_audio_is_parking_active(player->dma_channel)) {
		player->dma_cookie = dmaengine_submit(player->dma_descriptor);
		return 0;
	}

	/* Reset uniperipheral player */
	set__AUD_UNIPERIF_SOFT_RST__SOFT_RST(player);
	while (get__AUD_UNIPERIF_SOFT_RST__SOFT_RST(player))
		udelay(5);

	/* Launch FDMA transfer */
	player->dma_cookie = dmaengine_submit(player->dma_descriptor);

	/* Enable player interrupts (and clear possible stalled ones) */
	enable_irq(player->irq);
	set__AUD_UNIPERIF_ITS_BCLR__DMA_ERROR(player);
	set__AUD_UNIPERIF_ITM_BSET__DMA_ERROR(player);
	set__AUD_UNIPERIF_ITS_BCLR__FIFO_ERROR(player);
	set__AUD_UNIPERIF_ITM_BSET__FIFO_ERROR(player);

	/*
	 * If spdif formatting is required, then it must be enabled when we set
	 * the operation mode else it will not work.
	 */

	ctrl  = get__AUD_UNIPERIF_CTRL(player);
	ctrl &= ~mask__AUD_UNIPERIF_CTRL__OPERATION(player);

	/* Enable spdif formatting for hdmi and spdif only */
	if (player->info->player_type != SND_STM_UNIPERIF_PLAYER_TYPE_PCM)
		ctrl |= (1 << shift__AUD_UNIPERIF_CTRL__SPDIF_FMT(player));

	/*
	 * For compatibility with Player2, we do not use encoded mode when it
	 * is selected. Instead we use normal linear PCM mode, but with the
	 * channel status set for encoded mode and the validity bits set. We do
	 * this because Player2 will provide properly formatted data in the
	 * same 32-bit data format. We are essentially only turning off some of
	 * the useful hardware features (that may or may not work!) and Player2
	 * is doing the work in software.
	 */

	if (player->stream_settings.encoding_mode ==
			SNDRV_STM_UNIPERIF_SPDIF_ENCODING_MODE_ENCODED) {
		/*
		set__AUD_UNIPERIF_ITS_BCLR__PA_PB_SYNC_LOST(player);
		set__AUD_UNIPERIF_ITM_BSET__PA_PB_SYNC_LOST(player);

		ctrl |= (value__AUD_UNIPERIF_CTRL__OPERATION_ENC_DATA(player) <<
		*/
		ctrl |= (value__AUD_UNIPERIF_CTRL__OPERATION_PCM_DATA(player) <<
			shift__AUD_UNIPERIF_CTRL__OPERATION(player));
	} else {
		ctrl |= (value__AUD_UNIPERIF_CTRL__OPERATION_PCM_DATA(player) <<
			shift__AUD_UNIPERIF_CTRL__OPERATION(player));
	}

	/* Launch the player */
	set__AUD_UNIPERIF_CTRL(player, ctrl);

	/* Wake up & unmute converter */
	if (player->conv_group) {
		snd_stm_conv_enable(player->conv_group,
				0, substream->runtime->channels - 1);
		snd_stm_conv_unmute(player->conv_group);
	}

	return 0;
}

static int snd_stm_uniperif_player_halt(struct snd_pcm_substream *substream)
{
	struct snd_stm_uniperif_player *player =
			snd_pcm_substream_chip(substream);

	snd_stm_printd(1, "%s(substream=0x%p)\n", __func__, substream);

	BUG_ON(!player);
	BUG_ON(!snd_stm_magic_valid(player));

	/* Mute & shutdown converter */
	if (player->conv_group) {
		snd_stm_conv_mute(player->conv_group);
		snd_stm_conv_disable(player->conv_group);
	}

	/* Disable interrupts */
	set__AUD_UNIPERIF_ITM_BCLR__DMA_ERROR(player);
	set__AUD_UNIPERIF_ITM_BCLR__PA_PB_SYNC_LOST(player);
	set__AUD_UNIPERIF_ITM_BCLR__FIFO_ERROR(player);
	disable_irq_nosync(player->irq);

	/* Stop uniperipheral player */
	set__AUD_UNIPERIF_CTRL__OPERATION_OFF(player);
	clk_disable(player->clock);

	/* Stop FDMA transfer */
	dmaengine_terminate_all(player->dma_channel);

	/* Reset current configuration */
	player->current_rate = 0;
	player->current_format = 0;
	player->current_channels = 0;

	return 0;
}

static int snd_stm_uniperif_player_stop(struct snd_pcm_substream *substream)
{
	struct snd_stm_uniperif_player *player =
			snd_pcm_substream_chip(substream);

	snd_stm_printd(1, "%s(substream=0x%p)\n", __func__, substream);

	BUG_ON(!player);
	BUG_ON(!snd_stm_magic_valid(player));

	/* If player is parking enabled, then activate parking and return */
	if (player->info->parking_enabled == 1)
		return dma_audio_parking_enable(player->dma_channel);

	/* Actually stop the uniperipheral player */
	return snd_stm_uniperif_player_halt(substream);
}

static int snd_stm_uniperif_player_pause(struct snd_pcm_substream *substream)
{
	struct snd_stm_uniperif_player *player =
			snd_pcm_substream_chip(substream);
	unsigned int ctrl;

	snd_stm_printd(1, "%s(substream=0x%p)\n", __func__, substream);

	BUG_ON(!player);
	BUG_ON(!snd_stm_magic_valid(player));

	/* Pause/resume is not supported when parking active */
	BUG_ON(dma_audio_is_parking_active(player->dma_channel));

	/*
	 * If spdif formatting is required, then it must be enabled when we set
	 * the operation mode else it will not work.
	 */

	ctrl  = get__AUD_UNIPERIF_CTRL(player);
	ctrl &= ~mask__AUD_UNIPERIF_CTRL__OPERATION(player);

	/* Enable spdif formatting for hdmi and spdif only */
	if (player->info->player_type != SND_STM_UNIPERIF_PLAYER_TYPE_PCM)
		ctrl |= (1 << shift__AUD_UNIPERIF_CTRL__SPDIF_FMT(player));

	/* "Mute" player
	 * Documentation describes this mode in a wrong way - data is _not_
	 * consumed in the "mute" mode, so it is actually a "pause" mode
	 */
	if (player->stream_settings.encoding_mode ==
	    SNDRV_STM_UNIPERIF_SPDIF_ENCODING_MODE_ENCODED) {
		ctrl |= (value__AUD_UNIPERIF_CTRL__OPERATION_MUTE_PAUSE_BURST(
				 player) <<
			shift__AUD_UNIPERIF_CTRL__OPERATION(player));
	} else {
		ctrl |= (value__AUD_UNIPERIF_CTRL__OPERATION_MUTE_PCM_NULL(
				 player) <<
			shift__AUD_UNIPERIF_CTRL__OPERATION(player));
	}

	set__AUD_UNIPERIF_CTRL(player, ctrl);

	return 0;
}

static inline int snd_stm_uniperif_player_release(struct snd_pcm_substream
		*substream)
{
	struct snd_stm_uniperif_player *player =
		snd_pcm_substream_chip(substream);
	unsigned int ctrl;

	snd_stm_printd(1, "%s(substream=0x%p)\n", __func__, substream);

	BUG_ON(!player);
	BUG_ON(!snd_stm_magic_valid(player));

	/* Pause/resume is not supported when parking active */
	BUG_ON(dma_audio_is_parking_active(player->dma_channel));

	/*
	 * If spdif formatting is required, then it must be enabled when we set
	 * the operation mode else it will not work.
	 */

	ctrl  = get__AUD_UNIPERIF_CTRL(player);
	ctrl &= ~mask__AUD_UNIPERIF_CTRL__OPERATION(player);

	/* Enable spdif formatting for hdmi and spdif only */
	if (player->info->player_type != SND_STM_UNIPERIF_PLAYER_TYPE_PCM)
		ctrl |= (1 << shift__AUD_UNIPERIF_CTRL__SPDIF_FMT(player));

	/* "Unmute" player */
	if (player->stream_settings.encoding_mode ==
			SNDRV_STM_UNIPERIF_SPDIF_ENCODING_MODE_ENCODED) {
		/*
		 * Player2 provides us with properly formatted encoded data, so
		 * we actually use linear pcm mode (but with channel status and
		 * validity bits setup for encoded mode).
		 */
		/*
		ctrl |= (value__AUD_UNIPERIF_CTRL__OPERATION_ENC_DATA(
		*/
		ctrl |= (value__AUD_UNIPERIF_CTRL__OPERATION_PCM_DATA(
				 player) <<
			shift__AUD_UNIPERIF_CTRL__OPERATION(player));
	} else {
		ctrl |= (value__AUD_UNIPERIF_CTRL__OPERATION_PCM_DATA(
				 player) <<
			shift__AUD_UNIPERIF_CTRL__OPERATION(player));
	}

	set__AUD_UNIPERIF_CTRL(player, ctrl);

	return 0;
}

static int snd_stm_uniperif_player_trigger(struct snd_pcm_substream *substream,
		int command)
{
	snd_stm_printd(1, "%s(substream=0x%p, command=%d)\n",
		       __func__, substream, command);

	switch (command) {
	case SNDRV_PCM_TRIGGER_START:
		return snd_stm_uniperif_player_start(substream);
	case SNDRV_PCM_TRIGGER_RESUME:
		return snd_stm_uniperif_player_start(substream);
	case SNDRV_PCM_TRIGGER_STOP:
		return snd_stm_uniperif_player_stop(substream);
	case SNDRV_PCM_TRIGGER_SUSPEND:
		return snd_stm_uniperif_player_halt(substream);
	case SNDRV_PCM_TRIGGER_PAUSE_PUSH:
		return snd_stm_uniperif_player_pause(substream);
	case SNDRV_PCM_TRIGGER_PAUSE_RELEASE:
		return snd_stm_uniperif_player_release(substream);
	default:
		return -EINVAL;
	}
}

static snd_pcm_uframes_t snd_stm_uniperif_player_pointer(
		struct snd_pcm_substream *substream)
{
	struct snd_stm_uniperif_player *player =
		snd_pcm_substream_chip(substream);
	struct snd_pcm_runtime *runtime = substream->runtime;
	int residue, hwptr;
	snd_pcm_uframes_t pointer;

	snd_stm_printd(2, "%s(substream=0x%p)\n", __func__, substream);

	BUG_ON(!player);
	BUG_ON(!snd_stm_magic_valid(player));
	BUG_ON(!runtime);

	/* Pause/resume is not supported when parking active */
	if (dma_audio_is_parking_active(player->dma_channel)) {
		residue = 0;
		hwptr = 0;
	} else {
		struct dma_tx_state state;
		enum dma_status status;

		status = player->dma_channel->device->device_tx_status(
				player->dma_channel,
				player->dma_cookie, &state);

		residue = state.residue;
		hwptr = (runtime->dma_bytes - residue) % runtime->dma_bytes;
	}

	pointer = bytes_to_frames(runtime, hwptr);

	snd_stm_printd(2, "FDMA residue value is %i and buffer size is %u"
			" bytes...\n", residue, runtime->dma_bytes);
	snd_stm_printd(2, "... so HW pointer in frames is %lu (0x%lx)!\n",
			pointer, pointer);

	return pointer;
}

static int snd_stm_uniperif_player_silence(struct snd_pcm_substream *substream,
		int channel, snd_pcm_uframes_t pos, snd_pcm_uframes_t count)
{
	int result = 0;
	struct snd_stm_uniperif_player *player =
		snd_pcm_substream_chip(substream);
	struct snd_pcm_runtime *runtime = substream->runtime;

	snd_stm_printd(2,
		       "%s(substream=0x%p, channel=%d, pos=%lu, count=%lu)\n",
		       __func__, substream, channel, pos, count);

	BUG_ON(!player);
	BUG_ON(!snd_stm_magic_valid(player));
	BUG_ON(!runtime);
	BUG_ON(channel != -1); /* Interleaved buffer */

	/*in normal and RAW mode we are only inserting NULL samples,
	  for normal mode IP will do formatting*/
	/*Why for RAW mode formatting is not required ???*/
	result = snd_pcm_format_set_silence(runtime->format,
				runtime->dma_area +
				frames_to_bytes(runtime, pos),
				runtime->channels * count);

	return result;
}


static struct snd_pcm_ops snd_stm_uniperif_player_pcm_ops = {
	.open =      snd_stm_uniperif_player_open,
	.close =     snd_stm_uniperif_player_close,
	.mmap =      snd_stm_buffer_mmap,
	.ioctl =     snd_pcm_lib_ioctl,
	.hw_params = snd_stm_uniperif_player_hw_params,
	.hw_free =   snd_stm_uniperif_player_hw_free,
	.prepare =   snd_stm_uniperif_player_prepare,
	.trigger =   snd_stm_uniperif_player_trigger,
	.pointer =   snd_stm_uniperif_player_pointer,
	.silence =   snd_stm_uniperif_player_silence,
};



/*
 * ALSA uniperipheral spdif controls
 */

static int snd_stm_uniperif_player_ctl_iec958_get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_stm_uniperif_player *player = snd_kcontrol_chip(kcontrol);

	snd_stm_printd(1, "%s(kcontrol=0x%p, ucontrol=0x%p)\n",
		       __func__, kcontrol, ucontrol);

	BUG_ON(!player);
	BUG_ON(!snd_stm_magic_valid(player));

	spin_lock(&player->default_settings_lock);
	ucontrol->value.iec958 = player->stream_settings.iec958;
	spin_unlock(&player->default_settings_lock);

	return 0;
}

static int snd_stm_uniperif_player_ctl_iec958_put(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_stm_uniperif_player *player = snd_kcontrol_chip(kcontrol);
	int changed = 0;

	snd_stm_printd(1, "%s(kcontrol=0x%p, ucontrol=0x%p)\n",
		       __func__, kcontrol, ucontrol);

	BUG_ON(!player);
	BUG_ON(!snd_stm_magic_valid(player));

	spin_lock(&player->default_settings_lock);

	/* If user settings differ from the default, update default */
	if (snd_stm_iec958_cmp(&player->default_settings.iec958,
				&ucontrol->value.iec958) != 0) {
		player->default_settings.iec958 = ucontrol->value.iec958;
		changed = 1;
	}

	/* If user settings differ from the current, update current */
	if (snd_stm_iec958_cmp(&player->stream_settings.iec958,
				&ucontrol->value.iec958) != 0) {
		player->stream_settings.iec958 = ucontrol->value.iec958;
		changed = 1;

		/*
		 * Player2 fails to set the max word size to 24-bit for setting
		 * the channel status for linear pcm. It does however set the
		 * max word length correctly for encoded mode. So if we check
		 * for the max word length being zero, then we know the channel
		 * status is for linear pcm mode and the max word length should
		 * be set to 24-bit.
		 */

		if (player->stream_settings.iec958.status[4] == 0)
			player->stream_settings.iec958.status[4] = 0x0b;
	}

	/* If settings changed and uniperipheral in operation, update */
	if (changed)
		snd_stm_uniperif_player_set_channel_status(player, NULL);

	spin_unlock(&player->default_settings_lock);

	return changed;
}

/* "Raw Data" switch controls data input mode - "RAW" means that played
 * data are already properly formated (VUC bits); in "normal" mode
 * this data will be added by driver according to setting passed in\
 * following controls So that play things in PCM mode*/

static int snd_stm_uniperif_player_ctl_raw_get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_stm_uniperif_player *player = snd_kcontrol_chip(kcontrol);

	snd_stm_printd(1, "%s(kcontrol=0x%p, ucontrol=0x%p)\n",
		       __func__, kcontrol, ucontrol);

	BUG_ON(!player);
	BUG_ON(!snd_stm_magic_valid(player));

	spin_lock(&player->default_settings_lock);
	ucontrol->value.integer.value[0] =
			(player->default_settings.input_mode ==
			SNDRV_STM_UNIPERIF_SPDIF_INPUT_MODE_RAW);
	spin_unlock(&player->default_settings_lock);

	return 0;
}

static int snd_stm_uniperif_player_ctl_raw_put(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_stm_uniperif_player *player = snd_kcontrol_chip(kcontrol);
	int changed = 0;
	struct snd_pcm_hardware hardware;
	enum snd_stm_uniperif_spdif_input_mode input_mode;

	snd_stm_printd(1, "%s(kcontrol=0x%p, ucontrol=0x%p)\n",
		       __func__, kcontrol, ucontrol);

	BUG_ON(!player);
	BUG_ON(!snd_stm_magic_valid(player));

	if (ucontrol->value.integer.value[0]) {
		hardware = snd_stm_uniperif_player_raw_hw;
		input_mode = SNDRV_STM_UNIPERIF_SPDIF_INPUT_MODE_RAW;
	} else {
		hardware = snd_stm_uniperif_player_pcm_hw;
		input_mode = SNDRV_STM_UNIPERIF_SPDIF_INPUT_MODE_NORMAL;
	}

	spin_lock(&player->default_settings_lock);
	changed = (input_mode != player->default_settings.input_mode);
	player->hardware = hardware;
	player->default_settings.input_mode = input_mode;
	spin_unlock(&player->default_settings_lock);

	return changed;
}

/*
 * The 'Encoded Data' switch should select between linear PCM and encoded
 * operation of the uniperipheral. The main difference between the modes being
 * how mute data is generated. In linear PCM mode null data is used and in
 * encoded mode pause bursts are used.
 */

static int snd_stm_uniperif_player_ctl_encoded_get(
		struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_stm_uniperif_player *player = snd_kcontrol_chip(kcontrol);

	snd_stm_printd(1, "%s(kcontrol=0x%p, ucontrol=0x%p)\n",
		       __func__, kcontrol, ucontrol);

	BUG_ON(!player);
	BUG_ON(!snd_stm_magic_valid(player));

	spin_lock(&player->default_settings_lock);
	ucontrol->value.integer.value[0] =
			(player->default_settings.encoding_mode ==
			SNDRV_STM_UNIPERIF_SPDIF_ENCODING_MODE_ENCODED);
	spin_unlock(&player->default_settings_lock);

	return 0;
}

static int snd_stm_uniperif_player_ctl_encoded_put(
		struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_stm_uniperif_player *player = snd_kcontrol_chip(kcontrol);
	int changed = 0;
	enum snd_stm_uniperif_spdif_encoding_mode encoding_mode;

	snd_stm_printd(1, "%s(kcontrol=0x%p, ucontrol=0x%p)\n",
		       __func__, kcontrol, ucontrol);

	BUG_ON(!player);
	BUG_ON(!snd_stm_magic_valid(player));

	if (ucontrol->value.integer.value[0])
		encoding_mode = SNDRV_STM_UNIPERIF_SPDIF_ENCODING_MODE_ENCODED;
	else
		encoding_mode = SNDRV_STM_UNIPERIF_SPDIF_ENCODING_MODE_PCM;

	spin_lock(&player->default_settings_lock);

	/* If user settings differ from the default, update default */
	if (encoding_mode != player->default_settings.encoding_mode) {
		player->default_settings.encoding_mode = encoding_mode;
		changed = 1;
	}

	/* If user settings differ from the current, update current */
	if (encoding_mode != player->stream_settings.encoding_mode) {
		player->stream_settings.encoding_mode = encoding_mode;
		changed = 1;
	}

	/* If settings changed and uniperipheral in operation, update */
	if (changed) {
		set__AUD_UNIPERIF_USER_VALIDITY__VALIDITY_LEFT(player,
				ucontrol->value.integer.value[0]);
		set__AUD_UNIPERIF_USER_VALIDITY__VALIDITY_RIGHT(player,
				ucontrol->value.integer.value[0]);
	}

	spin_unlock(&player->default_settings_lock);

	return changed;
}

/* Three following controls are valid for encoded mode only - they
 * control IEC 61937 preamble and data burst periods (see mentioned
 * standard for more informations) */

static int snd_stm_uniperif_player_ctl_preamble_info(
		struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_info *uinfo)
{
	uinfo->type = SNDRV_CTL_ELEM_TYPE_BYTES;
	uinfo->count = SPDIF_PREAMBLE_BYTES;
	return 0;
}

static int snd_stm_uniperif_player_ctl_preamble_get(
		struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_stm_uniperif_player *player  = snd_kcontrol_chip(kcontrol);

	snd_stm_printd(1, "%s(kcontrol=0x%p, ucontrol=0x%p)\n",
		       __func__, kcontrol, ucontrol);

	BUG_ON(!player);
	BUG_ON(!snd_stm_magic_valid(player));

	spin_lock(&player->default_settings_lock);
	memcpy(ucontrol->value.bytes.data,
			player->default_settings.iec61937_preamble,
			SPDIF_PREAMBLE_BYTES);
	spin_unlock(&player->default_settings_lock);

	return 0;
}

static int snd_stm_uniperif_player_ctl_preamble_put(
		struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_stm_uniperif_player *player = snd_kcontrol_chip(kcontrol);
	int changed = 0;

	snd_stm_printd(1, "%s(kcontrol=0x%p, ucontrol=0x%p)\n",
		       __func__, kcontrol, ucontrol);

	BUG_ON(!player);
	BUG_ON(!snd_stm_magic_valid(player));

	spin_lock(&player->default_settings_lock);
	if (memcmp(player->default_settings.iec61937_preamble,
			ucontrol->value.bytes.data,
			SPDIF_PREAMBLE_BYTES) != 0) {
		changed = 1;
		memcpy(player->default_settings.iec61937_preamble,
			ucontrol->value.bytes.data, SPDIF_PREAMBLE_BYTES);
	}
	spin_unlock(&player->default_settings_lock);

	return changed;
}

static int snd_stm_uniperif_player_ctl_repetition_info(struct snd_kcontrol
		*kcontrol, struct snd_ctl_elem_info *uinfo)
{
	uinfo->type = SNDRV_CTL_ELEM_TYPE_INTEGER;
	uinfo->count = 1;
	uinfo->value.integer.min = 0;
	uinfo->value.integer.max = 0xffff;
	return 0;
}

static int snd_stm_uniperif_player_ctl_audio_repetition_get(struct snd_kcontrol
		*kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	struct snd_stm_uniperif_player *player = snd_kcontrol_chip(kcontrol);

	snd_stm_printd(1, "%s(kcontrol=0x%p, ucontrol=0x%p)\n",
		       __func__, kcontrol, ucontrol);

	BUG_ON(!player);
	BUG_ON(!snd_stm_magic_valid(player));

	spin_lock(&player->default_settings_lock);
	ucontrol->value.integer.value[0] =
		player->default_settings.iec61937_audio_repetition;
	spin_unlock(&player->default_settings_lock);

	return 0;
}

static int snd_stm_uniperif_player_ctl_audio_repetition_put(struct snd_kcontrol
		*kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	struct snd_stm_uniperif_player *player = snd_kcontrol_chip(kcontrol);
	int changed = 0;

	snd_stm_printd(1, "%s(kcontrol=0x%p, ucontrol=0x%p)\n",
		       __func__, kcontrol, ucontrol);

	BUG_ON(!player);
	BUG_ON(!snd_stm_magic_valid(player));

	spin_lock(&player->default_settings_lock);
	if (player->default_settings.iec61937_audio_repetition !=
			ucontrol->value.integer.value[0]) {
		changed = 1;
		player->default_settings.iec61937_audio_repetition =
				ucontrol->value.integer.value[0];
	}
	spin_unlock(&player->default_settings_lock);

	return changed;
}

static int snd_stm_uniperif_player_ctl_pause_repetition_get(struct snd_kcontrol
		*kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	struct snd_stm_uniperif_player *player = snd_kcontrol_chip(kcontrol);

	snd_stm_printd(1, "%s(kcontrol=0x%p, ucontrol=0x%p)\n",
		       __func__, kcontrol, ucontrol);

	BUG_ON(!player);
	BUG_ON(!snd_stm_magic_valid(player));

	spin_lock(&player->default_settings_lock);
	ucontrol->value.integer.value[0] =
		player->default_settings.iec61937_pause_repetition;
	spin_unlock(&player->default_settings_lock);

	return 0;
}

static int snd_stm_uniperif_player_ctl_pause_repetition_put(struct snd_kcontrol
		*kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	struct snd_stm_uniperif_player *player = snd_kcontrol_chip(kcontrol);
	int changed = 0;

	snd_stm_printd(1, "%s(kcontrol=0x%p, ucontrol=0x%p)\n",
		       __func__, kcontrol, ucontrol);

	BUG_ON(!player);
	BUG_ON(!snd_stm_magic_valid(player));

	spin_lock(&player->default_settings_lock);
	if (player->default_settings.iec61937_pause_repetition !=
			ucontrol->value.integer.value[0]) {
		changed = 1;
		player->default_settings.iec61937_pause_repetition =
				ucontrol->value.integer.value[0];
	}
	spin_unlock(&player->default_settings_lock);

	return changed;
}

static struct snd_kcontrol_new snd_stm_uniperif_player_ctls[] = {
	{
		.iface = SNDRV_CTL_ELEM_IFACE_PCM,
		.name = SNDRV_CTL_NAME_IEC958("", PLAYBACK, DEFAULT),
		.info = snd_stm_ctl_iec958_info,
		.get = snd_stm_uniperif_player_ctl_iec958_get,
		.put = snd_stm_uniperif_player_ctl_iec958_put,
	}, {
		.access = SNDRV_CTL_ELEM_ACCESS_READ,
		.iface = SNDRV_CTL_ELEM_IFACE_MIXER,
		.name =	SNDRV_CTL_NAME_IEC958("", PLAYBACK, CON_MASK),
		.info =	snd_stm_ctl_iec958_info,
		.get = snd_stm_ctl_iec958_mask_get_con,
	}, {
		.access = SNDRV_CTL_ELEM_ACCESS_READ,
		.iface = SNDRV_CTL_ELEM_IFACE_MIXER,
		.name =	SNDRV_CTL_NAME_IEC958("", PLAYBACK, PRO_MASK),
		.info =	snd_stm_ctl_iec958_info,
		.get = snd_stm_ctl_iec958_mask_get_pro,
	}, {
		.iface = SNDRV_CTL_ELEM_IFACE_PCM,
		.name = SNDRV_CTL_NAME_IEC958("Raw Data ", PLAYBACK, DEFAULT),
		.info = snd_stm_ctl_boolean_info,
		.get = snd_stm_uniperif_player_ctl_raw_get,
		.put = snd_stm_uniperif_player_ctl_raw_put,
	}, {
		.iface = SNDRV_CTL_ELEM_IFACE_PCM,
		.name = SNDRV_CTL_NAME_IEC958("Encoded Data ",
				PLAYBACK, DEFAULT),
		.info = snd_stm_ctl_boolean_info,
		.get = snd_stm_uniperif_player_ctl_encoded_get,
		.put = snd_stm_uniperif_player_ctl_encoded_put,
	}, {
		.iface = SNDRV_CTL_ELEM_IFACE_PCM,
		.name = SNDRV_CTL_NAME_IEC958("Preamble ", PLAYBACK, DEFAULT),
		.info = snd_stm_uniperif_player_ctl_preamble_info,
		.get = snd_stm_uniperif_player_ctl_preamble_get,
		.put = snd_stm_uniperif_player_ctl_preamble_put,
	}, {
		.iface = SNDRV_CTL_ELEM_IFACE_PCM,
		.name = SNDRV_CTL_NAME_IEC958("Audio Burst Period ",
				PLAYBACK, DEFAULT),
		.info = snd_stm_uniperif_player_ctl_repetition_info,
		.get = snd_stm_uniperif_player_ctl_audio_repetition_get,
		.put = snd_stm_uniperif_player_ctl_audio_repetition_put,
	}, {
		.iface = SNDRV_CTL_ELEM_IFACE_PCM,
		.name = SNDRV_CTL_NAME_IEC958("Pause Burst Period ",
				PLAYBACK, DEFAULT),
		.info = snd_stm_uniperif_player_ctl_repetition_info,
		.get = snd_stm_uniperif_player_ctl_pause_repetition_get,
		.put = snd_stm_uniperif_player_ctl_pause_repetition_put,
	}
};



/*
 * ALSA lowlevel device implementation
 */

#define DUMP_REGISTER(r) \
		snd_iprintf(buffer, "AUD_UNIPERIF_%s (offset 0x%02x) =" \
				" 0x%08x\n", __stringify(r), \
				offset__AUD_UNIPERIF_##r(player), \
				get__AUD_UNIPERIF_##r(player))

static void snd_stm_uniperif_player_dump_registers(struct snd_info_entry *entry,
		struct snd_info_buffer *buffer)
{
	struct snd_stm_uniperif_player *player = entry->private_data;

	BUG_ON(!player);
	BUG_ON(!snd_stm_magic_valid(player));

	snd_iprintf(buffer, "--- %s ---\n", dev_name(player->device));
	snd_iprintf(buffer, "base = 0x%p\n", player->base);

	DUMP_REGISTER(SOFT_RST);
	/*DUMP_REGISTER(FIFO_DATA);*/ /* Register is write-only */
	DUMP_REGISTER(STA);
	DUMP_REGISTER(ITS);
	/*DUMP_REGISTER(ITS_BCLR);*/  /* Register is write-only */
	/*DUMP_REGISTER(ITS_BSET);*/  /* Register is write-only */
	DUMP_REGISTER(ITM);
	/*DUMP_REGISTER(ITM_BCLR);*/  /* Register is write-only */
	/*DUMP_REGISTER(ITM_BSET);*/  /* Register is write-only */
	DUMP_REGISTER(SPDIF_PA_PB);
	DUMP_REGISTER(SPDIF_PC_PD);
	DUMP_REGISTER(SPDIF_PAUSE_LAT);
	DUMP_REGISTER(SPDIF_FRAMELEN_BURST);
	DUMP_REGISTER(CONFIG);
	DUMP_REGISTER(CTRL);
	DUMP_REGISTER(I2S_FMT);
	DUMP_REGISTER(STATUS_1);
	DUMP_REGISTER(CHANNEL_STA_REG0);
	DUMP_REGISTER(CHANNEL_STA_REG1);
	DUMP_REGISTER(CHANNEL_STA_REG2);
	DUMP_REGISTER(CHANNEL_STA_REG3);
	DUMP_REGISTER(CHANNEL_STA_REG4);
	DUMP_REGISTER(CHANNEL_STA_REG5);
	DUMP_REGISTER(USER_VALIDITY);
	DUMP_REGISTER(DFV0);
	DUMP_REGISTER(CONTROLABILITY);
	DUMP_REGISTER(CRC_CTRL);
	DUMP_REGISTER(CRC_WINDOW);
	DUMP_REGISTER(CRC_VALUE_IN);
	DUMP_REGISTER(CRC_VALUE_OUT);

	snd_iprintf(buffer, "\n");
}

static int snd_stm_uniperif_player_register(struct snd_device *snd_device)
{
	int result = 0;
	struct snd_stm_uniperif_player *player = snd_device->device_data;

	snd_stm_printd(1, "%s(snd_device=0x%p)\n", __func__, snd_device);

	BUG_ON(!player);
	BUG_ON(!snd_stm_magic_valid(player));

	/* TODO: well, hardcoded - shall anyone use it?
	 * And what it actually means? */
       /* In uniperif doc use of backstalling is avoided*/
	set__AUD_UNIPERIF_CONFIG__BACK_STALL_REQ_DISABLE(player);
	set__AUD_UNIPERIF_CTRL__ROUNDING_OFF(player);
	set__AUD_UNIPERIF_CTRL__SPDIF_LAT_OFF(player);
	set__AUD_UNIPERIF_CONFIG__IDLE_MOD_DISABLE(player);

	/* Get frequency synthesizer channel */

	player->clock = snd_stm_clk_get(player->device, "uni_player_clk",
			snd_device->card, player->info->card_device);
	if (!player->clock || IS_ERR(player->clock)) {
		snd_stm_printe("Failed to get a clock for '%s'!\n",
			dev_name(player->device));
		return -EINVAL;
	}

	/* Registers view in ALSA's procfs */

	result = snd_stm_info_register(&player->proc_entry,
			dev_name(player->device),
			snd_stm_uniperif_player_dump_registers, player);
	if (result < 0) {
		snd_stm_printe("Failed to register with procfs\n");
		return result;
	}

	/* Register the ALSA controls (unless we are of type PCM) */

	if (player->info->player_type != SND_STM_UNIPERIF_PLAYER_TYPE_PCM) {
		int i;

		snd_stm_printd(1, "Adding ALSA controls\n");

		for (i = 0; i < ARRAY_SIZE(snd_stm_uniperif_player_ctls); i++) {
			snd_stm_uniperif_player_ctls[i].device =
				player->info->card_device;
			result = snd_ctl_add(snd_device->card,
				snd_ctl_new1(&snd_stm_uniperif_player_ctls[i],
					     player));
			if (result < 0) {
				snd_stm_printe(
					"Failed to add ALSA control!\n");
				return result;
			}
			snd_stm_uniperif_player_ctls[i].index++;
		}
	}

	return result;
}

static int snd_stm_uniperif_player_disconnect(struct snd_device *snd_device)
{
	struct snd_stm_uniperif_player *player = snd_device->device_data;

	snd_stm_printd(1, "%s(snd_device=0x%p)\n", __func__, snd_device);

	BUG_ON(!player);
	BUG_ON(!snd_stm_magic_valid(player));

	snd_stm_clk_put(player->clock);

	snd_stm_info_unregister(player->proc_entry);

	return 0;
}

static struct snd_device_ops snd_stm_uniperif_player_snd_device_ops = {
	.dev_register = snd_stm_uniperif_player_register,
	.dev_disconnect = snd_stm_uniperif_player_disconnect,
};



/*
 * Platform driver routines
 */

static int snd_stm_uniperif_player_probe(struct platform_device *pdev)
{
	int result = 0;
	struct snd_stm_uniperif_player *player;
	struct snd_card *card = snd_stm_card_get();
	static unsigned int channels_2_10[] = { 2, 4, 6, 8, 10 };
	unsigned int i;

	snd_stm_printd(0, "%s('%s')\n", __func__, dev_name(&pdev->dev));

	BUG_ON(!card);

	player = kzalloc(sizeof(*player), GFP_KERNEL);
	if (!player) {
		snd_stm_printe("Can't allocate memory "
				"for a device description!\n");
		result = -ENOMEM;
		goto error_alloc;
	}
	snd_stm_magic_set(player);
	player->info = pdev->dev.platform_data;
	BUG_ON(!player->info);
	player->ver = player->info->ver;
	BUG_ON(player->ver <= 0);
	player->device = &pdev->dev;

	spin_lock_init(&player->default_settings_lock);

	/* Set player specific options */

	snd_stm_printd(0, "Uniperipheral player '%s'\n", player->info->name);

	switch (player->info->player_type) {
	case SND_STM_UNIPERIF_PLAYER_TYPE_HDMI:
		snd_stm_printd(0, "Player type is hdmi\n");
		player->hardware = snd_stm_uniperif_player_pcm_hw;
		player->stream_settings.input_mode =
				SNDRV_STM_UNIPERIF_SPDIF_INPUT_MODE_NORMAL;
		break;
	case SND_STM_UNIPERIF_PLAYER_TYPE_PCM:
		snd_stm_printd(0, "Player type is pcm\n");
		player->hardware = snd_stm_uniperif_player_pcm_hw;
		break;
	case SND_STM_UNIPERIF_PLAYER_TYPE_SPDIF:
		snd_stm_printd(0, "Player type is spdif\n");
		/* Default to normal mode where hardware does everything */
		player->hardware = snd_stm_uniperif_player_pcm_hw;
		player->stream_settings.input_mode =
				SNDRV_STM_UNIPERIF_SPDIF_INPUT_MODE_NORMAL;
		break;
	default:
		snd_stm_printe("Unknown player type\n");
		goto error_player_type;
	}

	snd_stm_printd(0, "Parking is %s\n",
			player->info->parking_enabled ? "enabled" : "disabled");

	/* Set default iec958 status bits (I expect user to override!) */

	/* Consumer, PCM, copyright, 2ch, mode 0 */
	player->default_settings.iec958.status[0] = 0x00;
	/* Broadcast reception category */
	player->default_settings.iec958.status[1] = 0x04;
	/* Do not take into account source or channel number */
	player->default_settings.iec958.status[2] = 0x00;
	/* Sampling frequency not indicated */
	player->default_settings.iec958.status[3] = 0x01;
	/* Max sample word 24-bit, sample word length not indicated */
	player->default_settings.iec958.status[4] = 0x01;

	/* Get resources */

	result = snd_stm_memory_request(pdev, &player->mem_region,
			&player->base);
	if (result < 0) {
		snd_stm_printe("Memory region request failed!\n");
		goto error_memory_request;
	}
	player->fifo_phys_address = player->mem_region->start +
		offset__AUD_UNIPERIF_FIFO_DATA(player);
	snd_stm_printd(0, "FIFO physical address: 0x%lx.\n",
			player->fifo_phys_address);

	result = snd_stm_irq_request(pdev, &player->irq,
			snd_stm_uniperif_player_irq_handler, player);
	if (result < 0) {
		snd_stm_printe("IRQ request failed!\n");
		goto error_irq_request;
	}

	player->dma_max_transfer_size = 40;

	/* Get player capabilities */

	BUG_ON(player->info->channels <= 0);
	BUG_ON(player->info->channels > 10);
	BUG_ON(player->info->channels % 2 != 0);

	player->channels_constraint.list = channels_2_10;
	player->channels_constraint.count = player->info->channels / 2;
	player->channels_constraint.mask = 0;

	for (i = 0; i < player->channels_constraint.count; i++)
		snd_stm_printd(0, "Player capable of playing %u-channels PCM\n",
				player->channels_constraint.list[i]);

	/* Create ALSA lowlevel device */

	result = snd_device_new(card, SNDRV_DEV_LOWLEVEL, player,
			&snd_stm_uniperif_player_snd_device_ops);
	if (result < 0) {
		snd_stm_printe("ALSA low level device creation failed!\n");
		goto error_device;
	}

	/* Create ALSA PCM device */

	result = snd_pcm_new(card, NULL, player->info->card_device, 1, 0,
			&player->pcm);
	if (result < 0) {
		snd_stm_printe("ALSA PCM instance creation failed!\n");
		goto error_pcm;
	}
	player->pcm->private_data = player;
	strcpy(player->pcm->name, player->info->name);

	snd_pcm_set_ops(player->pcm, SNDRV_PCM_STREAM_PLAYBACK,
			&snd_stm_uniperif_player_pcm_ops);

	/* Initialize buffer */

	player->buffer = snd_stm_buffer_create(player->pcm,
			player->device,
			player->hardware.buffer_bytes_max);
	if (!player->buffer) {
		snd_stm_printe("Cannot initialize buffer!\n");
		result = -ENOMEM;
		goto error_buffer_init;
	}

	/* Register in converters router */

	player->conv_source = snd_stm_conv_register_source(
			&platform_bus_type, dev_name(&pdev->dev),
			player->info->channels,
			card, player->info->card_device);
	if (!player->conv_source) {
		snd_stm_printe("Cannot register in converters router!\n");
		result = -ENOMEM;
		goto error_conv_register_source;
	}

	/* Claim the pads */

	if (player->info->pad_config) {
		player->pads = stm_pad_claim(player->info->pad_config,
				dev_name(&pdev->dev));
		if (!player->pads) {
			snd_stm_printe("Failed to claimed pads for '%s'!\n",
					dev_name(&pdev->dev));
			result = -EBUSY;
			goto error_pad_claim;
		}
	}

	/* Done now */

	platform_set_drvdata(pdev, player);

	pm_runtime_enable(&pdev->dev);

	return 0;
error_pad_claim:
	snd_stm_conv_unregister_source(player->conv_source);
error_conv_register_source:
	snd_stm_buffer_dispose(player->buffer);
error_buffer_init:
	/* snd_pcm_free() is not available - PCM device will be released
	 * during card release */
error_pcm:
	snd_device_free(card, player);
error_device:
	snd_stm_irq_release(player->irq, player);
error_irq_request:
	snd_stm_memory_release(player->mem_region, player->base);
error_player_type:
error_memory_request:
	snd_stm_magic_clear(player);
	kfree(player);
error_alloc:
	return result;
}

static int snd_stm_uniperif_player_remove(struct platform_device *pdev)
{
	struct snd_stm_uniperif_player *player = platform_get_drvdata(pdev);

	snd_stm_printd(1, "%s(pdev=%p)\n", __func__, pdev);

	BUG_ON(!player);
	BUG_ON(!snd_stm_magic_valid(player));

	pm_runtime_disable(&pdev->dev);

	if (player->dma_channel) {
		dmaengine_terminate_all(player->dma_channel);
		dma_release_channel(player->dma_channel);
		player->dma_channel = NULL;
	}

	if (player->pads)
		stm_pad_release(player->pads);

	snd_stm_conv_unregister_source(player->conv_source);
	snd_stm_buffer_dispose(player->buffer);
	snd_stm_irq_release(player->irq, player);
	snd_stm_memory_release(player->mem_region, player->base);

	snd_stm_magic_clear(player);
	kfree(player);

	return 0;
}


/*
 * Power management
 */

#ifdef CONFIG_PM
static int snd_stm_uniperif_player_suspend(struct device *dev)
{
	struct snd_stm_uniperif_player *player = dev_get_drvdata(dev);
	struct snd_card *card = snd_stm_card_get();

	snd_stm_printd(1, "%s(dev=%p)\n", __func__, dev);

	/* Check if this device is already suspended */
	if (dev->power.runtime_status == RPM_SUSPENDED)
		return 0;

	/* Halt the player if parking currently active */
	if (dma_audio_is_parking_active(player->dma_channel)) {
		/* Halt the player */
		snd_stm_uniperif_player_halt(player->substream);

		/* Release the dma channel */
		dma_release_channel(player->dma_channel);
		player->dma_channel = NULL;

		/* Release any converter */
		if (player->conv_group) {
			snd_stm_conv_release_group(player->conv_group);
			player->conv_group = NULL;
		}
	}

	/* Abort if the player is still running */
	if (get__AUD_UNIPERIF_CTRL__OPERATION(player)) {
		snd_stm_printe("Cannot runtime suspend as '%s' running!\n",
				dev_name(dev));
		return -EBUSY;
	}

	/* Indicate power off (with power) and suspend all streams */
	snd_power_change_state(card, SNDRV_CTL_POWER_D3hot);
	snd_pcm_suspend_all(player->pcm);

	return 0;
}

static int snd_stm_uniperif_player_resume(struct device *dev)
{
	struct snd_card *card = snd_stm_card_get();

	snd_stm_printd(1, "%s(dev=%p)\n", __func__, dev);

	/* Check if this device is already active */
	if (dev->power.runtime_status == RPM_ACTIVE)
		return 0;

	snd_power_change_state(card, SNDRV_CTL_POWER_D0);

	return 0;
}

static const struct dev_pm_ops snd_stm_uniperif_player_pm_ops = {
	.suspend = snd_stm_uniperif_player_suspend,
	.freeze	 = snd_stm_uniperif_player_suspend,
	.resume	 = snd_stm_uniperif_player_resume,
	.thaw	 = snd_stm_uniperif_player_resume,
};
#endif


/*
 * Module initialization
 */

static struct platform_driver snd_stm_uniperif_player_driver = {
	.driver.name	= "snd_uni_player",
#ifdef CONFIG_PM
	.driver.pm	= &snd_stm_uniperif_player_pm_ops,
#endif
	.probe		= snd_stm_uniperif_player_probe,
	.remove		= snd_stm_uniperif_player_remove,
};

static int __init snd_stm_uniperif_player_init(void)
{
	return platform_driver_register(&snd_stm_uniperif_player_driver);
}

static void snd_stm_uniperif_player_exit(void)
{
	platform_driver_unregister(&snd_stm_uniperif_player_driver);
}

MODULE_AUTHOR("John Boddie <john.boddie@st.com>");
MODULE_DESCRIPTION("STMicroelectronics uniperipheral player driver");
MODULE_LICENSE("GPL");

module_init(snd_stm_uniperif_player_init);
module_exit(snd_stm_uniperif_player_exit);
