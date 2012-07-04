/*
 * STMicroelectronics FDMA dmaengine driver
 *
 * Copyright (c) 2012 STMicroelectronics Limited
 *
 * Author: John Boddie <john.boddie@st.com>
 *
 * This code borrows heavily from drivers/stm/fdma.c!
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/device.h>
#include <linux/dmapool.h>
#include <linux/platform_device.h>

#include <linux/stm/platform.h>
#include <linux/stm/clk.h>
#include <linux/stm/dma.h>

#include "stm_fdma.h"


/*
 * Interrupt functions
 */

static void stm_fdma_tasklet_error(unsigned long data)
{
	struct stm_fdma_chan *fchan = (struct stm_fdma_chan *) data;
	struct stm_fdma_desc *fdesc;
	unsigned long irqflags = 0;

	spin_lock_irqsave(&fchan->lock, irqflags);

	BUG_ON(list_empty(&fchan->desc_active));

	/* Remove the first descriptor from the active list */
	fdesc = list_first_entry(&fchan->desc_active, struct stm_fdma_desc,
			node);
	list_del_init(&fdesc->node);

	/* Any cyclic channel is no longer cyclic after a descriptor error! */
	clear_bit(STM_FDMA_IS_CYCLIC, &fchan->flags);

	spin_unlock_irqrestore(&fchan->lock, irqflags);

	/* Dump the error */
	dev_err(fchan->fdev->dev, "Error: channel %d, descriptor %d\n",
			fchan->id, fdesc->dma_desc.cookie);

	/* Start the next descriptor */
	stm_fdma_desc_start(fchan);

	/* DUMP THE DESCRIPTOR */

	/* Complete the descriptor */
	stm_fdma_desc_complete(fchan, fdesc);
}

static inline void stm_fdma_irq_error(struct stm_fdma_chan *fchan)
{
	/*
	 * FDMA spec states, in case of error transfer "may be aborted". Let's
	 * make the behaviour explicit and stop the transfer here.
	 */

	spin_lock(&fchan->lock);

	stm_fdma_hw_channel_pause(fchan, 0);
	fchan->state = STM_FDMA_STATE_STOPPING;

	spin_unlock(&fchan->lock);

	/* Schedule channel tasklet to handle completion */
	tasklet_schedule(&fchan->tasklet_error);
}

static void stm_fdma_tasklet_complete(unsigned long data)
{
	struct stm_fdma_chan *fchan = (struct stm_fdma_chan *) data;
	struct stm_fdma_desc *fdesc;
	unsigned long irqflags = 0;

	spin_lock_irqsave(&fchan->lock, irqflags);

	/* Terminate all may result in a completion when active list empty */
	if (list_empty(&fchan->desc_active)) {
		spin_unlock_irqrestore(&fchan->lock, irqflags);
		return;
	}

	/* Get the head of the active list */
	fdesc = list_first_entry(&fchan->desc_active, struct stm_fdma_desc,
			node);

	if (test_bit(STM_FDMA_IS_CYCLIC, &fchan->flags)) {
		/* Assume end of period and issue callback */
		struct dma_async_tx_descriptor *desc = &fdesc->dma_desc;

		spin_unlock_irqrestore(&fchan->lock, irqflags);

		if (desc->callback)
			desc->callback(desc->callback_param);
	} else {
		spin_unlock_irqrestore(&fchan->lock, irqflags);

		/* Complete the descriptor */
		stm_fdma_desc_complete(fchan, fdesc);

		/* Start the next descriptor */
		stm_fdma_desc_start(fchan);
	}
}

static inline void stm_fdma_irq_complete(struct stm_fdma_chan *fchan)
{
	spin_lock(&fchan->lock);

	switch (stm_fdma_hw_channel_status(fchan)) {
	case CMD_STAT_STATUS_PAUSED:
		switch (fchan->state) {
		case STM_FDMA_STATE_RUNNING:	/* Hit a pause node */
		case STM_FDMA_STATE_PAUSING:
			fchan->state = STM_FDMA_STATE_PAUSED;
			break;

		case STM_FDMA_STATE_STOPPING:
			stm_fdma_hw_channel_reset(fchan);
			fchan->state = STM_FDMA_STATE_IDLE;
			break;

		default:
			dev_err(fchan->fdev->dev, "Invalid state transition\n");
			BUG();
		}
		break;

	case CMD_STAT_STATUS_IDLE:
		switch (fchan->state) {
		case STM_FDMA_STATE_RUNNING:
		case STM_FDMA_STATE_PAUSING:
		case STM_FDMA_STATE_STOPPING:
			fchan->state = STM_FDMA_STATE_IDLE;
			break;

		default:
			dev_err(fchan->fdev->dev, "Invalid state transition\n");
			BUG();
		}
		break;

	case CMD_STAT_STATUS_RUNNING:
		break;

	default:
		dev_err(fchan->fdev->dev, "Invalid FDMA channel status\n");
		BUG();
	}

	spin_unlock(&fchan->lock);

	/* Run channel tasklet to handle completion */
	tasklet_schedule(&fchan->tasklet_complete);
}

static irqreturn_t stm_fdma_irq_handler(int irq, void *dev_id)
{
	struct stm_fdma_device *fdev = dev_id;
	irqreturn_t result = IRQ_NONE;
	u32 status;
	int c;

	/* Read and immediately clear the interrupt status */
	status = readl(fdev->io_base + fdev->regs.int_sta);
	writel(status, fdev->io_base + fdev->regs.int_clr);

	/* Process each channel raising an interrupt */
	for (c = STM_FDMA_MIN_CHANNEL; status != 0; status >>= 2, ++c) {
		/*
		 * On error both interrupts raised, so check for error first.
		 *
		 * When switching to the parking buffer we set each node of the
		 * currently active descriptor to interrupt on complete. This
		 * results in missed interrupt error. We suppress this error
		 * here and handle the interrupt as a normal completion.
		 */
		if (unlikely(status & 2)) {
			int ignore = 0;

			/* If parked, suppress any missed interrupt error */
			if (test_bit(STM_FDMA_IS_PARKED,
					&fdev->ch_list[c].flags))
				if (stm_fdma_hw_channel_error(
						&fdev->ch_list[c]) ==
						CMD_STAT_ERROR_INTR)
					ignore = 1;

			/* Only handle error if not suppressing it */
			if (!ignore) {
				stm_fdma_irq_error(&fdev->ch_list[c]);
				result = IRQ_HANDLED;
				continue;
			}
		}

		if (status & 1) {
			stm_fdma_irq_complete(&fdev->ch_list[c]);
			result = IRQ_HANDLED;
		}
	}

	return result;
}



