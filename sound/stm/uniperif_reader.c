/*
 *   STMicroelectronics System-on-Chips' Uniperipheral reader driver
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
#include <linux/delay.h>
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

#define DEFAULT_FORMAT (SND_STM_FORMAT__I2S | SND_STM_FORMAT__SUBFRAME_32_BITS)

/* The sample count field (NSAMPLES in CTRL register) is 19 bits wide */
#define MAX_SAMPLES_PER_PERIOD ((1 << 20) - 1)


/*
 * Uniperipheral reader instance definition
 */

struct snd_stm_uniperif_reader {
	/* System informations */
	struct snd_stm_pcm_reader_info *info;
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


	snd_stm_magic_field;
};

static struct snd_pcm_hardware snd_stm_uniperif_reader_hw = {
	.info		= (SNDRV_PCM_INFO_MMAP |
				SNDRV_PCM_INFO_MMAP_VALID |
				SNDRV_PCM_INFO_INTERLEAVED |
				SNDRV_PCM_INFO_BLOCK_TRANSFER),
	.formats	= (SNDRV_PCM_FMTBIT_S32_LE),

	.rates		= SNDRV_PCM_RATE_CONTINUOUS,
	.rate_min	= 32000,
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



/*
 * Uniperipheral reader implementation
 */

static irqreturn_t snd_stm_uniperif_reader_irq_handler(int irq, void *dev_id)
{
	irqreturn_t result = IRQ_NONE;
	struct snd_stm_uniperif_reader *reader = dev_id;
	unsigned int status;

	snd_stm_printd(2, "%s(irq=%d, dev_id=0x%p)\n", __func__, irq, dev_id);

	BUG_ON(!reader);
	BUG_ON(!snd_stm_magic_valid(reader));

	/* Get interrupt status & clear them immediately */
	preempt_disable();
	status = get__AUD_UNIPERIF_ITS(reader);
	set__AUD_UNIPERIF_ITS_BCLR(reader, status);
	preempt_enable();

	/* Overflow? */
	if (unlikely(status & mask__AUD_UNIPERIF_ITS__FIFO_ERROR(reader))) {
		snd_stm_printe("Underflow detected in reader '%s'!\n",
				dev_name(reader->device));

		snd_pcm_stop(reader->substream, SNDRV_PCM_STATE_XRUN);

		result = IRQ_HANDLED;
	}

	/* Some alien interrupt??? */
	BUG_ON(result != IRQ_HANDLED);

	return result;
}

static void snd_stm_uniperif_reader_callback_node_done(unsigned long param)
{
	struct snd_stm_uniperif_reader *reader =
			(struct snd_stm_uniperif_reader *)param;

	snd_stm_printd(2, "%s(param=0x%lx)\n", __func__, param);

	BUG_ON(!reader);
	BUG_ON(!snd_stm_magic_valid(reader));

	if (!get__AUD_UNIPERIF_CTRL__OPERATION(reader))
		return;

	snd_stm_printd(2, "Period elapsed ('%s')\n",
			dev_name(reader->device));

	snd_pcm_period_elapsed(reader->substream);
}

static void snd_stm_uniperif_reader_callback_node_error(unsigned long param)
{
	struct snd_stm_uniperif_reader *reader =
			(struct snd_stm_uniperif_reader *)param;

	snd_stm_printd(2, "%s(param=0x%lx)\n", __func__, param);

	BUG_ON(!reader);
	BUG_ON(!snd_stm_magic_valid(reader));

	if (!get__AUD_UNIPERIF_CTRL__OPERATION(reader))
		return;

	snd_stm_printe("Error during FDMA transfer in reader '%s'!\n",
		       dev_name(reader->device));

	snd_pcm_stop(reader->substream, SNDRV_PCM_STATE_XRUN);
}

static int snd_stm_uniperif_reader_open(struct snd_pcm_substream *substream)
{
	int result;
	struct snd_stm_uniperif_reader *reader =
			snd_pcm_substream_chip(substream);
	struct snd_pcm_runtime *runtime = substream->runtime;

	snd_stm_printd(1, "%s(substream=0x%p)\n", __func__, substream);

	BUG_ON(!reader);
	BUG_ON(!snd_stm_magic_valid(reader));
	BUG_ON(!runtime);

	snd_pcm_set_sync(substream);  /* TODO: ??? */

	/* Get attached converters handle */

	reader->conv_group =
			snd_stm_conv_request_group(reader->conv_source);
	if (reader->conv_group)
		snd_stm_printd(1, "'%s' is attached to '%s' converter(s)...\n",
				dev_name(reader->device),
				snd_stm_conv_get_name(reader->conv_group));
	else
		snd_stm_printd(1, "Warning! No converter attached to '%s'!\n",
				dev_name(reader->device));

	/* Get default data */

	/* Set up constraints & pass hardware capabilities info to ALSA */
	{
		static unsigned int channels_2_10[] = { 2, 4, 6, 8, 10 };
		unsigned int i;

		/* need to make hardware constrain for reader */
		BUG_ON(reader->info->channels <= 0);
		BUG_ON(reader->info->channels > 10);
		BUG_ON(reader->info->channels % 2 != 0);

		reader->channels_constraint.list = channels_2_10;
		reader->channels_constraint.count =
			reader->info->channels / 2;

		reader->channels_constraint.mask = 0;
		for (i = 0; i < reader->channels_constraint.count; i++)
			snd_stm_printd(0,
				"Reader capable of capturing %u-channels PCM\n",
				reader->channels_constraint.list[i]);

		result = snd_pcm_hw_constraint_list(runtime, 0,
			SNDRV_PCM_HW_PARAM_CHANNELS,
			&reader->channels_constraint);
		if (result < 0) {
			snd_stm_printe("Can't set channels constraint!\n");
			return result;
		}
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
			reader->fdma_max_transfer_size * 4);
	if (result < 0) {
		snd_stm_printe("Can't set buffer bytes constraint!\n");
		return result;
	}

	runtime->hw = snd_stm_uniperif_reader_hw;

	/* Interrupt handler will need the substream pointer... */
	reader->substream = substream;

	return 0;
}

static int snd_stm_uniperif_reader_close(struct snd_pcm_substream *substream)
{
	struct snd_stm_uniperif_reader *reader =
			snd_pcm_substream_chip(substream);

	snd_stm_printd(1, "%s(substream=0x%p)\n", __func__, substream);

	BUG_ON(!reader);
	BUG_ON(!snd_stm_magic_valid(reader));

	if (reader->conv_group) {
		snd_stm_conv_release_group(reader->conv_group);
		reader->conv_group = NULL;
	}

	reader->substream = NULL;

	return 0;
}

static int snd_stm_uniperif_reader_hw_free(struct snd_pcm_substream *substream)
{
	struct snd_stm_uniperif_reader *reader =
			snd_pcm_substream_chip(substream);
	struct snd_pcm_runtime *runtime = substream->runtime;

	snd_stm_printd(1, "%s(substream=0x%p)\n", __func__, substream);

	BUG_ON(!reader);
	BUG_ON(!snd_stm_magic_valid(reader));
	BUG_ON(!runtime);


	/* This callback may be called more than once... */

	if (snd_stm_buffer_is_allocated(reader->buffer)) {
		/* Let the FDMA stop */
		dma_wait_for_completion(reader->fdma_channel);

		/* Free buffer */
		snd_stm_buffer_free(reader->buffer);

		/* Free FDMA parameters & configuration */
		dma_params_free(reader->fdma_params);
		dma_req_free(reader->fdma_channel, reader->fdma_request);
		kfree(reader->fdma_params);
	}

	return 0;
}

static int snd_stm_uniperif_reader_hw_params(
		struct snd_pcm_substream *substream,
		struct snd_pcm_hw_params *hw_params)
{
	int result;
	struct snd_stm_uniperif_reader *reader =
			snd_pcm_substream_chip(substream);
	struct snd_pcm_runtime *runtime = substream->runtime;
	int buffer_bytes, period_bytes, periods, frame_bytes, transfer_bytes;
	unsigned int transfer_size;
	struct stm_dma_req_config fdma_req_config = {
		.rw        = REQ_CONFIG_READ,
		.opcode    = REQ_CONFIG_OPCODE_4,
		.increment = 0,
		.hold_off  = 0,
		.initiator = reader->info->fdma_initiator,
	};
	int i;

