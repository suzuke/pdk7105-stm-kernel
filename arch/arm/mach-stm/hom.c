/*
 * -------------------------------------------------------------------------
 * Copyright (C) 2012  STMicroelectronics
 * Author: Francesco M. Virlinzi  <francesco.virlinzi@st.com>
 *
 * May be copied or modified under the terms of the GNU General Public
 * License V.2 ONLY.  See linux/COPYING for more information.
 *
 * -------------------------------------------------------------------------
 */

#include <linux/hom.h>
#include <linux/export.h>
#include <linux/stm/hom.h>
#include <linux/init.h>
#include <linux/errno.h>
#include <linux/hardirq.h>
#include <linux/jiffies.h>
#include <linux/delay.h>
#include <linux/io.h>
#include <linux/preempt.h>
#include <linux/suspend.h>

#include <linux/stm/poke_table.h>
#include <asm/idmap.h>
#include <asm/thread_info.h>
#include <asm/cacheflush.h>
#include <asm-generic/sections.h>
#include <asm-generic/bug.h>
#include <asm/mmu.h>
#include <asm/hardware/gic.h>
#include <asm/smp_scu.h>
#include <asm/mmu_context.h>
#include <asm/tlbflush.h>
#include <asm/system.h>
#include <asm/pgtable-hwdef.h>
#include <asm/pgalloc.h>

#include "core.h"
#include "hom.h"
#include "pokeloop.h"

#undef  dbg_print

#ifdef CONFIG_HOM_DEBUG
#define dbg_print(fmt, args...)		pr_info("%s: " fmt, __func__, ## args)
void hom_printk(char *buf, ...)
{
	pr_info("%s\n", buf);
}
#else
#define dbg_print(fmt, args...)
#endif

extern volatile int pen_release;

static void __iomem *virtual_eram_iomem;

static struct stm_mem_hibernation *platform;
static const unsigned long hom_end_table[] = { END_MARKER };

static void hom_init_early_console(void __iomem *asc_base,
	unsigned long asc_clk)
{
#define BAUDRATE_VAL_M1(bps, clk)				\
	((((bps * (1 << 14)) + (1 << 13)) / (clk / (1 << 6))))
	writel(0x1189 & ~0x80, asc_base + 0x0c);/* ctrl */
	writel(BAUDRATE_VAL_M1(115200, asc_clk), asc_base); /* baud */
	writel(20, asc_base + 0x1c);		/* timeout */
	writel(1, asc_base + 0x10);		/* int */
	writel(0x1189, asc_base + 0x0c);	/* ctrl */

	mdelay(100);

	pr_info("[STM][HoM]: Early console ready\n");
}

static void prepare_hom_frozen_data(struct hom_frozen_data *frozen_data,
	 pgd_t *hom_pg_dir)
{
	__asm__ __volatile__(
		"mrc	p15, 0, r2, c2, c0, 0\n"	/* TTBR0 */
		"mrc	p15, 0, r3, c2, c0, 1\n"	/* TTBR1 */
		"mrc	p15, 0, r4, c2, c0, 2\n"	/* TTBCR */
		"stm	%0, {r2 - r5}\n"
		: : "r" (&frozen_data->ttbr0)
		: "memory", "r2", "r3", "r4");

	frozen_data->pg_dir = __pa(hom_pg_dir);

#ifdef CONFIG_HOM_DEBUG
	{
	unsigned int i, *p = (unsigned int *)frozen_data;
	for (i = 0; i < (HFD_END >> 2); ++i)
		pr_info("[STM][HoM]: frozen_data[%i] = 0x%x\n", i, p[i]);
	}
#endif
}

static void stm_hom_prepare_eram(struct stm_mem_hibernation *platform,
				 struct stm_hom_eram_data *eram)
{
	void *__pa_eram = platform->eram_iomem;
	void *__va_eram = virtual_eram_iomem;
	struct hom_table *table;
	/*
	 * 1. copy the __pokeloop code in eram
	 */
	eram->pa_pokeloop = __pa_eram;
	memcpy_toio(__va_eram, stm_pokeloop, stm_pokeloop_sz);
	__va_eram += stm_pokeloop_sz;
	__pa_eram += stm_pokeloop_sz;
	/*
	 * 2. copy all the tables into eram
	 */
	eram->pa_table = __pa_eram;
	list_for_each_entry(table, &platform->table, node) {
		memcpy_toio(__va_eram, table->addr, table->size);
		__va_eram += table->size;
		__pa_eram += table->size;
	}
	memcpy_toio(__va_eram, hom_end_table,
		ARRAY_SIZE(hom_end_table) * sizeof(long));
	__va_eram += ARRAY_SIZE(hom_end_table) * sizeof(long);
	__pa_eram += ARRAY_SIZE(hom_end_table) * sizeof(long);

	/*
	 * 3. copy the the_eram code into eram
	 */
	eram->pa_stm_eram_code = __pa_eram;
	memcpy_toio(__va_eram, stm_hom_on_eram, stm_hom_on_eram_sz);
	__va_eram += stm_hom_on_eram_sz;
	__pa_eram += stm_hom_on_eram_sz;
}

