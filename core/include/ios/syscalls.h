/*
	starstruck - a Free Software reimplementation for the Nintendo/BroadOn IOS.
	syscallcore - internal communications over software interrupts

	Copyright (C) 2021	DacoTaco

# This code is licensed to you under the terms of the GNU GPL, version 2;
# see file COPYING or http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt
*/

//required pragma to make GCC not optimize out our parameters/calls
#pragma once
#pragma GCC push_options
#pragma GCC optimize ("O1")
#include "types.h"
#include "ios/ipc.h"
#include "ios/ahb.h"

s32 OSCreateThread(u32 main, void *arg, u32 *stack_top, u32 stacksize, u32 priority, u32 detached);
s32 OSJoinThread(s32 threadId, u32* returnedValue);
s32 OSStopThread(s32 threadid, u32 returnValue);
s32 OSGetThreadId(void);
s32 OSGetProcessId(void);
s32 OSStartThread(s32 threadid);
void OSYieldThread(void);
s32 OSGetThreadPriority(s32 threadid);
s32 OSSetThreadPriority(s32 threadid, u32 priority);
s32 OSCreateMessageQueue(void *ptr, u32 size);
s32 OSDestroyMessageQueue(s32 queueid);
s32 OSSendMessage(s32 queueid, void *message, u32 flags);
s32 OSReceiveMessage(s32 queueid, void *message, u32 flags);
s32 OSRegisterEventHandler(u8 device, s32 queueid, void* message);
s32 OSUnregisterEventHandler(u8 device);
s32 OSCreateTimer(u32 delayUs, u32 periodUs, const s32 queueid, void *message);
s32 OSDestroyTimer(s32 timerId);
s32 OSStopTimer(s32 timerId);
s32 OSRestartTimer(s32 timerId, u32 timeUs, u32 repeatTimeUs);
u32 OSGetTimerValue(void);
s32 OSCreateHeap(void *ptr, u32 size);
s32 OSDestroyHeap(s32 heapid);
void* OSAllocateMemory(s32 heapid, u32 size);
void* OSAlignedAllocateMemory(s32 heapid, u32 size, u32 align);
s32 OSFreeMemory(s32 heapid, void *ptr);
s32 OSRegisterResourceManager(const char* devicePath, const s32 queueid);
s32 OSOpenFD(const char* path, int mode);
s32 OSCloseFD(s32 fd);
s32 OSReadFD(s32 fd, void *buf, u32 len);
s32 OSWriteFD(s32 fd, const void *buf, u32 len);
s32 OSSeekFD(s32 fd, s32 offset, s32 origin);
s32 OSIoctlFD(s32 fd, u32 requestId, void *inputBuffer, u32 inputBufferLength, void *outputBuffer, u32 outputBufferLength);
s32 OSIoctlvFD(s32 fd, u32 requestId, u32 vectorInputCount, u32 vectorIOCount, IoctlvMessageData *vectors);
s32 OSOpenFDAsync(const char* path, s32 mode, s32 messageQueueId, IpcMessage* message);
s32 OSCloseFDAsync(s32 fd, s32 messageQueueId, IpcMessage* message);
s32 OSReadFDAsync(s32 fd, void *buf, u32 len, s32 messageQueueId, IpcMessage* message);
s32 OSWriteFDAsync(s32 fd, const void *buf, u32 len, s32 messageQueueId, IpcMessage* message);
s32 OSSeekFDAsync(s32 fd, s32 offset, s32 origin, s32 messageQueueId, IpcMessage* message);
s32 OSIoctlFDAsync(s32 fd, u32 requestId, void *inputBuffer, u32 inputBufferLength, void *outputBuffer, u32 outputBufferLength, s32 messageQueueId, IpcMessage* message);
s32 OSIoctlvFDAsync(s32 fd, u32 requestId, u32 vectorInputCount, u32 vectorIOCount, IoctlvMessageData *vectors, s32 messageQueueId, IpcMessage* message);
s32 OSResourceReply(IpcMessage* message, s32 requestReturnValue);
u32 OSGetUID(void);
s32 OSSetUID(u32 pid, u32 uid);
u16 OSGetGID(void);
s32 OSSetGID(u32 pid, u16 gid);
void OSAhbFlushFrom(AHBDEV type);
void OSAhbFlushTo(AHBDEV type);

void OSDCInvalidateRange(const void* start, u32 size);
void OSDCFlushRange(const void *start, u32 size);

u32 OSVirtualToPhysical(u32 virtualAddress);

s32 OSGetIOSCData(u32 keyHandle, u32* value);

//special IOS syscall to print something to debug device
void OSPrintk(const char* str);

#pragma GCC pop_options
