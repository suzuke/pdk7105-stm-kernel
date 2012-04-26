/*
 * File     : pokeloop.S
 *
 * Copyright (c) 2012 STMicroelectronics Limited.  All rights reserved.
 */

#include <linux/stm/poke_table.h>


/*
 * The poke table is a series of long words, in the format:
 *
 *	opcode, operand, operand (, operand)*
 *
 * An opcode of 0 marks the table end.
 *
 */

	.global		stm_pokeloop

	.balign	32
stm_pokeloop:

DoPoking:
	ldr	r3, [r0], #4	/* opcode */
	ldr	r2, [r0], #4	/* operand 1 */

/* END_MARKER/RETURN(x) */
DoRet:
	cmp	r3, #OP_END_POKES
	bne	DoPoke32
	mov	r0, r2		/* Exit code */
	bx	lr		/* Return point */


/* POKE32(A, VAL) */
DoPoke32:
	cmp	r3, #OP_POKE32
	bne	DoOr32
	ldr	r3, [r0], #4	/* read VAL */
	str	r3, [r2]	/* *A = VAL */
	b	DoPoking

/* OR32(A, VAL) */
DoOr32:
	cmp	r3, #OP_OR32
	bne	DoUpdate32
	ldr	r3, [r0], #4	/* VAL */
	ldr	r12, [r2]	/* *A */
	orr	r3, r3, r12	/* *A | OR */
	str	r3, [r2]	/* *A |= OR */
	b	DoPoking

/* UPDATE32(A, and, OR) */
DoUpdate32:
	cmp	r3, #OP_UPDATE32
	bne	DoWhileNe32
	ldr	r3, [r0], #4	/* read and */
	ldr	r12, [r2]	/* *A */
	and	r12, r12, r3	/* *A & AND */
	ldr	r3, [r0], #4	/* read OR */
	orr	r12, r12, r3	/* (*A & and) | OR */
	str	r12, [r2]	/* *A = ((*A & and) | OR) */
	b	DoPoking

/* WHILE_NE32(A, and, VAL) */
DoWhileNe32:
	cmp	r3, #OP_WHILE_NE32
	bne	DoPoking
1:	ldr	r3, [r0]	/* read and */
	ldr	r12, [r2]	/* *A */
	and	r3, r3, r12	/* *A & AND */
	ldr	r12, [r0, #4]	/* read VAL */
	cmp	r3, r12		/* if ((*A & and) == VAL) */
	bne	1b		/* loop if false */
	add	r0, r0, #8
	b	DoPoking

/* DELAY(ITERATIONS) */
DoDelay:
1:	subs	r2, r2, #1	/* if (--OPERATIONS == 0) */
	beq	1b		/* subs should have set Z bit if equal */
	b	DoPoking

	.balign 4
.global stm_pokeloop_sz
stm_pokeloop_sz:
	.long . - stm_pokeloop
