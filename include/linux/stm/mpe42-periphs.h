/*
 * include/linux/stm/mpe42-periphs.h
 *
 * Copyright (C) 2012 STMicroelectronics Limited.
 * Author: Francesco Virlinzi <francesco.virlinzi@st.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#define MPE42_ASC10_BASE		0xFD4FB000

#define MPE42_SYSTRACE_BASE		0xFD4C0000
#define MPE42_SYSTRACE_REGS		0xFD4DF000

#define MPE42_PIO_10_BASE		0xfd6b0000
#define MPE42_PIO_11_BASE		0xfd330000

/* Here there is an hole of 3000 registers */
#define MPE42_FVDP_FE_SYSCON_BASE	0xFDDF0000 /* 5000-5999 */
#define MPE42_FVDP_LITE_SYSCONF_BASE	0xFD6A0000 /* 6000-6999 */
#define MPE42_CPU_SYSCONF_BASE		0xFDDE0000 /* 7000-7999 */
#define MPE42_COMPO_SYSCONF_BASE	0xFD320000 /* 8000-8999 */
#define MPE42_TRANSPORT_SYSCONF_BASE	0xFD690000 /* 9000-9999*/

#define MPE42_DDR_PCTL_BASE(x)		(0xfdd70000 + (x) * 0x20000)

#define MPE42_ARM_INTERNAL		0xFFFE0000

#define MPE42_DDR_SYSCFG(x)	(MPE42_CPU_SYSCONF_BASE + \
					((x) - 7000) * 0x4)
#define MPE42_DDR_PWR_DWN(x)	MPE42_DDR_SYSCFG(7508 + (x) * 5)
# define MPE42_DDR_PWR_DWN_REQ		(2)
#define MPE42_DDR_PWR_STATUS(x) MPE42_DDR_SYSCFG(7572 + (x) * 2)
# define MPE42_DDR_PWR_STATUS_ACK	(4)
