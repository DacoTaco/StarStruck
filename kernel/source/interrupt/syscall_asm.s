/*
	StarStruck - a Free Software reimplementation for the Nintendo/BroadOn IOS.
	Syscalls - internal communications over software interrupts

	Copyright (C) 2021	DacoTaco

# This code is licensed to you under the terms of the GNU GPL, version 2;
# see file COPYING or http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt
*/

#include <asminc.h>
#include <ios/processor.h>

.arm
.extern handle_syscall
.globl v_swi

BEGIN_ASM_FUNC v_swi
#store registers from before the call. r0 = return, r1 - r12 is parameters. current lr(or pc before the call) is the return address
	stmdb	sp!, {lr}
#save state & load address of our state into r1 , which we will use to retrieve the state
	stmdb	sp!, {r0-r12, sp, lr}^
	mrs		r1, spsr
	stmdb	sp!, {r1}
	mov		r1, sp
#load syscall number into r0 (from r8/lr) and cut off the first few bytes of the instruction
#ifdef _THUMBMODE_
	ldrh	r0,[lr,#-2]
	bic		r0,r0,#0xFFFFFF00
#else
	ldr		r0,[lr,#-4]
	bic		r0,r0,#0xFF000000
#endif
#now that we have all our information saved, lets switch to system mode (which shares state with user mode, including stack and registers)
	msr		cpsr_c, #SPSR_SYSTEM_MODE
	
#syscall handler. we save r1 as it contains the address of where the pre-syscall state is saved
	push	{r1}
	blx		handle_syscall
	pop		{r1}

#syscall done, return to swi mode
	msr		cpsr_c, #0xd3
	
#restore stackpointer & state from the buffer
	mov		sp, r1
	ldmia	sp!, {r2}
	msr		spsr_cxsf, r2
#skip restoring r0 (return value)
	add		sp, sp, #0x04
	ldmia	sp!, {r1-r12, sp, lr}^
	ldmia	sp!, {lr}
	
#return to code
	movs	pc, lr
END_ASM_FUNC
