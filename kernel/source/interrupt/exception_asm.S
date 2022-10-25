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
.extern __swistack_addr
.extern ExceptionHandler

BEGIN_ASM_FUNC SetupExceptionsStack
	mrs	r0, cpsr

	@ Switch to FIQ mode
	msr cpsr_c, #0xd1
	@ Setup exception stack
	ldr	sp, =__excstack_addr

	@ Switch to SVC mode
	msr cpsr_c, #0xd3
	@ Setup exception stack
	ldr	sp, =__swistack_addr

	@ Switch to ABORT mode
	msr cpsr_c, #0xd7
	@ Setup exception stack
	ldr	sp, =__excstack_addr

	@ Switch to UNDF mode
	msr cpsr_c, #0xdb
	@ Setup exception stack
	ldr	sp, =__swistack_addr
	
	@ Restore mode
	msr cpsr_c, r0
	bx	lr
END_ASM_FUNC

.pool
	
BEGIN_ASM_FUNC InstructionAbortVector
	stmfd	sp!, {lr}
	stmfd	sp, {r0-lr}^
	sub		sp, sp, #0x3c
	mov		r2, sp
	mrs		r1, spsr
	mov		r0, #3

	blx		ExceptionHandler

	ldmfd	sp!, {r0-r12}
	add		sp, sp, #8
	ldmfd	sp!, {lr}
	subs	pc, lr, #4
END_ASM_FUNC

BEGIN_ASM_FUNC DataAbortVector
	stmfd	sp!, {lr}
	stmfd	sp, {r0-lr}^
	sub		sp, sp, #0x3c
	mov		r2, sp
	mrs		r1, spsr
	mov		r0, #4

	blx		ExceptionHandler

	ldmfd	sp!, {r0-r12}
	add		sp, sp, #8
	ldmfd	sp!, {lr}
	subs	pc, lr, #8
END_ASM_FUNC

BEGIN_ASM_FUNC ReservedExceptionVector
	stmfd	sp!, {lr}
	stmfd	sp, {r0-lr}^
	sub		sp, sp, #0x3c
	mov		r2, sp
	mrs		r1, spsr
	mov		r0, #5

	blx		ExceptionHandler

	ldmfd	sp!, {r0-r12}
	add		sp, sp, #8
	ldmfd	sp!, {lr}
	movs	pc, lr
END_ASM_FUNC

BEGIN_ASM_FUNC FastIrqVector
	stmfd	sp!, {lr}
	stmfd	sp, {r0-lr}^
	sub		sp, sp, #0x3c
	mov		r2, sp
	mrs		r1, spsr
	mov		r0, #7

	blx		ExceptionHandler

	ldmfd	sp!, {r0-r12}
	add		sp, sp, #8
	ldmfd	sp!, {lr}
	subs	pc, lr, #4
END_ASM_FUNC
