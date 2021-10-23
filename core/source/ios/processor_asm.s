/*
	mini - a Free Software replacement for the Nintendo/BroadOn IOS.
	random utilities

Copyright (C) 2008, 2009	Hector Martin "marcan" <marcan@marcansoft.com>

# This code is licensed to you under the terms of the GNU GPL, version 2;
# see file COPYING or http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt
*/

#include "asminc.h"

.arm
.globl debug_output
.globl get_cpsr
.globl memcpy32
.globl memcpy16
.globl memcpy8
.globl memset32
.globl memset16
.globl memset8
.text

BEGIN_ASM_FUNC debug_output
	@ load address of port
	mov	r3, #0xd800000
	@ load old value
	ldr	r2, [r3, #0xe0]
	@ clear debug byte
	bic	r2, r2, #0xFF0000
	@ insert new value
	and	r0, r0, #0xFF
	orr	r2, r2, r0, LSL #16
	@ store back
	str	r2, [r3, #0xe0]
	mov	pc, lr
END_ASM_FUNC
	
BEGIN_ASM_FUNC get_cpsr
	mrs	r0, cpsr
	bx		lr
END_ASM_FUNC

BEGIN_ASM_FUNC memcpy32
	bics	r2, #3
	bxeq	lr
1:	ldr		r3, [r1],#4
	str		r3, [r0],#4
	subs	r2, #4
	bne		1b
	bx		lr
END_ASM_FUNC

BEGIN_ASM_FUNC memset32
	bics	r2, #3
	bxeq	lr
1:	str		r1, [r0],#4
	subs	r2, #4
	bne		1b
	bx		lr
END_ASM_FUNC

BEGIN_ASM_FUNC memcpy16
	bics	r2, #1
	bxeq	lr
1:	ldrh	r3, [r1],#2
	strh	r3, [r0],#2
	subs	r2, #2
	bne		1b
	bx		lr
END_ASM_FUNC

BEGIN_ASM_FUNC memset16
	bics	r2, #1
	bxeq	lr
1:	strh	r1, [r0],#2
	subs	r2, #2
	bne		1b
	bx		lr
END_ASM_FUNC

BEGIN_ASM_FUNC memcpy8
	cmp		r2, #0
	bxeq	lr
1:	ldrb	r3, [r1],#1
	strb	r3, [r0],#1
	subs	r2, #1
	bne		1b
	bx		lr
END_ASM_FUNC

BEGIN_ASM_FUNC memset8
	cmp		r2, #0
	bxeq	lr
1:	strb	r1, [r0],#1
	subs	r2, #1
	bne		1b
	bx		lr
END_ASM_FUNC
