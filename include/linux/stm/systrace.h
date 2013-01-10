/*
 * Copyright (c) 2012 STMicroelectronics Limited
 *
 * System Trace Module IP v1 register offsets definitions.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __STM_SYSTRACE_H_
#define __STM_SYSTRACE_H_

#include <linux/platform_device.h>

/* Register Offset definitions for IPv1 */
#define STM_IPv1_CR_OFF 0x000
#define STM_IPv1_MMC_OFF 0x008
#define STM_IPv1_TER_OFF 0x010
#define STM_IPv1_ID0_OFF 0xFC0
#define STM_IPv1_ID1_OFF 0xFC8
#define STM_IPv1_ID2_OFF 0xFD0

/* A new channel ID each 16 bytes */
#define STM_IPv1_CH_SHIFT 4

/* A new core (master) ID each 4k */
#define STM_IPv1_MA_SHIFT 12

struct stm_plat_systrace_data {
	struct stm_pad_config *pad_config;
	struct stm_pad_state *pad_state;
	struct resource *mem_base;
	struct resource *mem_regs;
	void __iomem *base;
	void __iomem *regs;

	/*store the ptr to a per-instance structure for MTT. */
	void *private;

	/* Very basic channel pool mgt for the trace API.
	 * Will implement a bitmap if any support request,
	 */
	unsigned int last_ch_ker;
};

#endif /*__STM_SYSTRACE_H_*/