/*
 * Clock functions.
 */

static int stm_fdma_clk_get(struct stm_fdma_device *fdev)
{
	char *clks[STM_FDMA_CLKS] = {
		"fdma_slim_clk",
		"fdma_hi_clk",
		"fdma_low_clk",
		"fdma_ic_clk"
	};
	int i;

	for (i = 0; i < STM_FDMA_CLKS; ++i) {
		struct clk *clk = devm_clk_get(fdev->dev, clks[i]);
		if (IS_ERR(clk)) {
			dev_err(fdev->dev, "Failed to get clock %s "
				"(error=%d)\n", clks[i], (int) clk);
			return (int) clk;
		}

		fdev->clks[i] = clk;
	}

	return 0;
}

static int stm_fdma_clk_enable(struct stm_fdma_device *fdev)
{
	int i;

	for (i = 0; i < STM_FDMA_CLKS; ++i) {
		if (fdev->clks[i]) {
			int result = clk_enable(fdev->clks[i]);
			if (result)
				return result;
		}
	}

	return 0;
}

static void stm_fdma_clk_disable(struct stm_fdma_device *fdev)
{
	int i;

	for (i = 0; i < STM_FDMA_CLKS; ++i) {
		if (fdev->clks[i])
			clk_disable(fdev->clks[i]);
	}
}


/*
 * dmaengine callback functions
 */

dma_cookie_t stm_fdma_tx_submit(struct dma_async_tx_descriptor *desc)
{
	struct stm_fdma_desc *fdesc = to_stm_fdma_desc(desc);
	struct stm_fdma_chan *fchan = fdesc->fchan;
	unsigned long irqflags = 0;

	dev_dbg(fchan->fdev->dev, "%s(desc=%p)\n", __func__, desc);

	spin_lock_irqsave(&fchan->lock, irqflags);

	/* Assign descriptor next positive cookie */
	if (++fchan->dma_chan.cookie < 0)
		fchan->dma_chan.cookie = DMA_MIN_COOKIE;

	desc->cookie = fchan->dma_chan.cookie;

	/* Queue the descriptor */
	list_add_tail(&fdesc->node, &fchan->desc_queue);

	spin_unlock_irqrestore(&fchan->lock, irqflags);

	/* Attempt to start the next available descriptor */
	stm_fdma_desc_start(fchan);

	return desc->cookie;
}

static int stm_fdma_alloc_chan_resources(struct dma_chan *chan)
{
	struct stm_fdma_chan *fchan = to_stm_fdma_chan(chan);
	struct stm_fdma_device *fdev = fchan->fdev;
	struct stm_dma_paced_config *paced;
	unsigned long irqflags = 0;
	int result;

	dev_dbg(fdev->dev, "%s(chan=%p)\n", __func__, chan);

	/* Ensure firmware has loaded */
	if (stm_fdma_fw_check(fchan->fdev)) {
		dev_err(fchan->fdev->dev, "Firmware not loaded!\n");
		return -ENODEV;
	}

	spin_lock_irqsave(&fchan->lock, irqflags);

	/* Set the channel type */
	if (chan->private)
		fchan->type = ((struct stm_dma_config *) chan->private)->type;
	else
		fchan->type = STM_DMA_TYPE_FREE_RUNNING;

	/* Perform and channel specific configuration */
	switch (fchan->type) {
	case STM_DMA_TYPE_FREE_RUNNING:
		fchan->dma_addr = 0;
		result = 0;
		break;

	case STM_DMA_TYPE_PACED:
		paced = chan->private;
		fchan->dma_addr = paced->dma_addr;
		/* Allocate the dreq */
		fchan->dreq = stm_fdma_dreq_alloc(fchan, &paced->dreq_config);
		if (!fchan->dreq) {
			dev_err(fdev->dev, "Failed to configure paced dreq\n");
			result = -EINVAL;
			goto error;
		}
		/* Configure the dreq */
		result = stm_fdma_dreq_config(fchan, fchan->dreq);
		if (result) {
			dev_err(fdev->dev, "Failed to set paced dreq\n");
			stm_fdma_dreq_free(fchan, fchan->dreq);
			goto error;
		}
		break;

	case STM_DMA_TYPE_AUDIO:
		result = stm_fdma_audio_alloc_chan_resources(fchan);
		if (result) {
			dev_err(fdev->dev, "Failed to alloc audio resources\n");
			goto error;
		}
		break;

	case STM_DMA_TYPE_TELSS:
		result = stm_fdma_telss_alloc_chan_resources(fchan);
		if (result) {
			dev_err(fdev->dev, "Failed to alloc telss resources\n");
			goto error;
		}
		break;

	default:
		dev_err(fchan->fdev->dev, "Invalid channel type (%d)\n",
				fchan->type);
		result = -EINVAL;
		goto error;
	}

	/* Allocate descriptors */
	while (fchan->desc_count < STM_FDMA_DESCRIPTORS) {
		struct stm_fdma_desc *fdesc;

		spin_unlock_irqrestore(&fchan->lock, irqflags);

		/* Allocate a new descriptor */
		fdesc = stm_fdma_desc_alloc(fchan);
		if (!fdesc) {
			dev_err(fdev->dev, "Failed to allocate desc\n");
			break;
		}

		spin_lock_irqsave(&fchan->lock, irqflags);

		/* Add descriptor to the free list and increment count */
		list_add_tail(&fdesc->node, &fchan->desc_free);
		fchan->desc_count++;
	}

	spin_unlock_irqrestore(&fchan->lock, irqflags);


	return fchan->desc_count;

error:
	spin_unlock_irqrestore(&fchan->lock, irqflags);
	return result;
}

