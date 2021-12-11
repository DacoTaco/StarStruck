/*
	mini - a Free Software replacement for the Nintendo/BroadOn IOS.
	system startup

Copyright (C) 2008, 2009	Hector Martin "marcan" <marcan@marcansoft.com>

# This code is licensed to you under the terms of the GNU GPL, version 2;
# see file COPYING or http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt
*/

.arm

.extern _main
.extern __got_start
.extern __got_end
.extern __bss_start
.extern __bss_end
.extern __bss2_start
.extern __bss2_end
.extern __stack_addr
.extern debug_output

.globl _start

.extern v_undf
.extern v_swi
.extern v_instr_abrt
.extern v_data_abrt
.extern v_reserved
.extern v_irq
.extern v_fiq

.section .init

_vectors:
_start:
	ldr	pc, =v_reset
	ldr	pc, =v_undf
	ldr	pc, =v_swi
	ldr	pc, =v_instr_abrt
	ldr	pc, =v_data_abrt
	ldr	pc, =v_reserved
	ldr	pc, =v_irq
	ldr	pc, =v_fiq

.pool

v_reset:
	@ Switch to System mode
	msr cpsr_c, #0xdf

	@ Get loader base from ELF loader
	mov	r4, r0

	@ Output 0x42 to the debug port
	mov	r0, #0x42
	bl	debug_output

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