	snd_stm_printd(1, "%s(substream=0x%p, hw_params=0x%p)\n",
		       __func__, substream, hw_params);

	BUG_ON(!reader);
	BUG_ON(!snd_stm_magic_valid(reader));
	BUG_ON(!runtime);

	/* This function may be called many times, so let's be prepared... */
	if (snd_stm_buffer_is_allocated(reader->buffer))
		snd_stm_uniperif_reader_hw_free(substream);

	/* Get the numbers... */

	buffer_bytes = params_buffer_bytes(hw_params);
	periods = params_periods(hw_params);
	period_bytes = buffer_bytes / periods;
	BUG_ON(periods * period_bytes != buffer_bytes);

	/* Allocate buffer */

	result = snd_stm_buffer_alloc(reader->buffer, substream,
			buffer_bytes);
	if (result != 0) {
		snd_stm_printe("Can't allocate %d bytes buffer for '%s'!\n",
			       buffer_bytes, dev_name(reader->device));
		result = -ENOMEM;
		goto error_buf_alloc;
	}

	/* Set FDMA transfer size (number of opcodes generated
	 * after request line assertion) */

	frame_bytes = snd_pcm_format_physical_width(params_format(hw_params)) *
			params_channels(hw_params) / 8;
	transfer_bytes = snd_stm_pcm_transfer_bytes(frame_bytes,
			reader->fdma_max_transfer_size * 4);
	transfer_size = transfer_bytes / 4;

