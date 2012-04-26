/*
 * -------------------------------------------------------------------------
 * Copyright (C) 2012  STMicroelectronics
 * Author: Francesco M. Virlinzi  <francesco.virlinzi@st.com>
 *
 * May be copied or modified under the terms of the GNU General Public
 * License V.2 ONLY.  See linux/COPYING for more information.
 *
 * ------------------------------------------------------------------------- */

#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/suspend.h>
#include <linux/errno.h>
#include <linux/time.h>
#include <linux/delay.h>
#include <linux/irqflags.h>
#include <linux/kobject.h>
#include <linux/clk.h>
#include <linux/hardirq.h>
#include <linux/jiffies.h>
#include <linux/io.h>

#include <linux/stm/pm_notify.h>

#include <asm/idmap.h>
#include <asm/io.h>
#include <asm-generic/sections.h>
#include <asm/pgalloc.h>
#include <asm/system.h>
#include <asm/cacheflush.h>
#include <asm-generic/bug.h>

#include "suspend.h"

static struct stm_platform_suspend *platform_suspend;
static void __iomem *virtual_eram_iomem;

static unsigned long
stm_prepare_eram(struct stm_platform_suspend *platform,
		struct stm_suspend_eram_data *eram)
{
	unsigned long size = 0;
	void *__pa_eram = platform->eram_iomem;
	void *__va_eram = virtual_eram_iomem;
	/*
	 * 1. copy the __pokeloop code in eram
	 */
	eram->pa_pokeloop = __pa_eram;
	memcpy_toio(__va_eram, stm_pokeloop, stm_pokeloop_sz);
	__va_eram += stm_pokeloop_sz;
	__pa_eram += stm_pokeloop_sz;

	/*
	 * 2. copy the entry_data_table in eram
	 */
	eram->pa_table_enter = __pa_eram;
	size = platform->memstandby->enter_table_size;
	memcpy_toio(__va_eram, platform->memstandby->enter_table, size);
	__va_eram += size;
	__pa_eram += size;

	/*
	 * 3. copy the exit_data_table in eram
	 */
	eram->pa_table_exit = __pa_eram;
	size = platform->memstandby->exit_table_size;
	memcpy_toio(__va_eram, platform->memstandby->exit_table, size);
	__va_eram += size;
	__pa_eram += size;

	/*
	 * 4. copy the stm_eram code in eram
	 */
	eram->pa_stm_eram_code = __pa_eram;
	memcpy_toio(__va_eram, stm_suspend_on_eram, stm_suspend_on_eram_sz);
	__va_eram += stm_suspend_on_eram_sz;
	__pa_eram += stm_suspend_on_eram_sz;

	return stm_pokeloop_sz + stm_suspend_on_eram_sz +
		platform->memstandby->enter_table_size +
		platform->memstandby->exit_table_size;
}

static int stm_suspend_enter(suspend_state_t state)
{
	enum stm_pm_notify_ret notify_ret;
	int err = 0;
	int wake_irq = 0;
	unsigned long va_2_pa = (unsigned long)_text -
				(unsigned long)__pa(_text);
	struct stm_suspend_eram_data eram_data;

	/* Must wait for serial buffers to clear */
	pr_info("CPU is sleeping\n");
	mdelay(100);

	if (platform_suspend->pre_enter)
		err = platform_suspend->pre_enter(state);

	/*
	 * If the platform pre_enter returns an error
	 * than the suspend operation is aborted.
	 */
	if (err) {
		pr_err("[STM][PM] Error on Core Suspend\n");
		return err;
	}


	BUG_ON(in_irq());

stm_again_suspend:

	if (state == PM_SUSPEND_MEM && platform_suspend->memstandby) {
		unsigned long size;

		size = stm_prepare_eram(platform_suspend, &eram_data);

		flush_cache_all();
		flush_tlb_all();
		outer_flush_all();

		stm_suspend_exec_table(
			(struct stm_suspend_eram_data *)__pa(&eram_data),
			(void *)__pa(idmap_pgd), va_2_pa);
	} else {
		pr_info("Using standalone WFI\n");
		__asm__ __volatile__("wfi\n" : : : "memory");
	}

	pr_info("CPU woken up by: 0x%x\n", wake_irq);
	BUG_ON(in_irq());

	if (platform_suspend->get_wake_irq)
		wake_irq = platform_suspend->get_wake_irq();

	notify_ret = stm_pm_early_check(wake_irq);
	if (notify_ret == STM_PM_RET_AGAIN)
		goto stm_again_suspend;

	if (platform_suspend->post_enter)
		platform_suspend->post_enter(state);

	pr_info("CPU woken up by: 0x%x\n", wake_irq);
	return 0;
}

static int stm_suspend_valid_both(suspend_state_t state)
{
	return 1;
}

int __init stm_suspend_register(struct stm_platform_suspend *_suspend)
{
	int ioremap_size;

	if (!_suspend)
		return -EINVAL;

	ioremap_size = stm_suspend_on_eram_sz + stm_pokeloop_sz;

	if (_suspend->memstandby) {
		ioremap_size += _suspend->memstandby->enter_table_size;
		ioremap_size += _suspend->memstandby->exit_table_size;
	}
	virtual_eram_iomem = ioremap((phys_addr_t)_suspend->eram_iomem,
			ioremap_size);

	if (!virtual_eram_iomem)
		return -EINVAL;

	platform_suspend = _suspend;
	platform_suspend->ops.enter = stm_suspend_enter;
	platform_suspend->ops.valid = stm_suspend_valid_both;

	suspend_set_ops(&platform_suspend->ops);

	pr_info("[STM]: [PM]: Suspend support registered\n");

	return 0;
}
