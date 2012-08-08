/*
 * STMicroelectronics FDMA dmaengine driver descriptor functions
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
#include <linux/device.h>
#include <linux/dmapool.h>
#include <linux/dma-mapping.h>

#include <linux/stm/dma.h>

#include "stm_fdma.h"


/*
 * Descriptor functions
 */

struct stm_fdma_desc *stm_fdma_desc_alloc(struct stm_fdma_chan *fchan)
{
	struct stm_fdma_desc *fdesc;

	/* Allocate the descriptor */
	fdesc = kzalloc(sizeof(struct stm_fdma_desc), GFP_KERNEL);
	if (!fdesc) {
		dev_err(fchan->fdev->dev, "Failed to alloc desc\n");
		goto error_kzalloc;
	}

	/* Allocate the llu from the dma pool */
	fdesc->llu = dma_pool_alloc(fchan->fdev->dma_pool, GFP_KERNEL,
			&fdesc->dma_desc.phys);
	if (!fdesc->llu) {
		dev_err(fchan->fdev->dev, "Failed to alloc from dma pool\n");
		goto error_dma_pool;
	}

	/* Initialise the descriptor */
	fdesc->fchan = fchan;
	INIT_LIST_HEAD(&fdesc->llu_list);

	dma_async_tx_descriptor_init(&fdesc->dma_desc, &fchan->dma_chan);

	/* Set ack bit to ensure descriptor is ready for use */
	fdesc->dma_desc.flags = DMA_CTRL_ACK;
	fdesc->dma_desc.tx_submit = stm_fdma_tx_submit;

	return fdesc;

error_dma_pool:
	kfree(fdesc);
error_kzalloc:
	return NULL;
}

void stm_fdma_desc_free(struct stm_fdma_desc *fdesc)
{
	if (fdesc) {
		struct stm_fdma_device *fdev = fdesc->fchan->fdev;

		/* Free the llu back to the dma pool and free the descriptor */
		dma_pool_free(fdev->dma_pool, fdesc->llu, fdesc->dma_desc.phys);
		kfree(fdesc);
	}
}

struct stm_fdma_desc *stm_fdma_desc_get(struct stm_fdma_chan *fchan)
{
	struct stm_fdma_desc *fdesc = NULL, *tdesc, *_tdesc;
	unsigned long irqflags = 0;

	spin_lock_irqsave(&fchan->lock, irqflags);

	/* Search the free list for an available descriptor */
	list_for_each_entry_safe(tdesc, _tdesc, &fchan->desc_free, node) {
		/* If descriptor has been ACKed then remove from free list */
		if (async_tx_test_ack(&tdesc->dma_desc)) {
			list_del_init(&tdesc->node);
			fdesc = tdesc;
			break;
		}
		dev_dbg(fchan->fdev->dev, "Descriptor %p not ACKed\n", fdesc);
	}

	if (!fdesc) {
		spin_unlock_irqrestore(&fchan->lock, irqflags);

		/* No descriptors available, attempt to allocate a new one */
		dev_notice(fchan->fdev->dev, "Allocating a new descriptor\n");
		fdesc = stm_fdma_desc_alloc(fchan);
		if (!fdesc) {
			dev_err(fchan->fdev->dev, "Not enough descriptors\n");
			return NULL;
		}

		/* Increment number of descriptors allocated */
		spin_lock_irqsave(&fchan->lock, irqflags);
		fchan->desc_count++;
	}

	spin_unlock_irqrestore(&fchan->lock, irqflags);

	/* Re-initialise the descriptor */
	memset(fdesc->llu, 0, sizeof(struct stm_fdma_llu));
	INIT_LIST_HEAD(&fdesc->llu_list);
	fdesc->dma_desc.cookie = 0;
	fdesc->dma_desc.callback = NULL;
	fdesc->dma_desc.callback_param = NULL;
	fdesc->dma_desc.tx_submit = stm_fdma_tx_submit;

	return fdesc;
}

void stm_fdma_desc_put(struct stm_fdma_desc *fdesc)
{
	if (fdesc) {
		struct stm_fdma_chan *fchan = fdesc->fchan;
		unsigned long irqflags = 0;

		spin_lock_irqsave(&fchan->lock, irqflags);

		/* Move all linked descriptors to the free list */
		list_splice_init(&fdesc->llu_list, &fchan->desc_free);
		list_add(&fdesc->node, &fchan->desc_free);

		spin_unlock_irqrestore(&fchan->lock, irqflags);
	}
}

