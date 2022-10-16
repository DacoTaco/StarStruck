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

.globl v_irq
.globl irq_setup_stack
.globl DisableInterrupts
.globl RestoreInterrupts
.globl irq_wait

.extern __irqstack_addr
.extern irq_handler
.extern SaveThreadInfo
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

BEGIN_ASM_FUNC irq_setup_stack
	@ Switch to IRQ mode
	mrs	r0, cpsr
	msr cpsr_c, #0xd2
	
	@ Setup interrupt stack
	ldr	sp, =__irqstack_addr

	@ Restore mode
	msr cpsr_c, r0
	bx	lr
END_ASM_FUNC
	
BEGIN_ASM_FUNC v_irq
#reset the stackpointer
	ldr		sp, =__irqstack_addr
#load the pc from before the interrupt(which is stored in current lr)
	subs	lr, lr, #4
	stmdb	sp!, {lr}
#save the state and push the state's address to r0
	stmdb	sp, {r0-r12, sp, lr}^
	sub		sp, sp, #0x3C
	mrs		r1, spsr
	stmdb	sp!, {r1}
	mov		r0, sp
	_BL		SaveThreadInfo
	
#handle the irq request with r0 having the state
	mov		r0, sp
	blx		irq_handler

#yield execution to the next thread
	_BL		ScheduleYield
END_ASM_FUNC