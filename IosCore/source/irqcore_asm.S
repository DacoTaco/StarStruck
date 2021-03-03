/*
	mini - a Free Software replacement for the Nintendo/BroadOn IOS.
	IRQ support

Copyright (C) 2008, 2009	Hector Martin "marcan" <marcan@marcansoft.com>

# This code is licensed to you under the terms of the GNU GPL, version 2;
# see file COPYING or http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt
*/

#include "hollywood.h"
#include "irqcore.h"

.globl irq_kill
.globl irq_restore

irq_kill:
	mrs		r1, cpsr
	and		r0, r1, #(CPSR_IRQDIS|CPSR_FIQDIS)
	orr		r1, r1, #(CPSR_IRQDIS|CPSR_FIQDIS)
	msr		cpsr_c, r1
	bx		lr

irq_restore:
	mrs		r1, cpsr
	bic		r1, r1, #(CPSR_IRQDIS|CPSR_FIQDIS)
	orr		r1, r1, r0
	msr		cpsr_c, r1
	bx		lr

