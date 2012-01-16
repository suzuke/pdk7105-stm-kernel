
/*
 * arch/arm/mach-stm/include/mach/soc-fli7610.h
 *
 * Copyright (C) 2011 STMicroelectronics Limited.
 * Author: Srinivas Kandagatla <srinivas.kandagatla@st.com>
 *
 * copied from soc-fli7610.h
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __ASM_ARCH_SOC_FLI7610_H
#define __ASM_ARCH_SOC_FLI7610_H

#include <asm/mach/arch.h>	/* For struct machine_desc */
#include <linux/stm/fli7610-periphs.h>

/* Cortex A9 private memory region */
/* Size 2*4K, base address set by PERIPHBASE[31:13] pins */
/* See table 1-3 in Cortex-A9 MPCore TRM (ARM DDI 0407) */
/* Snoop Control Unit */
#define FLI7610_SCU_BASE		(0xFFFE0000 + 0x0000)
/* Private Generic Interrupt Controller */
#define FLI7610_GIC_CPU_BASE		(0xFFFE0000 + 0x0100)
/* Global timer */
#define FLI7610_GLOBAL_TIMER_BASE	(0xFFFE0000 + 0x0200)
/* Private Timer Watchdog */
#define FLI7610_TWD_BASE		(0xFFFE0000 + 0x0600)
#define FLI7610_TWD_SIZE		0x100

/* Interrupt distributor */
#define FLI7610_GIC_DIST_BASE		(0xFFFE0000 + 0x1000)

/* PL310 control registers */
/* defined by REGFILEBASE[31:12] pins, size 0x1000 */
/* See table 301 in PrimeCell Level 2 Cache Controller (PL310)
 * TRM (ARM DDI 0246) */
#define FLI7610_PL310_BASE		(0xFFFE0000 + 0x2000)

#ifndef __ASSEMBLER__

void fli7610_fixup(struct machine_desc *mdesc, struct tag *tags,
		char **from, struct meminfo *meminfo);
void fli7610_map_io(void);
void fli7610_gic_init_irq(void);

extern struct sys_timer fli7610_timer;

#endif /* __ASSEMBLER__ */

#endif  /* __ASM_ARCH_SOC_FLI7610_H */
