/*
 * arch/arm/mach-stm/include/mach/debug-macro.S
 *
 * Debugging macro include header
 *
 * Copyright (C) 2010 STMicroelectronics Limited
 * Author: Stuart Menefy <stuart.menefy@st.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifdef CONFIG_MACH_STM_STIH415
#include <mach/soc-stih415.h>

//#define DEBUG_LL_UART_OFFSET	(STIH415_ASC0_BASE & 0x03ffffff)
#define DEBUG_LL_UART_OFFSET	(STIH415_SBC_ASC0_BASE & 0x03ffffff)
#endif


#ifdef CONFIG_MACH_STM_FLI7610
#include <mach/soc-fli7610.h>
#define DEBUG_LL_UART_OFFSET	(FLI7610_CONSOLE_BASE & 0x03ffffff)
#endif



#ifndef DEBUG_LL_UART_OFFSET
#error "Unknown platform"
#endif

		.macro	addruart,rx
		mrc	p15, 0, \rx, c1, c0
		tst	\rx, #1			@ MMU enabled?
		moveq	\rx,      #0xfc000000	@ physical base
		movne	\rx,      #0xfb000000	@ virtual base
		add	\rx, \rx, #(DEBUG_LL_UART_OFFSET & 0x03fc0000)
		add	\rx, \rx, #(DEBUG_LL_UART_OFFSET & 0x0003f000)
		.endm

#include <asm/hardware/debug-stm-asc.S>
