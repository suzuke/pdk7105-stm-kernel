/*
 *  linux/arch/arm/mach-stm/hotplug.c
 *
 *  Copyright (C) 2002-2013 ARM Ltd.
 *  All Rights Reserved
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/smp.h>
#include <linux/cpu_pm.h>

#include <asm/cacheflush.h>

extern volatile int pen_release;

static inline void cpu_enter_lowpower(void)
{
	unsigned int v;

	flush_cache_all();
	asm volatile(
	/* Instruction and Data Memory Barrier */
	"	mcr	p15, 0, %1, c7, c5, 0\n"
	"	mcr	p15, 0, %1, c7, c10, 4\n"
	/*
	 * Auxiliary Control register:
	 *  - Don't take part in coherency
	 */
	"	mrc	p15, 0, %0, c1, c0, 1\n"
	"	bic	%0, %0, #0x40\n"
	"	mcr	p15, 0, %0, c1, c0, 1\n"
	/* System Control register:
	 * - Turn off Dcache
	 */
	"	mrc	p15, 0, %0, c1, c0, 0\n"
	"	bic	%0, %0, #0x4\n"
	"	mcr	p15, 0, %0, c1, c0, 0\n"
	  : "=&r" (v) : "r" (0) : "cc");
}

static inline void cpu_leave_lowpower(void)
{

	unsigned int v;

	asm volatile(
	/* System Control register:
	 * - Turn on Dcache
	 */
	"	mrc	p15, 0, %0, c1, c0, 0\n"
	"	orr	%0, %0, #0x4\n"
	"	mcr	p15, 0, %0, c1, c0, 0\n"
	/*
	 * Auxiliary Control register:
	 * - Take part in coherency
	 */
	"	mrc	p15, 0, %0, c1, c0, 1\n"
	"	orr	%0, %0, #0x40\n"
	"	mcr	p15, 0, %0, c1, c0, 1\n"

	  : "=&r" (v) : : "cc");
}

static inline void platform_do_lowpower(unsigned int cpu, int *spurious)
{
	/*
	 * there is no power-control hardware on this platform, so all
	 * we can do is put the core into WFI; this is safe as the calling
	 * code will have already disabled interrupts
	 */
	for (;;) {
		/*
		 * here's the WFI
		 */
		asm("wfi" : : : "memory");

		if (pen_release == cpu) {
			/*
			 * OK, proper wakeup, we're done
			 */
			break;
		}

		/*
		 * Getting here, means that we have come out of WFI without
		 * having been woken up - this shouldn't happen
		 *
		 * Just note it happening - when we're woken, we can report
		 * its occurrence.
		 */
		(*spurious)++;
	}
}

int platform_cpu_kill(unsigned int cpu)
{
	return 1;
}

/*
 * platform-specific code to shutdown a CPU
 *
 * Called with IRQs disabled
 */
void __cpuinit platform_cpu_die(unsigned int cpu)
{
	int spurious = 0;

	cpu_pm_enter();

	/*
	 * we're ready for shutdown now, so do it
	 */
	cpu_enter_lowpower();
	platform_do_lowpower(cpu, &spurious);

	/*
	 * bring this CPU back into the world of cache
	 * coherency, and then restore interrupts
	 */
	cpu_leave_lowpower();

	if (spurious)
		pr_warning("CPU%u: %u spurious wakeup calls\n", cpu, spurious);

	cpu_pm_exit();
}

int platform_cpu_disable(unsigned int cpu)
{
	/*
	 * we don't allow CPU 0 to be shutdown (it is still too special
	 * e.g. clock tick interrupts)
	 */
	return cpu == 0 ? -EPERM : 0;
}
