/*
 * STMicroelectronics FDMA dmaengine driver TELSS extensions
 *
 * Copyright (c) 2012 STMicroelectronics Limited
 *
 * Author: John Boddie <john.boddie@st.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/slab.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/dma-mapping.h>

#include <linux/stm/dma.h>

#include "stm_fdma.h"


/*
 * TELSS node control defines
 */

#define TELSS_CONTROL_REQ_MAP_MASK	0x0000001f
#define TELSS_CONTROL_REQ_MAP_EXT	0x0000001f
#define TELSS_CONTROL_TYPE_MASK		0x000000e0
#define TELSS_CONTROL_TYPE_TELSS	0x00000080
#define TELSS_CONTROL_DREQ_MASK		0x00001f00
#define TELSS_CONTROL_DREQ_SHIFT	8
#define TELSS_CONTROL_SECURE		0x00008000
#define TELSS_CONTROL_COMP_PAUSE	0x40000000
#define TELSS_CONTROL_COMP_IRQ		0x80000000


/*
 * TELSS node specific defines
 */

#define TELSS_PARAM_NUM_FRAMES_MAX	511
#define TELSS_PARAM_NUM_FRAMES_MASK	0x000001ff
#define TELSS_PARAM_NUM_FRAMES_SHIFT	0
#define TELSS_PARAM_WRDS_PER_FRM_MAX	31
#define TELSS_PARAM_WRDS_PER_FRM_MASK	0x00003e00
#define TELSS_PARAM_WRDS_PER_FRM_SHIFT	9
#define TELSS_PARAM_NUM_HANDSETS_MASK	0x0003c000
#define TELSS_PARAM_NUM_HANDSETS_SHIFT	14


/*
 * TELSS handset specific defines
 */

#define TELSS_HANDSET_BUF_OFF_MASK	0x00003fff
#define TELSS_HANDSET_BUF_OFF_SHIFT	0
#define TELSS_HANDSET_SLOT_1_ID_MASK	0x001fc000
#define TELSS_HANDSET_SLOT_1_ID_SHIFT	14
#define TELSS_HANDSET_SLOT_2_ID_MASK	0x0fe00000
#define TELSS_HANDSET_SLOT_2_ID_SHIFT	21
#define TELSS_HANDSET_SLOT_2_ID_VALID	0x10000000
#define TELSS_HANDSET_DUPLICATE_ENB	0x20000000
#define TELSS_HANDSET_DATA_LENGTH	0x40000000
#define TELSS_HANDSET_CALL_VALID	0x80000000


/*
 * TELSS parking buffer defines and structures
 */

struct stm_fdma_telss {
	u32 frame_count;
	u32 frame_size;
	u32 handset_count;
	u32 handset_params[STM_FDMA_LLU_TELSS_HANDSETS];
};


/*
 * TELSS dmaengine extension helper functions
 */

static void stm_fdma_telss_update_active(struct stm_fdma_chan *fchan,
		int handset, u32 params)
{
	dev_dbg(fchan->fdev->dev, "%s(fchan=%p, handset=%d, params=%08x)\n",
		__func__, fchan, handset, params);

	/* Update the currently active descriptor handset parameters */
	if (!list_empty(&fchan->desc_active)) {
		struct stm_fdma_desc *fdesc;
		struct stm_fdma_desc *child;

		/* Get the currently active decriptor */
		fdesc = list_first_entry(&fchan->desc_active,
				struct stm_fdma_desc, node);

		/* Update the first descriptor */
		fdesc->llu->telss.handset_param[handset] = params;

		/* Update the remaining descriptors */
		list_for_each_entry(child, &fdesc->llu_list, node) {
			child->llu->telss.handset_param[handset] = params;
		}
	}
}


/*
 * TELSS dmaengine extension resources functions
 */

