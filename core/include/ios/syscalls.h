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

s32 OSCreateThread(u32 main, void *arg, u32 *stack_top, u32 stacksize, u32 priority, u32 detached);
s32 OSJoinThread(u32 threadId, u32* returnedValue);
s32 OSStopThread(u32 threadid, u32 returnValue);
u32 OSGetThreadId(void);
s32 OSGetProcessId(void);
s32 OSStartThread( u32 threadid );
void OSYieldThread(void);
s32 OSGetThreadPriority(u32 threadid);
s32 OSSetThreadPriority(u32 threadid, u32 priority);
s32 OSCreateMessageQueue(void *ptr, u32 size);
s32 OSDestroyMessageQueue(u32 queueid);
s32 OSSendMessage(u32 queueid, void *message, u32 flags);
s32 OSReceiveMessage(u32 queueid, void *message, u32 flags);
s32 OSCreateHeap(void *ptr, u32 size);
s32 OSDestroyHeap(u32 heapid);
s32 OSRegisterEventHandler(u8 device, u32 queueid, void* message);
s32 OSUnregisterEventHandler(u8 device);
void* OSAllocateMemory(u32 heapid, u32 size);
void* OSAlignedAllocateMemory(u32 heapid, u32 size, u32 align);
s32 OSFreeMemory(u32 heapid, void *ptr);
s32 OSRegisterResourceManager(const char* devicePath, const u32 queueid);

//special IOS syscall to print something to debug device
void OSPrintk(const char* str);

#pragma GCC pop_options
