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
	stmib	sp, {r0-r12, sp, lr}^
	mrs		r8, spsr
	str		r8, [sp]
	str		lr, [sp, #0x40]
#always get the last 4 bytes that were executed. this is because a UNDF syscall is 4 bytes long
	ldr		r0,[lr,#-4]

	mov		r8, sp
	msr		cpsr_c, #SPSR_SYSTEM_MODE
	ldr		r8, [r8, #0x44]
	mov		sp, r8
	blx		UndefinedInstructionHandler
	msr		cpsr_c, #0xdb

	ldr		r11, [sp]
	msr		spsr_cxsf, r11
	mov		lr, r0
	ldmib	sp,{r0-r12, sp, lr}^
	mov		r0, lr
	ldr		lr, [sp, #0x40]
	movs	pc, lr
END_ASM_FUNC
