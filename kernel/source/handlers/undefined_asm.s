/*
	StarStruck - a Free Software reimplementation for the Nintendo/BroadOn IOS.
	undefined - undefined exception handler

	Copyright (C) 2021	DacoTaco

# This code is licensed to you under the terms of the GNU GPL, version 2;
# see file COPYING or http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt
*/

.globl v_undf;

.extern __swistack_addr
.extern undf_handler

v_undf:
#for info : see syscall asm
	stmdb	sp!, {lr}
	stmdb	sp!, {r0-r12, sp, lr}^
	mrs		r1, spsr
	stmdb	sp!, {r1}
	mov		r1, sp

	ldr		r0,[lr,#-4]
	bic		r0,r0,#0xFF000000

	msr		cpsr_c, #0x1f
	push	{r1}
	blx		undf_handler
	pop		{r1}
	msr		cpsr_c, #0xdb

	mov		sp, r1
	ldmia	sp!, {r2}
	msr		spsr_cxsf, r2
	add		sp, sp, #0x04
	ldmia	sp!, {r1-r12, sp, lr}^
	ldmia	sp!, {lr}
	movs	pc, lr
