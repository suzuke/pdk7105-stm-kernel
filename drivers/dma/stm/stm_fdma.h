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

#ifndef __STM_FDMA_H__
#define __STM_FDMA_H__


#include <linux/clk.h>
#include <linux/interrupt.h>
#include <linux/dmaengine.h>
#include <linux/libelf.h>
#include <linux/stm/dma.h>

#include "stm_fdma_llu.h"
#include "stm_fdma_regs.h"


/*
 * FDMA firmware specific
 */

#define EM_SLIM			102	/* No official SLIM ELF ID yet */
#define EF_SLIM_FDMA		2	/* ELF header e_flags indicates usage */

#define STM_FDMA_FW_NAME_LEN	23
#define STM_FDMA_FW_SEGMENTS	2

enum stm_fdma_fw_state {
	STM_FDMA_FW_STATE_INIT,
	STM_FDMA_FW_STATE_LOADING,
	STM_FDMA_FW_STATE_LOADED,
	STM_FDMA_FW_STATE_ERROR
};


/*
 * FDMA request line specific
 */

#define STM_FDMA_NUM_DREQS	32

struct stm_fdma_dreq_router {
	int (*route)(struct stm_fdma_dreq_router *router, int input_req_line,
			int fdma, int fdma_req_line);
	struct list_head list;
	u8 xbar_id;
};


/*
 * FDMA channel specific
 */

#define STM_FDMA_MIN_CHANNEL	0
#define STM_FDMA_MAX_CHANNEL	15
#define STM_FDMA_NUM_CHANNELS	16

#define STM_FDMA_IS_CYCLIC	1
#define STM_FDMA_IS_PARKED	2

enum stm_fdma_state {
	STM_FDMA_STATE_IDLE,
	STM_FDMA_STATE_RUNNING,
	STM_FDMA_STATE_STOPPING,
	STM_FDMA_STATE_PAUSED,
	STM_FDMA_STATE_ERROR
};

struct stm_fdma_desc;
struct stm_fdma_device;

struct stm_fdma_chan {
	struct stm_fdma_device *fdev;
	struct dma_chan dma_chan;

	u32 id;
	spinlock_t lock;
	unsigned long flags;
	enum stm_dma_type type;
	enum stm_fdma_state state;
	struct stm_dma_dreq_config *dreq;
	dma_addr_t dma_addr;

	u32 desc_count;
	struct list_head desc_free;
	struct list_head desc_queue;
	struct list_head desc_active;
	struct stm_fdma_desc *desc_park;

	struct tasklet_struct tasklet_complete;

	dma_cookie_t last_completed;

	void *extension;
};


/*
 * FDMA specific device structure
 */

#define STM_FDMA_CLKS		4

struct stm_fdma_device {
	struct platform_device *pdev;
	struct device *dev;
	struct dma_device dma_device;
	u32 fdma_id;

	struct clk *clks[STM_FDMA_CLKS];
	struct resource *io_res;
	void __iomem *io_base;
	spinlock_t lock;

	char fw_name[STM_FDMA_FW_NAME_LEN];
	struct ELF32_info *fw_elfinfo;
	enum stm_fdma_fw_state fw_state;
	wait_queue_head_t fw_load_q;

	struct stm_fdma_chan ch_list[STM_FDMA_NUM_CHANNELS];

	spinlock_t dreq_lock;
	struct stm_dma_dreq_config dreq_list[STM_FDMA_NUM_DREQS];
	u32 dreq_mask;

	struct dma_pool *dma_pool;

	struct stm_plat_fdma_hw *hw;
	struct stm_plat_fdma_fw_regs *fw;
	u8 xbar;

	struct stm_fdma_regs regs;

#ifdef CONFIG_DEBUG_FS
	/* debugfs */
	struct dentry *debug_dir;
	struct dentry *debug_regs;
	struct dentry *debug_dmem;
	struct dentry *debug_chans[STM_FDMA_NUM_CHANNELS];
#endif
};


/*
 * FDMA descriptor specific
 */

#define STM_FDMA_DESCRIPTORS	32

struct stm_fdma_desc {
	struct list_head node;

	struct stm_fdma_chan *fchan;

	struct stm_fdma_llu *llu;
	struct list_head llu_list;

	struct dma_async_tx_descriptor dma_desc;

	void *extension;
};


/*
 * Type functions
 */

static inline struct stm_fdma_chan *to_stm_fdma_chan(struct dma_chan *chan)
{
	BUG_ON(!chan);
	return container_of(chan, struct stm_fdma_chan, dma_chan);
}

static inline struct stm_fdma_device *to_stm_fdma_device(struct dma_device *dev)
{
	BUG_ON(!dev);
	return container_of(dev, struct stm_fdma_device, dma_device);
}

static inline struct stm_fdma_desc *to_stm_fdma_desc(
		struct dma_async_tx_descriptor *desc)
{
	BUG_ON(!desc);
	return container_of(desc, struct stm_fdma_desc, dma_desc);
}


