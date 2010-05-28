/*
 * arch/arm/mach-stm/include/mach/soc-nice.h
 *
 * Copyright (C) 2010 STMicroelectronics Limited.
 * Author: Stuart Menefy <stuart.menefy@st.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __ASM_ARCH_SOC_NICE_H
#define __ASM_ARCH_SOC_NICE_H

#include <asm/mach/arch.h>	/* For struct machine_desc */

#define NICE_ASC0_BASE 0xFED30000
#define NICE_ASC1_BASE 0xFED31000
#define NICE_ASC2_BASE 0xFED32000
#define NICE_ASC3_BASE 0xFED33000

#define NICE_PIO_MPE_BASE	0xfd6b0000
#define NICE_PIO_SAS_REAR_BASE	0xfe820000
#define NICE_PIO_SAS_FRONT_BASE	0xfee00000

/* Cortex A9 private memory region */
/* Size 2*4K, base address set by PERIPHBASE[31:13] pins */
/* See table 1-3 in Cortex-A9 MPCore TRM (ARM DDI 0407) */
#define NICE_SCU_BASE		(0xFFFE0000 + 0x0000)	/* Snoop Control Unit */
#define NICE_GIC_CPU_BASE	(0xFFFE0000 + 0x0100)	/* Private Generic Interrupt Controller */
#define NICE_GLOBAL_TIMER_BASE	(0xFFFE0000 + 0x0200)	/* Global timer */
#define NICE_TWD_BASE		(0xFFFE0000 + 0x0600)	/* Private Timer Watchdog */
#define NICE_TWD_SIZE		0x100

#define NICE_GIC_DIST_BASE	(0xFFFE0000 + 0x1000)	/* Interrupt distributor */

/* PL310 control registers */
/* defined by REGFILEBASE[31:12] pins, size 0x1000 */
/* See table 301 in PrimeCell Level 2 Cache Controller (PL310) TRM (ARM DDI 0246) */
#define NICE_PL310_BASE		(0xFFFE0000 + 0x2000)

#ifndef __ASSEMBLER__

void nice_fixup(struct machine_desc *mdesc, struct tag *tags,
		char **from, struct meminfo *meminfo);
void nice_map_io(void);
void nice_gic_init_irq(void);

extern struct sys_timer nice_timer;

#endif /* __ASSEMBLER__ */

#endif
