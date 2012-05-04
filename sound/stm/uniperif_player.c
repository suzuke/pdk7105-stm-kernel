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
#include <linux/stm/stm-dma.h>
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

#define PARKING_NODE_INIT	0
#define PARKING_NODE_LOOP	1
#define PARKING_NODE_EXIT	2
#define PARKING_NODE_COUNT	3

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
	int fdma_max_transfer_size;
	struct stm_dma_params *fdma_params;
	struct stm_dma_req *fdma_request;

	struct stm_pad_state *pads;

	struct snd_pcm_hardware hardware;

	/* Specific to spdif player */
	struct snd_stm_uniperif_spdif_settings default_settings;
	spinlock_t default_settings_lock; /* Protects default_settings */
	struct snd_stm_uniperif_spdif_settings stream_settings;

	int stream_iec958_pa_pb_sync_lost;
	int stream_iec958_status_cnt;
	int stream_iec958_subcode_cnt;

	/* Player state */
	enum {
		SND_STM_UNIPERIF_PLAYER_STATE_IDLE,
		SND_STM_UNIPERIF_PLAYER_STATE_RUNNING,
		SND_STM_UNIPERIF_PLAYER_STATE_XRUN,
		SND_STM_UNIPERIF_PLAYER_STATE_PARKINIT,
		SND_STM_UNIPERIF_PLAYER_STATE_PARKLOOP,
		SND_STM_UNIPERIF_PLAYER_STATE_PARKEXIT,
		SND_STM_UNIPERIF_PLAYER_STATE_STOP
	} state;

	/* Parking buffer */
	struct semaphore parking_active;
	struct snd_stm_buffer *parking_buffer;
	unsigned char *parking_dma_area;
	dma_addr_t parking_dma_addr;
	int parking_dma_bytes;
	struct stm_dma_params *parking_fdma_params;
	int parking_loop_node;
	int parking_exit_node;

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
				SNDRV_PCM_INFO_PAUSE),
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
				SNDRV_PCM_INFO_PAUSE),
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
		player->state = SND_STM_UNIPERIF_PLAYER_STATE_XRUN;
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
		player->state = SND_STM_UNIPERIF_PLAYER_STATE_XRUN;
		snd_pcm_stop(player->substream, SNDRV_PCM_STATE_XRUN);

		result = IRQ_HANDLED;
	}

	/* Some alien interrupt??? */
	BUG_ON(result != IRQ_HANDLED);

	return result;
}

static int snd_stm_uniperif_player_open(struct snd_pcm_substream *substream)
{
	int result;
	struct snd_stm_uniperif_player *player =
			snd_pcm_substream_chip(substream);
	struct snd_pcm_runtime *runtime = substream->runtime;

	snd_stm_printd(1, "%s(substream=0x%p)\n", __func__, substream);

	BUG_ON(!player);
	BUG_ON(!snd_stm_magic_valid(player));
	BUG_ON(!runtime);

	snd_pcm_set_sync(substream);  /* TODO: ??? */

	/* Get attached converters handle (only if idle) */

	if (player->state == SND_STM_UNIPERIF_PLAYER_STATE_IDLE) {
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
		return result;
	}

	/* It is better when buffer size is an integer multiple of period
	 * size... Such thing will ensure this :-O */
	result = snd_pcm_hw_constraint_integer(runtime,
			SNDRV_PCM_HW_PARAM_PERIODS);
	if (result < 0) {
		snd_stm_printe("Can't set periods constraint!\n");
		return result;
	}

	/* Make the period (so buffer as well) length (in bytes) a multiply
	 * of a FDMA transfer bytes (which varies depending on channels
	 * number and sample bytes) */
	result = snd_stm_pcm_hw_constraint_transfer_bytes(runtime,
			player->fdma_max_transfer_size * 4);
	if (result < 0) {
		snd_stm_printe("Can't set buffer bytes constraint!\n");
		return result;
	}

	runtime->hw = player->hardware;

	/* Interrupt handler will need the substream pointer... */
	player->substream = substream;

	return 0;
}

