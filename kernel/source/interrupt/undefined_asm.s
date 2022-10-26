/*
	StarStruck - a Free Software reimplementation for the Nintendo/BroadOn IOS.
	undefined - undefined exception handler

	Copyright (C) 2021	DacoTaco

# This code is licensed to you under the terms of the GNU GPL, version 2;
# see file COPYING or http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt
*/

#include <asminc.h>
#include <ios/processor.h>

.arm

.globl UndefinedInstructionVector;
.extern UndefinedInstructionHandler

BEGIN_ASM_FUNC UndefinedInstructionVector
#for info : see syscall asm
	stmdb	sp!, {lr}
	stmdb	sp, {r0-r12, sp, lr}^
	sub		sp, sp, #0x3C
	mrs		r1, spsr
	stmdb	sp!, {r1}
	mov		r1, sp

#ifdef _THUMBMODE_
	ldrh	r0,[lr,#-2]
	bic		r0,r0,#0xFFFFFF00
#else
	ldr		r0,[lr,#-4]
	bic		r0,r0,#0xFF000000
#endif

	msr		cpsr_c, #SPSR_SYSTEM_MODE
	blx		UndefinedInstructionHandler
	msr		cpsr_c, #0xdb

	ldmia	sp!, {r2}
	msr		spsr_cxsf, r2
	add		sp, sp, #0x04
	ldmia	sp, {r1-r12, sp, lr}^
	add		sp, sp, #0x38
	ldmia	sp!, {pc}^	
END_ASM_FUNC
