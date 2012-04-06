/*
 * -------------------------------------------------------------------------
 * Copyright (C) 2011  STMicroelectronics
 * Author: Francesco M. Virlinzi  <francesco.virlinzi@st.com>
 *
 * May be copied or modified under the terms of the GNU General Public
 * License V.2 ONLY.  See linux/COPYING for more information.
 *
 * -------------------------------------------------------------------------
 */

#include <linux/suspend.h>
#include <asm/suspend.h>
#include <asm/sections.h>

#include <asm/tlbflush.h>
#include <asm/mmu_context.h>

unsigned long swsusp_regs[12];

extern long __nosave_begin, __nosave_end;

int pfn_is_nosave(unsigned long pfn)
{
	unsigned long begin_pfn = __pa(&__nosave_begin) >> PAGE_SHIFT;
	unsigned long end_pfn = PAGE_ALIGN(__pa(&__nosave_end)) >> PAGE_SHIFT;

	return (pfn >= begin_pfn) && (pfn < end_pfn);
}

void save_processor_state(void)
{
	flush_thread();
}

void restore_processor_state(void)
{
	struct thread_info *ctf = current_thread_info();
	struct task_struct *task = ctf->task;
	struct mm_struct *current_mm = task->active_mm;

	cpu_switch_mm(current_mm->pgd, current_mm);
	local_flush_tlb_all();
}