	snd_stm_printd(1, "FDMA request trigger limit set to %d.\n",
			transfer_size);
	BUG_ON(buffer_bytes % transfer_bytes != 0);
	BUG_ON(transfer_size > reader->fdma_max_transfer_size);
	BUG_ON(transfer_size != 1 && transfer_size % 2 == 0);
	BUG_ON(transfer_size > mask__AUD_UNIPERIF_CONFIG__FDMA_TRIGGER_LIMIT(
			reader));
	set__AUD_UNIPERIF_CONFIG__FDMA_TRIGGER_LIMIT(
		reader, transfer_size);
		fdma_req_config.count = transfer_size;
	snd_stm_printd(1, "FDMA transfer size set to %d.\n",
			fdma_req_config.count);

	/* Configure FDMA transfer */

	reader->fdma_request = dma_req_config(reader->fdma_channel,
			reader->info->fdma_request_line, &fdma_req_config);
	if (!reader->fdma_request) {
		snd_stm_printe("Can't configure FDMA pacing channel for reader"
			       " '%s'!\n", dev_name(reader->device));
		result = -EINVAL;
		goto error_req_config;
	}

	reader->fdma_params =
			kmalloc(sizeof(*reader->fdma_params) *
			periods, GFP_KERNEL);
	if (!reader->fdma_params) {
		snd_stm_printe("Can't allocate %d bytes for FDMA parameters "
				"list!\n", sizeof(*reader->fdma_params)
				* periods);
		result = -ENOMEM;
		goto error_params_alloc;
	}

	snd_stm_printd(1, "Configuring FDMA transfer nodes:\n");

	for (i = 0; i < periods; i++) {
		dma_params_init(&reader->fdma_params[i], MODE_PACED,
				STM_DMA_LIST_CIRC);

		if (i > 0)
			dma_params_link(&reader->fdma_params[i - 1],
					(&reader->fdma_params[i]));

		dma_params_comp_cb(&reader->fdma_params[i],
				snd_stm_uniperif_reader_callback_node_done,
				(unsigned long)reader,
				STM_DMA_CB_CONTEXT_ISR);

		dma_params_err_cb(&reader->fdma_params[i],
				snd_stm_uniperif_reader_callback_node_error,
				(unsigned long)reader,
				STM_DMA_CB_CONTEXT_ISR);

		/* Get callback every time a node is completed */
		dma_params_interrupts(&reader->fdma_params[i],
				STM_DMA_NODE_COMP_INT);

		dma_params_DIM_0_x_1(&reader->fdma_params[i]);

		dma_params_req(&reader->fdma_params[i],
				reader->fdma_request);

		snd_stm_printd(1, "- %d: %d bytes from 0x%08x\n", i,
				period_bytes,
				runtime->dma_addr + i * period_bytes);

		dma_params_addrs(&reader->fdma_params[i],
				reader->fifo_phys_address,
				runtime->dma_addr + i * period_bytes,
				period_bytes);
	}

