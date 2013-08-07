/*
 * Copyright (C) 2013 STMicroelectronics Limited.
 *
 * Author: Giuseppe Cavallaro <peppe.cavallaro@st.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#ifndef __LINUX_STM_STID127_H
#define __LINUX_STM_STID127_H

#include <linux/stm/platform.h>

/* Peripheral macros */
#define STID127_ASC0_BASE			0xfe530000
#define STID127_ASC2_BASE			0xfe532000
#define STID127_PIO_PEAST0_BASE		0xfebc0000
#define STID127_PIO_WEST0_BASE		0xfebe0000
#define STID127_PIO_SOUTH0_BASE		0xfef70000
#define STID127_SYSCONF_HD_BASE		0xfe930000
#define STID127_SYSCONF_CPU_BASE	0xfe9a0000
#define STID127_SYSCONF_PEAST_BASE	0xfebd0000
#define STID127_SYSCONF_WEST_BASE	0xfebf0000
#define STID127_SYSCONF_PWEST_BASE	0xfec00000
#define STID127_SYSCONF_DOCSIS_BASE	0xfef90000
#define STID127_SYSCONF_SOUTH_BASE	0xfefa0000
#define STID127_SYSCONF_PSOUTH_BASE	0xfefd0000

#define STID127_DDR_PCTL_BASE		0xfe990000

/* Sysconfig defines */
#define SYSCONFG_GROUP(x) \
	( ((x) >= 1400) ? 7 : ((x) >= 1200) ? 6 : ((x) >= 1000) ? 5 :	\
	((x) >= 900) ? 4 : ((x) >= 700) ? 3 : ((x) >= 600) ? 2 :	\
	((x) >= 200) ? 1 : 0)

#define SYSCONF_OFFSET(x) \
	((x) % 100)

#define SYSCONF(x) \
	SYSCONFG_GROUP(x), SYSCONF_OFFSET(x)

/* Clk */
int stid127_plat_clk_init(void);
int stid127_plat_clk_alias_init(void);
#endif /* __LINUX_STM_STID127_H */
