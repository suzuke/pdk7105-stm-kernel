/*
 * arch/arm/include/asm/smp_gt.h
 *
 * Copyright (C) 2010 STMicroelectronics Limited.
 * Author: Stuart Menefy <stuart.menefy@st.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

void __init global_timer_init(void __iomem *base, unsigned int timer_irq);
void __init smp_gt_of_register(void);
