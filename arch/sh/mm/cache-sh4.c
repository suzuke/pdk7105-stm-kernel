/*
 * arch/sh/mm/cache-sh4.c
 *
 * Copyright (C) 1999, 2000, 2002  Niibe Yutaka
 * Copyright (C) 2001 - 2009  Paul Mundt
 * Copyright (C) 2003  Richard Curnow
 * Copyright (c) 2007 STMicroelectronics (R&D) Ltd.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 */
#include <linux/init.h>
#include <linux/mm.h>
#include <linux/io.h>
#include <linux/mutex.h>
#include <linux/fs.h>
#include <linux/highmem.h>
#include <asm/pgtable.h>
#include <asm/mmu_context.h>
#include <asm/cacheflush.h>
#include <linux/module.h>

/*
 * The maximum number of pages we support up to when doing ranged dcache
 * flushing. Anything exceeding this will simply flush the dcache in its
 * entirety.
 */
#define MAX_ICACHE_PAGES	32

static void __flush_cache_one(unsigned long addr,
		unsigned long phys, int way_count, unsigned long way_incr);
static void (*__flush_cache_one_uncached)(unsigned long addr,
		unsigned long phys, int way_count, unsigned long way_incr);

/*
 * Write back the range of D-cache, and purge the I-cache.
 *
 * Called from kernel/module.c:sys_init_module and routine for a.out format,
 * signal handler code and kprobes code
 */
static void sh4_flush_icache_range(void *args)
{
	struct flusher_data *data = args;
	unsigned long start, end;
	unsigned long flags, v;
	int i;

	start = data->addr1;
	end = data->addr2;

	/* If there are too many pages then just blow away the caches */
	if (((end - start) >> PAGE_SHIFT) >= MAX_ICACHE_PAGES) {
		local_flush_cache_all(NULL);
		return;
	}

	/*
	 * Selectively flush d-cache then invalidate the i-cache.
	 * This is inefficient, so only use this for small ranges.
	 */
	start &= ~(L1_CACHE_BYTES-1);
	end += L1_CACHE_BYTES-1;
	end &= ~(L1_CACHE_BYTES-1);

	local_irq_save(flags);
	jump_to_uncached();

	for (v = start; v < end; v += L1_CACHE_BYTES) {
		unsigned long icacheaddr;
		int j, n;

		__ocbwb(v);

		icacheaddr = CACHE_IC_ADDRESS_ARRAY | (v &
				cpu_data->icache.entry_mask);

		/* Clear i-cache line valid-bit */
		n = boot_cpu_data.icache.n_aliases;
		for (i = 0; i < cpu_data->icache.ways; i++) {
			for (j = 0; j < n; j++)
				__raw_writel(0, icacheaddr + (j * PAGE_SIZE));
			icacheaddr += cpu_data->icache.way_incr;
		}
	}

	back_to_cached();
	local_irq_restore(flags);
}

static inline void flush_cache_one(unsigned long start, unsigned long kaddr)
{
	unsigned long flags;
	struct cache_info *cache;
	int way_count;
	unsigned long way_incr;
	void (*fco)(unsigned long addr, unsigned long kaddr, int way_count,
			unsigned long way_incr);

	/*
	 * All types of SH-4 require PC to uncached to operate on the I-cache.
	 * Some types of SH-4 require PC to be uncached to operate on the
	 * D-cache.
	 */

	if (unlikely(start < CACHE_OC_ADDRESS_ARRAY)){
		cache = &boot_cpu_data.icache;
		fco = __flush_cache_one_uncached;
	} else {
		cache = &boot_cpu_data.dcache;
		fco = __flush_cache_one;
	}

	if (unlikely(boot_cpu_data.flags & CPU_HAS_P2_FLUSH_BUG))
		fco = __flush_cache_one_uncached;

	way_count = cache->ways;
	way_incr = cache->way_incr;

	local_irq_save(flags);
	fco(start, kaddr, way_count, way_incr);
	local_irq_restore(flags);
}

/*
 * Called just before the kernel reads a page cache page, or has written
 * to a page cache page, which may have been mapped into user space.
 * Write back & invalidate the D-cache of the page.
 * (To avoid "alias" issues)
 */