static void stm_fdma_free_chan_resources(struct dma_chan *chan)
{
	struct stm_fdma_chan *fchan = to_stm_fdma_chan(chan);
	struct stm_fdma_desc *fdesc, *_fdesc;
	unsigned long irqflags = 0;
	LIST_HEAD(list);

	dev_dbg(fchan->fdev->dev, "%s(chan=%p)\n", __func__, chan);

	/*
	 * Channel must not be running and there must be no active or queued
	 * descriptors. We cannot check for being idle as on PM suspend we may
	 * never get the transition from stopping to idle!
	 */
	BUG_ON(fchan->state == STM_FDMA_STATE_RUNNING);
	BUG_ON(!list_empty(&fchan->desc_queue));
	BUG_ON(!list_empty(&fchan->desc_active));

	spin_lock_irqsave(&fchan->lock, irqflags);

	/* Free all allocated transfer descriptors */
	list_for_each_entry_safe(fdesc, _fdesc, &fchan->desc_free, node) {
		list_del(&fdesc->node);
		stm_fdma_desc_free(fdesc);
	}

	/* Re-initialise the descriptor count and the free list */
	fchan->desc_count = 0;
	INIT_LIST_HEAD(&fchan->desc_free);

	spin_unlock_irqrestore(&fchan->lock, irqflags);

	/* Perform any channel configuration clean up */
	switch (fchan->type) {
	case STM_DMA_TYPE_FREE_RUNNING:
		break;

	case STM_DMA_TYPE_PACED:
		stm_fdma_dreq_free(fchan, fchan->dreq);
		break;

	case STM_DMA_TYPE_AUDIO:
		stm_fdma_audio_free_chan_resources(fchan);
		break;

	case STM_DMA_TYPE_TELSS:
		stm_fdma_telss_free_chan_resources(fchan);
		break;

	default:
		dev_err(fchan->fdev->dev, "Invalid channel type (%d)\n",
				fchan->type);
	}
}

static struct dma_async_tx_descriptor *stm_fdma_prep_dma_memcpy(
		struct dma_chan *chan, dma_addr_t dest, dma_addr_t src,
		size_t len, unsigned long flags)
{
	struct stm_fdma_chan *fchan = to_stm_fdma_chan(chan);
	struct stm_fdma_desc *fdesc;
	int result;

	dev_dbg(fchan->fdev->dev, "%s(chan=%p, dest=%08x, src=%08x, len=%d, fl"
			"ags=%08lx)\n", __func__, chan, dest, src, len, flags);

	/* Check parameters */
	if (len == 0) {
		dev_err(fchan->fdev->dev, "Invalid length\n");
		return NULL;
	}

	/* Only a single transfer allowed on cyclic channel */
	result = test_bit(STM_FDMA_IS_CYCLIC, &fchan->flags);
	if (result) {
		dev_err(fchan->fdev->dev, "Channel %d already in use for "
				"cyclic transfers!\n", fchan->id);
		return NULL;
	}

	/* Get a descriptor */
	fdesc = stm_fdma_desc_get(fchan);
	if (!fdesc) {
		dev_err(fchan->fdev->dev, "Not enough decriptors!\n");
		return NULL;
	}

	/* We only require a single descriptor */
	fdesc->llu->next = 0;
	/* Configure as free running with incrementing src/dst  */
	fdesc->llu->control = NODE_CONTROL_REQ_MAP_FREE_RUN;
	fdesc->llu->control |= NODE_CONTROL_SRC_INCR;
	fdesc->llu->control |= NODE_CONTROL_DST_INCR;
	fdesc->llu->control |= NODE_CONTROL_COMP_IRQ;
	/* Set the number of bytes to transfer */
	fdesc->llu->nbytes = len;
	/* Set the src/dst addresses */
	fdesc->llu->saddr = src;
	fdesc->llu->daddr = dest;
	/* Configure for 1D data */
	fdesc->llu->generic.length = len;
	fdesc->llu->generic.sstride = 0;
	fdesc->llu->generic.dstride = 0;

	/* Set descriptor cookie to error and save flags */
	fdesc->dma_desc.cookie = -EBUSY;
	fdesc->dma_desc.flags = flags;

	return &fdesc->dma_desc;
}

static struct dma_async_tx_descriptor *stm_fdma_prep_slave_sg(
		struct dma_chan *chan, struct scatterlist *sgl,
		unsigned int sg_len, enum dma_transfer_direction direction,
		unsigned long flags)
{
	struct stm_fdma_chan *fchan = to_stm_fdma_chan(chan);
	struct stm_fdma_desc *head = NULL;
	struct stm_fdma_desc *prev = NULL;
	struct scatterlist *sg;
	int result;
	int i;

	dev_dbg(fchan->fdev->dev, "%s(chan=%p, sgl=%p, sg_len=%d,"
			"direction=%d, flags=%08lx)\n", __func__, chan,
			sgl, sg_len, direction, flags);

	/* The slave must be configured! */
	if (!fchan->dma_addr) {
		dev_err(fchan->fdev->dev, "Slave not configured!\n");
		return NULL;
	}

	/* Only a single transfer allowed on cyclic channel */
	result = test_bit(STM_FDMA_IS_CYCLIC, &fchan->flags);
	if (result) {
		dev_err(fchan->fdev->dev, "Channel %d already in use for "
				"cyclic transfers!\n", fchan->id);
		return NULL;
	}

	/* Build a linked list */
	for_each_sg(sgl, sg, sg_len, i) {
		/* Allocate a descriptor for this node */
		struct stm_fdma_desc *fdesc = stm_fdma_desc_get(fchan);
		if (!fdesc) {
			dev_err(fchan->fdev->dev, "Not enough decriptors\n");
			goto error_desc_get;
		}

		/* Configure the desciptor llu */
		fdesc->llu->next = 0;

		switch (fchan->type) {
		case STM_DMA_TYPE_FREE_RUNNING:
			fdesc->llu->control = NODE_CONTROL_REQ_MAP_FREE_RUN;
			break;

		case STM_DMA_TYPE_PACED:
		case STM_DMA_TYPE_AUDIO:
			fdesc->llu->control = fchan->dreq->request_line;
			break;

		default:
			dev_err(fchan->fdev->dev, "Invalid channel type!\n");
			goto error_chan_type;
		}

		fdesc->llu->nbytes = sg_dma_len(sg);

		switch (direction) {
		case DMA_DEV_TO_MEM:
			fdesc->llu->control |= NODE_CONTROL_SRC_STATIC;
			fdesc->llu->control |= NODE_CONTROL_DST_INCR;
			fdesc->llu->saddr = fchan->dma_addr;
			fdesc->llu->daddr = sg_dma_address(sg);
			break;

		case DMA_MEM_TO_DEV:
			fdesc->llu->control |= NODE_CONTROL_SRC_INCR;
			fdesc->llu->control |= NODE_CONTROL_DST_STATIC;
			fdesc->llu->saddr = sg_dma_address(sg);
			fdesc->llu->daddr = fchan->dma_addr;
			break;

		default:
			dev_err(fchan->fdev->dev, "Invalid direction!\n");
			goto error_direction;
		}

		fdesc->llu->generic.length = sg_dma_len(sg);
		fdesc->llu->generic.sstride = 0;
		fdesc->llu->generic.dstride = 0;

		/* Add the descriptor to the chain */
		stm_fdma_desc_chain(&head, &prev, fdesc);
	}

	/* Set the last descriptor to generate an interrupt on completion */
	prev->llu->control |= NODE_CONTROL_COMP_IRQ;

	/* Set first descriptor of chain cookie to error and save flags */
	head->dma_desc.cookie = -EBUSY;
	head->dma_desc.flags = flags;

	return &head->dma_desc;

error_direction:
error_chan_type:
error_desc_get:
	stm_fdma_desc_put(head);
	return NULL;
}

