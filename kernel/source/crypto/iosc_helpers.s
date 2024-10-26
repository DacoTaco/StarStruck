/*
	StarStruck - a Free Software reimplementation for the Nintendo/BroadOn IOS.
	iosc helpers - stack swapping function

	Copyright (C) 2021	DacoTaco

# This code is licensed to you under the terms of the GNU GPL, version 2;
# see file COPYING or http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt
*/

#include <asminc.h>
#include <ios/processor.h>

.arm
.globl IOSC_SwapStack

/* void IOSC_SwapStack(u32 currentStackBase: r0, u32 newStackBase: r1) */
BEGIN_ASM_FUNC IOSC_SwapStack
	/*
	 * u32* src: r0
	 * u32* dst: r1
	 * u32* end: r2 (<= src)
	 */
	mov r2, sp
	/*
	 * r3 = current stack base - current stack pointer
	 * r3 = current stack base - (current stack base - current stack size)
	 * r3 = current stack size
	 */
	rsb r3, r2, r0
	/*
	 * r3 = new stack base - current stack size
	 * r3 = new stack pointer
	 */
	rsb r3, r3, r1
	mov sp, r3
	
	/*
	 * early exit if stack is empty. should not happen in practice.
	 */
	cmp r0, r2
	bxls lr

	/*
	 * while(src > end)
	 *   *(--dst) = *(--src);
	 */
IOSC_SwapStack_copy_loop:
	ldr r3,[r0, #-4]!
	cmp r0, r2
	str r3,[r1, #-4]!
	bhi IOSC_SwapStack_copy_loop
	bx lr
END_ASM_FUNC
