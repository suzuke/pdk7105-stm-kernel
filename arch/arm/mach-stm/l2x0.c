/*
 * L2 cache initialization for MPE41
 *
 * Copyright (C) 2012 STMicroelectronics Limited.
 * Author: Srinivas Kandagatla <srinivas.kandagatla@st.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */

#include <linux/init.h>
#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/stm/soc.h>
#include <mach/hardware.h>
#include <mach/mpe41.h>
#include <asm/hardware/cache-l2x0.h>

static int __init stm_l2x0_init(void)
{
#ifdef CONFIG_CACHE_L2X0
	void __iomem *base;
	u32 val;

	if (stm_soc_is_fli7610() || stm_soc_is_stih415())
		base = __io_address(MPE41_PL310_BASE);
	else
		return -ENOSYS;

	/*
	* Tag RAM Control register
	*
	* bit[10:8]    - 1 cycle of write accesses latency
	* bit[6:4]     - 1 cycle of read accesses latency
	* bit[2:0]     - 1 cycle of setup latency
	*
	* 1 cycle of latency for setup, read and write accesses
	*/
	val = readl(base + L2X0_TAG_LATENCY_CTRL);
	val &= 0xfffff888;
	writel(val, base + L2X0_TAG_LATENCY_CTRL);

	/*
	* Data RAM Control register
	*
	* bit[10:8]    - 2 cycles of write accesses latency
	* bit[6:4]     - 3 cycles of read accesses latency
	* bit[2:0]     - 2 cycle of setup latency
	*
	* 1 cycle of latency for setup, read and write accesses
	*/
	val = readl(base + L2X0_DATA_LATENCY_CTRL);
	val &= 0xfffff888;
	val |= 0xfffff9a9;
	writel(val, base + L2X0_DATA_LATENCY_CTRL);

	/* We have to ensure that bit 22 is set. This bit controls if
	 * shared uncacheable normal memory accesses are looked up in the cache
	 * or not. By default they are looked up in the cache. This can cause
	 * problems because the cache line can be speculated in via the kernel
	 * alias of the same physical page. For coherent dma mappings this means
	 * that the CPU will potentially see stale values, rather than what the
	 * device has put into main memory. The stale value should not cause any
	 * problems as it should never be accessed via the kernel mapping.
	 */
	l2x0_init(base, 0x1<<22, 0xffbfffff);
#endif
	return 0;
}

arch_initcall(stm_l2x0_init);
