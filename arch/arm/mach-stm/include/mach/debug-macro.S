/*
 * arch/arm/mach-stm/include/mach/debug-macro.S
 *
 * Debugging macro include header
 *
 * Copyright (C) 2010-2012 STMicroelectronics Limited
 *
 * Author(s): Stuart Menefy <stuart.menefy@st.com>
 *            Carmelo Amoroso <carmelo.amoroso@st.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifdef CONFIG_MACH_STM_FLI7610
#include <linux/stm/fli7610-periphs.h>
#ifdef CONFIG_MACH_STM_FLI76XXHDK01
#define DEBUG_LL_UART_BASE	FLI7610_CONSOLE_BASE
#endif
#endif

#ifdef CONFIG_MACH_STM_STIG125
#include <linux/stm/stig125-periphs.h>
#ifdef CONFIG_MACH_STM_B2044
#define DEBUG_LL_UART_BASE	STIG125_SBC_ASC0_BASE
#endif
#endif

#ifdef CONFIG_MACH_STM_STIH415
#include <linux/stm/stih415-periphs.h>
#ifdef CONFIG_MACH_STM_B2000
#define DEBUG_LL_UART_BASE	STIH415_ASC2_BASE
#elif defined CONFIG_MACH_STM_B2020
#define DEBUG_LL_UART_BASE	STIH415_SBC_ASC1_BASE
#endif
#endif

#ifndef DEBUG_LL_UART_BASE
#error "Unknown platform"
#endif

#include <mach/hardware.h>

		.macro	addruart, rp, rv, tmp
		ldr	\rp,      =DEBUG_LL_UART_BASE	@ physical base
		ldr	\rv,      =IO_ADDRESS(DEBUG_LL_UART_BASE) @ virtual base
		.endm

#include <asm/hardware/debug-stm-asc.S>
