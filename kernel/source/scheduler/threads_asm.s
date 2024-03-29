/*
	starstruck - a Free Software reimplementation for the Nintendo/BroadOn IOS.
	threads - manage threads on the starlet

	Copyright (C) 2021	DacoTaco

# This code is licensed to you under the terms of the GNU GPL, version 2;
# see file COPYING or http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt
*/

#include <asminc.h>

.arm
.globl SaveUserModeState
.globl YieldCurrentThread
.globl EndThread
.extern ScheduleYield
.extern ThreadQueue_PushThread
	
BEGIN_ASM_FUNC ReturnToLr
	bx		lr
END_ASM_FUNC

BEGIN_ASM_FUNC EndThread
	mov		r1, r0
	mov		r0, #0x00
#execute syscall CancelThread
	.long	0xE6000050
	mov		r0, r0
END_ASM_FUNC

#s32 YieldCurrentThread(ThreadQueue* queue)
BEGIN_ASM_FUNC YieldCurrentThread
	ldr		r1, =CurrentThread
	ldr		r1, [r1, #0x00]
	mrs     r2, cpsr
	str		r2, [r1, #0x00]
	stmib	r1, {r0-r12, sp, lr}^
	ldr		lr, =ReturnToLr
#load in the link register, which is at 0x40 of the registers
	str		lr, [r1, #0x40]

	cmp		r0, #0
	beq		yield
	_BL		ThreadQueue_PushThread
yield:
	ldr		pc, =ScheduleYield
END_ASM_FUNC
