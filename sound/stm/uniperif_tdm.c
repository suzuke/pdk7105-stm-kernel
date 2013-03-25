/*
 *   STMicroelectronics Uniperipheral TDM driver
 *
 *   Copyright (c) 2012,2013 STMicroelectronics Limited
 *
 *   Author: John Boddie <john.boddie@st.com>
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

#include <linux/init.h>
#include <linux/bpa2.h>
#include <linux/delay.h>
#include <linux/module.h>
#include <linux/platform_device.h>

#include <linux/stm/dma.h>
#include <linux/stm/platform.h>

#include <sound/core.h>

#include "common.h"
#include "reg_aud_uniperif_tdm.h"


#ifndef CONFIG_BPA2
#error "BPA2 must be configured for Uniperif TDM driver"
#endif


/*
 * Defines.
 */

#define UNIPERIF_TDM_MIN_HANDSETS	1
#define UNIPERIF_TDM_MAX_HANDSETS	10

#define UNIPERIF_TDM_MIN_PERIODS	2
#define UNIPERIF_TDM_MAX_PERIODS	10

#define UNIPERIF_TDM_MIN_TIMESLOTS	1
#define UNIPERIF_TDM_MAX_TIMESLOTS	128

#define UNIPERIF_TDM_FREQ_8KHZ		8000
#define UNIPERIF_TDM_FREQ_16KHZ		16000
#define UNIPERIF_TDM_FREQ_32KHZ		32000

#define UNIPERIF_TDM_HW_INFO		(SNDRV_PCM_INFO_MMAP | \
					 SNDRV_PCM_INFO_MMAP_VALID | \
					 SNDRV_PCM_INFO_INTERLEAVED | \
					 SNDRV_PCM_INFO_BLOCK_TRANSFER)

#define UNIPERIF_TDM_HW_FORMAT_CNB	(SNDRV_PCM_FMTBIT_S8 | \
					SNDRV_PCM_FMTBIT_U8)
#define UNIPERIF_TDM_HW_FORMAT_LNB	SNDRV_PCM_FMTBIT_S16_LE
#define UNIPERIF_TDM_HW_FORMAT_CWB	SNDRV_PCM_FMTBIT_S16_LE
#define UNIPERIF_TDM_HW_FORMAT_LWB	SNDRV_PCM_FMTBIT_S32_LE

#define UNIPERIF_TDM_BUF_OFF_MAX	((1UL << (14 + 3)) - 1)
#define UNIPERIF_TDM_BUF_OFF_ALIGN	(1UL << 3) /* 8-byte align */

#define UNIPERIF_TDM_DMA_MAXBURST	35  /* FIFO is 70 words, so half */


/*
 * Types.
 */

struct uniperif_tdm;

struct telss_handset {
	unsigned int id;			/* Handset ID */
	unsigned int ctl;			/* Control ID for handset */
	struct snd_pcm *pcm;			/* PCM device for handset */
	struct uniperif_tdm *tdm;		/* TDM handset is part of */
	struct snd_pcm_hardware hw;		/* Supported hw info */
	struct snd_pcm_substream *substream;	/* Substream for handset */

	unsigned int call_valid;		/* Call is valid */
	unsigned int call_ready;		/* Call has been marked valid */
	unsigned int period_act_sz;		/* Handset actual period size */
	unsigned int buffer_act_sz;		/* Handset actual buffer size */
	unsigned int buffer_pad_sz;		/* Handset padded buffer size */
	unsigned int buffer_offset;		/* Handset buffer offset */
	struct snd_stm_telss_handset_info *info;

	snd_stm_magic_field;
};

struct uniperif_tdm {
	struct device *dev;
	struct snd_stm_uniperif_tdm_info *info;

	struct resource *mem_region;
	void __iomem *base;
	unsigned int irq;
	struct snd_stm_clk *clk;		/* Clock */
	struct snd_stm_clk *pclk;		/* PCLK */
	struct stm_pad_state *pads;		/* PIOs */

	struct snd_info_entry *proc_entry;

	int open_ref;				/* No handsets opened */

	int start_ref;				/* No handsets started */

	struct bpa2_part *buffer_bpa2;		/* BPA2 partition */
	dma_addr_t buffer_phys;			/* Physical address */
	unsigned char *buffer_area;		/* Uncached address */
	size_t buffer_act_sz;			/* Actual buffer size */
	size_t buffer_pad_sz;			/* Padded buffer size */

	size_t period_act_sz;			/* Actual period size */
	int period_count;			/* Actual number of periods */

	dma_cookie_t dma_cookie;
	unsigned int dma_maxburst;
	struct dma_chan *dma_channel;
	struct stm_dma_telss_config dma_config;
	struct dma_async_tx_descriptor *dma_descriptor;
	struct dma_tx_state dma_state;
	enum dma_status dma_status;

	struct telss_handset handsets[UNIPERIF_TDM_MAX_HANDSETS];

	snd_stm_magic_field;
};


/*
 * Uniperipheral TDM globals.
 */

static unsigned int uniperif_tdm_handset_pcm_ctl;

struct uniperif_tdm_mem_fmt_map {
	unsigned int frame_size;
	unsigned int channels;
	unsigned int mem_fmt;
};


/*
 * Uniperipheral TDM implementation.
 */

static void uniperif_tdm_xrun(struct uniperif_tdm *tdm, const char *error)
{
	int i;

	/* Output error message */
	dev_err(tdm->dev, error);

	/* Put each handset into xrun */
	for (i = 0; i < tdm->info->handset_count; ++i)
		if (tdm->handsets[i].substream)
			if (tdm->handsets[i].call_valid)
				snd_pcm_stop(tdm->handsets[i].substream,
						SNDRV_PCM_STATE_XRUN);
}

static irqreturn_t uniperif_tdm_irq_handler(int irq, void *dev_id)
{
	struct uniperif_tdm *tdm = dev_id;
	irqreturn_t result = IRQ_NONE;
	unsigned int status;

	BUG_ON(!tdm);
	BUG_ON(!snd_stm_magic_valid(tdm));

	/* Get the interrupt status and clear immediately */
	preempt_disable();
	status = get__AUD_UNIPERIF_ITS(tdm);
	set__AUD_UNIPERIF_ITS_BCLR(tdm, status);
	preempt_enable();

	if (unlikely(status & mask__AUD_UNIPERIF_ITS__FIFO_ERROR(tdm))) {
		/* Disable interrupt so doesn't continually fire */
		set__AUD_UNIPERIF_ITM_BCLR__FIFO_ERROR(tdm);

		/* Stop each uniperipheral pcm device that is running */
		uniperif_tdm_xrun(tdm, "FIFO error!");

		result = IRQ_HANDLED;

	} else if (unlikely(status & mask__AUD_UNIPERIF_ITS__DMA_ERROR(tdm))) {
		/* Disable interrupt so doesn't continually fire */
		set__AUD_UNIPERIF_ITM_BCLR__DMA_ERROR(tdm);

		/* Stop each uniperipheral pcm device that is running */
		uniperif_tdm_xrun(tdm, "DMA error!");

		result = IRQ_HANDLED;
	}

	if (result != IRQ_HANDLED)
		dev_err(tdm->dev, "Unhandled IRQ: %08x", status);

	return result;
}

static bool uniperif_tdm_dma_filter_fn(struct dma_chan *chan, void *fn_param)
{
	struct uniperif_tdm *tdm = fn_param;
	struct stm_dma_telss_config *config = &tdm->dma_config;

	/* If fdma name has been specified, attempt to match channel to it */
	if (tdm->info->fdma_name)
		if (!stm_dma_is_fdma_name(chan, tdm->info->fdma_name))
			return false;

	/* If fdma channel has been specified, attempt to match channel to it */
	if (tdm->info->fdma_channel)
		if (!stm_dma_is_fdma_channel(chan, tdm->info->fdma_channel))
			return false;

	/* Setup basic dma type and buffer address */
	config->type = STM_DMA_TYPE_TELSS;
	config->dma_addr = tdm->mem_region->start +
			offset__AUD_UNIPERIF_FIFO_DATA(tdm);

	/* Setup dma dreq */
	config->dreq_config.request_line = tdm->info->fdma_request_line;
	config->dreq_config.direct_conn = tdm->info->fdma_direct_conn;
	config->dreq_config.initiator = tdm->info->fdma_initiator;
	config->dreq_config.increment = 0;
	config->dreq_config.hold_off = 0;
	config->dreq_config.maxburst = tdm->dma_maxburst;
	config->dreq_config.buswidth = DMA_SLAVE_BUSWIDTH_4_BYTES;
	config->dreq_config.direction = tdm->info->fdma_direction;

	/* Setup telss information */
	config->frame_size = tdm->info->frame_size - 1; /* 1=2, 2=3, etc */
	config->frame_count = tdm->info->frame_count;
	config->handset_count = tdm->info->handset_count;

	/* Save the channel config inside the channel structure */
	chan->private = config;

	dev_dbg(tdm->dev, "Using '%s' channel %d",
			dev_name(chan->device->dev), chan->chan_id);

	return true;
}

