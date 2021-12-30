/*
	starstruck - a Free Software reimplementation for the Nintendo/BroadOn IOS.
	syscallcore - internal communications over software interrupts

	Copyright (C) 2021	DacoTaco

# This code is licensed to you under the terms of the GNU GPL, version 2;
# see file COPYING or http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt
*/

#include "types.h"

s32 os_createThread(u32 main, void *arg, u32 *stack_top, u32 stacksize, s32 priority, u32 detached);
s32 os_joinThread(s32 threadId, u32* returnedValue);
s32 os_stopThread(s32 threadid, u32 returnValue);
s32 os_getThreadId(void);
s32 os_getProcessId(void);
s32 os_startThread( s32 threadid );
void os_yieldThread(void);
s32 os_getThreadPriority(s32 threadid);
s32 os_setThreadPriority(s32 threadid, s32 priority);
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