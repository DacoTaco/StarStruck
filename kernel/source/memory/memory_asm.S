/*
	mini - a Free Software replacement for the Nintendo/BroadOn IOS.
	memory management, MMU, caches, and flushing

Copyright (C) 2008, 2009	Hector Martin "marcan" <marcan@marcansoft.com>

# This code is licensed to you under the terms of the GNU GPL, version 2;
# see file COPYING or http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt
*/

#include <asminc.h>

.arm

.globl get_cr
.globl set_cr
.globl get_ttbr
.globl set_ttbr
.globl get_dacr
.globl set_dacr
.globl get_dfsr
.globl get_ifsr
.globl get_far

.globl _dc_inval_entries
.globl _dc_flush_entries
.globl _dc_flush
.globl _dc_inval
.globl _ic_inval
.globl _drain_write_buffer
.globl _tlb_inval

.text

BEGIN_ASM_FUNC get_cr
	mrc		p15, 0, r0, c1, c0, 0
	bx		lr
END_ASM_FUNC

BEGIN_ASM_FUNC set_cr
	mcr		p15, 0, r0, c1, c0, 0
	bx		lr
END_ASM_FUNC

BEGIN_ASM_FUNC get_ttbr
	mrc		p15, 0, r0, c2, c0, 0
	bx		lr
END_ASM_FUNC

BEGIN_ASM_FUNC set_ttbr
	mcr		p15, 0, r0, c2, c0, 0
	bx		lr
END_ASM_FUNC

BEGIN_ASM_FUNC get_dacr
	mrc		p15, 0, r0, c3, c0, 0
	bx		lr
END_ASM_FUNC

BEGIN_ASM_FUNC set_dacr
	mcr		p15, 0, r0, c3, c0, 0
	bx		lr
END_ASM_FUNC

BEGIN_ASM_FUNC get_dfsr
	mrc		p15, 0, r0, c5, c0, 0
	bx		lr
END_ASM_FUNC

BEGIN_ASM_FUNC get_ifsr
	mrc		p15, 0, r0, c5, c0, 1
	bx		lr
END_ASM_FUNC

BEGIN_ASM_FUNC get_far
	mrc		p15, 0, r0, c6, c0, 0
	bx		lr
END_ASM_FUNC

BEGIN_ASM_FUNC _dc_inval_entries
	mcr		p15, 0, r0, c7, c6, 1
	add		r0, #0x20
	subs	r1, #1
	bne		_dc_inval_entries
	bx		lr
END_ASM_FUNC

BEGIN_ASM_FUNC _dc_flush_entries
	mcr		p15, 0, r0, c7, c10, 1
	add		r0, #0x20
	subs	r1, #1
	bne		_dc_flush_entries
	bx		lr
END_ASM_FUNC

BEGIN_ASM_FUNC _dc_flush
	mrc		p15, 0, pc, c7, c10, 3
	bne		_dc_flush
	bx		lr
END_ASM_FUNC

BEGIN_ASM_FUNC _dc_inval
	mov		r0, #0
	mcr		p15, 0, r0, c7, c6, 0
	bx		lr
END_ASM_FUNC

BEGIN_ASM_FUNC _ic_inval
	mov		r0, #0
	mcr		p15, 0, r0, c7, c5, 0
	bx		lr
END_ASM_FUNC

BEGIN_ASM_FUNC _drain_write_buffer
	mov		r0, #0
	mcr		p15, 0, r0, c7, c10, 4
	bx		lr
END_ASM_FUNC

BEGIN_ASM_FUNC _tlb_inval
	mov		r0, #0
	mcr		p15, 0, r0, c8, c7
	bx		lr
END_ASM_FUNC

