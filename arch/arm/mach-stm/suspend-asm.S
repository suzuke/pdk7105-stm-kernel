/*
 * -------------------------------------------------------------------------
 * Copyright (C) 2012  STMicroelectronics
 * Author: Francesco M. Virlinzi  <francesco.virlinzi@st.com>
 *
 * May be copied or modified under the terms of the GNU General Public
 * License V.2 ONLY.  See linux/COPYING for more information.
 *
 * ------------------------------------------------------------------------- */

#include <linux/linkage.h>
#include <asm/cp15.h>

@ registers setting on stm_suspend_exec_table call:
@ r0 = __pa(&eram_data)
@ r1 = va_2_pa

	.balign 4
	.pushsection    .idmap.text, "ax"
ENTRY(stm_suspend_exec_table)
	stmfd   sp!, {r4-r12, lr}
	dmb
	mov	r9, r1                  @ r9 = va_2_pa offset

	@ jump on __pa(suspend_mmu_off) code
	adr	r1, suspend_mmu_off     @ r1 = __va(suspend_mmu_off)
	sub	r1, r1, r9		@ r1 = __pa(suspend_mmu_off)
	nop
	nop
	mov     pc, r1

	.balign 4
suspend_mmu_off:
	@ now running on identity mapping
	@ CPU can turn-off MMU
	mrc	p15, 0, r1, c1, c0, 0           @ read SCTLR
	bic	r1, r1, #CR_M                   @ disable MMU
	mcr     p15, 0, r1, c1, c0, 0           @ set SCTLR

	nop
	nop
	nop
	nop

	ldm	r0, {r4 - r7}			@ r4 = __pa(enter_table)
						@ r5 = __pa(exit_table)
						@ r6 = __pa(stm_eram_code)
						@ r7 = __pa(pokeloop)

	blx	r6	@ call the stm_suspend_on_eram
			@ placed in the ERAM

suspend_ready_for_mmu_on:
	@ turn-on MMU
	mrc	p15, 0, r0, c1, c0, 0		@ read SCTLR
	orr	r0, r0, #CR_M			@ enable MMU
	mcr	p15, 0, r0, c1, c0, 0		@ set SCTLR

	@ jump on __va(suspend_mmu_on) code
	adr	r0, suspend_mmu_on	@ r0 = pa(suspend_mmu_on)
	add     r0, r0, r9		@ r0 = va(suspend_mmu_on)
	nop
	nop
	mov     pc, r0

	.balign 4
suspend_mmu_on:
	@	restore the previus stak pointer and return
	ldmfd   sp!, {r4-r12, lr}
	mov	pc, lr


@ the stm_suspend_on_eram is the code running on ERAM
	.balign 4
.global stm_suspend_on_eram
stm_suspend_on_eram:
	mov	r0, r4		@ r0 = __pa_eram(table entry address)
	mov	r6, lr		@ save lr on r6

	blx	r7		@ call the poke loop to execute
				@ the entry_table
	wfi

	mov	r0, r5		@ r0 = __pa_eram(table exit address)
	blx	r7		@ call the poke loop to execute
				@ the exit_table

	mov	pc, r6		@ return on DRAM

	.balign 4
.global stm_suspend_on_eram_sz
stm_suspend_on_eram_sz:
	.long . - stm_suspend_on_eram