static int snd_stm_uniperif_player_close(struct snd_pcm_substream *substream)
{
	struct snd_stm_uniperif_player *player =
			snd_pcm_substream_chip(substream);

	snd_stm_printd(1, "%s(substream=0x%p)\n", __func__, substream);

	BUG_ON(!player);
	BUG_ON(!snd_stm_magic_valid(player));

	/* Don't do anything unless player is idle */
	if (player->state == SND_STM_UNIPERIF_PLAYER_STATE_IDLE) {
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
		long jiffies = HZ;

		switch (player->state) {
		case SND_STM_UNIPERIF_PLAYER_STATE_PARKINIT:
		case SND_STM_UNIPERIF_PLAYER_STATE_PARKLOOP:
		case SND_STM_UNIPERIF_PLAYER_STATE_PARKEXIT:
			/* Wait only 1 second for parking to start */
			if (down_timeout(&player->parking_active, jiffies)) {
				/* Parking failed to start */
				snd_stm_uniperif_player_stop(substream);
				snd_stm_uniperif_player_hw_free(substream);
			} else {
				/* Free playback buffer */
				snd_stm_buffer_free(player->buffer);

				/* Free FDMA parameters */
				dma_params_free(player->fdma_params);
				kfree(player->fdma_params);
			}
			break;

		case SND_STM_UNIPERIF_PLAYER_STATE_XRUN:
			/* Goto stop state and recurse to free */
			snd_stm_uniperif_player_stop(substream);
			snd_stm_uniperif_player_hw_free(substream);
			break;

		case SND_STM_UNIPERIF_PLAYER_STATE_IDLE:
		case SND_STM_UNIPERIF_PLAYER_STATE_STOP:
			/* Let the FDMA stop */
			dma_wait_for_completion(player->fdma_channel);

			/* Free playback buffer */
			snd_stm_buffer_free(player->buffer);

			/* Free FDMA parameters & configuration */
			dma_params_free(player->fdma_params);
			kfree(player->fdma_params);

			if (snd_stm_buffer_is_allocated(
				player->parking_buffer)) {
				/* Restore parking buffer DMA data */
				runtime->dma_area = player->parking_dma_area;
				runtime->dma_addr = player->parking_dma_addr;
				runtime->dma_bytes = player->parking_dma_bytes;

				/* Free parking buffer */
				snd_stm_buffer_free(player->parking_buffer);

				/* Free FDMA parameters */
				dma_params_free(player->parking_fdma_params);
				kfree(player->parking_fdma_params);

				/* Clear the parking loop/exit node pointers */
				player->parking_loop_node = 0;
				player->parking_exit_node = 0;
			}

			/* Free FDMA request */
			dma_req_free(player->fdma_channel,
					player->fdma_request);

			/* Player is now idle */
			player->state = SND_STM_UNIPERIF_PLAYER_STATE_IDLE;
			break;

		default:
			snd_stm_printe("Player '%s' unexpected state %d!\n",
					dev_name(player->device),
					player->state);
		}
	}

	return 0;
}

static void snd_stm_uniperif_player_update_parking_node_pointers(
		struct snd_stm_uniperif_player *player)
{
	struct stm_dma_node_addr node_addr;

	BUG_ON(!player);
	BUG_ON(!snd_stm_magic_valid(player));

	/* Try to determine the parking loop/exit node pointers */
	dma_get_node(player->fdma_channel, &node_addr, NULL);

	if (node_addr.curr_node == node_addr.next_node) {
		if (player->parking_loop_node == 0)
			player->parking_loop_node = node_addr.curr_node;
	} else {
		if (player->parking_exit_node == 0)
			if (node_addr.curr_node == player->parking_loop_node)
				player->parking_exit_node = node_addr.next_node;
	}
}

static void snd_stm_uniperif_player_parking_comp_cb(unsigned long param)
{
	struct snd_stm_uniperif_player *player =
			(struct snd_stm_uniperif_player *) param;

	BUG_ON(!player);
	BUG_ON(!snd_stm_magic_valid(player));

	/* Attempt to update parking loop/exit node pointers */
	snd_stm_uniperif_player_update_parking_node_pointers(player);

	switch (player->state) {
	case SND_STM_UNIPERIF_PLAYER_STATE_PARKLOOP:
		/* Only get this if park at end of a period (not sub-period) */
		break;

	case SND_STM_UNIPERIF_PLAYER_STATE_PARKEXIT:
		dma_configure_params(player->fdma_channel,
				     player->fdma_params);
		player->state = SND_STM_UNIPERIF_PLAYER_STATE_RUNNING;
		break;

	case SND_STM_UNIPERIF_PLAYER_STATE_STOP:
		/* Stop parking (e.g. change hw params, xrun) */
		break;

	default:
		snd_stm_printe("Player '%s' unexpected state %d!\n",
				dev_name(player->device), player->state);
	}
}