static void __hom_marker(int enable)
{
	unsigned long *_ztext = (unsigned long *)
		(CONFIG_HOM_TAG_VIRTUAL_ADDRESS);
	const long linux_marker[] = {
			0x7a6f7266,	/* froz */
			0x6c5f6e65,	/* en_l */
			0x78756e69 };	/* inux */
	int i;

	for (i = 0; i < ARRAY_SIZE(linux_marker); ++i)
		_ztext[i] = enable ? linux_marker[i] : -2;
	_ztext[ARRAY_SIZE(linux_marker)] = __pa(stm_defrost_kernel);
}

static inline void hom_marker_enable(void)
{
	__hom_marker(1);
}

static inline void hom_marker_disable(void)
{
	__hom_marker(0);
}

static int __cpuinitdata stm_hom_enter(void)
{
	unsigned long flag;
	struct mm_struct *mm = get_current()->mm;
	struct stm_hom_eram_data eram_data;
	unsigned long va_2_pa = (unsigned long) _text -
				(unsigned long) __pa(_text);

#ifdef CONFIG_SMP
	/*
	 * Initialize the pen to be consistend on resume
	 */
	write_pen_release(-1);
#endif

	local_irq_save(flag);
	/*
	 * Write the Linux Frozen Marker in the Main Memory
	 */
	hom_marker_enable();

	/*
	 * prepare the data required on resume when the MMU is still disabled
	 */
	prepare_hom_frozen_data(&hom_frozen_data, idmap_pgd);

	mdelay(100);
	BUG_ON(in_irq());

	stm_hom_prepare_eram(platform, &eram_data);

	/*
	 * Flush __all__ the caches to avoid some pending write operation
	 * on the memory is still in D-cache...
	 */
	flush_cache_all();
	flush_tlb_all();
	outer_flush_all();

	pr_info("stm pm hom: CPU Frozen\n");

	stm_hom_exec_on_eram((struct stm_hom_eram_data *) __pa(&eram_data),
			     (void *)__pa(idmap_pgd), va_2_pa);

	BUG_ON(in_irq());

	/*
	 * At this point the system is enough stable
	 * but the transition to the correct virtual memory
	 * environment isn't complete...
	 * a call to 'cpu_v7_switch_mm' will complete the job
	 */
	cpu_switch_mm(mm->pgd, mm);

	/*
	 * remove the marker in memory also if the bootloader already did that
	 */
	hom_marker_disable();

	flush_cache_all();

	/*
	 * Here an __early__ console initialization to avoid
	 * blocking printk.
	 * This is required if the kernel boots with 'no_console_suspend'
	 */
	if (platform->early_console_rate &&
	    platform->early_console_base)
		hom_init_early_console(platform->early_console_base,
			platform->early_console_rate);

	if (smp_processor_id())
		pr_err("stm hom: Error: Running on the wrong CPU\n");

	flush_cache_all();

	local_flush_tlb_all();

	disable_hlt();

	/*
	 * initialize the irq/abt/und stack frame
	 */
	cpu_init();

	/*
	 * the early_trap_init isn't required because the CPU is already
	 * running on the correct pgtable where the vectors_page is already
	 * mapped
	 */

	local_irq_restore(flag);

	/*
	 * clear the empty_zero_page
	 */
	memset(__va(PFN_PHYS(page_to_pfn(empty_zero_page))), 0, 0x1000);

#ifdef CONFIG_SMP
	/*
	 * Enable the SCU if required
	 */
	if (scu_get_core_count(scu_base_addr) > 1)
		scu_enable(scu_base_addr);
#endif
	return 0;
}

int __cpuinitdata stm_hom_register(struct stm_mem_hibernation *data)
{
	struct hom_table *table;
	unsigned long table_size = 0;

	if (!data || platform)
		return -EINVAL;

	platform = data;

	list_for_each_entry(table, &platform->table, node)
		table_size += table->size;

	virtual_eram_iomem = ioremap((unsigned long) data->eram_iomem,
		stm_pokeloop_sz + table_size + stm_hom_on_eram_sz);

	platform->ops.enter = stm_hom_enter;

	if (hom_set_ops(&platform->ops)) {
		platform = NULL;
		return -EINVAL;
	}
	pr_info("stm pm hom: HoM support registered\n");
	return 0;
}
EXPORT_SYMBOL_GPL(stm_hom_register);


static struct stm_hom_board *board_hom;

int stm_freeze_board(void)
{
	if (board_hom && board_hom->freeze)
		return board_hom->freeze();
	return 0;
}

int stm_restore_board(void)
{
	if (board_hom && board_hom->restore)
		return board_hom->restore();
	return 0;
}

int stm_hom_board_register(struct stm_hom_board *board)
{
	mutex_lock(&pm_mutex);
	board_hom = board;
	mutex_unlock(&pm_mutex);
	return 0;
}
EXPORT_SYMBOL_GPL(stm_hom_board_register);