static struct dma_async_tx_descriptor *stm_fdma_prep_dma_cyclic(
		struct dma_chan *chan, dma_addr_t buf_addr, size_t buf_len,
		size_t period_len, enum dma_transfer_direction direction)
{
	struct stm_fdma_chan *fchan = to_stm_fdma_chan(chan);
	struct stm_fdma_desc *head = NULL;
	struct stm_fdma_desc *prev = NULL;
	int result;
	int p;

	dev_dbg(fchan->fdev->dev, "%s(chan=%p, buf_addr=%08x, buf_len=%d,"
			"period_len=%d, direction=%d)\n", __func__, chan,
			buf_addr, buf_len, period_len, direction);

	/* The slave must be configured! */
	if (!fchan->dma_addr) {
		dev_err(fchan->fdev->dev, "Slave not configured!\n");
		return NULL;
	}

	/* Only a single transfer allowed on cyclic channel */
	result = test_and_set_bit(STM_FDMA_IS_CYCLIC, &fchan->flags);
	if (result) {
		dev_err(fchan->fdev->dev, "Channel %d already in use!\n",
				fchan->id);
		return NULL;
	}

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

		switch (fchan->type) {
		case STM_DMA_TYPE_FREE_RUNNING:
			fdesc->llu->control = NODE_CONTROL_REQ_MAP_FREE_RUN;
			break;

		case STM_DMA_TYPE_PACED:
		case STM_DMA_TYPE_AUDIO:
			fdesc->llu->control = fchan->dreq->request_line;
			break;

		default:
			dev_err(fchan->fdev->dev, "Invalid channel type!\n");
			goto error_chan_type;
		}

		fdesc->llu->control |= NODE_CONTROL_COMP_IRQ;

		fdesc->llu->nbytes = period_len;

		switch (direction) {
		case DMA_DEV_TO_MEM:
			fdesc->llu->control |= NODE_CONTROL_SRC_STATIC;
			fdesc->llu->control |= NODE_CONTROL_DST_INCR;
			fdesc->llu->saddr = fchan->dma_addr;
			fdesc->llu->daddr = buf_addr + (p * period_len);
			break;

		case DMA_MEM_TO_DEV:
			fdesc->llu->control |= NODE_CONTROL_SRC_INCR;
			fdesc->llu->control |= NODE_CONTROL_DST_STATIC;
			fdesc->llu->saddr = buf_addr + (p * period_len);
			fdesc->llu->daddr = fchan->dma_addr;
			break;

		default:
			dev_err(fchan->fdev->dev, "Invalid direction!\n");
			goto error_direction;
		}

		fdesc->llu->generic.length = period_len; /* 1D: node bytes */
		fdesc->llu->generic.sstride = 0;
		fdesc->llu->generic.dstride = 0;

		/* Add the descriptor to the chain */
		stm_fdma_desc_chain(&head, &prev, fdesc);
	}

	/* Ensure last llu points to first llu */
	prev->llu->next = head->dma_desc.phys;

	/* Set first descriptor of chain cookie to error */
	head->dma_desc.cookie = -EBUSY;

	return &head->dma_desc;

error_direction:
error_chan_type:
error_desc_get:
	stm_fdma_desc_put(head);
	clear_bit(STM_FDMA_IS_CYCLIC, &fchan->flags);
	return NULL;
}

static int stm_fdma_pause(struct stm_fdma_chan *fchan)
{
	unsigned long irqflags = 0;

	spin_lock_irqsave(&fchan->lock, irqflags);

	switch (fchan->state) {
	case STM_FDMA_STATE_IDLE:
		/* Hardware isn't set up yet, so treat this as an error */
		spin_unlock_irqrestore(&fchan->lock, irqflags);
		return -EBUSY;

	case STM_FDMA_STATE_PAUSED:
		/* Hardware is already paused */
		break;

	case STM_FDMA_STATE_RUNNING:
		/* Hardware is running, send the command */
		stm_fdma_hw_channel_pause(fchan, 0);
		/* Fall through */

	case STM_FDMA_STATE_PAUSING:
	case STM_FDMA_STATE_STOPPING:
		/* Hardware is pausing already, wait for interrupt */
		fchan->state = STM_FDMA_STATE_PAUSING;
		break;
	}

	spin_unlock_irqrestore(&fchan->lock, irqflags);

	return 0;
}

static int stm_fdma_resume(struct stm_fdma_chan *fchan)
{
	unsigned long irqflags = 0;

	spin_lock_irqsave(&fchan->lock, irqflags);

	if (fchan->state != STM_FDMA_STATE_PAUSED) {
		spin_unlock_irqrestore(&fchan->lock, irqflags);
		return -EBUSY;
	}

	stm_fdma_hw_channel_resume(fchan);

	fchan->state = STM_FDMA_STATE_RUNNING;

	spin_unlock_irqrestore(&fchan->lock, irqflags);

	return 0;
}