static int snd_stm_uniperif_player_parking_hw_params(
		struct snd_pcm_substream *substream,
		struct snd_pcm_hw_params *hw_params)
{
	struct snd_stm_uniperif_player *player =
			snd_pcm_substream_chip(substream);
	struct snd_pcm_runtime *runtime = substream->runtime;
	int frame_bytes;
	unsigned char *dma_area;
	dma_addr_t dma_addr;
	int dma_bytes;
	int result;
	int i;

	snd_stm_printd(1, "%s(substream=0x%p, hw_params=0x%p)\n",
		       __func__, substream, hw_params);

	BUG_ON(!player);
	BUG_ON(!snd_stm_magic_valid(player));
	BUG_ON(!runtime);

	BUG_ON(player->state != SND_STM_UNIPERIF_PLAYER_STATE_IDLE);

	/* Calculate the number of bytes in a frame */
	frame_bytes = snd_pcm_format_physical_width(params_format(hw_params)) *
			params_channels(hw_params) / 8;

	/* Ensure minimal parking buffer size is optimal for FDMA */
	frame_bytes *= PARKING_NBFRAMES;
	frame_bytes  = (frame_bytes < PARKING_BUFFER_SIZE) ?
				PARKING_BUFFER_SIZE : frame_bytes;

	/* Save playback buffer DMA data (snd_stm_buffer_alloc overwrites it) */
	dma_area = runtime->dma_area;
	dma_addr = runtime->dma_addr;
	dma_bytes = runtime->dma_bytes;

	/* Allocate parking buffer */
	result = snd_stm_buffer_alloc(player->parking_buffer, substream,
			frame_bytes);

	/* Save parking buffer DMA data */
	player->parking_dma_area = runtime->dma_area;
	player->parking_dma_addr = runtime->dma_addr;
	player->parking_dma_bytes = runtime->dma_bytes;

	/* Restore playback buffer DMA data */
	runtime->dma_area = dma_area;
	runtime->dma_addr = dma_addr;
	runtime->dma_bytes = dma_bytes;

	/* Check result of parking buffer allocation */
	if (result != 0) {
		snd_stm_printe("Player '%s' can't allocate %d bytes buffer!\n",
			       dev_name(player->device), frame_bytes);
		result = -ENOMEM;
		goto error_buf_alloc;
	}

	/* Clear the parking buffer */
	memset(player->parking_dma_area, 0x00, player->parking_dma_bytes);

	/* Allocate the FDMA parking nodes */
	player->parking_fdma_params = kmalloc(
		sizeof(*player->parking_fdma_params) * PARKING_NODE_COUNT,
		GFP_KERNEL);

	if (!player->parking_fdma_params) {
		snd_stm_printe("Player '%s' can't allocate %d bytes for FDMA "
				"parking parameters list!\n",
				dev_name(player->device),
				sizeof(*player->parking_fdma_params));
		result = -ENOMEM;
		goto error_params_alloc;
	}

	for (i = 0; i < PARKING_NODE_COUNT; ++i) {
		/* Configure the FDMA parking nodes */
		dma_params_init(&player->parking_fdma_params[i],
				MODE_PACED, STM_DMA_LIST_CIRC);

		dma_params_DIM_1_x_0(&player->parking_fdma_params[i]);

		dma_params_req(&player->parking_fdma_params[i],
			player->fdma_request);

		dma_params_addrs(&player->parking_fdma_params[i],
			player->parking_dma_addr,
			player->fifo_phys_address,
			player->parking_dma_bytes);

		dma_params_comp_cb(&player->parking_fdma_params[i],
			snd_stm_uniperif_player_parking_comp_cb,
			(unsigned long) player, STM_DMA_CB_CONTEXT_ISR);

		/* Get callback every time a node is completed */
		if (i != PARKING_NODE_LOOP)
			dma_params_interrupts(&player->parking_fdma_params[i],
						STM_DMA_NODE_COMP_INT);

		/* Compile the FDMA parking node */
		result = dma_compile_list(player->fdma_channel,
				&player->parking_fdma_params[i],
				GFP_KERNEL);

		if (result < 0) {
			snd_stm_printe("Player '%s' can't compile FDMA parking "
				       "parameters node %d!\n",
				       dev_name(player->device), i);
			goto error_compile_list;
		}
	}