	result = dma_compile_list(reader->fdma_channel,
				reader->fdma_params, GFP_KERNEL);
	if (result < 0) {
		snd_stm_printe("Can't compile FDMA parameters for"
			" reader '%s'!\n", dev_name(reader->device));
		goto error_compile_list;
	}

	return 0;

error_compile_list:
	kfree(reader->fdma_params);
error_params_alloc:
	dma_req_free(reader->fdma_channel, reader->fdma_request);
error_req_config:
	snd_stm_buffer_free(reader->buffer);
error_buf_alloc:
	return result;
}

static int snd_stm_uniperif_reader_prepare(struct snd_pcm_substream *substream)
{
	struct snd_stm_uniperif_reader *reader =
			snd_pcm_substream_chip(substream);
	struct snd_pcm_runtime *runtime = substream->runtime;
	unsigned int format, lr_pol;

	snd_stm_printd(1, "%s(substream=0x%p)\n", __func__, substream);

	BUG_ON(!reader);
	BUG_ON(!snd_stm_magic_valid(reader));
	BUG_ON(!runtime);
	BUG_ON(runtime->period_size * runtime->channels >=
			MAX_SAMPLES_PER_PERIOD);

	/* Get format value from connected converter */
	if (reader->conv_group)
		format = snd_stm_conv_get_format(reader->conv_group);
	else
		format = DEFAULT_FORMAT;

	/* Number of bits per subframe (which is one channel sample)
	 * on input. */

	switch (format & SND_STM_FORMAT__SUBFRAME_MASK) {
	case SND_STM_FORMAT__SUBFRAME_32_BITS:
		snd_stm_printd(1, "- 32 bits per subframe\n");
		set__AUD_UNIPERIF_I2S_FMT__NBIT_32(reader);
		set__AUD_UNIPERIF_I2S_FMT__DATA_SIZE_24(reader);
		break;
	case SND_STM_FORMAT__SUBFRAME_16_BITS:
		snd_stm_printd(1, "- 16 bits per subframe\n");
		set__AUD_UNIPERIF_I2S_FMT__NBIT_16(reader);
		set__AUD_UNIPERIF_I2S_FMT__DATA_SIZE_16(reader);
		break;
	default:
		snd_BUG();
		return -EINVAL;
	}

	switch (format & SND_STM_FORMAT__MASK) {
	case SND_STM_FORMAT__I2S:
		snd_stm_printd(1, "- I2S\n");
		set__AUD_UNIPERIF_I2S_FMT__ALIGN_LEFT(reader);
		set__AUD_UNIPERIF_I2S_FMT__PADDING_I2S_MODE(reader);
		lr_pol = value__AUD_UNIPERIF_I2S_FMT__LR_POL_LOW(reader);
		break;
	case SND_STM_FORMAT__LEFT_JUSTIFIED:
		snd_stm_printd(1, "- left justified\n");
		set__AUD_UNIPERIF_I2S_FMT__ALIGN_LEFT(reader);
		set__AUD_UNIPERIF_I2S_FMT__PADDING_SONY_MODE(reader);
		lr_pol = value__AUD_UNIPERIF_I2S_FMT__LR_POL_HIG(reader);
		break;
	case SND_STM_FORMAT__RIGHT_JUSTIFIED:
		snd_stm_printd(1, "- right justified\n");
		set__AUD_UNIPERIF_I2S_FMT__ALIGN_RIGHT(reader);
		set__AUD_UNIPERIF_I2S_FMT__PADDING_SONY_MODE(reader);
		lr_pol = value__AUD_UNIPERIF_I2S_FMT__LR_POL_HIG(reader);
		break;
	default:
		snd_BUG();
		return -EINVAL;
	}

	/* Configure data memory format */

	switch (runtime->format) {
	case SNDRV_PCM_FORMAT_S32_LE:
		/* Actually "16 bits/0 bits" means "32/28/24/20/18/16 bits
		 * on the left than zeros (if less than 32 bites)"... ;-) */
		set__AUD_UNIPERIF_CONFIG__MEM_FMT_16_0(reader);

		/* In x/0 bits memory mode there is no problem with
		 * L/R polarity */
		if (lr_pol)
			set__AUD_UNIPERIF_I2S_FMT__LR_POL_HIG(reader);
		else
			set__AUD_UNIPERIF_I2S_FMT__LR_POL_LOW(reader);
		/* One word of data is one sample, so period size
		 * times channels */
		set__AUD_UNIPERIF_I2S_FMT__NO_OF_SAMPLES_TO_READ(reader,
				runtime->period_size * runtime->channels);
		break;

	default:
		snd_BUG();
		return -EINVAL;
	}

	set__AUD_UNIPERIF_CONFIG__MSTR_CLKEDGE_RISING(reader);
	set__AUD_UNIPERIF_CTRL__READER_OUT_SEL_IN_MEM(reader);

	/* Serial audio interface format - for detailed explanation
	 * see ie.:
	 * http: www.cirrus.com/en/pubs/appNote/AN282REV1.pdf */

	set__AUD_UNIPERIF_I2S_FMT__ORDER_MSB(reader);

	/* Value FALLING of SCLK_EDGE bit in AUD_PCMOUT_FMT register that
	 * actually means "data clocking (changing) on the falling edge"
	 * (and we usually want this...) - STx7100 and cuts < 3.0 of
	 * STx7109 have this bit inverted comparing to what their
	 * datasheets claim... (specs say 1) */
	set__AUD_UNIPERIF_I2S_FMT__SCLK_EDGE_RISING(reader);

	/* Number of channels... */

	BUG_ON(runtime->channels % 2 != 0);
	BUG_ON(runtime->channels < 2);
	BUG_ON(runtime->channels > 10);

	set__AUD_UNIPERIF_I2S_FMT__NUM_CH(reader, runtime->channels / 2);

	/*one bit audio format. For HDMI To Be Check */
	set__AUD_UNIPERIF_CONFIG__ONE_BIT_AUD_DISABLE(reader);

	/*for pcmp1 DAC output */
	set__AUD_UNIPERIF_CTRL__SPDIF_FMT_OFF(reader);

	return 0;
}

static int snd_stm_uniperif_reader_start(struct snd_pcm_substream *substream)
{
	int result;
	struct snd_stm_uniperif_reader *reader =
			snd_pcm_substream_chip(substream);

	snd_stm_printd(1, "%s(substream=0x%p)\n", __func__, substream);

	BUG_ON(!reader);
	BUG_ON(!snd_stm_magic_valid(reader));


	/* reset pcm reader */

	set__AUD_UNIPERIF_SOFT_RST__SOFT_RST(reader);
	while (get__AUD_UNIPERIF_SOFT_RST__SOFT_RST(reader))
		udelay(5);

	/* Launch FDMA transfer */

	result = dma_xfer_list(reader->fdma_channel, reader->fdma_params);
	if (result != 0) {
		snd_stm_printe("Can't launch FDMA transfer for reader '%s'!\n",
				dev_name(reader->device));
		return -EINVAL;
	}
	while (dma_get_status(reader->fdma_channel) !=
			DMA_CHANNEL_STATUS_RUNNING)
		udelay(5);

	/* Enable reader interrupts (and clear possible stalled ones) */

	enable_irq(reader->irq);
	set__AUD_UNIPERIF_ITS_BCLR__FIFO_ERROR(reader);
	set__AUD_UNIPERIF_ITM_BSET__FIFO_ERROR(reader);

	/* Launch the reader */

	set__AUD_UNIPERIF_CTRL__OPERATION_PCM_DATA(reader);

	/* Wake up & unmute converter */

	if (reader->conv_group) {
		snd_stm_conv_enable(reader->conv_group,
				0, substream->runtime->channels - 1);
		snd_stm_conv_unmute(reader->conv_group);
	}

	return 0;
}

static int snd_stm_uniperif_reader_stop(struct snd_pcm_substream *substream)
{
	struct snd_stm_uniperif_reader *reader =
			snd_pcm_substream_chip(substream);

	snd_stm_printd(1, "%s(substream=0x%p)\n", __func__, substream);

	BUG_ON(!reader);
	BUG_ON(!snd_stm_magic_valid(reader));

	/* Mute & shutdown converter */

	if (reader->conv_group) {
		snd_stm_conv_mute(reader->conv_group);
		snd_stm_conv_disable(reader->conv_group);
	}

	/* Disable interrupts */

	set__AUD_UNIPERIF_ITM_BCLR__FIFO_ERROR(reader);
	disable_irq_nosync(reader->irq);

	/* Stop FDMA transfer */

	dma_stop_channel(reader->fdma_channel);

	/* Stop pcm reader */

	set__AUD_UNIPERIF_CTRL__OPERATION_OFF(reader);

	return 0;
}

static int snd_stm_uniperif_reader_trigger(struct snd_pcm_substream *substream,
		int command)
{
	snd_stm_printd(1, "%s(substream=0x%p, command=%d)\n",
		       __func__, substream, command);

	switch (command) {
	case SNDRV_PCM_TRIGGER_START:
		return snd_stm_uniperif_reader_start(substream);
	case SNDRV_PCM_TRIGGER_STOP:
		return snd_stm_uniperif_reader_stop(substream);
	default:
		return -EINVAL;
	}
}

static snd_pcm_uframes_t snd_stm_uniperif_reader_pointer(
		struct snd_pcm_substream *substream)
{
	struct snd_stm_uniperif_reader *reader =
		snd_pcm_substream_chip(substream);
	struct snd_pcm_runtime *runtime = substream->runtime;
	int residue, hwptr;
	snd_pcm_uframes_t pointer;