static void stm_fdma_stop(struct stm_fdma_chan *fchan)
{
	unsigned long irqflags = 0;

	dev_dbg(fchan->fdev->dev, "%s(fchan=%p)\n", __func__, fchan);

	spin_lock_irqsave(&fchan->lock, irqflags);

	switch (fchan->state) {
	case STM_FDMA_STATE_IDLE:
	case STM_FDMA_STATE_PAUSED:
		/* Channel is idle - just change state */
		fchan->state = STM_FDMA_STATE_IDLE;
		stm_fdma_hw_channel_reset(fchan);
		break;

	case STM_FDMA_STATE_RUNNING:
		/* Channel is running - issue stop (pause) command */
		stm_fdma_hw_channel_pause(fchan, 0);
		/* Fall through */

	case STM_FDMA_STATE_PAUSING:
	case STM_FDMA_STATE_STOPPING:
		/* Channel is pausing - just change state */
		fchan->state = STM_FDMA_STATE_STOPPING;
		break;
	}

	spin_unlock_irqrestore(&fchan->lock, irqflags);
}

static int stm_fdma_terminate_all(struct stm_fdma_chan *fchan)
{
	struct stm_fdma_desc *fdesc, *_fdesc;
	unsigned long irqflags = 0;
	LIST_HEAD(list);

	dev_dbg(fchan->fdev->dev, "%s(fchan=%p)\n", __func__, fchan);

	/* Stop the channel */
	stm_fdma_stop(fchan);

	spin_lock_irqsave(&fchan->lock, irqflags);

	/* Move active and queued descriptors to a temporary list */
	list_splice_init(&fchan->desc_queue, &list);
	list_splice_init(&fchan->desc_active, &list);

	/* Channel is no longer cyclic/parked after a terminate all! */
	clear_bit(STM_FDMA_IS_CYCLIC, &fchan->flags);
	clear_bit(STM_FDMA_IS_PARKED, &fchan->flags);

	spin_unlock_irqrestore(&fchan->lock, irqflags);

	list_for_each_entry_safe(fdesc, _fdesc, &list, node) {
		/* Unmap buffers for non-slave channels (e.g. memcpy) */
		if (!fchan->dma_chan.private)
			stm_fdma_desc_unmap_buffers(fdesc);

		/* Move from temporary list to free list (no callbacks!) */
		stm_fdma_desc_put(fdesc);
	}

	return 0;
}

static int stm_fdma_slave_config(struct stm_fdma_chan *fchan,
		struct dma_slave_config *config)
{
	struct stm_fdma_device *fdev = fchan->fdev;
	unsigned long irqflags = 0;
	dma_addr_t dma_addr = 0;

	dev_dbg(fdev->dev, "%s(fchan=%p, config=%p)\n", __func__,
			fchan, config);

	/* Save the supplied dma address */
	switch (config->direction) {
	case DMA_DEV_TO_MEM:
		dma_addr = config->src_addr;
		break;

	case DMA_MEM_TO_DEV:
		dma_addr = config->dst_addr;
		break;

	default:
		dev_err(fdev->dev, "Invalid slave config direction!\n");
		return -EINVAL;
	}

	spin_lock_irqsave(&fchan->lock, irqflags);
	fchan->dma_addr = dma_addr;
	spin_unlock_irqrestore(&fchan->lock, irqflags);

	spin_lock(&fdev->dreq_lock);

	/* Only update dreq configuration if we are using a dreq */
	if (fchan->dreq) {
		/* Update the max burst, bus width and direction */
		switch (config->direction) {
		case DMA_DEV_TO_MEM:
			fchan->dreq->maxburst = config->src_maxburst;
			fchan->dreq->buswidth = config->src_addr_width;
			fchan->dreq->direction = DMA_DEV_TO_MEM;
			break;

		case DMA_MEM_TO_DEV:
			fchan->dreq->maxburst = config->dst_maxburst;
			fchan->dreq->buswidth = config->dst_addr_width;
			fchan->dreq->direction = DMA_MEM_TO_DEV;
			break;

		default:
			dev_err(fdev->dev, "Invalid slave config direction!\n");
			spin_unlock(&fdev->dreq_lock);
			return -EINVAL;
		}

		spin_unlock(&fdev->dreq_lock);

		/* Configure the dreq and return */
		return stm_fdma_dreq_config(fchan, fchan->dreq);
	}

	spin_unlock(&fdev->dreq_lock);

	return 0;
}

static int stm_fdma_control(struct dma_chan *chan, enum dma_ctrl_cmd cmd,
		unsigned long arg)
{
	struct stm_fdma_chan *fchan = to_stm_fdma_chan(chan);
	struct dma_slave_config *config;

	dev_dbg(fchan->fdev->dev, "%s(chan=%p, cmd=%d, arg=%lu)\n", __func__,
			chan, cmd, arg);

	switch (cmd) {
	case DMA_PAUSE:
		return stm_fdma_pause(fchan);

	case DMA_RESUME:
		return stm_fdma_resume(fchan);
		break;

	case DMA_TERMINATE_ALL:
		return stm_fdma_terminate_all(fchan);

	case DMA_SLAVE_CONFIG:
		config = (struct dma_slave_config *) arg;
		return stm_fdma_slave_config(fchan, config);

	default:
		dev_err(fchan->fdev->dev, "Invalid control cmd (%d)\n", cmd);
	}

	return -ENOSYS;
}

