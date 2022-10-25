/*
	starstruck - a Free Software reimplementation for the Nintendo/BroadOn IOS.
	syscallcore - internal communications over software interrupts

	Copyright (C) 2021	DacoTaco

# This code is licensed to you under the terms of the GNU GPL, version 2;
# see file COPYING or http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt
*/

#include "types.h"

//required pragma to make GCC not optimize out our parameters/calls
#pragma GCC push_options
#pragma GCC optimize ("O1")

s32 OSCreateThread(u32 main, void *arg, u32 *stack_top, u32 stacksize, s32 priority, u32 detached);
s32 OSJoinThread(s32 threadId, u32* returnedValue);
s32 OSStopThread(s32 threadid, u32 returnValue);
s32 OSGetThreadId(void);
s32 OSGetProcessId(void);
s32 OSStartThread( s32 threadid );
void OSYieldThread(void);
s32 OSGetThreadPriority(s32 threadid);
s32 OSSetThreadPriority(s32 threadid, s32 priority);
s32 OSCreateMessageQueue(void *ptr, u32 size);
s32 OSDestroyMessageQueue(s32 queueid);
s32 OSSendMessage(s32 queueid, void *message, u32 flags);
s32 OSReceiveMessage(s32 queueid, void *message, u32 flags);
s32 OSCreateHeap(void *ptr, u32 size);
s32 OSDestroyHeap(s32 heapid);
s32 OSRegisterEventHandler(u8 device, s32 queueid, s32 message);
s32 OSUnregisterEventHandler(u8 device);
void* OSAllocateMemory(s32 heapid, u32 size);
void* OSAlignedAllocateMemory(s32 heapid, u32 size, u32 align);
s32 OSFreeMemory(s32 heapid, void *ptr);

//special IOS syscall to print something to debug device
void OSPrintk(const char* str);

#pragma GCC pop_options
