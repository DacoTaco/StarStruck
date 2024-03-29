/*
	mini - a Free Software replacement for the Nintendo/BroadOn IOS.
	IRQ support

Copyright (C) 2008, 2009	Hector Martin "marcan" <marcan@marcansoft.com>

# This code is licensed to you under the terms of the GNU GPL, version 2;
# see file COPYING or http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt
*/

#include <asminc.h>

#these instruction have to be done in arm mode
.arm

#include "core/hollywood.h"
#include "interrupt/irq.h"

.globl InterruptVector
.globl DisableInterrupts
.globl RestoreInterrupts
.globl irq_wait

.extern __irqstack_addr
.extern IrqHandler
.extern ScheduleYield

BEGIN_ASM_FUNC DisableInterrupts
	mrs		r1, cpsr
	and		r0, r1, #(CPSR_IRQDIS|CPSR_FIQDIS)
	orr		r1, r1, #(CPSR_IRQDIS|CPSR_FIQDIS)
	msr		cpsr_c, r1
	bx		lr
END_ASM_FUNC

BEGIN_ASM_FUNC RestoreInterrupts
	mrs		r1, cpsr
	bic		r1, r1, #(CPSR_IRQDIS|CPSR_FIQDIS)
	orr		r1, r1, r0
	msr		cpsr_c, r1
	bx		lr
END_ASM_FUNC

BEGIN_ASM_FUNC irq_wait
	mov	r0, #0
	mcr	p15, 0, r0, c7, c0, 4
	bx	lr
END_ASM_FUNC

BEGIN_ASM_FUNC InterruptVector
#set the stackpointer & save the thread context
	ldr		sp, =CurrentThread
	ldr		sp, [sp]
#save the context
	stmib	sp, {r0-r12, sp, lr}^

#set our sp back to the irq stack
	mov		r0, sp
	ldr		sp, =__irqstack_addr

#load the pc from before the interrupt(which is stored in current lr)
	sub		lr, lr, #4
	str		lr, [r0, #0x40]

	mrs		lr, spsr
	str		lr, [r0]
	
#handle the irq request with r0 having the state
	blx		IrqHandler

#yield execution to the next thread
	_BL		ScheduleYield
END_ASM_FUNC