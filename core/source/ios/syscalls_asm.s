/*
	starstruck - a Free Software reimplementation for the Nintendo/BroadOn IOS.
	syscalls - internal communications over software interrupts

	Copyright (C) DacoTaco

# This code is licensed to you under the terms of the GNU GPL, version 2;
# see file COPYING or http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt
*/

#include "asminc.h"
#pragma GCC push_options
#pragma GCC optimize ("O1")

.arm
#our syscall macro. all syscalls are basically the same hehe
.macro _SYSCALL name, syscall
	.globl \name
    BEGIN_ASM_FUNC \name
		swi		\syscall
		bx		lr
	END_ASM_FUNC
.endm

_SYSCALL OSCreateThread,			0x0000
_SYSCALL OSJoinThread,				0x0001
_SYSCALL OSStopThread,				0x0002
_SYSCALL OSGetThreadId,				0x0003
_SYSCALL OSGetProcessId,			0x0004
_SYSCALL OSStartThread,				0x0005
_SYSCALL OSSuspendThread,			0x0006
_SYSCALL OSYieldThread,				0x0007
_SYSCALL OSGetThreadPriority,		0x0008
_SYSCALL OSSetThreadPriority,		0x0009
_SYSCALL OSCreateMessageQueue,		0x000A
_SYSCALL OSDestroyMessageQueue,		0x000B
_SYSCALL OSSendMessage,				0x000C
_SYSCALL OSReceiveMessage,			0x000E
_SYSCALL OSRegisterEventHandler,	0x000F
_SYSCALL OSUnregisterEventHandler,	0x0010
_SYSCALL OSGetTimerValue,			0x0015
_SYSCALL OSCreateHeap,				0x0016
_SYSCALL OSDestroyHeap,				0x0017
_SYSCALL OSAllocateMemory,			0x0018
_SYSCALL OSAlignedAllocateMemory,	0x0019
_SYSCALL OSFreeMemory,				0x001A

/* this is a special svc syscall. its the only syscall left in IOS. only used for printk too */
.thumb
.globl OSPrintk
BEGIN_ASM_FUNC OSPrintk
	mov		r1, r0
	movs	r0, #4
	svc		0xAB
	bx		lr
END_ASM_FUNC

#pragma GCC pop_options
