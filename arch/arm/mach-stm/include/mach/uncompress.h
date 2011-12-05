/*
 * arch/arm/mach-stm/include/mach/uncompress.h
 *
 * Copyright (C) 2010 STMicroelectronics Limited
 * Author: Stuart Menefy <stuart.menefy@st.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */
#include <mach/hardware.h>
#include <asm/mach-types.h>

#ifdef CONFIG_MACH_STM_STIH415
#include <mach/soc-stih415.h>
#elif defined(CONFIG_MACH_STM_STX7108)
#include <mach/soc-stx7108.h>
#elif defined(CONFIG_MACH_STM_FLI7610)
#include <mach/soc-fli7610.h>
#endif

#define ASC_TX_BUF(base)	(*(volatile unsigned int*)((base) + 0x04))
#define ASC_CTRL(base)		(*(volatile unsigned int*)((base) + 0x0c))
#define ASC_STA(base)		(*(volatile unsigned int*)((base) + 0x14))

#define ASC_STA_TX_FULL		(1<<9)
#define ASC_STA_TX_EMPTY	(1<<1)

/*
 * Return the UART base address
 */
static inline unsigned long get_uart_base(void)
{
#ifdef CONFIG_MACH_STM_STIH415
	if (machine_is_stm_b2000())
		return STIH415_ASC2_BASE;
#elif defined(CONFIG_MACH_STM_FLI7610)
	if (machine_is_stm_nmhdk_fli7610())
		return FLI7610_CONSOLE_BASE;
#endif
	return 0;
}

/*
 * This does not append a newline
 */
static inline void putc(int c)
{
	unsigned long base = get_uart_base();

	while (ASC_STA(base) & ASC_STA_TX_FULL)
		barrier();

	ASC_TX_BUF(base) = c;
}

static inline void flush(void)
{
	unsigned long base = get_uart_base();

	while (!(ASC_STA(base) & ASC_STA_TX_EMPTY))
		barrier();
}

/*
 * nothing to do
 */
#define arch_decomp_setup()
#define arch_decomp_wdog()