	return 0;

error_compile_list:
	kfree(player->parking_fdma_params);
error_params_alloc:
	snd_stm_buffer_free(player->parking_buffer);
error_buf_alloc:
	return result;
}

static void snd_stm_uniperif_player_comp_cb(unsigned long param)
{
	struct snd_stm_uniperif_player *player =
			(struct snd_stm_uniperif_player *)param;

	BUG_ON(!player);
	BUG_ON(!snd_stm_magic_valid(player));

	/* Try to determine the parking loop/exit node pointers */
	if (player->info->parking_enabled)
		snd_stm_uniperif_player_update_parking_node_pointers(player);

	switch (player->state) {
	case SND_STM_UNIPERIF_PLAYER_STATE_RUNNING:
		/* Calculate period elapsed for playback */
		BUG_ON(!player->substream);
		snd_stm_printd(2, "Period elapsed ('%s')\n",
		       dev_name(player->device));

		snd_pcm_period_elapsed(player->substream);
		break;


	case SND_STM_UNIPERIF_PLAYER_STATE_PARKINIT:
		/* Set parking loop params */
		dma_configure_params(player->fdma_channel,
				     &player->parking_fdma_params[1]);

		up(&player->parking_active);
		player->state = SND_STM_UNIPERIF_PLAYER_STATE_PARKLOOP;
		break;

	case SND_STM_UNIPERIF_PLAYER_STATE_STOP:
		/* Calculate period elapsed for playback */
		BUG_ON(!player->substream);
		snd_stm_printd(2, "Period elapsed ('%s')\n",
		       dev_name(player->device));

		snd_pcm_period_elapsed(player->substream);
		break;

	default:
		snd_stm_printe("Player '%s' unexpected state %d!\n",
				dev_name(player->device), player->state);
	}
}

static int snd_stm_uniperif_player_hw_params(
		struct snd_pcm_substream *substream,
		struct snd_pcm_hw_params *hw_params)
{
	int result;
	struct snd_stm_uniperif_player *player =
			snd_pcm_substream_chip(substream);
	struct snd_pcm_runtime *runtime = substream->runtime;
	int buffer_bytes, frame_bytes, transfer_bytes, period_bytes, periods;
	unsigned int transfer_size;
	struct stm_dma_req_config fdma_req_config = {
		.rw        = REQ_CONFIG_WRITE,
		.opcode    = REQ_CONFIG_OPCODE_4,
		.increment = 0,
		.hold_off  = 0,
		.initiator = player->info->fdma_initiator,
	};
	int i;

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
			player->fdma_max_transfer_size * 4);
	transfer_size = transfer_bytes / 4;

	snd_stm_printd(1, "FDMA request trigger limit and transfer size set "
			"to %d.\n", transfer_size);

	BUG_ON(buffer_bytes % transfer_bytes != 0);
	BUG_ON(transfer_size > player->fdma_max_transfer_size);
	fdma_req_config.count = transfer_size;

	BUG_ON(transfer_size != 1 && transfer_size % 2 != 0);
	BUG_ON(transfer_size >
			mask__AUD_UNIPERIF_CONFIG__FDMA_TRIGGER_LIMIT(player));

	set__AUD_UNIPERIF_CONFIG__FDMA_TRIGGER_LIMIT(player, transfer_size);

	/* Configure FDMA transfer */
	if (player->current_fmda_cnt !=  fdma_req_config.count) {
		player->current_fmda_cnt = fdma_req_config.count;
		player->fdma_request = dma_req_config(player->fdma_channel,
					player->info->fdma_request_line,
					&fdma_req_config);
		if (!player->fdma_request) {
			snd_stm_printe("Player '%s' can't configure FDMA pacing"
				       " channel!\n", dev_name(player->device));
			result = -EINVAL;
			goto error_req_config;
		}
	}

	/* Allocate a FDMA playback nodes (one per sub-block per period) */
	player->fdma_params = kmalloc(
		(sizeof(*player->fdma_params) * periods * PARKING_SUBBLOCKS),
		GFP_KERNEL);

	if (!player->fdma_params) {
		snd_stm_printe("Can't allocate %d bytes for FDMA parameters "
				"list!\n", sizeof(*player->fdma_params) *
				periods * PARKING_SUBBLOCKS);
		result = -ENOMEM;
		goto error_params_alloc;
	}

	for (i = 0; i < (periods * PARKING_SUBBLOCKS); i++) {
		int sub_period_bytes = period_bytes / PARKING_SUBBLOCKS;

		dma_params_init(&player->fdma_params[i], MODE_PACED,
				STM_DMA_LIST_CIRC);

		dma_params_DIM_1_x_0(&player->fdma_params[i]);

		dma_params_req(&player->fdma_params[i], player->fdma_request);

		dma_params_addrs(&player->fdma_params[i],
				 runtime->dma_addr + i * sub_period_bytes,
				 player->fifo_phys_address,
				 sub_period_bytes);

		if (i > 0)
			dma_params_link(&player->fdma_params[i - 1],
					&player->fdma_params[i]);

		dma_params_comp_cb(&player->fdma_params[i],
				   snd_stm_uniperif_player_comp_cb,
				   (unsigned long) player,
				   STM_DMA_CB_CONTEXT_ISR);

		/* Get callback every time a node is completed */
		if ((i % PARKING_SUBBLOCKS) == (PARKING_SUBBLOCKS-1))
			dma_params_interrupts(&player->fdma_params[i],
					      STM_DMA_NODE_COMP_INT);
	}

	result = dma_compile_list(player->fdma_channel,
				player->fdma_params, GFP_KERNEL);
	if (result < 0) {
		snd_stm_printe("Can't compile FDMA parameters for player"
				" '%s'!\n", dev_name(player->device));
		goto error_compile_list;
	}

	if (player->info->parking_enabled) {
		/* Setup parking parameters only when idle */
		if (player->state == SND_STM_UNIPERIF_PLAYER_STATE_IDLE) {

			result = snd_stm_uniperif_player_parking_hw_params(
					substream, hw_params);
			if (result < 0) {
				snd_stm_printe("Player '%s' failed to setup "
						"parking hw params!\n",
						dev_name(player->device));
				goto error_compile_list;
			}
		}
	}

	return 0;

