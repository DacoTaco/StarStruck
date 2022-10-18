/*
	mini - a Free Software replacement for the Nintendo/BroadOn IOS.
	system startup

Copyright (C) 2008, 2009	Hector Martin "marcan" <marcan@marcansoft.com>

# This code is licensed to you under the terms of the GNU GPL, version 2;
# see file COPYING or http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt
*/

.arm

.extern _main
.extern __bss_start
.extern __bss_end
.extern __data_sram_start
.extern __data_sram_end
.extern __bss2_start
.extern __bss2_end
.extern __stack_addr
.extern debug_output

.globl _start

.extern UndefinedInstructionVector
.extern SupervisorCallVector
.extern InstructionAbortVector
.extern DataAbortVector
.extern ReservedExceptionVector
.extern InterruptVector
.extern FastIrqVector

.section .init

_vectors:
_start:
	ldr	pc, =ResetVector
	ldr	pc, =UndefinedInstructionVector
	ldr	pc, =SupervisorCallVector
	ldr	pc, =InstructionAbortVector
	ldr	pc, =DataAbortVector
	ldr	pc, =ReservedExceptionVector
	ldr	pc, =InterruptVector
	ldr	pc, =FastIrqVector

.pool

ResetVector:
	@ Switch to System mode
	msr cpsr_c, #0xdf

	@ Get loader base from ELF loader
	mov	r4, r0

	@ Output 0x42 to the debug port
	mov	r0, #0x42
	bl	debug_output

	@ invalidate ic & dc cache
    mcr p15, 0, r0, cr7, cr5, 0x0
    mcr p15, 0, r0, cr7, cr6, 0x0

	@ set IC & DC cache
    mrc p15, 0, r0, cr1, cr0, 0
    orr r0, r0, #0x1000
    orr r0, r0, #0x2
    mcr p15, 0, r0, cr1, cr0, 0

	@ hide boot0
	ldr r0, =#0x0D80018C
    ldr r1,[r0, #0]
    orr r1, r1, #0x1000
    str r1, [r0, #0]

	@ Set up a stack
	ldr	sp, =__stack_addr

	@ clear the stack to a marker value
	ldr	r1, =__stack_end
	ldr	r2, =__stack_addr
	ldr	r3, =0xDEADBEEF
stk_loop:
	@ check for the end
	cmp	r1, r2
	beq	done_stk
	@ clear the word and move on
	str	r3, [r1]
	add	r1, r1, #4
	b	stk_loop

done_stk:
	@ clear BSS
	ldr	r1, =__bss_start
	ldr	r2, =__bss_end
	mov	r3, #0
bss_loop:
	@ check for the end
	cmp	r1, r2
	beq	done_bss
	@ clear the word and move on
	str	r3, [r1]
	add	r1, r1, #4
	b	bss_loop

done_bss:
	@ clear BSS structs
	ldr	r1, =__data_sram_start
	ldr	r2, =__data_sram_end
	mov	r3, #0
sram_data_loop:
	@ check for the end
	cmp	r1, r2
	beq	done_sram_data
	@ clear the word and move on
	str	r3, [r1]
	add	r1, r1, #4
	b	sram_data_loop

done_sram_data:
	@ clear BSS2
	ldr	r1, =__bss2_start
	ldr	r2, =__bss2_end
	mov	r3, #0
bss2_loop:
	@ check for the end
	cmp	r1, r2
	beq	done_bss2
	@ clear the word and move on
	str	r3, [r1]
	add	r1, r1, #4
	b	bss2_loop

done_bss2:
	mov	r0, #0x84
	bl	debug_output
	@ take the plunge
	mov	r0, r4
	blx	_main
	@ _main returned! Go to whatever address it returned...
	mov	pc, r0

.pool
