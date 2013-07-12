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
	STM_DMA_TYPE_AUDIO,		/* S/W enhanced paced Tx channel */
	STM_DMA_TYPE_TELSS,
	STM_DMA_TYPE_MCHI,		/* For Rx only - for Tx use paced */
};


/*
 * Channel specific configuration structures
 */

struct stm_dma_config {
	enum stm_dma_type type;
};

struct stm_dma_dreq_config {
	u32 request_line;
	u32 direct_conn;
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

struct stm_dma_park_config {
	int sub_periods;
	int buffer_size;
};

struct stm_dma_audio_config {
	enum stm_dma_type type;
	dma_addr_t dma_addr;
	struct stm_dma_dreq_config dreq_config;
	struct stm_dma_park_config park_config;
};

struct stm_dma_mchi_config {
	enum stm_dma_type type;
	dma_addr_t dma_addr;
	struct stm_dma_dreq_config dreq_config;
	struct stm_dma_dreq_config pkt_start_rx_dreq_config;
	u32 rx_fifo_threshold_addr;
};

struct stm_dma_telss_config {
	enum stm_dma_type type;
	dma_addr_t dma_addr;
	struct stm_dma_dreq_config dreq_config;

	u32 frame_count;		/* No frames to transfer */
	u32 frame_size;			/* No words per frame, 0=1, 1=2 etc */
	u32 handset_count;		/* Number of handsets */
};

struct stm_dma_telss_handset_config {
	u16 buffer_offset;
	u16 period_offset;
	u32 period_stride;
	u16 first_slot_id;
	u16 second_slot_id;
	bool second_slot_id_valid;
	bool duplicate_enable;
	bool data_length;
	bool call_valid;
};


/*
 * Helper functions
 */

static inline int stm_dma_is_fdma_name(struct dma_chan *chan, const char *name)
{
	return !strcmp(dev_name(chan->device->dev), name);
}

static inline int stm_dma_is_fdma(struct dma_chan *chan, int id)
{
	return (chan->dev->dev_id == id);
}

static inline int stm_dma_is_fdma_channel(struct dma_chan *chan, int id)
{
	return (chan->chan_id == id);
}


/*
 * Audio channel extensions API
 */
int dma_audio_parking_config(struct dma_chan *chan,
		struct stm_dma_park_config *config);
int dma_audio_parking_enable(struct dma_chan *chan);
int dma_audio_is_parking_active(struct dma_chan *chan);
struct dma_async_tx_descriptor *dma_audio_prep_tx_cyclic(struct dma_chan *chan,
		dma_addr_t buf_addr, size_t buf_len, size_t period_len);

/*
 * MCHI channel extensions API
 */

struct dma_async_tx_descriptor *dma_mchi_prep_rx_cyclic(struct dma_chan *chan,
		struct scatterlist *sgl, unsigned int sg_len);


/*
 * TELSS channel extensions API
 */
int dma_telss_handset_config(struct dma_chan *chan, int handset,
		struct stm_dma_telss_handset_config *config);
int dma_telss_handset_control(struct dma_chan *chan, int handset, int valid);
struct dma_async_tx_descriptor *dma_telss_prep_dma_cyclic(
		struct dma_chan *chan, dma_addr_t buf_addr, size_t buf_len,
		size_t period_len, enum dma_transfer_direction direction);
int dma_telss_get_period(struct dma_chan *chan);


#endif /* __LINUX_STM_DMA_H__ */
