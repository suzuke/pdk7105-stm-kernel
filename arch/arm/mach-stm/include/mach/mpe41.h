/*
 * arch/arm/mach-stm/include/mach/mpe41.h
 *
 * Copyright (C) 2012 STMicroelectronics Limited.
 * Author: Srinivas Kandagatla <srinivas.kandagatla@st.com>
 *
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __ASM_ARCH_SOC_MPE41_H
#define __ASM_ARCH_SOC_MPE41_H

/* Cortex A9 private memory region */
/* Size 2*4K, base address set by PERIPHBASE[31:13] pins */
/* See table 1-3 in Cortex-A9 MPCore TRM (ARM DDI 0407) */
/* Snoop Control Unit */
#define MPE41_SCU_BASE		(0xFFFE0000 + 0x0000)
/* Private Generic Interrupt Controller */
#define MPE41_GIC_CPU_BASE		(0xFFFE0000 + 0x0100)
/* Global timer */
#define MPE41_GLOBAL_TIMER_BASE	(0xFFFE0000 + 0x0200)
/* Private Timer Watchdog */
#define MPE41_TWD_BASE		(0xFFFE0000 + 0x0600)
#define MPE41_TWD_SIZE		0x100

/* Interrupt distributor */
#define MPE41_GIC_DIST_BASE		(0xFFFE0000 + 0x1000)

/* PL310 control registers */
/* defined by REGFILEBASE[31:12] pins, size 0x1000 */
/* See table 301 in PrimeCell Level 2 Cache Controller (PL310)
 * TRM (ARM DDI 0246) */
#define MPE41_PL310_BASE		(0xFFFE0000 + 0x2000)

#ifndef __ASSEMBLER__

void mpe41_gic_init_irq(void);

extern struct sys_timer mpe41_timer;

#endif /* __ASSEMBLER__ */


#endif  /* __ASM_ARCH_SOC_MPE41_H */