static int stm_fdma_get_residue(struct stm_fdma_chan *fchan)
{
	unsigned long irqflags = 0;
	int count = 0;

	dev_dbg(fchan->fdev->dev, "%s(fchan=%p)\n", __func__, fchan);

	spin_lock_irqsave(&fchan->lock, irqflags);

	/* If channel is parked, return a notional residue */
	if (test_bit(STM_FDMA_IS_PARKED, &fchan->flags)) {
		BUG_ON(!fchan->desc_park);
		count = fchan->desc_park->llu->nbytes;
		goto unlock;
	}

	/* Only attempt to get residue on a non-idle channel */
	if (fchan->state != STM_FDMA_STATE_IDLE) {
		unsigned long stat1, stat2;
		struct stm_fdma_desc *fdesc, *child;
		unsigned long phys_node;
		int found_node = 0;

		/*
		 * Get the current node bytes remaining. We loop until status
		 * is identical in case we are moving on to the next node.
		 */

		do {
			stat1 = readl(CMD_STAT_REG(fchan));
			count = readl(NODE_COUNT_REG(fchan));
			stat2 = readl(CMD_STAT_REG(fchan));
		} while (stat1 != stat2);

		switch (stat1 & CMD_STAT_STATUS_MASK) {
		case CMD_STAT_STATUS_IDLE:
			/*
			 * Channel stopped but not yet taken the interrupt to
			 * change the channel state. Pretend still data to
			 * process and let interrupt do tidy up.
			 */
			count = 1;
			goto unlock;

		case CMD_STAT_STATUS_RUNNING:
		case CMD_STAT_STATUS_PAUSED:
			/*
			 * FDMA firmware modifies CMD_STAT before it updates
			 * the count. However as we write the count when
			 * starting the channel we assume it is valid.
			 */
			break;

		case CMD_STAT_CMD_START:
			/*
			 * Channel has not yet started running so count not yet
			 * loaded from the node. However as we write the count
			 * when starting the channel we assume it is valid.
			 */
			break;
		}

		/*
		 * The descriptor residue is calculated as the sum of the
		 * remaining bytes for the current node and each node left to
		 * process. Thus we need to find the current descriptor node
		 * using the physical node address and only add the number of
		 * bytes from the following nodes to the residue count.
		 */

		/* Convert the status to the physical node pointer address */
		phys_node = stat1 & CMD_STAT_DATA_MASK;

		/* Get the active descriptor */
		fdesc = list_first_entry(&fchan->desc_active,
				struct stm_fdma_desc, node);

		/* Does the physical node match the first descriptor node? */
		if (phys_node == fdesc->dma_desc.phys)
			found_node = 1;

		/* Loop through all descriptor child nodes */
		list_for_each_entry(child, &fdesc->llu_list, node) {

			/* If node has been found, add node nbytes to count */
			if (found_node) {
				count += child->llu->nbytes;
				continue;
			}

			/* Does physical node match child? */
			if (phys_node == child->dma_desc.phys)
				found_node = 1;
		}

		/* Ensure the current node is from the active descriptor */
		BUG_ON(found_node == 0);
	}

unlock:
	spin_unlock_irqrestore(&fchan->lock, irqflags);
	return count;
}

static enum dma_status stm_fdma_tx_status(struct dma_chan *chan,
		dma_cookie_t cookie, struct dma_tx_state *txstate)
{
	struct stm_fdma_chan *fchan = to_stm_fdma_chan(chan);
	unsigned long irqflags = 0;
	dma_cookie_t last_used;
	dma_cookie_t last_complete;
	enum dma_status status;
	int residue = 0;

	dev_dbg(fchan->fdev->dev, "%s(chan=%p, cookie=%08x, txstate=%p)\n",
			__func__, chan, cookie, txstate);

	spin_lock_irqsave(&fchan->lock, irqflags);

	/* Set the last cookie value returned to the client */
	last_used = chan->cookie;
	last_complete = fchan->last_completed;

	/* Check if channel is paused */
	if (fchan->state == STM_FDMA_STATE_PAUSED)
		status = DMA_PAUSED;
	else
		status = dma_async_is_complete(cookie, last_complete,
				last_used);

	spin_unlock_irqrestore(&fchan->lock, irqflags);

	/* Get the residue */
	if (status == DMA_SUCCESS)
		residue = 0;
	else
		residue = stm_fdma_get_residue(fchan);

	/* Set the state */
	dma_set_tx_state(txstate, last_complete, last_used, residue);

	return status;
}

static void stm_fdma_issue_pending(struct dma_chan *chan)
{
	struct stm_fdma_chan *fchan = to_stm_fdma_chan(chan);

	dev_dbg(fchan->fdev->dev, "%s(chan=%p)\n", __func__, chan);

	/* Try starting any next available descriptor */
	stm_fdma_desc_start(fchan);
}


/*
 * Platform driver initialise.
 */

