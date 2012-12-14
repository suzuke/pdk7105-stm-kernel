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

#define IRQ_GLOBALTIMER		27
#define IRQ_LOCALTIMER		29
#define IRQ_LOCALWDOG		30

#define IRQ_GIC_START		32

#if defined(CONFIG_MACH_STM_STIH415)
# define NR_GPIO_PORTS		27
#elif defined(CONFIG_MACH_STM_STIH416)
# define NR_GPIO_PORTS		30
#elif defined(CONFIG_MACH_STM_FLI7610)
# define NR_GPIO_PORTS		34
#elif defined(CONFIG_MACH_STM_STIG125)
# define NR_GPIO_PORTS		27
#else
# error	NR_GPIO_PORTS not defined for this chip
#endif

#define NR_PIO_IRQS	(8 * NR_GPIO_PORTS)
#define NR_MSI_IRQS	32
#define NR_IRQS		(IRQ_GIC_START + 224 + NR_PIO_IRQS + NR_MSI_IRQS)

#endif
