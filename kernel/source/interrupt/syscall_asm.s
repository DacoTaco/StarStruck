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
.extern HandleSyscall
.globl SupervisorCallVector

BEGIN_ASM_FUNC SupervisorCallVector
#store registers from before the call. r0 = return, r1 - r12 is parameters. current lr(or pc before the call) is the return address
#save state & load address of our state into the stack , which we will use to retrieve the state
	stmib	sp, {r0-r12, sp, lr}^
	mrs		r8, spsr
	str		r8, [sp]
	str		lr, [sp, #0x40]
#load syscall number into r0 and cut off the first few bytes of the instruction
#ifdef _THUMBMODE_
	ldrh	r0,[lr,#-2]
	bic		r0,r0,#0xFFFFFF00
#else
	ldr		r0,[lr,#-4]
	bic		r0,r0,#0xFF000000
#endif
#now that we have all our information saved, lets switch to system mode (which shares state with user mode, including stack and registers)
	msr		cpsr_c, #SPSR_SYSTEM_MODE
	
#syscall handler.
	blx		HandleSyscall

#syscall done, return to swi mode
	msr		cpsr_c, #0xd3
	
#restore context from the stack
	ldr		r11, [sp]
	msr		spsr_cxsf, r11
#hold on to our return value
	mov		lr, r0
#load status back in, restore r0 and go back to code
	ldmib	sp,{r0-r12, sp, lr}^
	mov		r0, lr
	ldr		lr, [sp, #0x40]
	movs	pc, lr
END_ASM_FUNC
