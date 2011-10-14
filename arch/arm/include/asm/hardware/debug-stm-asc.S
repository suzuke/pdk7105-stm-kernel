/*
 * arch/arm/include/asm/hardware/debug-stm-asc.S
 *
 * Debugging macro include header
 *
 * Copyright (C) 2010 STMicroelectronics Limited
 * Author: Stuart Menefy <stuart.menefy@st.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */

#define ASC_TX_BUF_OFF	0x04
#define ASC_CTRL_OFF	0x0c
#define ASC_STA_OFF	0x14

#define ASC_STA_TX_FULL		(1<<9)
#define ASC_STA_TX_EMPTY	(1<<1)

		.macro	senduart,rd,rx
		strb	\rd, [\rx, #ASC_TX_BUF_OFF]
		.endm

		.macro	waituart,rd,rx
1001:		ldr	\rd, [\rx, #ASC_STA_OFF]
		tst	\rd, #ASC_STA_TX_FULL
		bne	1001b
		.endm

		.macro	busyuart,rd,rx
1001:		ldr	\rd, [\rx, #ASC_STA_OFF]
		tst	\rd, #ASC_STA_TX_EMPTY
		beq	1001b
		.endm