error_compile_list:
	kfree(player->fdma_params);
error_params_alloc:
	dma_req_free(player->fdma_channel, player->fdma_request);
error_req_config:
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

	snd_stm_printd(1, "%s(substream=0x%p)\n", __func__, substream);

	BUG_ON(!player);
	BUG_ON(!snd_stm_magic_valid(player));
	BUG_ON(!runtime);
	BUG_ON(runtime->period_size * runtime->channels >=
			MAX_SAMPLES_PER_PERIOD);

	switch (player->state) {
	case SND_STM_UNIPERIF_PLAYER_STATE_IDLE:
	case SND_STM_UNIPERIF_PLAYER_STATE_XRUN:
		break;

	case SND_STM_UNIPERIF_PLAYER_STATE_PARKINIT:
		/* Part way through parking - attempt to stop */
		snd_stm_uniperif_player_stop(substream);
		break;

	case SND_STM_UNIPERIF_PLAYER_STATE_PARKLOOP:
	case SND_STM_UNIPERIF_PLAYER_STATE_PARKEXIT:
		changed  = (player->current_rate != runtime->rate);
		changed |= (player->current_format != runtime->format);
		changed |= (player->current_channels != runtime->channels);

		/* Return if new configuration is the same as current */
		if (changed == 0)
			return 0;

		/* Stop the player so we can set new configuration */
		snd_stm_uniperif_player_stop(substream);
		break;

	case SND_STM_UNIPERIF_PLAYER_STATE_STOP:
		break;

	default:
		snd_stm_printe("Player '%s' unexpected state %d!\n",
				dev_name(player->device), player->state);
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
	unsigned int ctrl;
	int result;
	struct snd_stm_uniperif_player *player =
			snd_pcm_substream_chip(substream);

	snd_stm_printd(1, "%s(substream=0x%p)\n", __func__, substream);

	BUG_ON(!player);
	BUG_ON(!snd_stm_magic_valid(player));

	switch (player->state) {
	case SND_STM_UNIPERIF_PLAYER_STATE_RUNNING:
		snd_stm_uniperif_player_stop(substream);
		break;

	case SND_STM_UNIPERIF_PLAYER_STATE_PARKINIT:
		snd_stm_uniperif_player_stop(substream);
		break;

	case SND_STM_UNIPERIF_PLAYER_STATE_PARKLOOP:
		/* Link parking exit node to playback node */
		dma_next_node(player->fdma_channel,
			&player->parking_fdma_params[PARKING_NODE_EXIT],
			player->fdma_params);

		/* Link parking loop node to parking exit node */
		dma_next_node(player->fdma_channel,
			&player->parking_fdma_params[PARKING_NODE_LOOP],
			&player->parking_fdma_params[PARKING_NODE_EXIT]);

		/* Indicate we are exiting parking */
		player->state = SND_STM_UNIPERIF_PLAYER_STATE_PARKEXIT;

		/* FDMA will switch to parking exit node the playback node */
		return 0;

	case SND_STM_UNIPERIF_PLAYER_STATE_PARKEXIT:
		snd_stm_uniperif_player_stop(substream);
		break;

	default:
		break;
	}

	/* Normal start from idle or stop. Reset uniperipheral player */

	set__AUD_UNIPERIF_SOFT_RST__SOFT_RST(player);
	while (get__AUD_UNIPERIF_SOFT_RST__SOFT_RST(player))
		udelay(5);

	/* Launch FDMA transfer */

	result = dma_xfer_list(player->fdma_channel, player->fdma_params);
	if (result != 0) {
		snd_stm_printe("Can't launch FDMA transfer for player '%s'!\n",
				dev_name(player->device));
		clk_disable(player->clock);
		return -EINVAL;
	}
	while (dma_get_status(player->fdma_channel) !=
			DMA_CHANNEL_STATUS_RUNNING)
		udelay(5);

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

	/* Indicate we are now in the running state */
	player->state = SND_STM_UNIPERIF_PLAYER_STATE_RUNNING;

	return 0;
}