static int uniperif_tdm_dma_setup(struct uniperif_tdm *tdm)
{
	dma_cap_mask_t mask;
	int result;
	int i;

	/* Set the dma channel capabilities we want */
	dma_cap_zero(mask);
	dma_cap_set(DMA_SLAVE, mask);
	dma_cap_set(DMA_CYCLIC, mask);

	/* Request a matching dma channel */
	tdm->dma_channel = dma_request_channel(mask,
			uniperif_tdm_dma_filter_fn, tdm);
	if (!tdm->dma_channel) {
		dev_err(tdm->dev, "Failed to request DMA channel");
		return -ENODEV;
	}

	for (i = 0; i < tdm->info->handset_count; ++i) {
		struct telss_handset *handset = &tdm->handsets[i];
		struct stm_dma_telss_handset_config config;

		/* Set the buffer offset (offset is shifted down 3-bits) */
		config.buffer_offset	= handset->buffer_offset >> 3;

		/* Set the remainder of the handset configuration */
		config.first_slot_id	= handset->info->slot1;
		config.second_slot_id	= handset->info->slot2;
		config.second_slot_id_valid = handset->info->slot2_valid;
		config.duplicate_enable	= handset->info->duplicate;
		config.data_length	= handset->info->data16;
		config.call_valid	= false;

		/* Set the last used handset configuration */
		result = dma_telss_handset_config(tdm->dma_channel, handset->id,
				&config);
		if (result) {
			dev_err(tdm->dev, "Failed to configure handset %d", i);
			dma_release_channel(tdm->dma_channel);
			return result;
		}
	}

	return 0;
}

static int uniperif_tdm_open(struct snd_pcm_substream *substream)
{
	struct telss_handset *handset = snd_pcm_substream_chip(substream);
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct uniperif_tdm *tdm;
	int result;

	BUG_ON(!handset);
	BUG_ON(!snd_stm_magic_valid(handset));
	BUG_ON(!handset->tdm);
	BUG_ON(!snd_stm_magic_valid(handset->tdm));

	tdm = handset->tdm;

	dev_dbg(tdm->dev, "%s(substream=%p)", __func__, substream);

	/* The substream should not be linked to the pcm device yet */
	BUG_ON(handset->substream);

	/* Set the pcm sync identifier for sound card */
	snd_pcm_set_sync(substream);

	/* Request dma channel on first load, can't do on probe as will hang */
	if (tdm->open_ref++ == 0) {
		/* Request dma channel and set default handset configuration */
		result = uniperif_tdm_dma_setup(tdm);
		if (result) {
			dev_err(tdm->dev, "Failed to request DMA channel");
			/* Close called on open failure - open_ref-- there */
			return result;
		}
	}

	/* Ensure application uses same period size in frames as driver */
	result = snd_pcm_hw_constraint_minmax(runtime,
			SNDRV_PCM_HW_PARAM_PERIOD_SIZE,
			tdm->info->frame_count, tdm->info->frame_count);
	if (result < 0) {
		dev_err(tdm->dev, "Failed to constrain period size");
		/* Close called on open failure - open_ref-- there */
		return result;
	}

	/* Set the basic hardware info */
	handset->hw.info = UNIPERIF_TDM_HW_INFO;

	/* C/NB: 8-bit at 8kHz = 8-bit */
	if (handset->info->cnb)
		handset->hw.formats |= UNIPERIF_TDM_HW_FORMAT_CNB;
	/* L/NB: 16-bit at 8kHz = 16-bit */
	if (handset->info->lnb)
		handset->hw.formats |= UNIPERIF_TDM_HW_FORMAT_LNB;
	/* C/WB: 8-bit at 16kHz = 16-bit */
	if (handset->info->cwb)
		handset->hw.formats |= UNIPERIF_TDM_HW_FORMAT_CWB;
	/* L/WB: 16-bit at 16kHz = 32-bit */
	if (handset->info->lwb)
		handset->hw.formats |= UNIPERIF_TDM_HW_FORMAT_LWB;

	/* Handsets only support a single fsync */
	handset->hw.rates = SNDRV_PCM_RATE_CONTINUOUS,
	handset->hw.rate_min = handset->info->fsync;
	handset->hw.rate_max = handset->info->fsync;

	/* Handsets only support a single channel */
	handset->hw.channels_min = 1;
	handset->hw.channels_max = 1;

	/* Ensure application uses same number of periods as driver */
	handset->hw.periods_min	= tdm->period_count;
	handset->hw.periods_max	= tdm->period_count;

	/* Allow application to adjust period bytes depending on frame size */
	handset->hw.period_bytes_min = 1;
	handset->hw.period_bytes_max = handset->period_act_sz;

	/* Allow application to use up to same size buffer as driver */
	handset->hw.buffer_bytes_max = handset->buffer_act_sz;

	/* Link the hardware parameters to the runtime */
	substream->runtime->hw = handset->hw;

	/* Link the substream to pcm device */
	handset->substream = substream;
	return 0;
}

static int uniperif_tdm_close(struct snd_pcm_substream *substream)
{
	struct telss_handset *handset = snd_pcm_substream_chip(substream);
	struct uniperif_tdm *tdm;

	BUG_ON(!handset);
	BUG_ON(!snd_stm_magic_valid(handset));
	BUG_ON(!handset->tdm);
	BUG_ON(!snd_stm_magic_valid(handset->tdm));

	tdm = handset->tdm;

	dev_dbg(tdm->dev, "%s(substream=%p)", __func__, substream);

	/* On last close we must release the dma channel */
	if (--handset->tdm->open_ref == 0) {
		if (tdm->dma_channel) {
			/* Release the dma channel */
			dma_release_channel(tdm->dma_channel);
			tdm->dma_channel = NULL;
		}
	}

	/* Unlink the substream from the pcm device */
	handset->substream = NULL;

	return 0;
}

static int uniperif_tdm_hw_params(struct snd_pcm_substream *substream,
		struct snd_pcm_hw_params *hw_params)
{
	struct telss_handset *handset = snd_pcm_substream_chip(substream);
	struct snd_pcm_runtime *runtime = substream->runtime;

	BUG_ON(!handset);
	BUG_ON(!snd_stm_magic_valid(handset));
	BUG_ON(!runtime);
	BUG_ON(!handset->tdm);
	BUG_ON(!snd_stm_magic_valid(handset->tdm));

	dev_dbg(handset->tdm->dev, "%s(substream=%p, hw_params=%p)", __func__,
		   substream, hw_params);

	/* Only process once regardless of how many times called */
	if ((runtime->dma_addr != 0) && (runtime->dma_area != NULL))
		return 0;

	/* Buffer is already allocated, just set handset dma pointers */
	runtime->dma_addr = handset->tdm->buffer_phys + handset->buffer_offset;
	runtime->dma_area = handset->tdm->buffer_area + handset->buffer_offset;
	runtime->dma_bytes = handset->buffer_act_sz;

	return 0;
}

