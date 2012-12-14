/*
 * arch/arm/mach-stm/include/mach/soc-stih416.h
 *
 * Copyright (C) 2012 STMicroelectronics Limited.
 * Author: Francesco Virlinzi <francesco.virlinzi@st.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __ASM_ARCH_SOC_MPE42_H
#define __ASM_ARCH_SOC_MPE42_H

/* Cortex A9 private memory region */
/* Size 2*4K, base address set by PERIPHBASE[31:13] pins */
/* See table 1-3 in Cortex-A9 MPCore TRM (ARM DDI 0407) */

#define MPE42_ARM_INTERNAL		0xFFFE0000

#define MPE42_SCU_BASE		(MPE42_ARM_INTERNAL + 0x0000)
#define MPE42_GIC_CPU_BASE	(MPE42_ARM_INTERNAL + 0x0100)
#define MPE42_GLOBAL_TIMER_BASE	(MPE42_ARM_INTERNAL + 0x0200)
#define MPE42_TWD_BASE		(MPE42_ARM_INTERNAL + 0x0600)
#define MPE42_TWD_SIZE		0x100

#define MPE42_GIC_DIST_BASE	(MPE42_ARM_INTERNAL + 0x1000)

/* PL310 control registers
 * defined by REGFILEBASE[31:12] pins, size 0x1000
 * See table 301 in:
 * PrimeCell Level 2 Cache Controller (PL310) TRM (ARM DDI 0246)
 */
#define MPE42_PL310_BASE	(MPE42_ARM_INTERNAL + 0x2000)

#ifndef __ASSEMBLER__

void mpe42_gic_init_irq(void);

void mpe42_map_io(void);

extern struct sys_timer mpe42_timer;

#endif /* __ASSEMBLER__ */

#endif