int stm_fdma_telss_alloc_chan_resources(struct stm_fdma_chan *fchan)
{
	struct stm_dma_telss_config *config = fchan->dma_chan.private;
	struct stm_fdma_telss *telss;
	int result;

	dev_dbg(fchan->fdev->dev, "%s(fchan=%p)\n", __func__, fchan);

	BUG_ON(fchan->type != STM_DMA_TYPE_TELSS);

	/* Ensure frame count is within range 1-511 */
	if ((config->frame_count == 0) ||
		config->frame_count > TELSS_PARAM_NUM_FRAMES_MAX) {
		dev_err(fchan->fdev->dev, "Invalid telss frame count\n");
		return -EINVAL;
	}

	/* Ensure the frames size in words does not exceed maximum */
	if (config->frame_size > TELSS_PARAM_WRDS_PER_FRM_MAX) {
		dev_err(fchan->fdev->dev, "Invalid telss frame size\n");
		return -EINVAL;
	}

	/* Ensure the number of handsets is within range 1-10 */
	if ((config->handset_count == 0) ||
		(config->handset_count > STM_FDMA_LLU_TELSS_HANDSETS)) {
		dev_err(fchan->fdev->dev, "Invalid telss handset count\n");
		return -EINVAL;
	}

	/* Allocate the dreq */
	fchan->dreq = stm_fdma_dreq_alloc(fchan, &config->dreq_config);
	if (!fchan->dreq) {
		dev_err(fchan->fdev->dev, "Failed to allocate telss dreq\n");
		return -EBUSY;
	}

	/* Configure the dreq */
	result = stm_fdma_dreq_config(fchan, fchan->dreq);
	if (result) {
		dev_err(fchan->fdev->dev, "Failed to configure telss dreq\n");
		goto error_dreq_config;
	}

	/* Set the initial dma address */
	fchan->dma_addr = config->dma_addr;

	/* Allocate the telss channel structure */
	telss = kzalloc(sizeof(struct stm_fdma_telss), GFP_KERNEL);
	if (!telss) {
		dev_err(fchan->fdev->dev, "Failed to alloc telss structure\n");
		result = -ENOMEM;
		goto error_kzalloc;
	}

	/* Save all the telss configuration in the one place */
	telss->frame_count = config->frame_count;
	telss->frame_size = config->frame_size;
	telss->handset_count = config->handset_count;

	/* Save telss extensions structure in the channel */
	fchan->extension = telss;

	return 0;

error_kzalloc:
error_dreq_config:
	stm_fdma_dreq_free(fchan, fchan->dreq);
	return result;
}

void stm_fdma_telss_free_chan_resources(struct stm_fdma_chan *fchan)
{
	BUG_ON(fchan->type != STM_DMA_TYPE_TELSS);

	dev_dbg(fchan->fdev->dev, "%s(fchan=%p)\n", __func__, fchan);

	BUG_ON(fchan->type != STM_DMA_TYPE_TELSS);

	/* Free the telss data (kfree handles null arg) */
	kfree(fchan->extension);
	fchan->extension = NULL;

	/* Free the dreq */
	stm_fdma_dreq_free(fchan, fchan->dreq);
}


/*
 * TELSS dmaengine extensions API
 */

int dma_telss_handset_config(struct dma_chan *chan, int handset,
		struct stm_dma_telss_handset_config *config)
{
	struct stm_fdma_chan *fchan = to_stm_fdma_chan(chan);
	struct stm_fdma_telss *telss = fchan->extension;
	unsigned long irqflags = 0;

	dev_dbg(fchan->fdev->dev, "%s(fchan=%p, handset=%d, config=%p)\n",
			__func__, fchan, handset, config);

	BUG_ON(fchan->type != STM_DMA_TYPE_TELSS);
	BUG_ON(!telss);

	/* Ensure handset is within the supported range */
	if ((handset < 0) || (handset >= telss->handset_count)) {
		dev_err(fchan->fdev->dev, "Invalid handset (%d) (range 0-%d)\n",
				handset, telss->handset_count);
	}

	/* Ensure that duplicate and second slot are not both enabled */
	if (config->second_slot_id_valid && config->duplicate_enable) {
		dev_err(fchan->fdev->dev, "Cannot have a valid second slot ID"
				"and duplicate enabled");
		return -EINVAL;
	}

	spin_lock_irqsave(&fchan->lock, irqflags);

	/* Set the buffer offset */
	telss->handset_params[handset] &= ~TELSS_HANDSET_BUF_OFF_MASK;
	telss->handset_params[handset] |=
			config->buffer_offset << TELSS_HANDSET_BUF_OFF_SHIFT;

	/* Set the first slot id */
	telss->handset_params[handset] &= ~TELSS_HANDSET_SLOT_1_ID_MASK;
	telss->handset_params[handset] |=
		config->first_slot_id << TELSS_HANDSET_SLOT_1_ID_SHIFT;

	/* Set the second slot id */
	telss->handset_params[handset] &= ~TELSS_HANDSET_SLOT_2_ID_MASK;
	telss->handset_params[handset] |=
		config->second_slot_id << TELSS_HANDSET_SLOT_2_ID_SHIFT;