static int uniperif_tdm_hw_free(struct snd_pcm_substream *substream)
{
	struct telss_handset *handset = snd_pcm_substream_chip(substream);
	struct snd_pcm_runtime *runtime = substream->runtime;

	BUG_ON(!handset);
	BUG_ON(!snd_stm_magic_valid(handset));
	BUG_ON(!runtime);
	BUG_ON(!handset->tdm);
	BUG_ON(!snd_stm_magic_valid(handset->tdm));

	dev_dbg(handset->tdm->dev, "%s(substream=%p)", __func__, substream);

	/* Only process once regardless of how many times called */
	if ((runtime->dma_addr == 0) && (runtime->dma_area == NULL))
		return 0;

	/* Buffer is never freed, just null handset dma pointers */
	runtime->dma_addr = 0;
	runtime->dma_area = NULL;
	runtime->dma_bytes = 0;

	return 0;
}

static int uniperif_tdm_prepare(struct snd_pcm_substream *substream)
{
	struct telss_handset *handset = snd_pcm_substream_chip(substream);
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct stm_dma_telss_handset_config config;
	struct uniperif_tdm *tdm;
	int result;

	BUG_ON(!handset);
	BUG_ON(!snd_stm_magic_valid(handset));
	BUG_ON(!runtime);
	BUG_ON(!handset->tdm);
	BUG_ON(!snd_stm_magic_valid(handset->tdm));

	tdm = handset->tdm;

	dev_dbg(tdm->dev, "%s(substream=%p)", __func__, substream);

	/* Set the buffer offset (offset is shifted down 3-bits) */
	config.buffer_offset	= handset->buffer_offset >> 3;

	/* Set the remainder of the handset configuration */
	config.first_slot_id	= handset->info->slot1;
	config.second_slot_id	= handset->info->slot2;
	config.call_valid	= false;

	/* Determine the correct duplicate and data length */
	switch (substream->runtime->format) {
	case SNDRV_PCM_FORMAT_S8:
	case SNDRV_PCM_FORMAT_U8:
		dev_dbg(tdm->dev, "SNDRV_PCM_FORMAT_S8/U8");
		config.second_slot_id_valid = false;
		config.duplicate_enable	= false;
		config.data_length = false;
		break;

	case SNDRV_PCM_FORMAT_S16_LE:
		dev_dbg(tdm->dev, "SNDRV_PCM_FORMAT_S16_LE");
		config.second_slot_id_valid = false;
		config.duplicate_enable	= false;
		config.data_length = true;
		break;

	case SNDRV_PCM_FORMAT_S32_LE:
		dev_dbg(tdm->dev, "SNDRV_PCM_FORMAT_S32_LE");
		config.second_slot_id_valid = true;
		config.duplicate_enable	= false;
		config.data_length = true;
		break;

	default:
		dev_err(tdm->dev, "Unsupported audio format %d",
				substream->runtime->format);
		BUG();
		return -EINVAL;
	}

	/* Set the handset configuration */
	result = dma_telss_handset_config(tdm->dma_channel, handset->id,
			&config);
	if (result) {
		dev_err(tdm->dev, "Failed to configure handset");
		return result;
	}

	return 0;
}

static void uniperif_tdm_dma_callback(void *param)
{
	struct uniperif_tdm *tdm = param;
	int period;
	int i;

	BUG_ON(!tdm);
	BUG_ON(!snd_stm_magic_valid(tdm));

	/* Get the period currently being processed */
	period = dma_telss_get_period(tdm->dma_channel);

	/* Update buffer pointers for every handset that is running */
	for (i = 0; i < tdm->info->handset_count; ++i) {
		/* Skip null substreams */
		if (!tdm->handsets[i].substream)
			continue;

		/* Skip non-valid calls */
		if (!tdm->handsets[i].call_valid)
			continue;

		/*
		 * A handset buffer is always filled from period 0 onwards. If
		 * the FDMA is already running to process another handset with
		 * a valid call, when the new handset is marked as valid, the
		 * FDMA will begin processing the new call from the next period.
		 * Most likely the next period will not be period 0 (where the
		 * new call data is waiting) and the FDMA will process the new
		 * call data out of order. To prevent this, we wait until the
		 * FDMA is processing the last period before marking the new
		 * call as valid (in reality, the FDMA will cache the next node
		 * it processes, so we wait for the penultimate period).
		 */

		/* Check if call is ready */
		if (!tdm->handsets[i].call_ready) {
			/* Check if penultimate period... */
			if (period == (tdm->period_count - 2)) {
				/* Mark call as active */
				(void) dma_telss_handset_control(
						tdm->dma_channel,
						tdm->handsets[i].id,
						1);

				/* Indicate call is now ready */
				tdm->handsets[i].call_ready = 1;
			}

			continue;
		}


		/* Update the hwptr */
		snd_pcm_period_elapsed(tdm->handsets[i].substream);
	}
}

static int uniperif_tdm_start(struct snd_pcm_substream *substream)
{
	struct telss_handset *handset = snd_pcm_substream_chip(substream);
	struct uniperif_tdm *tdm;
	int result;

	BUG_ON(!handset);
	BUG_ON(!snd_stm_magic_valid(handset));
	BUG_ON(!handset->tdm);
	BUG_ON(!snd_stm_magic_valid(handset->tdm));

	tdm = handset->tdm;

	dev_dbg(tdm->dev, "%s(substream=%p)", __func__, substream);

	/* The pcm interface should not be already started */
	BUG_ON(handset->call_valid);

	/* Is this the first call? */
	if (tdm->start_ref++ == 0) {
		/* The first call can be immediately marked as valid */
		result = dma_telss_handset_control(tdm->dma_channel,
						   handset->id, 1);
		if (result) {
			dev_err(tdm->dev, "Failed mark call active");
			goto error_handset_control;
		}

		/* Indicate the call is ready */
		handset->call_ready = 1;

		/* Prepare the dma descriptor */
		tdm->dma_descriptor = dma_telss_prep_dma_cyclic(
				tdm->dma_channel,
				tdm->buffer_phys,
				tdm->buffer_act_sz,
				tdm->period_act_sz,
				handset->period_act_sz, /* stride */
				tdm->info->fdma_direction);
		if (!tdm->dma_descriptor) {
			dev_err(tdm->dev, "Failed to prepare dma descriptor");
			result = -ENOMEM;
			goto error_prep_dma_cyclic;
		}

		/* Set the dma callback */
		tdm->dma_descriptor->callback = uniperif_tdm_dma_callback;
		tdm->dma_descriptor->callback_param = tdm;

		/* Start dma transfer */
		tdm->dma_cookie = dmaengine_submit(tdm->dma_descriptor);

		/* Enable interrupts */
		set__AUD_UNIPERIF_ITS_BCLR__FIFO_ERROR(tdm);
		set__AUD_UNIPERIF_ITM_BSET__FIFO_ERROR(tdm);
		set__AUD_UNIPERIF_ITS_BCLR__DMA_ERROR(tdm);
		set__AUD_UNIPERIF_ITM_BSET__DMA_ERROR(tdm);
		enable_irq(tdm->irq);

		/* The tdm is already running (it supplies telss with clock) */
	}

	/* Indicate the pcm interface is started and the call is valid */
	handset->call_valid = 1;

	return 0;

error_prep_dma_cyclic:
	dma_telss_handset_control(tdm->dma_channel, handset->id, 0);
	handset->call_ready = 0;
error_handset_control:
	tdm->start_ref--;
	return result;
}

static int uniperif_tdm_stop(struct snd_pcm_substream *substream)
{
	struct telss_handset *handset = snd_pcm_substream_chip(substream);
	struct uniperif_tdm *tdm;
	int result;

	BUG_ON(!handset);
	BUG_ON(!snd_stm_magic_valid(handset));
	BUG_ON(!handset->tdm);
	BUG_ON(!snd_stm_magic_valid(handset->tdm));

	tdm = handset->tdm;

	dev_dbg(tdm->dev, "%s(substream=%p)", __func__, substream);

	/* The pcm interface should be started */
	BUG_ON(!(handset->call_valid));

	/* Immediately mark the call as no longer valid */
	result = dma_telss_handset_control(tdm->dma_channel, handset->id, 0);
	if (result) {
		dev_err(tdm->dev, "Failed clear call valid");
		return result;
	}

	/* Is this the last call?*/
	if (--tdm->start_ref == 0) {
		/* Disable interrupts */
		disable_irq_nosync(tdm->irq);
		set__AUD_UNIPERIF_ITM_BCLR__FIFO_ERROR(tdm);
		set__AUD_UNIPERIF_ITM_BCLR__DMA_ERROR(tdm);

		/* Terminate the dma */
		dmaengine_terminate_all(tdm->dma_channel);
	}

	/* Indicate the pcm interface is not started */
	handset->call_valid = 0;
	handset->call_ready = 0;

	return 0;
}

