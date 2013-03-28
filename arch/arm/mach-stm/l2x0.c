/*
 * L2 cache initialization for MPE41 and MPE42
 *
 * Copyright (C) 2012 STMicroelectronics Limited.
 * Author: Srinivas Kandagatla <srinivas.kandagatla@st.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */
#ifndef CONFIG_OF

#include <linux/init.h>
#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/stm/soc.h>
#include <mach/hardware.h>
#include <mach/mpe41.h>
#include <mach/mpe42.h>
#include <mach/soc-stig125.h>
#include <asm/hardware/cache-l2x0.h>

#define STM_L2X0_AUX_CTRL_MASK \
	(0xffffffff & ~(L2X0_AUX_CTRL_WAY_SIZE_MASK | \
			L2X0_AUX_CTRL_SHARE_OVERRIDE_MASK | \
			L2X0_AUX_CTRL_PREFETCH_MASK))

static int __init stm_l2x0_init(void)
{
#ifdef CONFIG_CACHE_L2X0
	void __iomem *base;
	u32 val, aux_ctrl;
	/* Default value for almost all chips */
	/* 1 cycle of latency for setup, read and write accesses */
	u32 tag_latency  = 0;
	/* 2, 3, 2 cycles of latency for setup, read and write accesses */
	u32 data_latency = 0x121;
	/* 128 KB */
	u32 way_size = 0x4;

	if (stm_soc_is_fli7610() || stm_soc_is_stih415())
		base = __io_address(MPE41_PL310_BASE);
	else if (stm_soc_is_stig125()) {
		base = __io_address(STIG125_PL310_BASE);
		/* 64 KB */
		way_size = 0x3;
	} else if (stm_soc_is_stih416()) {
		base = __io_address(MPE42_PL310_BASE);
		/* 3 cycles of latency for setup, read and write accesses */
		data_latency = 0x222;
		/* 2 cycles of latency for setup, read and write accesses */
		tag_latency = 0x111;
	}
	else
		return -ENOSYS;

	/*
	* Tag RAM Control register
	*
	* bit[10:8]    - # cycles of write accesses latency
	* bit[6:4]     - # cycles of read accesses latency
	* bit[2:0]     - # cycles of setup latency
	*
	*/
	val = readl(base + L2X0_TAG_LATENCY_CTRL);
	val &= 0xfffff888;
	val |= tag_latency;
	writel(val, base + L2X0_TAG_LATENCY_CTRL);

	/*
	* Data RAM Control register
	*
	* bit[10:8]    - # cycles of write accesses latency
	* bit[6:4]     - # cycles of read accesses latency
	* bit[2:0]     - # cycles of setup latency
	*
	*/
	val = readl(base + L2X0_DATA_LATENCY_CTRL);
	val &= 0xfffff888;
	val |= data_latency;
	writel(val, base + L2X0_DATA_LATENCY_CTRL);

	/* We have to ensure that bit 22 is set. This bit controls if
	 * shared uncacheable normal memory accesses are looked up in the cache
	 * or not. By default they are looked up in the cache. This can cause
	 * problems because the cache line can be speculated in via the kernel
	 * alias of the same physical page. For coherent dma mappings this means
	 * that the CPU will potentially see stale values, rather than what the
	 * device has put into main memory. The stale value should not cause any
	 * problems as it should never be accessed via the kernel mapping.
	 *
	 * Instruction and Data prefetching now enabled (bit 28 and 29)
	 */
	aux_ctrl = (0x1 << L2X0_AUX_CTRL_SHARE_OVERRIDE_SHIFT) |
		(0x1 << L2X0_AUX_CTRL_DATA_PREFETCH_SHIFT) |
		(0x1 << L2X0_AUX_CTRL_INSTR_PREFETCH_SHIFT) |
		(way_size << L2X0_AUX_CTRL_WAY_SIZE_SHIFT);

	l2x0_init(base, aux_ctrl, STM_L2X0_AUX_CTRL_MASK);
#endif
	return 0;
}

arch_initcall(stm_l2x0_init);
#endif