	/* Set the second slot id valid bit */
	telss->handset_params[handset] &= ~TELSS_HANDSET_SLOT_2_ID_VALID;
	if (config->second_slot_id_valid)
		telss->handset_params[handset] |= TELSS_HANDSET_SLOT_2_ID_VALID;

	/* Set the duplicate enable bit */
	telss->handset_params[handset] &= ~TELSS_HANDSET_DUPLICATE_ENB;
	if (config->duplicate_enable)
		telss->handset_params[handset] |= TELSS_HANDSET_DUPLICATE_ENB;

	/* Set the data length bit */
	telss->handset_params[handset] &= ~TELSS_HANDSET_DATA_LENGTH;
	if (config->data_length)
		telss->handset_params[handset] |= TELSS_HANDSET_DATA_LENGTH;

	/* Set the call valid bit */
	telss->handset_params[handset] &= ~TELSS_HANDSET_CALL_VALID;
	if (config->call_valid)
		telss->handset_params[handset] |= TELSS_HANDSET_CALL_VALID;

	/* Update any active transfers */
	stm_fdma_telss_update_active(fchan, handset,
			telss->handset_params[handset]);

	spin_unlock_irqrestore(&fchan->lock, irqflags);

	return 0;
}
EXPORT_SYMBOL(dma_telss_handset_config);

int dma_telss_handset_control(struct dma_chan *chan, int handset, int valid)
{
	struct stm_fdma_chan *fchan = to_stm_fdma_chan(chan);
	struct stm_fdma_telss *telss = fchan->extension;
	unsigned long irqflags = 0;

	dev_dbg(fchan->fdev->dev, "%s(fchan=%p, handset=%d, active=%d)\n",
			__func__, fchan, handset, valid);

	BUG_ON(fchan->type != STM_DMA_TYPE_TELSS);
	BUG_ON(!telss);

	/* Ensure handset is within the supported range */
	if ((handset < 0) || (handset >= telss->handset_count)) {
		dev_err(fchan->fdev->dev, "Invalid handset (%d) (range 0-%d)\n",
				handset, telss->handset_count);
	}

	spin_lock_irqsave(&fchan->lock, irqflags);

	/* Update the call valid bit in the cached handset parameters */
	telss->handset_params[handset] &= ~TELSS_HANDSET_CALL_VALID;
	if (valid)
		telss->handset_params[handset] |= TELSS_HANDSET_CALL_VALID;

	/* Update any active transfers */
	stm_fdma_telss_update_active(fchan, handset,
			telss->handset_params[handset]);

	spin_unlock_irqrestore(&fchan->lock, irqflags);

	return 0;
}
EXPORT_SYMBOL(dma_telss_handset_control);

struct dma_async_tx_descriptor *dma_telss_prep_dma_cyclic(
		struct dma_chan *chan, dma_addr_t buf_addr, size_t buf_len,
		size_t period_len, size_t period_stride,
		enum dma_transfer_direction direction)
{
	struct stm_fdma_chan *fchan = to_stm_fdma_chan(chan);
	struct stm_fdma_telss *telss = fchan->extension;
	struct stm_fdma_desc *head = NULL;
	struct stm_fdma_desc *prev = NULL;
	int result;
	int p, h;
	u32 tmp;

	dev_dbg(fchan->fdev->dev, "%s(chan=%p, buf_addr=%08x, buf_len=%d,"
			"period_len=%d, period_stride=%d, direction=%d)\n",
			__func__, chan, buf_addr, buf_len, period_len,
			period_stride, direction);

	/* Only allow this function on telss channels */
	BUG_ON(fchan->type != STM_DMA_TYPE_TELSS);
	BUG_ON(!telss);

	/* The slave must be configured! */
	if (!fchan->dma_addr) {
		dev_err(fchan->fdev->dev, "Slave not configured!\n");
		return NULL;
	}

	/* Only a single transfer allowed as channel is cyclic */
	result = test_and_set_bit(STM_FDMA_IS_CYCLIC, &fchan->flags);
	if (result) {
		dev_err(fchan->fdev->dev, "Channel %d already in use\n",
				fchan->id);
		return NULL;
	}

	/* Ensure we have dreq information */
	BUG_ON(!fchan->dreq);