static int uniperif_tdm_trigger(struct snd_pcm_substream *substream,
		int command)
{
	switch (command) {
	case SNDRV_PCM_TRIGGER_START:
		return uniperif_tdm_start(substream);
	case SNDRV_PCM_TRIGGER_STOP:
		return uniperif_tdm_stop(substream);
	default:
		BUG();
		return -EINVAL;
	}

	return 0;
}

/* return -1 on xrun */
static snd_pcm_uframes_t uniperif_tdm_pointer(
		struct snd_pcm_substream *substream)
{
	struct telss_handset *handset = snd_pcm_substream_chip(substream);
	struct snd_pcm_runtime *runtime = substream->runtime;
	int period;

	BUG_ON(!handset);
	BUG_ON(!snd_stm_magic_valid(handset));
	BUG_ON(!runtime);
	BUG_ON(!handset->tdm);
	BUG_ON(!snd_stm_magic_valid(handset->tdm));

	/*dev_dbg(tdm->dev, "%s(substream=%p)", __func__, substream);*/

	/* We should only be here when handset call is valid */
	BUG_ON(!handset->call_valid);

	/* Get current period being processed */
	period = dma_telss_get_period(handset->tdm->dma_channel);

	/* Convert period offset to a frame offset */
	return period * handset->tdm->info->frame_count;
}

#define COPY_FROM_USER(t, dma, usr, a, s) \
	do { \
		t data; \
		/*t *ptr = (t *) dma;*/		\
		int i; \
		for (i = 0; i < a; ++i) { \
			__get_user(data, &((t __user *) usr)[i]); \
			/*ptr[i] = s(data);*/			  \
			((t *) dma)[i] = s(data);		  \
		} \
	} while (0);

#define COPY_TO_USER(t, dma, usr, a, s) \
	do { \
		t data; \
		/*t *ptr = (t *) dma;*/		\
		int i; \
		for (i = 0; i < a; ++i) { \
			/*data = s(ptr[i]);*/	\
			data = s(((t *) dma)[i]);		  \
			__put_user(data, &((t __user *) usr)[i]); \
		} \
	} while (0);

static int uniperif_tdm_copy(struct snd_pcm_substream *substream,
		int channel, snd_pcm_uframes_t pos, void __user *buf,
		snd_pcm_uframes_t count)
{
	struct telss_handset *handset = snd_pcm_substream_chip(substream);
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct uniperif_tdm *tdm;
	unsigned long period;
	unsigned long offset;
	unsigned long amount;
	void *dma;
	int result;

	BUG_ON(!handset);
	BUG_ON(!snd_stm_magic_valid(handset));
	BUG_ON(!handset->tdm);
	BUG_ON(!snd_stm_magic_valid(handset->tdm));
	BUG_ON(channel != -1);	/* Only support interleaved buffers */

	tdm = handset->tdm;

	/*
	dev_dbg(tdm->dev, "%s(substream=%p, channel=%d, pos=%ld, buf=%p, "
		"count=%ld)", __func__, substream, channel, pos, buf, count);
	*/

	/* Copy the data to the dma buffers maximum one period at a time */
	while (count) {
		/* Determine period containing the current frame position */
		period = pos / tdm->info->frame_count;
		period %= tdm->period_count;

		/* Point at the period in the handset dma buffer */
		dma = runtime->dma_area + (period * handset->period_act_sz);

		/* Determine period offset for the current frame position */
		offset = pos % tdm->info->frame_count;

		/* Point at the offset in the dma buffer */
		dma += frames_to_bytes(runtime, offset);

		/* Determine how many frames can be copied to this period */
		amount = tdm->info->frame_count - offset;
		amount = (amount > count) ? count : amount;

		/* Actually copy data to/from dma buffer */
		if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK)
			result = copy_from_user(dma, buf,
					frames_to_bytes(runtime, amount));
		else
			result = copy_to_user(buf, dma,
					frames_to_bytes(runtime, amount));

		BUG_ON(result);

		/* Update position, count and user buffer pointer */
		pos += amount;
		count -= amount;
		buf += frames_to_bytes(runtime, amount);
	}

	return 0;
}

static struct snd_pcm_ops uniperif_tdm_pcm_ops = {
	.open		= uniperif_tdm_open,
	.close		= uniperif_tdm_close,
	.mmap		= snd_stm_buffer_mmap,
	.ioctl		= snd_pcm_lib_ioctl,
	.hw_params	= uniperif_tdm_hw_params,
	.hw_free	= uniperif_tdm_hw_free,
	.prepare	= uniperif_tdm_prepare,
	.trigger	= uniperif_tdm_trigger,
	.pointer	= uniperif_tdm_pointer,
	.copy		= uniperif_tdm_copy,
};


/*
 * ALSA low-level device functions.
 */

static int uniperif_tdm_lookup_mem_fmt(struct uniperif_tdm *tdm,
		unsigned int *mem_fmt, unsigned int *channels)
{
	static struct uniperif_tdm_mem_fmt_map mem_fmt_table[] = {
		/* 16/16 memory formats for frame size (prefer over 16/0) */
		{1, 2, value__AUD_UNIPERIF_CONFIG__MEM_FMT_16_16(NULL)},
		{2, 4, value__AUD_UNIPERIF_CONFIG__MEM_FMT_16_16(NULL)},
		{3, 6, value__AUD_UNIPERIF_CONFIG__MEM_FMT_16_16(NULL)},
		{4, 8, value__AUD_UNIPERIF_CONFIG__MEM_FMT_16_16(NULL)},
		/* 16/0 memory formats for frame size */
		{2, 2, value__AUD_UNIPERIF_CONFIG__MEM_FMT_16_0(NULL)},
		{4, 4, value__AUD_UNIPERIF_CONFIG__MEM_FMT_16_0(NULL)},
		{6, 6, value__AUD_UNIPERIF_CONFIG__MEM_FMT_16_0(NULL)},
		{8, 8, value__AUD_UNIPERIF_CONFIG__MEM_FMT_16_0(NULL)},
	};
	static int mem_fmt_table_size = ARRAY_SIZE(mem_fmt_table);
	int i;

	BUG_ON(!tdm);
	BUG_ON(!snd_stm_magic_valid(tdm));
	BUG_ON(!mem_fmt);
	BUG_ON(!channels);

	/* Search table based on frame size */
	for (i = 0; i < mem_fmt_table_size; ++i) {
		/* Skip table entry if frame size doesn't match */
		if (tdm->info->frame_size != mem_fmt_table[i].frame_size)
			continue;

		/* Frame size matches, return memory format and channel count */
		*mem_fmt = mem_fmt_table[i].mem_fmt;
		*channels = mem_fmt_table[i].channels / 2;
		return 0;
	}

	return -EINVAL;
}

