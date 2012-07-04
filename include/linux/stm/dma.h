/*
 * STMicroelectronics FDMA dmaengine driver
 *
 * Copyright (c) 2012 STMicroelectronics Limited
 *
 * Author: John Boddie <john.boddie@st.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __LINUX_STM_DMA_H__
#define __LINUX_STM_DMA_H__


#include <linux/dmaengine.h>


/*
 * Channel type enumerations
 */

enum stm_dma_type {
	STM_DMA_TYPE_FREE_RUNNING,
	STM_DMA_TYPE_PACED,
};


/*
 * Channel specific configuration structures
 */

struct stm_dma_config {
	enum stm_dma_type type;
};

struct stm_dma_dreq_config {
	u32 request_line;
	u32 initiator;
	u32 increment;
	u32 data_swap;
	u32 hold_off;
	u32 maxburst;
	enum dma_slave_buswidth buswidth;
	enum dma_transfer_direction direction;
};

struct stm_dma_paced_config {
	enum stm_dma_type type;
	dma_addr_t dma_addr;
	struct stm_dma_dreq_config dreq_config;
};

/*
 * Helper functions
 */

static inline int stm_dma_is_fdma_name(struct dma_chan *chan, const char *name)
{
	return !strcmp(dev_name(chan->device->dev), name);
}

static inline int stm_dma_is_fdma_channel(struct dma_chan *chan, int id)
{
	return (chan->chan_id == id);
}


#endif /* __LINUX_STM_DMA_H__ */
