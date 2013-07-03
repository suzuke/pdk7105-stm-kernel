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
#include <linux/clk.h>
#include <linux/of.h>
#include <linux/of_address.h>

#include <linux/stm/platform.h>

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
static void __iomem *virtual_tag_iomem;

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

	pr_info("stm pm hom: Early console ready\n");
}

static void prepare_hom_frozen_data(struct hom_frozen_data *frozen_data,
	 pgd_t *hom_pg_dir)
{
	__asm__ __volatile__(
		"mrc	p15, 0, r2, c2, c0, 0\n"	/* TTBR0 */
		"mrc	p15, 0, r3, c2, c0, 1\n"	/* TTBR1 */
		"mrc	p15, 0, r4, c2, c0, 2\n"	/* TTBCR */
		"stm	%0, {r2 - r4}\n"
		: : "r" (&frozen_data->ttbr0)
		: "memory", "r2", "r3", "r4");

	frozen_data->pg_dir = __pa(hom_pg_dir);

#ifdef CONFIG_HOM_DEBUG
	{
	unsigned int i, *p = (unsigned int *)frozen_data;
	for (i = 0; i < (HFD_END >> 2); ++i)
		pr_info("stm pm hom: frozen_data[%i] = 0x%x\n", i, p[i]);
	__asm__ __volatile__("mrs	%0, cpsr\n"
				: "=r" (i) : : );
	pr_info("stm pm hom: cpsr = %x\n", i);
	i = 0;
	__asm__ __volatile__("add	%0, %0, sp\n"
				: "+r" (i) : : );
	pr_info("stm pm hom: sp = %x\n", i);
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
	const long linux_marker[] = {
			0x7a6f7266,	/* froz */
			0x6c5f6e65,	/* en_l */
			0x78756e69 };	/* inux */
	int i;

	for (i = 0; i < ARRAY_SIZE(linux_marker); ++i)
		writel(enable ? linux_marker[i] : -2,
			virtual_tag_iomem + i * sizeof(long));
	writel(__pa(stm_defrost_kernel), virtual_tag_iomem +
		ARRAY_SIZE(linux_marker) * sizeof(long));
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

	hom_mark_step(0x10);
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

	hom_mark_step(0x20);
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

	hom_mark_step(0x30);
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

	hom_mark_step(0x40);
#ifdef CONFIG_SMP
	/*
	 * Enable the SCU if required
	 */
	if (scu_get_core_count(scu_base_addr) > 1)
		scu_enable(scu_base_addr);
#endif
	hom_mark_step(0x50);
	return 0;
}

#ifdef CONFIG_OF
void stm_hom_parse_early_console(u32 *console_base,
				const char **clk)
{
	struct device_node *np = NULL;
	const char *name;
	u64 size;
	unsigned int flags;
	const __be32 *addrp;
	const char *clkname;
	if (of_chosen) {
		name = of_get_property(of_chosen, "linux,stdout-path", NULL);
		if (name == NULL)
			return;

		np = of_find_node_by_path(name);
		if (!np)
			return;
	}
	addrp = of_get_address(np, 0, &size, &flags);
	if (addrp)
		*console_base = be32_to_cpu(*addrp);

	if (!of_property_read_string(np, "st,clk-id",  &clkname))
		*clk = clkname;
	of_node_put(np);
	return;
}

#else

void stm_hom_parse_early_console(u32 *console_base,
				const char **clk)
{

	struct stm_plat_asc_data *console_pdata =
		stm_asc_console_device->dev.platform_data;
	*console_base = stm_asc_console_device->resource[0].start;
	*clk = console_pdata->clk_id;
	return;
}

#endif
static void __cpuinitdata
stm_hom_early_console_setup(struct stm_mem_hibernation *platform)
{
	struct clk *asc_clk;
	const char *clkname = NULL;
	u32 console_base;
	stm_hom_parse_early_console(&console_base, &clkname);
	/* setup the early console */
	platform->early_console_base = (void *)ioremap(console_base, 0x1000);

	asc_clk = clk_get(NULL, clkname ? : "comms_clk");

	if (IS_ERR(asc_clk))
		pr_info("stm pm hom: Failed to get clk info:%s\n", clkname);
	else
		platform->early_console_rate = clk_get_rate(asc_clk);

	pr_info("stm pm hom: early console: iobase: 0x%x; clk: %luMHz\n",
		(unsigned int) platform->early_console_base,
		(unsigned long)platform->early_console_rate / 1000000);
}

int __cpuinitdata stm_hom_register(struct stm_mem_hibernation *data)
{
	struct hom_table *table;
	unsigned long table_size = 0;
	void *va_tag_iomem = __va(CONFIG_HOM_TAG_PHYSICAL_ADDRESS);

	if (va_tag_iomem < (void *)CONFIG_PAGE_OFFSET ||
	    va_tag_iomem >= (void *)swapper_pg_dir) {
		pr_err("stm pm hom: Error: HoM Tag is out of memory space!\n");
		return -EINVAL;
	}

	virtual_tag_iomem = (void *)va_tag_iomem;

	if (!data || platform)
		return -EINVAL;

	platform = data;

	list_for_each_entry(table, &platform->table, node)
		table_size += table->size;

	virtual_eram_iomem = ioremap((unsigned long) data->eram_iomem,
		stm_pokeloop_sz + table_size + stm_hom_on_eram_sz);

	platform->ops.enter = stm_hom_enter;

	/* setup the early console */
	stm_hom_early_console_setup(platform);

	if (hom_set_ops(&platform->ops)) {
		platform = NULL;
		return -EINVAL;
	}
	pr_info("stm pm hom: HoM support registered\n");
	return 0;
}
EXPORT_SYMBOL_GPL(stm_hom_register);

int stm_freeze_board(struct stm_wakeup_devices *dev_wk)
{
	if (platform->board->freeze)
		return platform->board->freeze(dev_wk);
	return 0;
}

int stm_restore_board(struct stm_wakeup_devices *dev_wk)
{
	if (platform->board->restore)
		return platform->board->restore(dev_wk);
	return 0;
}

int __init stm_setup_lmi_retention_gpio(struct stm_mem_hibernation *platform,
	unsigned long lmi_retention_table[])
{
	int lmi_gpio_port, lmi_gpio_pin;
	int ret;
	struct stm_hom_board *board = platform->board;

	if (!board)
		return -EINVAL;

	ret = gpio_request(board->lmi_retention_gpio, "LMI retention mode");
	if (ret) {
		pr_err("stm pm hom: GPIO for retention mode not acquired\n");
		return ret;
	};


	lmi_gpio_port = stm_gpio_port(board->lmi_retention_gpio);
	lmi_gpio_pin = stm_gpio_pin(board->lmi_retention_gpio);

	pr_info("stm pm hom: LMI_Retention GPIO[%d][%d]\n",
		lmi_gpio_port, lmi_gpio_pin);

	gpio_direction_output(board->lmi_retention_gpio, 1);

	/*
	 * Update the lmi_retention_table based on the
	 * lmi_retention gpio this board uses. Table format:
	 *    [0] OP_POKE32
	 *    [1] address
	 *    [2] value
	 */
	lmi_retention_table[1] = STM_GPIO_REG_CLR_POUT + platform->gpio_iomem +
		lmi_gpio_port * 0x1000;
	lmi_retention_table[2] = 1 << lmi_gpio_pin;

	return 0;
}