	/* Build a cyclic linked list */
	for (p = 0; p < (buf_len / period_len); ++p) {
		/* Allocate a descriptor for this period */
		struct stm_fdma_desc *fdesc = stm_fdma_desc_get(fchan);
		if (!fdesc) {
			dev_err(fchan->fdev->dev, "Not enough decriptors\n");
			goto error_desc_get;
		}

		/* Configure the desciptor llu */
		fdesc->llu->next = 0;
		fdesc->llu->control = TELSS_CONTROL_REQ_MAP_EXT;
		fdesc->llu->control |= TELSS_CONTROL_TYPE_TELSS;
		fdesc->llu->control |= fchan->dreq->request_line <<
				TELSS_CONTROL_DREQ_SHIFT;
		fdesc->llu->control |= TELSS_CONTROL_COMP_IRQ;

		fdesc->llu->nbytes = period_len;

		switch (direction) {
		case DMA_DEV_TO_MEM:
			fdesc->llu->saddr = fchan->dma_addr;
			fdesc->llu->daddr = buf_addr + (p * period_stride);
			break;

		case DMA_MEM_TO_DEV:
			fdesc->llu->saddr = buf_addr + (p * period_stride);
			fdesc->llu->daddr = fchan->dma_addr;
			break;

		default:
			dev_err(fchan->fdev->dev, "Invalid direction!\n");
			goto error_direction;
		}

		/* Configure the telss node specific parameters */
		tmp = telss->frame_count & TELSS_PARAM_NUM_FRAMES_MASK;
		fdesc->llu->telss.node_param = tmp;

		tmp = telss->frame_size << TELSS_PARAM_WRDS_PER_FRM_SHIFT;
		tmp &= TELSS_PARAM_WRDS_PER_FRM_MASK;
		fdesc->llu->telss.node_param |= tmp;

		tmp = telss->handset_count << TELSS_PARAM_NUM_HANDSETS_SHIFT;
		tmp &= TELSS_PARAM_NUM_HANDSETS_MASK;
		fdesc->llu->telss.node_param |= tmp;

		/* Configure the telss handset specific parameters */
		for (h = 0; h < telss->handset_count; ++h) {
			fdesc->llu->telss.handset_param[h] =
					telss->handset_params[h];
		}

		/* Add the descriptor to the chain */
		stm_fdma_desc_chain(&head, &prev, fdesc);
	}

	/* Ensure last llu points to first llu */
	prev->llu->next = head->dma_desc.phys;

	/* Set first descriptor of chain cookie to error */
	head->dma_desc.cookie = -EBUSY;

	return &head->dma_desc;

error_direction:
error_desc_get:
	stm_fdma_desc_put(head);
	clear_bit(STM_FDMA_IS_CYCLIC, &fchan->flags);
	return NULL;
}
EXPORT_SYMBOL(dma_telss_prep_dma_cyclic);

int dma_telss_get_period(struct dma_chan *chan)
{
	struct stm_fdma_chan *fchan = to_stm_fdma_chan(chan);
	struct stm_fdma_telss *telss = fchan->extension;
	int period = 0;

	dev_dbg(fchan->fdev->dev, "%s(chan=%p)\n", __func__, chan);

	/* Only allow this function on telss channels */
	BUG_ON(fchan->type != STM_DMA_TYPE_TELSS);
	BUG_ON(!telss);

	/* Only attempt to get residue on a non-idle channel */
	if (fchan->state != STM_FDMA_STATE_IDLE) {
		unsigned long stat1, stat2;
		struct stm_fdma_desc *fdesc, *child;
		unsigned long phys_node;

		/* Stop thread being preempted in case removing descriptor */
		preempt_disable();

		/* Loop until sure we are not transitioning a node */
		do {
			stat1 = readl(CMD_STAT_REG(fchan));
			stat2 = readl(CMD_STAT_REG(fchan));
		} while (stat1 != stat2);

		/* Convert the status to the physical node pointer address */
		phys_node = stat1 & CMD_STAT_DATA_MASK;

		/* Get the active descriptor */
		fdesc = list_first_entry(&fchan->desc_active,
				struct stm_fdma_desc, node);

		/* Does the physical node match the first descriptor node? */
		if (phys_node != fdesc->dma_desc.phys) {
			/* Loop through all descriptor child nodes */
			list_for_each_entry(child, &fdesc->llu_list, node) {
				period++;

				/* Does physical node match child? */
				if (phys_node == child->dma_desc.phys)
					break;
			}
		}

		/* Restore preemption */
		preempt_enable();
	}

	return period;
}
EXPORT_SYMBOL(dma_telss_get_period);