static int snd_stm_uniperif_player_park(struct snd_pcm_substream *substream)
{
	struct snd_stm_uniperif_player *player =
			snd_pcm_substream_chip(substream);
	struct snd_pcm_runtime *runtime = substream->runtime;

	snd_stm_printd(1, "%s(substream=0x%p)\n", __func__, substream);

	BUG_ON(!player);
	BUG_ON(!snd_stm_magic_valid(player));
	BUG_ON(!runtime);

	BUG_ON(player->state != SND_STM_UNIPERIF_PLAYER_STATE_RUNNING);

	/* Ensure lock is not free */
	while (!down_trylock(&player->parking_active))
		;

	/* Indicate switch to parking buffer */
	player->state = SND_STM_UNIPERIF_PLAYER_STATE_PARKINIT;

	/* Link parking entry node to parking loop node */
	dma_next_node(player->fdma_channel,
			&player->parking_fdma_params[PARKING_NODE_INIT],
			&player->parking_fdma_params[PARKING_NODE_LOOP]);

	/* Link parking loop node to itself (loops to itself whilst parked) */
	dma_next_node(player->fdma_channel,
			&player->parking_fdma_params[PARKING_NODE_LOOP],
			&player->parking_fdma_params[PARKING_NODE_LOOP]);

	/* Link playback node to parking entry node (transitions to parked) */
	dma_next_node(player->fdma_channel, player->fdma_params,
			&player->parking_fdma_params[PARKING_NODE_INIT]);

	/* Set channel status for linear pcm */
	player->stream_settings.iec958.status[0] &= ~0x02;
	player->stream_settings.iec958.status[4] = 0x0b;
	snd_stm_uniperif_player_set_channel_status(player, runtime);

	/* Clear validity bits as linear pcm */
	set__AUD_UNIPERIF_USER_VALIDITY__VALIDITY_LEFT(player, 0);
	set__AUD_UNIPERIF_USER_VALIDITY__VALIDITY_RIGHT(player, 0);

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
	clk_disable(player->clock);
	set__AUD_UNIPERIF_CTRL__OPERATION_OFF(player);

	/* Stop FDMA transfer */
	dma_stop_channel(player->fdma_channel);

	/* Reset current configuration */
	player->current_rate = 0;
	player->current_format = 0;
	player->current_channels = 0;

	/* Player should be in the stop state */
	player->state = SND_STM_UNIPERIF_PLAYER_STATE_STOP;

	return 0;
}