	snd_stm_printd(2, "%s(substream=0x%p)\n", __func__, substream);

	BUG_ON(!reader);
	BUG_ON(!snd_stm_magic_valid(reader));
	BUG_ON(!runtime);

	residue = get_dma_residue(reader->fdma_channel);
	hwptr = (runtime->dma_bytes - residue) % runtime->dma_bytes;
	pointer = bytes_to_frames(runtime, hwptr);


	snd_stm_printd(2, "FDMA residue value is %i and buffer size is %u"
			" bytes...\n", residue, runtime->dma_bytes);
	snd_stm_printd(2, "... so HW pointer in frames is %lu (0x%lx)!\n",
			pointer, pointer);

	return pointer;
}

static struct snd_pcm_ops snd_stm_uniperif_reader_pcm_ops = {
	.open =      snd_stm_uniperif_reader_open,
	.close =     snd_stm_uniperif_reader_close,
	.mmap =      snd_stm_buffer_mmap,
	.ioctl =     snd_pcm_lib_ioctl,
	.hw_params = snd_stm_uniperif_reader_hw_params,
	.hw_free =   snd_stm_uniperif_reader_hw_free,
	.prepare =   snd_stm_uniperif_reader_prepare,
	.trigger =   snd_stm_uniperif_reader_trigger,
	.pointer =   snd_stm_uniperif_reader_pointer,
};


/*
 * ALSA lowlevel device implementation
 */

#define DUMP_REGISTER(r) \
		snd_iprintf(buffer, "AUD_UNIPERIF_%s (offset 0x%02x) =" \
				" 0x%08x\n", __stringify(r), \
				offset__AUD_UNIPERIF_##r(reader), \
				get__AUD_UNIPERIF_##r(reader))

static void snd_stm_uniperif_reader_dump_registers(struct snd_info_entry *entry,
		struct snd_info_buffer *buffer)
{
	struct snd_stm_uniperif_reader *reader = entry->private_data;

	BUG_ON(!reader);
	BUG_ON(!snd_stm_magic_valid(reader));

	snd_iprintf(buffer, "--- %s ---\n", dev_name(reader->device));
	snd_iprintf(buffer, "base = 0x%p\n", reader->base);

	DUMP_REGISTER(SOFT_RST);
	/*DUMP_REGISTER(FIFO_DATA);*/
	DUMP_REGISTER(STA);
	DUMP_REGISTER(ITS);
	/*DUMP_REGISTER(ITS_BCLR);*/
	/*DUMP_REGISTER(ITS_BSET);*/
	DUMP_REGISTER(ITM);
	/*DUMP_REGISTER(ITM_BCLR);*/
	/*DUMP_REGISTER(ITM_BSET);*/
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

static int snd_stm_uniperif_reader_register(struct snd_device *snd_device)
{
	int result = 0;
	struct snd_stm_uniperif_reader *reader = snd_device->device_data;

	snd_stm_printd(1, "%s(snd_device=0x%p)\n", __func__, snd_device);

	BUG_ON(!reader);
	BUG_ON(!snd_stm_magic_valid(reader));

	/* TODO: well, hardcoded - shall anyone use it?
	 * And what it actually means? */
       /* In uniperif doc use of backstalling is avoided*/
	set__AUD_UNIPERIF_CONFIG__BACK_STALL_REQ_DISABLE(reader);
	set__AUD_UNIPERIF_CTRL__ROUNDING_OFF(reader);
	set__AUD_UNIPERIF_CTRL__SPDIF_LAT_OFF(reader);
	set__AUD_UNIPERIF_CONFIG__IDLE_MOD_DISABLE(reader);

	/* Registers view in ALSA's procfs */

	result = snd_stm_info_register(&reader->proc_entry,
			dev_name(reader->device),
			snd_stm_uniperif_reader_dump_registers, reader);

	return result;
}

static int snd_stm_uniperif_reader_disconnect(struct snd_device *snd_device)
{
	struct snd_stm_uniperif_reader *reader = snd_device->device_data;

	snd_stm_printd(1, "%s(snd_device=0x%p)\n", __func__, snd_device);

	BUG_ON(!reader);
	BUG_ON(!snd_stm_magic_valid(reader));

	snd_stm_info_unregister(reader->proc_entry);

	return 0;
}

static struct snd_device_ops snd_stm_uniperif_reader_snd_device_ops = {
	.dev_register = snd_stm_uniperif_reader_register,
	.dev_disconnect = snd_stm_uniperif_reader_disconnect,
};


/*
 * Platform driver routines
 */

static int snd_stm_uniperif_reader_probe(struct platform_device *pdev)
{
	int result = 0;
	struct snd_stm_uniperif_reader *reader;
	struct snd_card *card = snd_stm_card_get();

	snd_stm_printd(0, "%s('%s')\n", __func__, dev_name(&pdev->dev));

	BUG_ON(!card);

	reader = kzalloc(sizeof(*reader), GFP_KERNEL);
	if (!reader) {
		snd_stm_printe("Can't allocate memory "
				"for a device description!\n");
		result = -ENOMEM;
		goto error_alloc;
	}
	snd_stm_magic_set(reader);
	reader->info = pdev->dev.platform_data;
	BUG_ON(!reader->info);
	reader->ver = reader->info->ver;
	BUG_ON(reader->ver <= 0);
	reader->device = &pdev->dev;

	/* Get resources */

	result = snd_stm_memory_request(pdev, &reader->mem_region,
			&reader->base);
	if (result < 0) {
		snd_stm_printe("Memory region request failed!\n");
		goto error_memory_request;
	}
	reader->fifo_phys_address = reader->mem_region->start +
		offset__AUD_UNIPERIF_FIFO_DATA(reader);
	snd_stm_printd(0, "FIFO physical address: 0x%lx.\n",
			reader->fifo_phys_address);

	result = snd_stm_irq_request(pdev, &reader->irq,
			snd_stm_uniperif_reader_irq_handler, reader);
	if (result < 0) {
		snd_stm_printe("IRQ request failed!\n");
		goto error_irq_request;
	}

	result = snd_stm_fdma_request_by_name(pdev, &reader->fdma_channel,
			reader->info->fdma_name);
	if (result < 0) {
		snd_stm_printe("FDMA request failed!\n");
		goto error_fdma_request;
	}

	reader->fdma_max_transfer_size = 30;

	/* Get reader capabilities */

	snd_stm_printd(0, "Reader's name is '%s'\n", reader->info->name);

	/* Create ALSA lowlevel device */

	result = snd_device_new(card, SNDRV_DEV_LOWLEVEL, reader,
			&snd_stm_uniperif_reader_snd_device_ops);
	if (result < 0) {
		snd_stm_printe("ALSA low level device creation failed!\n");
		goto error_device;
	}

	/* Create ALSA PCM device */

	result = snd_pcm_new(card, NULL, reader->info->card_device, 0, 1,
			&reader->pcm);
	if (result < 0) {
		snd_stm_printe("ALSA PCM instance creation failed!\n");
		goto error_pcm;
	}
	reader->pcm->private_data = reader;
	strcpy(reader->pcm->name, reader->info->name);

	snd_pcm_set_ops(reader->pcm, SNDRV_PCM_STREAM_CAPTURE,
			&snd_stm_uniperif_reader_pcm_ops);

	/* Initialize buffer */

	reader->buffer = snd_stm_buffer_create(reader->pcm,
			reader->device,
			snd_stm_uniperif_reader_hw.buffer_bytes_max);
	if (!reader->buffer) {
		snd_stm_printe("Cannot initialize buffer!\n");
		result = -ENOMEM;
		goto error_buffer_init;
	}

	/* Register in converters router */

	reader->conv_source = snd_stm_conv_register_source(
			&platform_bus_type, dev_name(&pdev->dev),
			reader->info->channels,
			card, reader->info->card_device);
	if (!reader->conv_source) {
		snd_stm_printe("Cannot register in converters router!\n");
		result = -ENOMEM;
		goto error_conv_register_source;
	}
	/* Claim the pads */

	if (reader->info->pad_config) {
		reader->pads = stm_pad_claim(reader->info->pad_config,
				dev_name(&pdev->dev));
		if (!reader->pads) {
			snd_stm_printe("Failed to claimed pads for '%s'!\n",
					dev_name(&pdev->dev));
			result = -EBUSY;
			goto error_pad_claim;
		}
	}

	/* Done now */

	platform_set_drvdata(pdev, reader);

	return 0;
error_pad_claim:
	snd_stm_conv_unregister_source(reader->conv_source);
error_conv_register_source:
	snd_stm_buffer_dispose(reader->buffer);
error_buffer_init:
	/* snd_pcm_free() is not available - PCM device will be released
	 * during card release */
error_pcm:
	snd_device_free(card, reader);
error_device:
	snd_stm_fdma_release(reader->fdma_channel);
error_fdma_request:
	snd_stm_irq_release(reader->irq, reader);
error_irq_request:
	snd_stm_memory_release(reader->mem_region, reader->base);
error_memory_request:
	snd_stm_magic_clear(reader);
	kfree(reader);
error_alloc:
	return result;
}

static int snd_stm_uniperif_reader_remove(struct platform_device *pdev)
{
	struct snd_stm_uniperif_reader *reader = platform_get_drvdata(pdev);

	snd_stm_printd(1, "%s(pdev=%p)\n", __func__, pdev);

	BUG_ON(!reader);
	BUG_ON(!snd_stm_magic_valid(reader));

	if (reader->pads)
		stm_pad_release(reader->pads);

	snd_stm_conv_unregister_source(reader->conv_source);
	snd_stm_buffer_dispose(reader->buffer);
	snd_stm_fdma_release(reader->fdma_channel);
	snd_stm_irq_release(reader->irq, reader);
	snd_stm_memory_release(reader->mem_region, reader->base);

	snd_stm_magic_clear(reader);
	kfree(reader);

	return 0;
}

static struct platform_driver snd_stm_uniperif_reader_driver = {
	.driver.name = "snd_uniperif_reader",
	.probe = snd_stm_uniperif_reader_probe,
	.remove = snd_stm_uniperif_reader_remove,
};


/*
 * Initialization
 */

static int __init snd_stm_uniperif_reader_init(void)
{
	return platform_driver_register(&snd_stm_uniperif_reader_driver);
}

static void snd_stm_uniperif_reader_exit(void)
{
	platform_driver_unregister(&snd_stm_uniperif_reader_driver);
}

MODULE_AUTHOR("John Boddie <john.boddie@st.com>");
MODULE_DESCRIPTION("STMicroelectronics uniperipheral reader driver");
MODULE_LICENSE("GPL");

module_init(snd_stm_uniperif_reader_init);
module_exit(snd_stm_uniperif_reader_exit);
