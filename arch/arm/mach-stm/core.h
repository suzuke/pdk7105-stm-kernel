/*
 * linux/arch/arm/mach-stm/core.h
 *
 * Copyright (C) 2010 STMicroelectronics Limited.
 * Author: Stuart Menefy <stuart.menefy@st.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __ASM_ARCH_CORE_H
#define __ASM_ARCH_CORE_H

#include <linux/io.h>

extern void __iomem *gic_cpu_base_addr;
extern void __iomem *scu_base_addr;

#endif
