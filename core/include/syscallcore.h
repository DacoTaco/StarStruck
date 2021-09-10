/*
	starstruck - a Free Software reimplementation for the Nintendo/BroadOn IOS.
	syscallcore - internal communications over software interrupts

	Copyright (C) 2021	DacoTaco

# This code is licensed to you under the terms of the GNU GPL, version 2;
# see file COPYING or http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt
*/

#include "types.h"

#define SYSCALL_CREATETHREAD			0x0000
#define SYSCALL_STOPTHREAD				0x0002
#define SYSCALL_STARTTHREAD				0x0005
#define SYSCALL_YIELDTHREAD				0x0007
#define SYSCALL_GETTHREADPRIORITY		0x0008
#define SYSCALL_SETTHREADPRIORITY		0x0009
#define SYSCALL_CREATEMESSAGEQUEUE		0x000A
#define SYSCALL_DESTROYMESSAGEQUEUE		0x000B
#define SYSCALL_SENDMESSAGE				0x000C
#define SYSCALL_RECEIVEMESSAGE			0x000E
#define SYSCALL_REGISTEREVENTHANDLER	0x000F
#define SYSCALL_UNREGISTEREVENTHANDLER	0x0010
#define SYSCALL_CREATEHEAP				0x0016
#define SYSCALL_DESTROYHEAP				0x0017
#define SYSCALL_MALLOC					0x0018
#define SYSCALL_MEMALIGN				0x0019
#define SYSCALL_MEMFREE					0x001A

s32 os_createThread(s32 main, void *arg, u32 *stack_top, u32 stacksize, s32 priority, u32 detached);
s32 os_stopThread( s32 threadid, u32 return_value );
s32 os_startThread( s32 threadid );
s32 os_yieldThread(void);
s32 os_getThreadPriority( s32 threadid );
s32 os_setThreadPriority( s32 threadid, s32 priority );
s32 os_createMessageQueue(void *ptr, u32 size);
s32 os_destroyMessageQueue(s32 queueid);
s32 os_sendMessage(s32 queueid, void *message, u32 flags);
s32 os_receiveMessage(s32 queueid, void *message, u32 flags);
s32 os_createHeap(void *ptr, u32 size);
s32 os_destroyHeap(s32 heapid);
s32 os_registerEventHandler(u8 device, s32 queueid, s32 message);
s32 os_unregisterEventHandler(u8 device);
void* os_allocateMemory(s32 heapid, u32 size);
void* os_alignedAllocateMemory(s32 heapid, u32 size, u32 align);
s32 os_freeMemory(s32 heapid, void *ptr);