static int __init stm_fdma_probe(struct platform_device *pdev)
{
	struct stm_plat_fdma_data *pdata;
	struct stm_fdma_device *fdev;
	struct resource *iores;
	int irq;
	int result;
	int i;

	pdata = pdev->dev.platform_data;

	/* Allocate FDMA device structure */
	fdev = devm_kzalloc(&pdev->dev, sizeof(*fdev), GFP_KERNEL);
	if (!fdev) {
		dev_err(&pdev->dev, "Failed to allocate device structure\n");
		return -ENOMEM;
	}

	/* Initialise structures */
	fdev->dev = &pdev->dev;
	fdev->pdev = pdev;
	fdev->fw = pdata->fw;
	fdev->hw = pdata->hw;
	fdev->xbar = pdata->xbar;
	spin_lock_init(&fdev->lock);

	/* Retrieve FDMA platform memory resource */
	iores = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!iores) {
		dev_err(&pdev->dev, "Failed to get memory resource\n");
		return -ENXIO;
	}

	/* Retrieve the FDMA platfrom interrupt handler */
	irq = platform_get_irq(pdev, 0);
	if (irq < 0) {
		dev_err(&pdev->dev, "Failed to get irq resource\n");
		return -ENXIO;
	}

	/* Request the FDMA memory region */
	fdev->io_res = devm_request_mem_region(&pdev->dev, iores->start,
				resource_size(iores), pdev->name);
	if (fdev->io_res == NULL) {
		dev_err(&pdev->dev, "Failed to request memory region\n");
		return -EBUSY;
	}

	/* Remap the FDMA memory region into uncached memory */
	fdev->io_base = devm_ioremap_nocache(&pdev->dev, iores->start,
				resource_size(iores));
	if (fdev->io_base == NULL) {
		dev_err(&pdev->dev, "Failed to ioremap memory region\n");
		return -ENXIO;
	}
	dev_notice(&pdev->dev, "Base address: %p\n", fdev->io_base);

	/* Get all FDMA clocks */
	result = stm_fdma_clk_get(fdev);
	if (result) {
		dev_err(&pdev->dev, "Failed to get all clocks\n");
		return result;
	}

	/* Enable the FDMA clocks */
	result = stm_fdma_clk_enable(fdev);
	if (result) {
		dev_err(&pdev->dev, "Failed to enable clocks\n");
		goto error_clk_enb;
	}

	/* Initialise list of FDMA channels */
	INIT_LIST_HEAD(&fdev->dma_device.channels);
	for (i = STM_FDMA_MIN_CHANNEL; i <= STM_FDMA_MAX_CHANNEL; ++i) {
		struct stm_fdma_chan *fchan = &fdev->ch_list[i];
		struct dma_chan *chan = &fchan->dma_chan;

		/* Set the channel data */
		fchan->fdev = fdev;
		fchan->id = i;

		/* Initialise channel lock and descriptor lists */
		spin_lock_init(&fchan->lock);
		INIT_LIST_HEAD(&fchan->desc_free);
		INIT_LIST_HEAD(&fchan->desc_queue);
		INIT_LIST_HEAD(&fchan->desc_active);

		/* Initialise the irq tasklet */
		tasklet_init(&fchan->tasklet_error, stm_fdma_tasklet_error,
				(unsigned long) fchan);
		tasklet_init(&fchan->tasklet_complete,
				stm_fdma_tasklet_complete,
				(unsigned long) fchan);

		/* Set the dmaengine channel data */
		chan->device = &fdev->dma_device;

		/* Add the dmaengine channel to the dmaengine device */
		list_add_tail(&chan->device_node, &fdev->dma_device.channels);
	}

	/* Initialise the FDMA dreq data (reserve 0 & 31 for FDMA use) */
	spin_lock_init(&fdev->dreq_lock);
	fdev->dreq_mask = (1 << 0) | (1 << 31);

	/* Create the dma llu pool */
	fdev->dma_pool = dma_pool_create(dev_name(fdev->dev), NULL,
			STM_FDMA_LLU_SIZE, STM_FDMA_LLU_ALIGN, 0);
	if (fdev->dma_pool == NULL) {
		dev_err(fdev->dev, "Failed to create dma pool\n");
		result = -ENOMEM;
		goto error_dma_pool;
	}

	/* Set the FDMA register offsets */
	fdev->regs.id = fdev->hw->slim_regs.id;
	fdev->regs.ver = fdev->hw->slim_regs.ver;
	fdev->regs.en = fdev->hw->slim_regs.en;
	fdev->regs.clk_gate = fdev->hw->slim_regs.clk_gate;
	fdev->regs.rev_id = fdev->fw->rev_id;
	fdev->regs.cmd_statn = fdev->fw->cmd_statn;
	fdev->regs.req_ctln = fdev->fw->req_ctln;
	fdev->regs.ptrn = fdev->fw->ptrn;
	fdev->regs.cntn = fdev->fw->cntn;
	fdev->regs.saddrn = fdev->fw->saddrn;
	fdev->regs.daddrn = fdev->fw->daddrn;
	fdev->regs.node_size = fdev->fw->node_size ? : LEGACY_NODE_DATA_SIZE;
	fdev->regs.sync_reg = fdev->hw->periph_regs.sync_reg;
	fdev->regs.cmd_sta = fdev->hw->periph_regs.cmd_sta;
	fdev->regs.cmd_set = fdev->hw->periph_regs.cmd_set;
	fdev->regs.cmd_clr = fdev->hw->periph_regs.cmd_clr;
	fdev->regs.cmd_mask = fdev->hw->periph_regs.cmd_mask;
	fdev->regs.int_sta = fdev->hw->periph_regs.int_sta;
	fdev->regs.int_set = fdev->hw->periph_regs.int_set;
	fdev->regs.int_clr = fdev->hw->periph_regs.int_clr;
	fdev->regs.int_mask = fdev->hw->periph_regs.int_mask;

	/* Install the FDMA interrupt handler */
	result = devm_request_irq(&pdev->dev, irq, stm_fdma_irq_handler,
			IRQF_DISABLED|IRQF_SHARED, dev_name(&pdev->dev), fdev);
	if (result < 0) {
		dev_err(&pdev->dev, "Failed to request irq\n");
		result = -EBUSY;
		goto error_req_irq;
	}

	/* Create the firmware loading wait queue */
	init_waitqueue_head(&fdev->fw_load_q);

	/* Set the FDMA device capabilities */
	dma_cap_set(DMA_SLAVE,  fdev->dma_device.cap_mask);
	dma_cap_set(DMA_CYCLIC, fdev->dma_device.cap_mask);
	dma_cap_set(DMA_MEMCPY, fdev->dma_device.cap_mask);

	/* Initialise the dmaengine device */
	fdev->dma_device.dev = &pdev->dev;

	fdev->dma_device.device_alloc_chan_resources =
		stm_fdma_alloc_chan_resources;
	fdev->dma_device.device_free_chan_resources =
		stm_fdma_free_chan_resources;
	fdev->dma_device.device_prep_dma_memcpy	= stm_fdma_prep_dma_memcpy;
	fdev->dma_device.device_prep_slave_sg	= stm_fdma_prep_slave_sg;
	fdev->dma_device.device_prep_dma_cyclic	= stm_fdma_prep_dma_cyclic;
	fdev->dma_device.device_control		= stm_fdma_control;
	fdev->dma_device.device_tx_status	= stm_fdma_tx_status;
	fdev->dma_device.device_issue_pending	= stm_fdma_issue_pending;

	/* Register the dmaengine device */
	result = dma_async_device_register(&fdev->dma_device);
	if (result) {
		dev_err(&pdev->dev, "Failed to register DMA device\n");
		goto error_register;
	}

	/* Register the device with debugfs */
	stm_fdma_debugfs_register(fdev);

	/* Associate this FDMA with platform device */
	platform_set_drvdata(pdev, fdev);

	return 0;

error_register:
error_req_irq:
	dma_pool_destroy(fdev->dma_pool);
error_dma_pool:
	/* Kill tasklet for each channel */
	for (i = STM_FDMA_MIN_CHANNEL; i <= STM_FDMA_MAX_CHANNEL; ++i) {
		struct stm_fdma_chan *fchan = &fdev->ch_list[i];

		tasklet_disable(&fchan->tasklet_error);
		tasklet_disable(&fchan->tasklet_complete);

		tasklet_kill(&fchan->tasklet_error);
		tasklet_kill(&fchan->tasklet_complete);
	}