/*
 * Function prototypes
 */

#ifdef CONFIG_DEBUG_FS

void stm_fdma_debugfs_init(void);
void stm_fdma_debugfs_exit(void);
void stm_fdma_debugfs_register(struct stm_fdma_device *fdev);
void stm_fdma_debugfs_unregister(struct stm_fdma_device *fdev);

#else

#define stm_fdma_debugfs_init()		do { } while (0)
#define stm_fdma_debugfs_exit()		do { } while (0)
#define stm_fdma_debugfs_register(a)	do { } while (0)
#define stm_fdma_debugfs_unregister(a)	do { } while (0)

#endif


dma_cookie_t stm_fdma_tx_submit(struct dma_async_tx_descriptor *desc);

struct stm_fdma_desc *stm_fdma_desc_alloc(struct stm_fdma_chan *fchan);
void stm_fdma_desc_free(struct stm_fdma_desc *fdesc);
struct stm_fdma_desc *stm_fdma_desc_get(struct stm_fdma_chan *fchan);
void stm_fdma_desc_put(struct stm_fdma_desc *fdesc);
void stm_fdma_desc_chain(struct stm_fdma_desc **head,
		struct stm_fdma_desc **prev, struct stm_fdma_desc *fdesc);
void stm_fdma_desc_start(struct stm_fdma_chan *fchan);
void stm_fdma_desc_unmap_buffers(struct stm_fdma_desc *fdesc);
void stm_fdma_desc_complete(unsigned long data);

int stm_fdma_register_dreq_router(struct stm_fdma_dreq_router *router);
void stm_fdma_unregister_dreq_router(struct stm_fdma_dreq_router *router);
struct stm_dma_dreq_config *stm_fdma_dreq_alloc(struct stm_fdma_chan *fchan,
		struct stm_dma_dreq_config *config);
void stm_fdma_dreq_free(struct stm_fdma_chan *fchan,
		struct stm_dma_dreq_config *config);
int stm_fdma_dreq_config(struct stm_fdma_chan *fchan,
		struct stm_dma_dreq_config *config);


int stm_fdma_fw_check(struct stm_fdma_device *fdev);
int stm_fdma_fw_load(struct stm_fdma_device *fdev, struct ELF32_info *elfinfo);

void stm_fdma_hw_enable(struct stm_fdma_device *fdev);
void stm_fdma_hw_disable(struct stm_fdma_device *fdev);
void stm_fdma_hw_get_revisions(struct stm_fdma_device *fdev,
		int *hw_major, int *hw_minor, int *fw_major, int *fw_minor);
void stm_fdma_hw_channel_reset(struct stm_fdma_chan *fchan);
int stm_fdma_hw_channel_enable_all(struct stm_fdma_device *fdev);
int stm_fdma_hw_channel_disable_all(struct stm_fdma_device *fdev);
int stm_fdma_hw_channel_set_dreq(struct stm_fdma_chan *fchan,
		struct stm_dma_dreq_config *config);
void stm_fdma_hw_channel_start(struct stm_fdma_chan *fchan,
		struct stm_fdma_desc *fdesc);
void stm_fdma_hw_channel_pause(struct stm_fdma_chan *fchan, int flush);
void stm_fdma_hw_channel_resume(struct stm_fdma_chan *fchan);
void stm_fdma_hw_channel_switch(struct stm_fdma_chan *fchan,
		struct stm_fdma_desc *fdesc, struct stm_fdma_desc *tdesc,
		int ioc);
int stm_fdma_hw_channel_status(struct stm_fdma_chan *fchan);
int stm_fdma_hw_channel_error(struct stm_fdma_chan *fchan);


/*
 * Extension API function prototypes
 */

#ifdef CONFIG_STM_FDMA_AUDIO
int stm_fdma_audio_alloc_chan_resources(struct stm_fdma_chan *fchan);
void stm_fdma_audio_free_chan_resources(struct stm_fdma_chan *fchan);
#else
#define stm_fdma_audio_alloc_chan_resources(f)	0
#define stm_fdma_audio_free_chan_resources(f)	do { } while (0)
#endif

#ifdef CONFIG_STM_FDMA_MCHI
int stm_fdma_mchi_alloc_chan_resources(struct stm_fdma_chan *fchan);
void stm_fdma_mchi_free_chan_resources(struct stm_fdma_chan *fchan);
#else
#define stm_fdma_mchi_alloc_chan_resources(f)	0
#define stm_fdma_mchi_free_chan_resources(f)	do { } while (0)
#endif

#ifdef CONFIG_STM_FDMA_TELSS
int stm_fdma_telss_alloc_chan_resources(struct stm_fdma_chan *fchan);
void stm_fdma_telss_free_chan_resources(struct stm_fdma_chan *fchan);
#else
#define stm_fdma_telss_alloc_chan_resources(f)	0
#define stm_fdma_telss_free_chan_resources(f)	do { } while (0)
#endif


#endif /* __STM_FDMA_H__ */