static int uniperif_tdm_set_freqs(struct uniperif_tdm *tdm)
{
	dev_dbg(tdm->dev, "%s(tdm=%p)", __func__, tdm);

	BUG_ON(!tdm);
	BUG_ON(!snd_stm_magic_valid(tdm));

	/* Set the frequency parameters (FS_REF == FS01) */
	switch (tdm->info->fs01_rate) {
	case UNIPERIF_TDM_FREQ_8KHZ:
		set__AUD_UNIPERIF_TDM_FS_REF_FREQ__8KHZ(tdm);
		set__AUD_UNIPERIF_TDM_FS01_FREQ__8KHZ(tdm);
		set__AUD_UNIPERIF_TDM_FS01_WIDTH__1BIT(tdm);
		break;

	case UNIPERIF_TDM_FREQ_16KHZ:
		set__AUD_UNIPERIF_TDM_FS_REF_FREQ__16KHZ(tdm);
		set__AUD_UNIPERIF_TDM_FS01_FREQ__16KHZ(tdm);
		set__AUD_UNIPERIF_TDM_FS01_WIDTH__1BIT(tdm);
		break;

	default:
		dev_err(tdm->dev, "Invalid fs01 (%ld)", tdm->info->fs01_rate);
		return -EINVAL;
	}

	/* Set the number of timeslots required */
	BUG_ON(tdm->info->timeslots < UNIPERIF_TDM_MIN_TIMESLOTS);
	BUG_ON(tdm->info->timeslots > UNIPERIF_TDM_MAX_TIMESLOTS);

	set__AUD_UNIPERIF_TDM_FS_REF_DIV__NUM_TIMESLOT(tdm,
			tdm->info->timeslots);

	/* Set the frequency parameters (FS_REF == FS01) */
	switch (tdm->info->fs02_rate) {
	case UNIPERIF_TDM_FREQ_8KHZ:
		set__AUD_UNIPERIF_TDM_FS02_FREQ__8KHZ(tdm);
		set__AUD_UNIPERIF_TDM_FS02_WIDTH__1BIT(tdm);
		break;

	case UNIPERIF_TDM_FREQ_16KHZ:
		set__AUD_UNIPERIF_TDM_FS02_FREQ__16KHZ(tdm);
		set__AUD_UNIPERIF_TDM_FS02_WIDTH__1BIT(tdm);
		break;

	default:
		dev_err(tdm->dev, "Invalid fs02 (%ld)", tdm->info->fs02_rate);
		return -EINVAL;
	}

	/* Set the frame sync 2 timeslot delay */
	set__AUD_UNIPERIF_TDM_FS02_TIMESLOT_DELAY__PCM_CLOCK(tdm,
			tdm->info->fs02_delay_clock);
	set__AUD_UNIPERIF_TDM_FS02_TIMESLOT_DELAY__TIMESLOT(tdm,
			tdm->info->fs02_delay_timeslot);

	return 0;
}

static void uniperif_tdm_configure_timeslots(struct uniperif_tdm *tdm)
{
	dev_dbg(tdm->dev, "%s(tdm=%p)", __func__, tdm);

	BUG_ON(!tdm);
	BUG_ON(!snd_stm_magic_valid(tdm));

	/* Set the timeslot start position */
	set__AUD_UNIPERIF_TDM_DATA_MSBIT_START__DELAY(tdm,
			tdm->info->msbit_start);

	/* Set word 1 timeslot msb/lsb */
	if (tdm->info->timeslot_info->word_num > 0) {
		set__AUD_UNIPERIF_TDM_WORD_POS_1_2__TS_1_MSB(tdm,
				tdm->info->timeslot_info->word_pos[0].msb);
		set__AUD_UNIPERIF_TDM_WORD_POS_1_2__TS_1_LSB(tdm,
				tdm->info->timeslot_info->word_pos[0].lsb);
	}

	/* Set word 2 timeslot msb/lsb */
	if (tdm->info->timeslot_info->word_num > 1) {
		set__AUD_UNIPERIF_TDM_WORD_POS_1_2__TS_2_MSB(tdm,
				tdm->info->timeslot_info->word_pos[1].msb);
		set__AUD_UNIPERIF_TDM_WORD_POS_1_2__TS_2_LSB(tdm,
				tdm->info->timeslot_info->word_pos[1].lsb);
	}

	/* Set word 3 timeslot msb/lsb */
	if (tdm->info->timeslot_info->word_num > 2) {
		set__AUD_UNIPERIF_TDM_WORD_POS_3_4__TS_3_MSB(tdm,
				tdm->info->timeslot_info->word_pos[2].msb);
		set__AUD_UNIPERIF_TDM_WORD_POS_3_4__TS_3_LSB(tdm,
				tdm->info->timeslot_info->word_pos[2].lsb);
	}

	/* Set word 4 timeslot msb/lsb */
	if (tdm->info->timeslot_info->word_num > 3) {
		set__AUD_UNIPERIF_TDM_WORD_POS_3_4__TS_4_MSB(tdm,
				tdm->info->timeslot_info->word_pos[3].msb);
		set__AUD_UNIPERIF_TDM_WORD_POS_3_4__TS_4_LSB(tdm,
				tdm->info->timeslot_info->word_pos[3].lsb);
	}

	/* Set word 5 timeslot msb/lsb */
	if (tdm->info->timeslot_info->word_num > 4) {
		set__AUD_UNIPERIF_TDM_WORD_POS_5_6__TS_5_MSB(tdm,
				tdm->info->timeslot_info->word_pos[4].msb);
		set__AUD_UNIPERIF_TDM_WORD_POS_5_6__TS_5_LSB(tdm,
				tdm->info->timeslot_info->word_pos[4].lsb);
	}

	/* Set word 6 timeslot msb/lsb */
	if (tdm->info->timeslot_info->word_num > 5) {
		set__AUD_UNIPERIF_TDM_WORD_POS_5_6__TS_6_MSB(tdm,
				tdm->info->timeslot_info->word_pos[5].msb);
		set__AUD_UNIPERIF_TDM_WORD_POS_5_6__TS_6_LSB(tdm,
				tdm->info->timeslot_info->word_pos[5].lsb);
	}

	/* Set word 7 timeslot msb/lsb */
	if (tdm->info->timeslot_info->word_num > 6) {
		set__AUD_UNIPERIF_TDM_WORD_POS_7_8__TS_7_MSB(tdm,
				tdm->info->timeslot_info->word_pos[6].msb);
		set__AUD_UNIPERIF_TDM_WORD_POS_7_8__TS_7_LSB(tdm,
				tdm->info->timeslot_info->word_pos[6].lsb);
	}

	/* Set word 8 timeslot msb/lsb */
	if (tdm->info->timeslot_info->word_num > 7) {
		set__AUD_UNIPERIF_TDM_WORD_POS_7_8__TS_8_MSB(tdm,
				tdm->info->timeslot_info->word_pos[7].msb);
		set__AUD_UNIPERIF_TDM_WORD_POS_7_8__TS_8_LSB(tdm,
				tdm->info->timeslot_info->word_pos[7].msb);
	}
}

static int uniperif_tdm_configure(struct uniperif_tdm *tdm)
{
	unsigned int mem_fmt;
	unsigned int channels;
	int result;

	dev_dbg(tdm->dev, "%s(tdm=%p)", __func__, tdm);

	BUG_ON(!tdm);
	BUG_ON(!snd_stm_magic_valid(tdm));

	/* Look up what memory format to use based on frame size & channels */
	result = uniperif_tdm_lookup_mem_fmt(tdm, &mem_fmt, &channels);
	if (result) {
		dev_err(tdm->dev, "Failed to lookup mem fmt");
		return result;
	}

	/* Set the input memory format */
	set__AUD_UNIPERIF_CONFIG__MEM_FMT(tdm, mem_fmt);
	/* Set the fdma trigger limit */
	set__AUD_UNIPERIF_CONFIG__FDMA_TRIGGER_LIMIT(tdm, tdm->dma_maxburst);

	/* ??? Could be an issue with 8-bit data ??? */
	set__AUD_UNIPERIF_I2S_FMT__NBIT_16(tdm);
	/* Set the output data size for 32-bit i2s */
	set__AUD_UNIPERIF_I2S_FMT__DATA_SIZE_32(tdm);
	/* Set the output data lr clock polarity to high for i2s */
	set__AUD_UNIPERIF_I2S_FMT__LR_POL_HIG(tdm);
	/* Set the output data alignment to left for i2s */
	set__AUD_UNIPERIF_I2S_FMT__ALIGN_LEFT(tdm);
	/* Set the output data format to MSB for i2s */
	set__AUD_UNIPERIF_I2S_FMT__ORDER_MSB(tdm);
	/* Set the output data channel count */
	set__AUD_UNIPERIF_I2S_FMT__NUM_CH(tdm, channels);
	/* Set the output data padding to Sony mode */
	set__AUD_UNIPERIF_I2S_FMT__PADDING_SONY_MODE(tdm);

	/* Set the clock edge (usually Tx = rising edge, Rx = falling edge) */
	if (tdm->info->rising_edge)
		set__AUD_UNIPERIF_I2S_FMT__SCLK_EDGE_RISING(tdm);
	else
		set__AUD_UNIPERIF_I2S_FMT__SCLK_EDGE_FALLING(tdm);

	/* We don't use the memory block read interrupt, so set this to zero */
	set__AUD_UNIPERIF_I2S_FMT__NO_OF_SAMPLES_TO_READ(tdm, 0);

	/* Enable the tdm functionality */
	set__AUD_UNIPERIF_TDM_ENABLE__TDM_ENABLE(tdm);

	/* Only set the frequencies if a player */
	if (tdm->info->fdma_direction == DMA_MEM_TO_DEV) {
		result = uniperif_tdm_set_freqs(tdm);
		if (result) {
			dev_err(tdm->dev, "Failed to set freqs");
			return result;
		}
	}

	/* Configure the time slots */
	uniperif_tdm_configure_timeslots(tdm);

	return 0;
}

