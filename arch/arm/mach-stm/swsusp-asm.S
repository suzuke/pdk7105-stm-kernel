/*
 * -------------------------------------------------------------------------
 * Copyright (C) 2011  STMicroelectronics
 * Author: Francesco M. Virlinzi  <francesco.virlinzi@st.com>
 *
 * May be copied or modified under the terms of the GNU General Public
 * License V.2 ONLY.  See linux/COPYING for more information.
 *
 * -------------------------------------------------------------------------
 */

#include <linux/linkage.h>

.text		;
.balign 4	;

ENTRY(swsusp_arch_suspend)
	@ save all the registers on swsusp_regs
	ldr	r1, _swsusp_regs
	mrs	r3, cpsr
	stm	r1, {r3-r14}

	@ save lr on the stack
	stmfd   sp!, {lr}

	@ Now call the swsusp_save
	bl	swsusp_save

	@ swsusp_save leaves in r0 the value the
	@ swsusp_arch_suspend has to return

	@ restore lr directly on pc to return
	ldmfd	sp!, {pc}

ENDPROC(swsusp_arch_suspend)

.balign 4
_swsusp_regs:	.long   swsusp_regs

ENTRY(swsusp_arch_resume)
	@ this function uses all the avaialble register
	ldr	r12, _pblist		@ load the restore_pblist
	ldr	r12, [r12]

_cpy_page:
	ldm	r12, {r10, r11, r12}	@ r10: source address
					@ r11: destination address
					@ r12: next pbe address
	mov	r9, #128		@ r9: cache line counter
_cpy_line:
	ldm	r10!, {r0 - r7}		@ read a cache line
	stm	r11!, {r0 - r7}		@ write a cache line

	adds	r9, r9, #-1
	bne	_cpy_line		@ check if the page is complete

	cmp	r12, #0			@ check if there are other 'pages'(pbe)
	bne	_cpy_page

	bl	v7_flush_dcache_all

	@ reload the latest sp value
	ldr	r0, _swsusp_regs

	@ restore all the registers
	ldm	r0, {r3-r14}
	msr	cpsr, r3

	mov	r0, #0
	mov	pc, lr
ENDPROC(swsusp_arch_resume)

.align 2
_pblist:	.long	restore_pblist