static int snd_stm_uniperif_player_stop(struct snd_pcm_substream *substream)
{
	struct snd_stm_uniperif_player *player =
			snd_pcm_substream_chip(substream);

	snd_stm_printd(1, "%s(substream=0x%p)\n", __func__, substream);

	BUG_ON(!player);
	BUG_ON(!snd_stm_magic_valid(player));

	switch (player->state) {
	case SND_STM_UNIPERIF_PLAYER_STATE_RUNNING:
		if (player->info->parking_enabled == 0)
			snd_stm_uniperif_player_halt(substream);
		else
			snd_stm_uniperif_player_park(substream);
		break;

	case SND_STM_UNIPERIF_PLAYER_STATE_XRUN:
	case SND_STM_UNIPERIF_PLAYER_STATE_PARKINIT:
	case SND_STM_UNIPERIF_PLAYER_STATE_PARKLOOP:
	case SND_STM_UNIPERIF_PLAYER_STATE_PARKEXIT:
		snd_stm_uniperif_player_halt(substream);
		break;

	case SND_STM_UNIPERIF_PLAYER_STATE_STOP:
		snd_stm_uniperif_player_halt(substream);
		break;

	default:
		snd_stm_printe("Player '%s' unexpected state %d!\n",
			       dev_name(player->device), player->state);
	}

	return 0;
}

static int snd_stm_uniperif_player_pause(struct snd_pcm_substream *substream)
{
	struct snd_stm_uniperif_player *player =
			snd_pcm_substream_chip(substream);
	unsigned int ctrl;

	snd_stm_printd(1, "%s(substream=0x%p)\n", __func__, substream);

	BUG_ON(!player);
	BUG_ON(!snd_stm_magic_valid(player));

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
	case SNDRV_PCM_TRIGGER_STOP:
		return snd_stm_uniperif_player_stop(substream);
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
	struct stm_dma_node_addr node_addr;

	snd_stm_printd(2, "%s(substream=0x%p)\n", __func__, substream);

	BUG_ON(!player);
	BUG_ON(!snd_stm_magic_valid(player));
	BUG_ON(!runtime);

	switch (player->state) {
	case SND_STM_UNIPERIF_PLAYER_STATE_RUNNING:
		dma_get_node(player->fdma_channel, &node_addr, NULL);

		if ((node_addr.curr_node == player->parking_loop_node) ||
		    (node_addr.curr_node == player->parking_exit_node))
			residue = runtime->dma_bytes;
		else
			residue = get_dma_residue(player->fdma_channel);
		break;

	case SND_STM_UNIPERIF_PLAYER_STATE_PARKLOOP:
	case SND_STM_UNIPERIF_PLAYER_STATE_PARKEXIT:
		/* Number of bytes remaining is always DMA size for parking */
		residue = runtime->dma_bytes;
		break;

	default:
		snd_stm_printe("Player '%s' unexpected state %d!\n",
				dev_name(player->device), player->state);
		return 0;
	}

	hwptr = (runtime->dma_bytes - residue) % runtime->dma_bytes;
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
	if (changed & get__AUD_UNIPERIF_CTRL__OPERATION(player))
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
	if (changed && get__AUD_UNIPERIF_CTRL__OPERATION(player)) {
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

	BUG_ON(!player->info->clock_name);
	snd_stm_printd(0, "Player connected to clock '%s'.\n",
			player->info->clock_name);
	player->clock = snd_stm_clk_get(player->device,
			player->info->clock_name, snd_device->card,
			player->info->card_device);

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
	sema_init(&player->parking_active, 0);

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

	result = snd_stm_fdma_request_by_name(pdev, &player->fdma_channel,
			player->info->fdma_name);
	if (result < 0) {
		snd_stm_printe("FDMA request failed!\n");
		goto error_fdma_request;
	}

	player->fdma_max_transfer_size = 40;

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

	/* Initialize parking buffer */

	player->parking_buffer = snd_stm_buffer_create(player->pcm,
			player->device,
			PARKING_BUFFER_SIZE);
	if (!player->parking_buffer) {
		snd_stm_printe("Player '%s' can't create parking buffer!\n",
				dev_name(player->device));
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
	snd_stm_fdma_release(player->fdma_channel);
error_fdma_request:
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

	if (player->pads)
		stm_pad_release(player->pads);

	snd_stm_conv_unregister_source(player->conv_source);
	snd_stm_buffer_dispose(player->buffer);
	snd_stm_fdma_release(player->fdma_channel);
	snd_stm_irq_release(player->irq, player);
	snd_stm_memory_release(player->mem_region, player->base);

	snd_stm_magic_clear(player);
	kfree(player);

	return 0;
}

static struct platform_driver snd_stm_uniperif_player_driver = {
	.driver.name = "snd_uniperif_player",
	.probe = snd_stm_uniperif_player_probe,
	.remove = snd_stm_uniperif_player_remove,
};



/*
 * Initialization
 */

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