void stm_fdma_desc_chain(struct stm_fdma_desc **head,
		struct stm_fdma_desc **prev, struct stm_fdma_desc *fdesc)
{
	if (!(*head)) {
		/* First descriptor becomes the head */
		*head = fdesc;
	} else {
		/* Link previous descriptor to this one */
		(*prev)->llu->next = fdesc->dma_desc.phys;
		list_add_tail(&fdesc->node, &(*head)->llu_list);
	}

	/* Descriptor just added now becomes the previous */
	*prev = fdesc;
}

void stm_fdma_desc_start(struct stm_fdma_chan *fchan)
{
	struct stm_fdma_desc *fdesc;
	unsigned long irqflags = 0;

	spin_lock_irqsave(&fchan->lock, irqflags);

	/* There must be no active descriptors */
	if (!list_empty(&fchan->desc_active))
		goto unlock;

	/* Descriptor queue must not be empty */
	if (list_empty(&fchan->desc_queue))
		goto unlock;

	/* Remove first descriptor from queue and add to active list */
	fdesc = list_first_entry(&fchan->desc_queue, struct stm_fdma_desc,
				node);
	list_del_init(&fdesc->node);
	list_add_tail(&fdesc->node, &fchan->desc_active);


	if (test_bit(STM_FDMA_IS_PARKED, &fchan->flags)) {
		/* Switch from parking to next descriptor (no interrupt) */
		BUG_ON(!fchan->desc_park);
		stm_fdma_hw_channel_switch(fchan, fchan->desc_park, fdesc, 0);
		clear_bit(STM_FDMA_IS_PARKED, &fchan->flags);
	} else {
		/* Start the channel for the descriptor */
		stm_fdma_hw_channel_start(fchan, fdesc);
		fchan->state = STM_FDMA_STATE_RUNNING;
	}

unlock:
	spin_unlock_irqrestore(&fchan->lock, irqflags);
}

void stm_fdma_desc_unmap_buffers(struct stm_fdma_desc *fdesc)
{
	struct dma_async_tx_descriptor *desc = &fdesc->dma_desc;
	struct device *dev = desc->chan->device->dev;

	if (!(desc->flags & DMA_COMPL_SKIP_SRC_UNMAP)) {
		if (desc->flags & DMA_COMPL_SRC_UNMAP_SINGLE)
			dma_unmap_single(dev, fdesc->llu->saddr,
					fdesc->llu->nbytes, DMA_MEM_TO_DEV);
		else
			dma_unmap_page(dev, fdesc->llu->saddr,
					fdesc->llu->nbytes, DMA_MEM_TO_DEV);
	}

	if (!(desc->flags & DMA_COMPL_SKIP_DEST_UNMAP)) {
		if (desc->flags & DMA_COMPL_DEST_UNMAP_SINGLE)
			dma_unmap_single(dev, fdesc->llu->daddr,
					fdesc->llu->nbytes, DMA_DEV_TO_MEM);
		else
			dma_unmap_page(dev, fdesc->llu->daddr,
					fdesc->llu->nbytes, DMA_DEV_TO_MEM);
	}
}

void stm_fdma_desc_complete(struct stm_fdma_chan *fchan,
		struct stm_fdma_desc *fdesc)
{
	struct dma_async_tx_descriptor *desc = &fdesc->dma_desc;
	unsigned long irqflags = 0;

	dev_dbg(fchan->fdev->dev, "%s(fchan=%p, fdesc=%p)\n", __func__,
			fchan, fdesc);

	spin_lock_irqsave(&fchan->lock, irqflags);

	/* Set the cookie to the last completed descriptor cookie */
	fchan->last_completed = desc->cookie;

	/* Unmap dma address for descriptor and children */
	if (!fchan->dma_chan.private) {
		struct stm_fdma_desc *child;

		stm_fdma_desc_unmap_buffers(fdesc);

		list_for_each_entry(child, &fdesc->llu_list, node)
			stm_fdma_desc_unmap_buffers(child);
	}

	/* Retire this descriptor to the free list */
	list_splice_init(&fdesc->llu_list, &fchan->desc_free);
	list_move(&fdesc->node, &fchan->desc_free);

	spin_unlock_irqrestore(&fchan->lock, irqflags);

	/* Issue callback */
	if (desc->callback)
		desc->callback(desc->callback_param);
}
