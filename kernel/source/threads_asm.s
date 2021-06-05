/*
	starstruck - a Free Software reimplementation for the Nintendo/BroadOn IOS.
	threads - manage threads on the starlet

	Copyright (C) 2021	DacoTaco

# This code is licensed to you under the terms of the GNU GPL, version 2;
# see file COPYING or http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt
*/

.globl SaveUserModeState
.globl RestoreUserModeState
.extern SaveThreadInfo
.extern RestoreThreadInfo

SaveUserModeState:
	stmdb	sp!, {r0-r12, sp, lr}^
	mrs		r1, spsr
	stmdb	sp!, {r1}
	mov		r0, sp
	stmdb	sp!, {lr}
	bl		SaveThreadInfo
	ldmia	sp!, {r1}
	mov		pc, r1

RestoreUserModeState:	
#restore state, store r0 temp on the stack
	mov		r3,	r0
	mov		r0, sp
	stmdb	sp!, {r3}
	stmdb	sp!, {r1, lr}
	bl		RestoreThreadInfo
	ldmia	sp!, {r1, lr}
	ldmia	sp!, {r3}
#restore r0 and return
	mov		r0, r3
	ldmia	sp!, {r2}
	msr		spsr_cxsf, r2
	cmp		r1, #0
	bne		swi_restore
thread_restore:
	ldmia	sp!, {r0-r12, sp, lr}^
	b		return
swi_restore:
#skip r0, as it is our return value
	add		sp, sp, #0x04
	ldmia	sp!, {r1-r12, sp, lr}^
return:
	mov		pc, lr