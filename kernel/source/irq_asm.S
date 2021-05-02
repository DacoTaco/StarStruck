/*
	mini - a Free Software replacement for the Nintendo/BroadOn IOS.
	IRQ support

Copyright (C) 2008, 2009	Hector Martin "marcan" <marcan@marcansoft.com>

# This code is licensed to you under the terms of the GNU GPL, version 2;
# see file COPYING or http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt
*/

#include "hollywood.h"
#include "irq.h"

.globl v_irq
.globl irq_setup_stack
.globl irq_kill
.globl irq_restore
.extern __irqstack_addr
.extern irq_handler
.extern SaveUserModeState
.extern RestoreUserModeState

irq_setup_stack:
	@ Switch to IRQ mode
	mrs	r0, cpsr
	msr cpsr_c, #0xd2
	
	@ Setup interrupt stack
	ldr	sp, =__irqstack_addr

	@ Restore mode
	msr cpsr_c, r0
	bx	lr

v_irq:
	@load the pc from before the interrupt(which is stored in current lr)
	subs	lr, lr, #4
	stmdb	sp!, {lr}
	@save the state and push the state's address to r0
	bl 		SaveUserModeState
	mov		r0, sp
	@handle the irq request with r0 having the state
	blx		irq_handler
	
	@load registers back and return	
	bl		RestoreUserModeState
	@get the pc address, set sp back to start and return
	ldmia	sp!, {lr}
	ldr		sp, =__irqstack_addr
	movs	pc, lr
	
v_irq_old:
    push    {r0-r3, r9, r12, lr}

    blx     irq_handler

    pop     {r0-r3, r9, r12, lr}
    subs    pc, lr, #4
