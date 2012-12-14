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

#ifndef __ASM_ARCH_SOC_STIH416_H
#define __ASM_ARCH_SOC_STIH416_H

#include <asm/mach/arch.h>	/* For struct machine_desc */
#include <linux/stm/mpe42-periphs.h>

#ifndef __ASSEMBLER__

void stih416_fixup(struct machine_desc *mdesc, struct tag *tags,
		char **from, struct meminfo *meminfo);
void stih416_map_io(void);
void stih416_gic_init_irq(void);

extern struct sys_timer stih416_timer;

#endif /* __ASSEMBLER__ */

#endif
