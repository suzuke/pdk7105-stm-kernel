/*
 * Copyright (C) 2012 STMicroelectronics Limited.
 *
 * Author: Francesco Virlinzi <francesco.virlinzi@st.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __ASM_ARCH_SOC_STID127_H
#define __ASM_ARCH_SOC_STID127_H

#include <linux/stm/stid127.h>

/* Cortex A9 private memory region */
/* Size 2*4K, base address set by PERIPHBASE[31:13] pins */
/* See table 1-3 in Cortex-A9 MPCore TRM (ARM DDI 0407) */

/* Snoop Control Unit */
#define STID127_SCU_BASE		(0xFFFE0000 + 0x0000)

/* Private Generic Interrupt Controller */
#define STID127_GIC_CPU_BASE		(0xFFFE0000 + 0x0100)

/* Global timer */
#define STID127_GLOBAL_TIMER_BASE	(0xFFFE0000 + 0x0200)

/* Private Timer Watchdog */
#define STID127_TWD_BASE		(0xFFFE0000 + 0x0600)
#define STID127_TWD_SIZE		0x100

/* Interrupt distributor */
#define STID127_GIC_DIST_BASE		(0xFFFE0000 + 0x1000)

/* PL310 control registers */
/* defined by REGFILEBASE[31:12] pins, size 0x1000 */
/* See table 301 in PrimeCell Level 2 Cache Controller
 * (PL310)TRM (ARM DDI 0246) */
#define STID127_PL310_BASE		(0xFFFE0000 + 0x2000)

#ifndef __ASSEMBLER__

void stid127_map_io(void);
void stid127_gic_init_irq(void);
extern struct sys_timer stid127_timer;

#endif /* __ASSEMBLER__ */

#endif