error_clk_enb:
	stm_fdma_clk_disable(fdev);
	return result;
}

static int __exit stm_fdma_remove(struct platform_device *pdev)
{
	struct stm_fdma_device *fdev = platform_get_drvdata(pdev);
	int i;

	/* Clear the platform driver data */
	platform_set_drvdata(pdev, NULL);

	/* Unregister the device from debugfs */
	stm_fdma_debugfs_unregister(fdev);

	/* Unregister the dmaengine device */
	dma_async_device_unregister(&fdev->dma_device);

	/* Disable all channels */
	stm_fdma_hw_channel_disable_all(fdev);

	/* Turn off and release the FDMA clocks */
	stm_fdma_clk_disable(fdev);

	/* Free the firmware ELF file */
	if (fdev->fw_elfinfo)
		ELF32_free(fdev->fw_elfinfo);

	/* Destroy the dma pool */
	dma_pool_destroy(fdev->dma_pool);

	/* Kill tasklet for each channel */
	for (i = STM_FDMA_MIN_CHANNEL; i <= STM_FDMA_MAX_CHANNEL; ++i) {
		struct stm_fdma_chan *fchan = &fdev->ch_list[i];

		tasklet_disable(&fchan->tasklet_error);
		tasklet_disable(&fchan->tasklet_complete);

		tasklet_kill(&fchan->tasklet_error);
		tasklet_kill(&fchan->tasklet_complete);
	}

	return 0;
}


/*
 * Power management
 */

#ifdef CONFIG_PM
static int stm_fdma_pm_suspend(struct device *dev)
{
	struct stm_fdma_device *fdev = dev_get_drvdata(dev);
	int result;

	/* We can only suspend after firmware has been loaded */
	if (!fdev->fw_loaded) {
		dev_err(fdev->dev, "Cannot freeze as firmware never loaded\n");
		return 0;
	}

	/*
	 * At this point the channel users are already suspended. This makes
	 * safe the 'channel_disable_all' call.
	 */

	/* Disable all channels (prevents memory access in self-refresh) */
	result = stm_fdma_hw_channel_disable_all(fdev);
	if (result) {
		dev_err(fdev->dev, "Failed to disable channels on suspend\n");
		return -ENODEV;
	}

	/* Disable the FDMA clocks */
	stm_fdma_clk_disable(fdev);

	return 0;
}

static int stm_fdma_pm_resume(struct device *dev)
{
	struct stm_fdma_device *fdev = dev_get_drvdata(dev);
	int result;

	/* We can only resume after firmware has been loaded */
	if (!fdev->fw_loaded) {
		dev_err(fdev->dev, "Cannot resume as firmware never loaded\n");
		return 0;
	}

	/* Enable the FDMA clocks */
	stm_fdma_clk_enable(fdev);

	/* Enable all channels */
	result = stm_fdma_hw_channel_enable_all(fdev);
	if (!result) {
		dev_err(fdev->dev, "Failed to enable channels on resume\n");
		return -ENODEV;
	}

	return 0;
}

#ifdef CONFIG_HIBERNATION
static int stm_fdma_pm_freeze(struct device *dev)
{
	struct stm_fdma_device *fdev = dev_get_drvdata(dev);
	int result;

	/* We can only freeze after firmware has been loaded */
	if (!fdev->fw_loaded) {
		dev_err(fdev->dev, "Cannot freeze as firmware never loaded\n");
		return 0;
	}

	/*
	 * At this point the channel users are already suspended. This makes
	 * safe the 'channel_disable_all' call.
	 */

	/* Disable all channels (prevents memory access in self-refresh) */
	result = stm_fdma_hw_channel_disable_all(fdev);
	if (result) {
		dev_err(fdev->dev, "Failed to disable channels on freeze\n");
		return -ENODEV;
	}

	/* Disable the FDMA clocks */
	stm_fdma_clk_disable(fdev);

	return 0;
}

static int stm_fdma_pm_restore(struct device *dev)
{
	struct stm_fdma_device *fdev = dev_get_drvdata(dev);
	int result;

	/* We can only restore after firmware has been loaded */
	if (!fdev->fw_loaded) {
		dev_err(fdev->dev, "Cannot restore as firmware never loaded\n");
		return 0;
	}

	/* Enable the FDMA clocks */
	stm_fdma_clk_enable(fdev);

	/* Reload the firmware and initialise the hardware */
	result = stm_fdma_fw_load(fdev, fdev->fw_elfinfo);
	if (result) {
		dev_err(fdev->dev, "Failed to reload firmware\n");
		return result;
	}

	return 0;
}
#else
#define stm_fdma_pm_freeze NULL
#define stm_fdma_pm_restore NULL
#endif

static const struct dev_pm_ops stm_fdma_pm_ops = {
	.suspend	= stm_fdma_pm_suspend,
	.resume		= stm_fdma_pm_resume,
	.freeze		= stm_fdma_pm_freeze,
	.restore	= stm_fdma_pm_restore,
};
#endif


/*
 * Module initialisation
 */

static struct platform_driver stm_fdma_platform_driver = {
	.driver.name	= "stm-fdma",
#ifdef CONFIG_PM
	.driver.pm	= &stm_fdma_pm_ops,
#endif
	.probe		= stm_fdma_probe,
	.remove		= stm_fdma_remove,
};

static int __init stm_fdma_init(void)
{
	int result;

	/* Initialise debugfs support */
	stm_fdma_debugfs_init();

	/* Register the platform driver */
	result = platform_driver_register(&stm_fdma_platform_driver);
	if (result)
		goto error_register;

	return 0;

error_register:
	/* Shutdown debugfs support */
	stm_fdma_debugfs_exit();
	return result;
}

static void __exit stm_fdma_exit(void)
{
	/* Shutdown debugfs support */
	stm_fdma_debugfs_exit();

	/* Unregister the platform driver */
	platform_driver_unregister(&stm_fdma_platform_driver);
}

MODULE_AUTHOR("John Boddie <john.boddie@st.com>");
MODULE_DESCRIPTION("STMicroelectronics FDMA dmaengine driver");
MODULE_LICENSE("GPL");

module_init(stm_fdma_init);
module_exit(stm_fdma_exit);
