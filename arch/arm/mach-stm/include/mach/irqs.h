/*
 * arch/arm/mach-stm/include/mach/irqs.h
 *
 * Copyright (C) 2010 STMicroelectronics Limited.
 * Author: Stuart Menefy <stuart.menefy@st.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __ASM_ARCH_IRQS_H
#define __ASM_ARCH_IRQS_H

#include <mach/irqs-stih415.h>

#define IRQ_GLOBALTIMER		27
#define IRQ_LOCALTIMER		29
#define IRQ_LOCALWDOG		30

#define IRQ_GIC_START		32

#define NR_PIO_IRQS (8*27)
#define NR_IRQS			(IRQ_GIC_START + 224 + NR_PIO_IRQS)

#endif
