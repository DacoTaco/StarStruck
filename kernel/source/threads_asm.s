/*
	starstruck - a Free Software reimplementation for the Nintendo/BroadOn IOS.
	threads - manage threads on the starlet

	Copyright (C) 2021	DacoTaco

# This code is licensed to you under the terms of the GNU GPL, version 2;
# see file COPYING or http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt
*/

.globl SaveUserModeState
.globl ReturnToLr
.globl RestoreAndReturnToUserMode
.extern SaveThreadInfo
.extern __irqstack_addr
.extern __swistack_addr

SaveUserModeState:
	stmdb	sp!, {r0-r12, sp, lr}^
	mrs		r1, spsr
	stmdb	sp!, {r1}
	mov		r0, sp
	stmdb	sp!, {lr}
	bl		SaveThreadInfo
	ldmia	sp!, {r1}
	mov		pc, r1
	
ReturnToLr:
	bx		lr

#RestoreAndReturnToUserMode(return_value, registers, swi_mode)
RestoreAndReturnToUserMode:	
#restore the status register
	ldmia	r1!, {r4}
	msr		spsr_cxsf, r4
#check the mode this was called in. In SWI mode we need to skip restoring r0
	cmp		r2, #0
	bne		swi_restore
thread_restore:
	ldmia	r1!, {r0-r12, sp, lr}^
	b		return
swi_restore:
	add		r1, r1, #0x04
	ldmia	r1!, {r1-r12, sp, lr}^
return:
	ldmia	r1!, {lr}
#ios loads the threads sys_stack_top back in to sp, resetting the stack
#	add		sp, r2, #0x68
	mrs		r2, cpsr
	
	msr 	cpsr_c, #0xd2
	ldr		sp, =__irqstack_addr

	msr 	cpsr_c, #0xd3
	ldr		sp, =__swistack_addr

	msr 	cpsr_c, #0xdb
	ldr		sp, =__swistack_addr

	msr 	cpsr_c, r2
	movs	pc, lr
	