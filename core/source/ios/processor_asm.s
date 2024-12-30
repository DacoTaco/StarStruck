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
.globl GetCurrentStatusRegister
.globl GetSavedStatusRegister
.globl BusyDelay
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
	bx lr
END_ASM_FUNC
	
BEGIN_ASM_FUNC GetCurrentStatusRegister
	mrs		r0, cpsr
	bx		lr
END_ASM_FUNC

BEGIN_ASM_FUNC GetSavedStatusRegister
	mrs		r0, spsr
	bx		lr
END_ASM_FUNC

.thumb
BEGIN_ASM_FUNC BusyDelay
	push {lr}
	mov r3, #0
	sub r0, #4
	add r2, r0, #0
	cmp r3, r0
	bcs BusyDelay_done
BusyDelay_loop:
	add r3, #1
	cmp r3, r2
	bcc BusyDelay_loop
BusyDelay_done:
	pop  {r0}
	bx r0
END_ASM_FUNC
.arm
