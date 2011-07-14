/*
 * arch/arm/mach-stm/include/mach/soc-stih415.h
 *
 * Copyright (C) 2010 STMicroelectronics Limited.
 * Author: Stuart Menefy <stuart.menefy@st.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __ASM_ARCH_SOC_STIH415_H
#define __ASM_ARCH_SOC_STIH415_H

#include <asm/mach/arch.h>	/* For struct machine_desc */
#include <linux/stm/stih415-periphs.h>

/* Cortex A9 private memory region */
/* Size 2*4K, base address set by PERIPHBASE[31:13] pins */
/* See table 1-3 in Cortex-A9 MPCore TRM (ARM DDI 0407) */
#define STIH415_SCU_BASE		(0xFFFE0000 + 0x0000)	/* Snoop Control Unit */
#define STIH415_GIC_CPU_BASE		(0xFFFE0000 + 0x0100)	/* Private Generic Interrupt Controller */
#define STIH415_GLOBAL_TIMER_BASE	(0xFFFE0000 + 0x0200)	/* Global timer */
#define STIH415_TWD_BASE		(0xFFFE0000 + 0x0600)	/* Private Timer Watchdog */
#define STIH415_TWD_SIZE		0x100

#define STIH415_GIC_DIST_BASE		(0xFFFE0000 + 0x1000)	/* Interrupt distributor */

/* PL310 control registers */
/* defined by REGFILEBASE[31:12] pins, size 0x1000 */
/* See table 301 in PrimeCell Level 2 Cache Controller (PL310) TRM (ARM DDI 0246) */
#define STIH415_PL310_BASE		(0xFFFE0000 + 0x2000)

#ifndef __ASSEMBLER__

void stih415_fixup(struct machine_desc *mdesc, struct tag *tags,
		char **from, struct meminfo *meminfo);
void stih415_map_io(void);
void stih415_gic_init_irq(void);

extern struct sys_timer stih415_timer;

#endif /* __ASSEMBLER__ */

#endif