static void sh4_flush_dcache_page(void *arg)
{
	struct page *page = arg;
#ifndef CONFIG_SMP
	struct address_space *mapping = page_mapping(page);

	if (mapping && !mapping_mapped(mapping))
		clear_bit(PG_dcache_clean, &page->flags);
	else
#endif
		/* page->mapping is NULL for argv/env pages, which
		 * must be flushed here (there is no call to
		 * update_mmu_cache in this case). Or there is a user
		 * mapping for this page, so we flush. */
		flush_kernel_dcache_page(page);

	wmb();
}

void flush_kernel_dcache_page_addr(unsigned long kaddr)
{
	unsigned long addr = CACHE_OC_ADDRESS_ARRAY;
	int i, n;

	/* Loop all the D-cache */
	n = boot_cpu_data.dcache.n_aliases;
	for (i = 0; i < n; i++, addr += PAGE_SIZE)
		flush_cache_one(addr, kaddr);
}
EXPORT_SYMBOL(flush_kernel_dcache_page_addr);

/* TODO: Selective icache invalidation through IC address array.. */
static void flush_icache_all(void)
{
	unsigned long flags, ccr;

	local_irq_save(flags);
	jump_to_uncached();

	/* Flush I-cache */
	ccr = __raw_readl(CCR);
	ccr |= CCR_CACHE_ICI;
	__raw_writel(ccr, CCR);

	/*
	 * back_to_cached() will take care of the barrier for us, don't add
	 * another one!
	 */

	back_to_cached();
	local_irq_restore(flags);
}

static void flush_dcache_all(void)
{
	unsigned long addr, end_addr, entry_offset;

	end_addr = CACHE_OC_ADDRESS_ARRAY +
		(current_cpu_data.dcache.sets <<
		 current_cpu_data.dcache.entry_shift) *
			current_cpu_data.dcache.ways;

	entry_offset = 1 << current_cpu_data.dcache.entry_shift;

	for (addr = CACHE_OC_ADDRESS_ARRAY; addr < end_addr; ) {
		__raw_writel(0, addr); addr += entry_offset;
		__raw_writel(0, addr); addr += entry_offset;
		__raw_writel(0, addr); addr += entry_offset;
		__raw_writel(0, addr); addr += entry_offset;
		__raw_writel(0, addr); addr += entry_offset;
		__raw_writel(0, addr); addr += entry_offset;
		__raw_writel(0, addr); addr += entry_offset;
		__raw_writel(0, addr); addr += entry_offset;
	}
}

static void sh4_flush_cache_all(void *unused)
{
	flush_dcache_all();
	flush_icache_all();
}

/*
 * Note : (RPC) since the caches are physically tagged, the only point
 * of flush_cache_mm for SH-4 is to get rid of aliases from the
 * D-cache.  The assumption elsewhere, e.g. flush_cache_range, is that
 * lines can stay resident so long as the virtual address they were
 * accessed with (hence cache set) is in accord with the physical
 * address (i.e. tag).  It's no different here.
 *
 * Caller takes mm->mmap_sem.
 */
static void sh4_flush_cache_mm(void *arg)
{
	struct mm_struct *mm = arg;

	if (cpu_context(smp_processor_id(), mm) == NO_CONTEXT)
		return;

	flush_dcache_all();
}

/*
 * Write back and invalidate I/D-caches for the page.
 *
 * ADDR: Virtual Address (U0 address)
 * PFN: Physical page number
 */
