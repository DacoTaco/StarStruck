/*
	mini - a Free Software replacement for the Nintendo/BroadOn IOS.
	exception handling

Copyright (C) 2008, 2009	Hector Martin "marcan" <marcan@marcansoft.com>

# This code is licensed to you under the terms of the GNU GPL, version 2;
# see file COPYING or http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt
*/

#include <asminc.h>

.arm
.globl InstructionAbortVector
.globl DataAbortVector
.globl ReservedExceptionVector
.globl FastIrqVector
.globl SetupExceptionsStack

.extern __excstack_addr
.extern ExceptionHandler

BEGIN_ASM_FUNC SetupExceptionsStack
	mrs	r0, cpsr

	@ Switch to FIQ mode
	msr cpsr_c, #0xd1
	@ Setup exception stack
	ldr	sp, =__excstack_addr

	@ Switch to ABORT mode
	msr cpsr_c, #0xd7
	@ Setup exception stack
	ldr	sp, =__excstack_addr

	@ Switch to UNDF mode
	msr cpsr_c, #0xdb
	@ Setup exception stack
	ldr	sp, =__excstack_addr
	
	@ Restore mode
	msr cpsr_c, r0
	bx	lr
END_ASM_FUNC

.pool
	
BEGIN_ASM_FUNC InstructionAbortVector
	stmib	sp!, {lr}
	stmib	sp, {r0-lr}^
	add		sp, sp, #0x3c
	mov		r2, sp
	mrs		r1, spsr
	mov		r0, #3

	blx		ExceptionHandler

	ldmdb	sp!, {r0-r12}
	sub		sp, sp, #8
	ldmdb	sp!, {lr}
	subs	pc, lr, #4
END_ASM_FUNC

BEGIN_ASM_FUNC DataAbortVector
	stmib	sp!, {lr}
	stmib	sp, {r0-lr}^
	add		sp, sp, #0x3c
	mov		r2, sp
	mrs		r1, spsr
	mov		r0, #4

	blx		ExceptionHandler

	ldmdb	sp!, {r0-r12}
	sub		sp, sp, #8
	ldmdb	sp!, {lr}
	subs	pc, lr, #8
END_ASM_FUNC

BEGIN_ASM_FUNC ReservedExceptionVector
	stmib	sp!, {lr}
	stmib	sp, {r0-lr}^
	add		sp, sp, #0x3c
	mov		r2, sp
	mrs		r1, spsr
	mov		r0, #5

	blx		ExceptionHandler

	ldmdb	sp!, {r0-r12}
	sub		sp, sp, #8
	ldmdb	sp!, {lr}
	movs	pc, lr
END_ASM_FUNC

BEGIN_ASM_FUNC FastIrqVector
	stmib	sp!, {lr}
	stmib	sp, {r0-lr}^
	add		sp, sp, #0x3c
	mov		r2, sp
	mrs		r1, spsr
	mov		r0, #7

	blx		ExceptionHandler

	ldmdb	sp!, {r0-r12}
	sub		sp, sp, #8
	ldmdb	sp!, {lr}
	subs	pc, lr, #4
END_ASM_FUNC
