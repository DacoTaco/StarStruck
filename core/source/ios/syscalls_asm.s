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

_SYSCALL os_createThread,			0x0000
_SYSCALL os_joinThread,				0x0001
_SYSCALL os_stopThread,				0x0002
_SYSCALL os_getThreadId,			0x0003
_SYSCALL os_getProcessId,			0x0004
_SYSCALL os_startThread,			0x0005
_SYSCALL os_suspendThread,			0x0006
_SYSCALL os_yieldThread,			0x0007
_SYSCALL os_getThreadPriority,		0x0008
_SYSCALL os_setThreadPriority,		0x0009
_SYSCALL os_createMessageQueue,		0x000A
_SYSCALL os_destroyMessageQueue,	0x000B
_SYSCALL os_sendMessage,			0x000C
_SYSCALL os_receiveMessage,			0x000E
_SYSCALL os_registerEventHandler,	0x000F
_SYSCALL os_unregisterEventHandler,	0x0010
_SYSCALL os_getTimerValue,			0x0015
_SYSCALL os_createHeap,				0x0016
_SYSCALL os_destroyHeap,			0x0017
_SYSCALL os_allocateMemory,			0x0018
_SYSCALL os_alignedAllocateMemory,	0x0019
_SYSCALL os_freeMemory,				0x001A

/* this is a special svc syscall. its the only syscall left in IOS. only used for printk too */
.thumb
.globl os_printk
BEGIN_ASM_FUNC os_printk
	mov		r1, r0
	movs	r0, #4
	svc		0xAB
	bx		lr
END_ASM_FUNC

#pragma GCC pop_options
