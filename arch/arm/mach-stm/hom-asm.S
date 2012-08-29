/*
 *  Copyright (C) 2012  STMicroelectronics
 *  Author: Francesco Virlinzi <francesco.virlinzi@st.com>
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 */

#include <linux/init.h>
#include <linux/linkage.h>

#include <asm/assembler.h>
#include <asm/domain.h>
#include <asm/ptrace.h>
#include <asm/asm-offsets.h>
#include <asm/memory.h>
#include <asm/thread_info.h>
#include <asm/cp15.h>

#include "hom.h"

.macro	hom_data_get	off, reg
	adr	\reg, hom_frozen_data
	ldr	\reg, [\reg, #\off]
.endm

.macro hom_data_set	off, reg, tmp_reg
	adr	\tmp_reg, hom_frozen_data
	str	\reg, [\tmp_reg, #\off]
	add	\tmp_reg, \tmp_reg, #\off
	mcr	p15, 0, \tmp_reg, c7, c10, 1	@ clean & invalidate by MVA
	dsb
	isb
.endm

#ifdef CONFIG_HOM_DEBUG
.macro hom_debug_data	reg0, reg1
	hom_data_set	HFD_DEBUG_DATA_OFF, \reg0, \reg1
.endm

.macro hom_debug_step	value, reg0, reg1
	ldr		\reg0, =\value
	hom_data_set	HFD_DEBUG_OFF, \reg0, \reg1
.endm
#else
.macro hom_debug_data	reg0, reg1
.endm

.macro hom_debug_step	value, reg0, reg1
.endm
#endif

	.text
	.pushsection    .idmap.text, "aw"
ENTRY(stm_hom_exec_on_eram)
	@ save all the regsiter on the current stack
	mrs	r3, cpsr
	stmfd   sp!, {r3-r12, lr}

	@ save the latest sp value
	hom_data_set HFD_SP_OFF, sp, r3

	@ flush out the Dcache to be sure all the saved
	@ register and the latest sp value saved in stm_hom_frozen_sp
	@ is really on the DRAM
	mov	r7, sp
	mcr	p15, 0, r7, c7, c10, 1		@ clean & invalidate by MVA
	add	r7, r7, #32
	mcr	p15, 0, r7, c7, c10, 1		@ clean & invalidate by MVA
	add	r7, r7, #32
	mcr	p15, 0, r7, c7, c10, 1		@ clean & invalidate by MVA

	dmb

	@ install hom_pgd
	mcr	p15, 0, r0, c8, c7, 0
	mcr	p15, 0, r1, c2, c0, 0		@ set TTBR 0
	mcr	p15, 0, r1, c2, c0, 1		@ set TTBR 1
	mcr	p15, 0, r0, c8, c7, 0

	@ jump on __pa(hom_mmu_off) code
	adr	r1, hom_mmu_off		@ r1 = __va(hom_mmu_off)
	sub	r1, r1, r2		@ r1 = __pa(hom_mmu_off)
	nop
	nop
	mov     pc, r1

        .balign 4
hom_mmu_off:
	@ now running on identity mapping
	@ CPU can turn-off MMU
	mrc	p15, 0, r1, c1, c0, 0		@ read SCTLR
        bic	r1, r1, #CR_M			@ disable MMU
	mcr	p15, 0, r1, c1, c0, 0		@ set SCTLR

	nop
	nop
	nop
	nop

	ldm	r0, {r4 - r6}			@ r4 = __pa(enter_table)
						@ r5 = __pa(stm_eram_code)
						@ r6 = __pa(pokeloop)


	mov	r0, r4	@ move __pa(enter_table) to be ready
			@ when pokeloop is executed

	blx	r5	@ call the stm_hom_on_eram
			@ placed in the ERAM


.balign		4
ENTRY(stm_hom_on_eram)
	/*
	 * runs the poke tables to turn-off the SOC
	 */
	blx	r6	@ call the __pokeloop on eram
1:
	wfi		@ WaitForInterrupt until
			@ the SOC is turned-off
	b 1b

.balign 4
ENTRY(stm_hom_on_eram_sz)
	.long	. - stm_hom_on_eram

/*
 * Condition at the entry of stm_defrost_kernel:
 *
 */
ENTRY(stm_defrost_kernel)
	@ the stm_defrost_kernel:
	@ - returns to the caller of stm_hom_exec_table in case of CPU-0
	@ - jumps on hom_secondary_entry in case of CPU != 0

	setmode PSR_F_BIT | PSR_I_BIT | SVC_MODE @ ensure svc mode
						 @ and irqs disabled

	@ check between main and secondary core
	mrc	p15, 0, r0, c0, c0, 5
	tst	r0, #(1 << 31)		@ Register has MPCore format?
	beq	stm_defrost_main_cpu
	tst	r0, #(1 << 30)		@ 1 means this is a uni-processor ARM
	bne	stm_defrost_main_cpu
	ands	r0, #0xFF		@ Is CPU 0 (in affinity level 0)?
	beq	stm_defrost_main_cpu

	@ ARM core != 0 jump on stm_defrost_secondary_cpu

	b	stm_defrost_secondary_cpu

stm_defrost_main_cpu:

	hom_debug_step	1, r3, r4

	mrc	p15, 0, r9, c0, c0		@ get processor id
	bl	__lookup_processor_type		@ r5=procinfo r9=cpuid
						@ __lookup_processor_type
						@ returns the procinfo on r5
	mov	r10, r5

	@ the page tables are already built (done before suspend)
	@ in same place in DRAM

	@ r8 to initialize TTBR1
	hom_data_get HFD_MMU_PG_DIR_OFF, r8

	adr	lr, BSYM(1f)	@ return (PIC) address


 ARM(	add     pc, r10,  #PROCINFO_INITFUNC	)
 THUMB( add	r12, r10, #PROCINFO_INITFUNC	)
 THUMB( mov	pc, r12				)

1:
	@ reload control register on r0 to be safe
	mrc	p15, 0, r0, c1, c0, 0		@ read control register
	bic	r0, r0, r5			@ clear bits them
	orr	r0, r0, r6			@ set them
 THUMB( orr	r0, r0, #1 << 30	)	@ Thumb exceptions

	@ initialize TTBR0/TTBR1/TTBCR
	hom_data_get HFD_MMU_PG_DIR_OFF,	r4
	mov	r8, r4
	ldr r13, =hom_vm_entry

	@ registers setting to call __enable_mmu
	@ r0	= cp#15 control register
	@ r4	= page table pointer
	@ r13	= *virtual* address to jump to upon completion

	b	__enable_mmu

hom_vm_entry:
	@ Now the CPU is running in the virtual address space
	hom_debug_step	2, r7, r8

	@ this is a critical moment!
	@ the CPU is already running in VM space but only
	@ the kernel is mapped.
	@ the following operations on :
	@ - HFD_MMU_TTBR0_OFF
	@ and
	@ - HFD_MMU_TTBR1_OFF
	@ will do something magic!
	@ after the store operations macically the system will see:
	@ - All the DRAM
	@ - All the io-devices
	@ - - on iotable
	@ - - on vm_regions
	@ - Excption Vector already available

	@ the system can jump on the original page table there was...

	hom_data_get HFD_MMU_TTBR0_OFF, r0
	hom_data_get HFD_MMU_TTBR1_OFF, r1
	hom_data_get HFD_MMU_TTBCR_OFF, r2

	mcr     p15, 0, r0, c2, c0, 0		@ set TTBR 0
	mcr     p15, 0, r1, c2, c0, 1		@ set TTBR 1
	mcr	p15, 0, r2, c2, c0, 2		@ set TTBCR

	@ now the CPU-0 is using the original page table
	@ it's safer a tlb invalidate all to be sure
	@ all the previuos information coming from the
	@ hom_pg_dir are rejected
	mcr	p15, 0, r0, c8, c7, 0

	hom_debug_step	3, r7, r8

	@ the final sp can be loaded
	hom_data_get HFD_SP_OFF, sp

	hom_debug_step	4, r7, r8

	@ restore the previus stack pointer and return
	ldmfd	sp!, {r3-r12, lr}

	hom_debug_step	5, r0, r1

	msr	cpsr, r3	@ restore the control register
	nop

	hom_debug_step	6, r0, r1

	@ disable alignement fault checking
	mrc	p15, 0, r0, c1, c0, 0
	bic	r0, r0, #2
	mcr	p15, 0, r0, c1, c0, 0

	hom_debug_step	5, r1, r2

	@ return
	mov	pc, lr

ENDPROC(stm_defrost_kernel)

#ifdef CONFIG_SMP
stm_defrost_secondary_cpu:

	@ wait the green ligh from the CPU-0
	mrc	p15, 0, r0, c0, c0, 5
	and	r0, r0, #15
	adr	r4, 1f
	ldmia	r4, {r5, r6}
	sub	r4, r4, r5
	add	r6, r6, r4

pen:    ldr	r7, [r6]
	cmp	r7, r0
	bne	pen

	@ not clear why... but secondary_startup doesn't work...
	b secondary_startup

	.balign 4
1:
	.long	.
	.long pen_release
#else
stm_defrost_secondary_cpu:
	@ the secondary CPUs stop here forever
1:	b	1b
#endif
	.balign 32
ENTRY(hom_frozen_data)
	.long	0, 0, 0, 0, 0, 0, 0, 0
