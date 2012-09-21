/*
 * include/linux/stm/mpe41-periphs.h
 *
 * Copyright (C) 2012 STMicroelectronics Limited.
 * Author: Francesco Virlinzi <francesco.virlinzi@st.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#define MPE41_PIO_RIGHT_BASE		0xfd6b0000
#define MPE41_PIO_LEFT_BASE		0xfd330000

#define MPE41_LEFT_SYSCONF_BASE		0xfd690000 /* 400-429 */
#define MPE41_RIGHT_SYSCONF_BASE	0xfd320000 /* 500-595 */
#define MPE41_SYSTEM_SYSCONF_BASE	0xfdde0000 /* 600-686 */

#define MPE41_DDR0_PCTL_BASE		0xfdd70000
#define MPE41_DDR1_PCTL_BASE		0xfdd91000