static void sh4_flush_cache_page(void *args)
{
	struct flusher_data *data = args;
	struct vm_area_struct *vma;
	unsigned long address, pfn, kaddr;
	unsigned int alias_mask;

	vma = data->vma;
	address = data->addr1 & PAGE_MASK;
	pfn = data->addr2;
	kaddr = (unsigned long)pfn_to_kaddr(pfn);

	if (cpu_context(smp_processor_id(), vma->vm_mm) == NO_CONTEXT)
		return;

	alias_mask = boot_cpu_data.dcache.alias_mask;

	/* We only need to flush D-cache when we have alias */
	if ((address^kaddr) & alias_mask) {
		/* Loop 4K of the D-cache */
		flush_cache_one(
			CACHE_OC_ADDRESS_ARRAY | (address & alias_mask),
			kaddr);
		/* Loop another 4K of the D-cache */
		flush_cache_one(
			CACHE_OC_ADDRESS_ARRAY | (kaddr & alias_mask),
			kaddr);
	}

	if (vma->vm_flags & VM_EXEC) {
		alias_mask = boot_cpu_data.dcache.alias_mask;
		/*
		 * Evict entries from the portion of the cache from which code
		 * may have been executed at this address (virtual).  There's
		 * no need to evict from the portion corresponding to the
		 * physical address as for the D-cache, because we know the
		 * kernel has never executed the code through its identity
		 * translation.
		 */
		flush_cache_one(
			CACHE_IC_ADDRESS_ARRAY | (address & alias_mask),
			kaddr);
	}
}

/*
 * Write back and invalidate D-caches.
 *
 * START, END: Virtual Address (U0 address)
 *
 * NOTE: We need to flush the _physical_ page entry.
 * Flushing the cache lines for U0 only isn't enough.
 * We need to flush for P1 too, which may contain aliases.
 */
static void sh4_flush_cache_range(void *args)
{
	struct flusher_data *data = args;
	struct vm_area_struct *vma;
	unsigned long start, end;

	vma = data->vma;
	start = data->addr1;
	end = data->addr2;

	if (cpu_context(smp_processor_id(), vma->vm_mm) == NO_CONTEXT)
		return;

	/*
	 * If cache is only 4k-per-way, there are never any 'aliases'.  Since
	 * the cache is physically tagged, the data can just be left in there.
	 */
	if (boot_cpu_data.dcache.n_aliases == 0)
		return;

	flush_dcache_all();

	if (vma->vm_flags & VM_EXEC)
		flush_icache_all();
}

/**
 * __flush_cache_one
 *
 * @addr:  address in memory mapped cache array
 * @phys:  address to flush (has to match tags if addr has 'A' bit
 *         set i.e. associative write)
 *
 * The offset into the cache array implied by 'addr' selects the
 * 'colour' of the virtual address range that will be flushed.  The
 * operation (purge/write-back) is selected by the lower 2 bits of
 * 'phys'.
 */
static void __flush_cache_one(unsigned long addr,
		unsigned long phys, int way_count, unsigned long way_incr)
{
	unsigned long base_addr = addr;
	unsigned long a, ea, p;

	/*
	 * We know there will be >=1 iteration, so write as do-while to avoid
	 * pointless nead-of-loop check for 0 iterations.
	 */
	do {
		ea = base_addr + PAGE_SIZE;
		a = base_addr;
		p = phys;

		do {
			*(volatile unsigned long *)a = p;
			/*
			 * Next line: intentionally not p+32, saves an add, p
			 * will do since only the cache tag bits need to
			 * match.
			 */
			*(volatile unsigned long *)(a+32) = p;
			a += 64;
			p += 64;
		} while (a < ea);

		base_addr += way_incr;
	} while (--way_count != 0);
}

extern void __weak sh4__flush_region_init(void);

/*
 * SH-4 has virtually indexed and physically tagged cache.
 */
void __init sh4_cache_init(void)
{
	printk("PVR=%08x CVR=%08x PRR=%08x\n",
		__raw_readl(CCN_PVR),
		__raw_readl(CCN_CVR),
		__raw_readl(CCN_PRR));

	/*
	 * Pre-calculate the address of the uncached version of
	 * __flush_cache_4096 so we can call it directly.
	 */
	__flush_cache_one_uncached =
		&__flush_cache_one + cached_to_uncached;

	local_flush_icache_range	= sh4_flush_icache_range;
	local_flush_dcache_page		= sh4_flush_dcache_page;
	local_flush_cache_all		= sh4_flush_cache_all;
	local_flush_cache_mm		= sh4_flush_cache_mm;
	local_flush_cache_dup_mm	= sh4_flush_cache_mm;
	local_flush_cache_page		= sh4_flush_cache_page;
	local_flush_cache_range		= sh4_flush_cache_range;

	sh4__flush_region_init();
}
