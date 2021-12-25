/*
	mini - a Free Software replacement for the Nintendo/BroadOn IOS.
	memory management, MMU, caches, and flushing

Copyright (C) 2008, 2009	Hector Martin "marcan" <marcan@marcansoft.com>

# This code is licensed to you under the terms of the GNU GPL, version 2;
# see file COPYING or http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt
*/

#include <asminc.h>

.arm

.globl FlushMemory
.globl TlbInvalidate
.globl GetControlRegister
.globl SetControlRegister
.globl GetTranslationTableBaseRegister
.globl SetTranslationTableBaseRegister
.globl GetDomainAccessControlRegister
.globl SetDomainAccessControlRegister
.globl GetDataFaultStatusRegister
.globl SetDataFaultStatusRegister
.globl GetInstructionFaultStatusRegister
.globl SetInstructionFaultStatusRegister
.globl GetFaultAddressRegister
.globl SetFaultAddressRegister

.globl _dc_inval_entries
.globl _dc_flush_entries
.globl _dc_flush
.globl _dc_inval
.globl _ic_inval


.text

BEGIN_ASM_FUNC GetControlRegister
	mrc		p15, 0, r0, c1, c0, 0
	bx		lr
END_ASM_FUNC

BEGIN_ASM_FUNC SetControlRegister
	mcr		p15, 0, r0, c1, c0, 0
	bx		lr
END_ASM_FUNC

BEGIN_ASM_FUNC GetTranslationTableBaseRegister
	mrc		p15, 0, r0, c2, c0, 0
	bx		lr
END_ASM_FUNC

BEGIN_ASM_FUNC SetTranslationTableBaseRegister
	mcr		p15, 0, r0, c2, c0, 0
	bx		lr
END_ASM_FUNC

BEGIN_ASM_FUNC GetDomainAccessControlRegister
	mrc		p15, 0, r0, c3, c0, 0
	bx		lr
END_ASM_FUNC

BEGIN_ASM_FUNC SetDomainAccessControlRegister
	mcr		p15, 0, r0, c3, c0, 0
	bx		lr
END_ASM_FUNC

BEGIN_ASM_FUNC GetDataFaultStatusRegister
	mrc		p15, 0, r0, c5, c0, 0
	bx		lr
END_ASM_FUNC

BEGIN_ASM_FUNC SetDataFaultStatusRegister
	mcr		p15, 0, r0, c5, c0, 0
	bx		lr
END_ASM_FUNC

BEGIN_ASM_FUNC GetInstructionFaultStatusRegister
	mrc		p15, 0, r0, c5, c0, 1
	bx		lr
END_ASM_FUNC

BEGIN_ASM_FUNC SetInstructionFaultStatusRegister
	mcr		p15, 0, r0, c5, c0, 1
	bx		lr
END_ASM_FUNC

BEGIN_ASM_FUNC GetFaultAddressRegister
	mrc		p15, 0, r0, c6, c0, 0
	bx		lr
END_ASM_FUNC

BEGIN_ASM_FUNC SetFaultAddressRegister
	mcr		p15, 0, r0, c6, c0, 0
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

BEGIN_ASM_FUNC _dc_invalidate
	mrc		p15, 0, pc, c7, c14, 3
	bne		_dc_invalidate
	bx		lr
END_ASM_FUNC

BEGIN_ASM_FUNC _ic_invalidate
	mov		r0, #0
	mcr		p15, 0, r0, c7, c5, 0
	bx		lr
END_ASM_FUNC

BEGIN_ASM_FUNC FlushMemory
	mov		r0, #0
	mcr		p15, 0, r0, c7, c10, 4
	bx		lr
END_ASM_FUNC

BEGIN_ASM_FUNC TlbInvalidate
	mov		r0, #0
	mcr		p15, 0, r0, c8, c7, 0
	bx		lr
END_ASM_FUNC