static int uniperif_tdm_clk_get(struct uniperif_tdm *tdm)
{
	struct snd_card *card = snd_stm_card_get(SND_STM_CARD_TYPE_TELSS);
	int result;

	dev_dbg(tdm->dev, "%s(tdm=%p)", __func__, tdm);

	BUG_ON(!tdm);
	BUG_ON(!snd_stm_magic_valid(tdm));

	/* Only configure clk if frequency specified (i.e. player) */
	if (!tdm->info->clk_rate)
		return 0;

	/* Get the tdm clock */
	tdm->clk = snd_stm_clk_get(tdm->dev, "uniperif_tdm_clk", card,
			tdm->info->card_device);
	if (IS_ERR(tdm->clk)) {
		dev_err(tdm->dev, "Failed to get clk");
		return -EINVAL;
	}

	/* Enable clock here as it must be permanently on */
	result = snd_stm_clk_enable(tdm->clk);
	if (result) {
		dev_err(tdm->dev, "Failed to enable clk");
		goto error_clk_enable;
	}

	/* Set clock rate (rate at which telss devices run) */
	result = snd_stm_clk_set_rate(tdm->clk, tdm->info->clk_rate);
	if (result) {
		dev_err(tdm->dev, "Failed to set clk rate");
		goto error_clk_set_rate;
	}

	return 0;

error_clk_set_rate:
	snd_stm_clk_disable(tdm->clk);
error_clk_enable:
	snd_stm_clk_put(tdm->clk);
	return result;
}

static void uniperif_tdm_clk_put(struct uniperif_tdm *tdm)
{
	dev_dbg(tdm->dev, "%s(tdm=%p)", __func__, tdm);

	BUG_ON(!tdm);
	BUG_ON(!snd_stm_magic_valid(tdm));

	if (!IS_ERR(tdm->clk)) {
		/* Disable the clock */
		snd_stm_clk_disable(tdm->clk);
		/* Relinquish the clock */
		snd_stm_clk_put(tdm->clk);
		/* Null the clock pointer */
		tdm->clk = NULL;
	}
}

static int uniperif_tdm_pclk_get(struct uniperif_tdm *tdm)
{
	struct snd_card *card = snd_stm_card_get(SND_STM_CARD_TYPE_TELSS);
	int result;

	dev_dbg(tdm->dev, "%s(tdm=%p)", __func__, tdm);

	BUG_ON(!tdm);
	BUG_ON(!snd_stm_magic_valid(tdm));

	/* Only configure pclk if frequency specified (i.e. player) */
	if (!tdm->info->pclk_rate)
		return 0;

	/* Get the tdm pclk */
	tdm->pclk = snd_stm_clk_get(tdm->dev, "uniperif_tdm_pclk", card,
			tdm->info->card_device);
	if (IS_ERR(tdm->pclk)) {
		dev_err(tdm->dev, "Failed to get pclk");
		return -EINVAL;
	}

	/* Enable pclk here as it must be permanently on */
	result = snd_stm_clk_enable(tdm->pclk);
	if (result) {
		dev_err(tdm->dev, "Failed to enable pclk");
		goto error_pclk_enable;
	}

	/* Set pclk rate (rate at which telss devices run) */
	result = snd_stm_clk_set_rate(tdm->pclk, tdm->info->pclk_rate);
	if (result) {
		dev_err(tdm->dev, "Failed to set pclk rate");
		goto error_pclk_set_rate;
	}

	return 0;

error_pclk_set_rate:
	snd_stm_clk_disable(tdm->pclk);
error_pclk_enable:
	snd_stm_clk_put(tdm->pclk);
	return result;
}

static void uniperif_tdm_pclk_put(struct uniperif_tdm *tdm)
{
	dev_dbg(tdm->dev, "%s(tdm=%p)", __func__, tdm);

	BUG_ON(!tdm);
	BUG_ON(!snd_stm_magic_valid(tdm));

	if (!IS_ERR(tdm->pclk)) {
		/* Disable the clock */
		snd_stm_clk_disable(tdm->pclk);
		/* Relinquish the clock */
		snd_stm_clk_put(tdm->pclk);
		/* Null the clock pointer */
		tdm->pclk = NULL;
	}
}

static void uniperif_tdm_hw_reset(struct uniperif_tdm *tdm)
{
	dev_dbg(tdm->dev, "%s(tdm=%p)", __func__, tdm);

	BUG_ON(!tdm);
	BUG_ON(!snd_stm_magic_valid(tdm));

	/* Issue a soft reset to ensure consistent IP internal state */
	while (get__AUD_UNIPERIF_SOFT_RST__SOFT_RST(tdm))
		udelay(5);
}

static void uniperif_tdm_hw_enable(struct uniperif_tdm *tdm)
{
	int i;

	dev_dbg(tdm->dev, "%s(tdm=%p)", __func__, tdm);

	BUG_ON(!tdm);
	BUG_ON(!snd_stm_magic_valid(tdm));

	/* If we are a player, prime the FIFO to start things off */
	if (tdm->info->fdma_direction == DMA_MEM_TO_DEV)
		for (i = 0; i < tdm->info->frame_size; ++i)
			set__AUD_UNIPERIF_FIFO_DATA(tdm, 0x00000000);

	/* The uniperipheral must be on to supply a clock to telss hardware */
	set__AUD_UNIPERIF_CTRL__OPERATION_PCM_DATA(tdm);

	/* Issue a soft reset to ensure consistent IP internal state */
	uniperif_tdm_hw_reset(tdm);
}

static void uniperif_tdm_hw_disable(struct uniperif_tdm *tdm)
{
	dev_dbg(tdm->dev, "%s(tdm=%p)", __func__, tdm);

	BUG_ON(!tdm);
	BUG_ON(!snd_stm_magic_valid(tdm));

	/* Disable the tdm functionality */
	set__AUD_UNIPERIF_TDM_ENABLE__TDM_DISABLE(tdm);

	/* Set operating mode to off */
	set__AUD_UNIPERIF_CTRL__OPERATION_OFF(tdm);

	/* Issue a soft reset to ensure consistent IP internal state */
	uniperif_tdm_hw_reset(tdm);
}

#define UNIPERIF_TDM_DUMP_REGISTER(r) \
		snd_iprintf(buffer, "AUD_UNIPERIF_%-23s (offset 0x%04x) = " \
				"0x%08x\n", __stringify(r), \
				offset__AUD_UNIPERIF_##r(tdm), \
				get__AUD_UNIPERIF_##r(tdm))

static void uniperif_tdm_dump_registers(struct snd_info_entry *entry,
		struct snd_info_buffer *buffer)
{
	struct uniperif_tdm *tdm = entry->private_data;

	BUG_ON(!tdm);
	BUG_ON(!snd_stm_magic_valid(tdm));

	snd_iprintf(buffer, "--- %s (0x%p) ---\n", dev_name(tdm->dev),
			tdm->base);

	UNIPERIF_TDM_DUMP_REGISTER(SOFT_RST);
	UNIPERIF_TDM_DUMP_REGISTER(STA);
	UNIPERIF_TDM_DUMP_REGISTER(ITS);
	UNIPERIF_TDM_DUMP_REGISTER(ITM);
	UNIPERIF_TDM_DUMP_REGISTER(CONFIG);
	UNIPERIF_TDM_DUMP_REGISTER(CTRL);
	UNIPERIF_TDM_DUMP_REGISTER(I2S_FMT);
	UNIPERIF_TDM_DUMP_REGISTER(STATUS_1);
	UNIPERIF_TDM_DUMP_REGISTER(DFV0);
	UNIPERIF_TDM_DUMP_REGISTER(CONTROLABILITY);
	UNIPERIF_TDM_DUMP_REGISTER(CRC_CTRL);
	UNIPERIF_TDM_DUMP_REGISTER(CRC_WINDOW);
	UNIPERIF_TDM_DUMP_REGISTER(CRC_VALUE_IN);
	UNIPERIF_TDM_DUMP_REGISTER(CRC_VALUE_OUT);
	UNIPERIF_TDM_DUMP_REGISTER(TDM_ENABLE);

	/* Only output frequency registers if a player */
	if (tdm->info->fdma_direction == DMA_MEM_TO_DEV) {
		UNIPERIF_TDM_DUMP_REGISTER(TDM_FS_REF_FREQ);
		UNIPERIF_TDM_DUMP_REGISTER(TDM_FS_REF_DIV);
		UNIPERIF_TDM_DUMP_REGISTER(TDM_FS01_FREQ);
		UNIPERIF_TDM_DUMP_REGISTER(TDM_FS01_WIDTH);
		UNIPERIF_TDM_DUMP_REGISTER(TDM_FS02_FREQ);
		UNIPERIF_TDM_DUMP_REGISTER(TDM_FS02_WIDTH);
		UNIPERIF_TDM_DUMP_REGISTER(TDM_FS02_TIMESLOT_DELAY);
	}

	UNIPERIF_TDM_DUMP_REGISTER(TDM_DATA_MSBIT_START);
	UNIPERIF_TDM_DUMP_REGISTER(TDM_WORD_POS_1_2);
	UNIPERIF_TDM_DUMP_REGISTER(TDM_WORD_POS_3_4);
	UNIPERIF_TDM_DUMP_REGISTER(TDM_WORD_POS_5_6);
	UNIPERIF_TDM_DUMP_REGISTER(TDM_WORD_POS_7_8);

	snd_iprintf(buffer, "\n");
}

static int uniperif_tdm_register(struct snd_device *snd_device)
{
	struct uniperif_tdm *tdm = snd_device->device_data;
	int result;

	dev_dbg(tdm->dev, "%s(snd_device=%p)", __func__, snd_device);

	BUG_ON(!tdm);
	BUG_ON(!snd_stm_magic_valid(tdm));

	/* Get the tdm clock */
	result = uniperif_tdm_clk_get(tdm);
	if (result) {
		dev_err(tdm->dev, "Failed to get clock");
		goto error_clk_get;
	}

	/* Get the tdm pclk */
	result = uniperif_tdm_pclk_get(tdm);
	if (result) {
		dev_err(tdm->dev, "Failed to get pclk");
		goto error_pclk_get;
	}

	/* Configure the tdm device */
	result = uniperif_tdm_configure(tdm);
	if (result) {
		dev_err(tdm->dev, "Failed to configure");
		goto error_configure;
	}

	/* Add procfs info entry */
	result = snd_stm_info_register(&tdm->proc_entry, dev_name(tdm->dev),
			uniperif_tdm_dump_registers, tdm);
	if (result) {
		dev_err(tdm->dev, "Failed to register with procfs");
		goto error_info_register;
	}

	/* Enable the tdm device */
	uniperif_tdm_hw_enable(tdm);

	return 0;

error_info_register:
error_configure:
	uniperif_tdm_pclk_put(tdm);
error_pclk_get:
	uniperif_tdm_clk_put(tdm);
error_clk_get:
	return result;
}

static int uniperif_tdm_disconnect(struct snd_device *snd_device)
{
	struct uniperif_tdm *tdm = snd_device->device_data;

	dev_dbg(tdm->dev, "%s(snd_device=%p)", __func__, snd_device);

	BUG_ON(!tdm);
	BUG_ON(!snd_stm_magic_valid(tdm));

	/* Disable the tdm device */
	uniperif_tdm_hw_disable(tdm);

	/* Relinquish the clocks */
	uniperif_tdm_pclk_put(tdm);
	uniperif_tdm_clk_put(tdm);

	/* Remove procfs info entry */
	snd_stm_info_unregister(tdm->proc_entry);

	return 0;
}

static struct snd_device_ops uniperif_tdm_snd_device_ops = {
	.dev_register	= uniperif_tdm_register,
	.dev_disconnect	= uniperif_tdm_disconnect,
};


/*
 * Platform driver initialisation.
 */

static int __devinit uniperif_tdm_handset_init(struct uniperif_tdm *tdm, int id)
{
	struct snd_card *card = snd_stm_card_get(SND_STM_CARD_TYPE_TELSS);
	struct telss_handset *handset;
	int direction;
	int playback;
	int capture;
	char *name;
	int result;

	dev_dbg(tdm->dev, "%s(tdm=%p, id=%d)", __func__, tdm, id);

	BUG_ON(!tdm);
	BUG_ON(!snd_stm_magic_valid(tdm));

	/* Get a pointer to the handset structure */
	handset = &tdm->handsets[id];

	/* Initialise handset structure */
	handset->id = id;
	handset->ctl = uniperif_tdm_handset_pcm_ctl++;
	handset->tdm = tdm;
	handset->info = &tdm->info->handset_info[id];
	handset->period_act_sz = tdm->period_act_sz / tdm->info->handset_count;
	handset->buffer_act_sz = tdm->buffer_act_sz / tdm->info->handset_count;
	handset->buffer_pad_sz = tdm->buffer_pad_sz / tdm->info->handset_count;
	handset->buffer_offset = handset->buffer_pad_sz * id;
	snd_stm_magic_set(handset);

	/* Ensure handset buffer offset is within the fdma node range */
	BUG_ON(handset->buffer_offset > UNIPERIF_TDM_BUF_OFF_MAX);

	switch (tdm->info->fdma_direction) {
	case DMA_MEM_TO_DEV:
		name = "TELSS Player #%d";
		capture = 0;
		playback = 1;
		direction = SNDRV_PCM_STREAM_PLAYBACK;
		break;

	case DMA_DEV_TO_MEM:
		name = "TELSS Reader #%d";
		capture = 1;
		playback = 0;
		direction = SNDRV_PCM_STREAM_CAPTURE;
		break;

	default:
		dev_err(tdm->dev, "Cannot determine if player/reader");
		return -EINVAL;
	}

	/* Create a new ALSA playback pcm device for handset */
	result = snd_pcm_new(card, NULL, handset->ctl, playback, capture,
				&handset->pcm);
	if (result) {
		dev_err(tdm->dev, "Failed to create pcm for handset %d", id);
		return result;
	}

	/* Set the pcm device name */
	snprintf(handset->pcm->name, sizeof(handset->pcm->name), name, id);
	dev_notice(tdm->dev, "'%s'", handset->pcm->name);

	/* Link the pcm device to the handset */
	handset->pcm->private_data = handset;

	/* Set the pcm device ops */
	snd_pcm_set_ops(handset->pcm, direction, &uniperif_tdm_pcm_ops);

	return 0;
}

static int __devinit uniperif_tdm_probe(struct platform_device *pdev)
{
	struct snd_card *card = snd_stm_card_get(SND_STM_CARD_TYPE_TELSS);
	struct uniperif_tdm *tdm;
	int result;
	int pages;
	int h;

	BUG_ON(!pdev);
	BUG_ON(!pdev->dev.platform_data);

	dev_dbg(&pdev->dev, "%s(pdev=%p)\n", __func__, pdev);

	/* Allocate device structure */
	tdm = devm_kzalloc(&pdev->dev, sizeof(*tdm), GFP_KERNEL);
	if (!tdm) {
		dev_err(&pdev->dev, "Failed to allocate device structure");
		return -ENOMEM;
	}

	/* Initialise device structure */
	tdm->dev = &pdev->dev;
	tdm->info = pdev->dev.platform_data;
	snd_stm_magic_set(tdm);

	dev_notice(&pdev->dev, "'%s'", tdm->info->name);

	/* Request memory region */
	result = snd_stm_memory_request(pdev, &tdm->mem_region, &tdm->base);
	if (result < 0) {
		dev_err(&pdev->dev, "Failed memory request");
		return result;
	}

	/* Request irq */
	result = snd_stm_irq_request(pdev, &tdm->irq, uniperif_tdm_irq_handler,
			tdm);
	if (result < 0) {
		dev_err(&pdev->dev, "Failed IRQ request");
		return result;
	}

	/* Claim the pads */
	if (tdm->info->pad_config) {
		tdm->pads = stm_pad_claim(tdm->info->pad_config,
				dev_name(&pdev->dev));
		if (!tdm->pads) {
			dev_err(&pdev->dev, "Failed to claim pads");
			return -EBUSY;
		}
	}

	/* Link device to sound card (sound card will manage device) */
	result = snd_device_new(card, SNDRV_DEV_LOWLEVEL, tdm,
			&uniperif_tdm_snd_device_ops);
	if (result < 0) {
		dev_err(&pdev->dev, "Failed to create ALSA sound device");
		goto error_device_new;
	}

	/* Bounds check the number of handsets (handsets != I2S channels) */
	BUG_ON(tdm->info->handset_count < UNIPERIF_TDM_MIN_HANDSETS);
	BUG_ON(tdm->info->handset_count > UNIPERIF_TDM_MAX_HANDSETS);

	/*
	 * Here we calculate various period and buffer sizes. Each handset
	 * buffer is contiguous and aligned to 8-bytes as the offset to each
	 * buffer must be shifted down 3-bits when programmed into the FDMA
	 * handset node parameters. All handset buffers must fit within the
	 * programmable handset offset address range (i.e. 0 - 131,064).
	 */

	/* Calculate actual period size (for all handsets) */
	tdm->period_act_sz = tdm->info->frame_size * 4 * tdm->info->frame_count;

	/* Calculate number of periods that we can fit within handset offset */
	tdm->period_count = UNIPERIF_TDM_BUF_OFF_MAX;
	tdm->period_count /= tdm->period_act_sz;
	tdm->period_count /= tdm->info->handset_count;

	/* Bounds check the number of periods */
	if (tdm->period_count > UNIPERIF_TDM_MAX_PERIODS)
		tdm->period_count = UNIPERIF_TDM_MAX_PERIODS;

	BUG_ON(tdm->period_count < UNIPERIF_TDM_MIN_PERIODS);
	BUG_ON(tdm->period_count > UNIPERIF_TDM_MAX_PERIODS);

	/* Calculate actual buffer size */
	tdm->buffer_act_sz = tdm->period_act_sz * tdm->period_count;

	/* Calculate padded buffer size (aligns buffer for each handset) */
	tdm->buffer_pad_sz = tdm->buffer_act_sz / tdm->info->handset_count;
	tdm->buffer_pad_sz += UNIPERIF_TDM_BUF_OFF_ALIGN;
	tdm->buffer_pad_sz &= ~UNIPERIF_TDM_BUF_OFF_ALIGN;
	tdm->buffer_pad_sz *= tdm->info->handset_count;
	BUG_ON(tdm->buffer_pad_sz > UNIPERIF_TDM_BUF_OFF_MAX);

	/* Calculate dma maxburst/trigger limit as a multiple of frame size */
	tdm->dma_maxburst = UNIPERIF_TDM_DMA_MAXBURST / tdm->info->frame_size;
	tdm->dma_maxburst *= tdm->info->frame_size;

	/*
	 * We can't use the standard core buffer functions as they associate
	 * with a given pcm device and substream. We are in essence sharing a
	 * single buffer with multiple pcm devices so need something special.
	 */

	tdm->buffer_bpa2 = bpa2_find_part(CONFIG_SND_STM_BPA2_PARTITION_NAME);
	if (!tdm->buffer_bpa2) {
		dev_err(&pdev->dev, "Failed to find BPA2 partition");
		result = -ENOMEM;
		goto error_bpa2_find;
	}
	dev_notice(&pdev->dev, "BPA2 '%s' at %p",
			CONFIG_SND_STM_BPA2_PARTITION_NAME, tdm->buffer_bpa2);

	/*
	 * This driver determines the number of periods and the period size in
	 * frames that the user application may use. As such we only need to
	 * allocate the dma buffer once.
	 */

	/* Round size up to the nearest page boundary */
	pages = (tdm->buffer_pad_sz + PAGE_SIZE - 1) / PAGE_SIZE;

	/* Allocate the physical pages for internal buffer */
	tdm->buffer_phys = bpa2_alloc_pages(tdm->buffer_bpa2, pages,
				0, GFP_KERNEL);
	if (!tdm->buffer_phys) {
		dev_err(tdm->dev, "Failed to allocate BPA2 pages");
		result = -ENOMEM;
		goto error_bpa2_alloc;
	}

	/* Map the physical pages to uncached memory */
	tdm->buffer_area = ioremap_nocache(tdm->buffer_phys,
				tdm->buffer_pad_sz);
	BUG_ON(!tdm->buffer_area);

	dev_notice(tdm->dev, "Buffer at %08x (phys)", tdm->buffer_phys);
	dev_notice(tdm->dev, "Buffer at %p (virt)", tdm->buffer_area);

	/* Process each handset to be supported */
	for (h = 0; h < tdm->info->handset_count; ++h) {
		/* Create a new pcm device for the handset */
		result = uniperif_tdm_handset_init(tdm, h);
		if (result) {
			dev_err(&pdev->dev, "Failed to init handset %d", h);
			goto error_handset_init;
		}
	}

	/* Register the telss sound card */
	result = snd_card_register(card);
	if (result) {
		dev_err(&pdev->dev, "Failed to register card (%d)", result);
		goto error_card_register;
	}

	platform_set_drvdata(pdev, tdm);

	return 0;

error_card_register:
error_handset_init:
	iounmap(tdm->buffer_area);
	bpa2_free_pages(tdm->buffer_bpa2, tdm->buffer_phys);
error_bpa2_alloc:
error_bpa2_find:
	snd_device_free(card, tdm);
error_device_new:
	if (tdm->pads)
		stm_pad_release(tdm->pads);

	return result;
}

static int __devexit uniperif_tdm_remove(struct platform_device *pdev)
{
	struct uniperif_tdm *tdm = platform_get_drvdata(pdev);

	dev_dbg(&pdev->dev, "%s(pdev=%p)\n", __func__, pdev);

	BUG_ON(!tdm);
	BUG_ON(!snd_stm_magic_valid(tdm));

	/* Unmap and free the internal buffer */
	iounmap(tdm->buffer_area);
	bpa2_free_pages(tdm->buffer_bpa2, tdm->buffer_phys);

	/* Release any claimed pads */
	if (tdm->pads)
		stm_pad_release(tdm->pads);

	snd_stm_magic_clear(tdm);

	return 0;
}


/*
 * Power management
 */

#ifdef CONFIG_PM
static int uniperif_tdm_suspend(struct device *dev)
{
	dev_dbg(dev, "%s(dev=%p)", __func__, dev);
	dev_err(dev, "PM not supported!");
	return -ENOTSUPP;
}

static int uniperif_tdm_resume(struct device *dev)
{
	dev_dbg(dev, "%s(dev=%p)", __func__, dev);
	dev_err(dev, "PM not supported!");
	return -ENOTSUPP;
}

static const struct dev_pm_ops uniperif_tdm_pm_ops = {
	.suspend = uniperif_tdm_suspend,
	.resume	 = uniperif_tdm_resume,
	.freeze	 = uniperif_tdm_suspend,
	.restore = uniperif_tdm_resume,
};
#endif


/*
 * Module initialisation.
 */

static struct platform_driver uniperif_tdm_driver = {
	.driver.name	= "snd_uniperif_tdm",
#ifdef CONFIG_PM
	.driver.pm	= &uniperif_tdm_pm_ops,
#endif
	.probe		= uniperif_tdm_probe,
	.remove		= uniperif_tdm_remove,
};

static int __init uniperif_tdm_init(void)
{
	return platform_driver_register(&uniperif_tdm_driver);
}

static void __exit uniperif_tdm_exit(void)
{
	platform_driver_unregister(&uniperif_tdm_driver);
}

module_init(uniperif_tdm_init);
module_exit(uniperif_tdm_exit);

MODULE_AUTHOR("John Boddie <john.boddie@st.com>");
MODULE_DESCRIPTION("STMicroelectronics Uniperipheral TDM driver");
MODULE_LICENSE("GPL");
