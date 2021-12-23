/*
	starstruck - a Free Software reimplementation for the Nintendo/BroadOn IOS.
	threads - manage threads on the starlet

	Copyright (C) 2021	DacoTaco

# This code is licensed to you under the terms of the GNU GPL, version 2;
# see file COPYING or http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt
*/

#ifndef __THREADS_H__
#define __THREADS_H__

#include <types.h>

#define MAX_PROCESSES		20
#define MAX_THREADS 		100

typedef enum 
{
	Unset = 0,
	Ready = 1,
	Running = 2,
	Stopped = 3,
	Waiting = 4,
	Dead = 5,
	Faulted = 6
} ThreadState;

//Note : do -NOT- mess with these types. this is the order of the registers and how the irq asm code pushes them on the stack
//messing with these without the asm WILL break everything.
typedef struct 
{	
	u32 statusRegister;
	u32 registers[13];
	u32 stackPointer;
	u32 linkRegister;
	u32 programCounter;
} ThreadContext;

CHECK_OFFSET(ThreadContext, 0x00, statusRegister);
CHECK_OFFSET(ThreadContext, 0x04, registers);
CHECK_OFFSET(ThreadContext, 0x38, stackPointer);
CHECK_OFFSET(ThreadContext, 0x3C, linkRegister);
CHECK_OFFSET(ThreadContext, 0x40, programCounter);
CHECK_SIZE(ThreadContext, 0x44);

typedef struct ThreadInfo
{
	ThreadContext threadContext;
	struct ThreadInfo* nextThread;
	s32 initialPriority;
	s32 priority;
	u32 threadState;
	u32 processId;
	s32 isDetached;	
	u32 returnValue;
	struct ThreadQueue* joinQueue;
	struct ThreadQueue* threadQueue;
	ThreadContext userContext;
	u32 defaultThreadStack;
} ThreadInfo ALIGNED(0x10);

CHECK_OFFSET(ThreadInfo, 0x00, threadContext);
CHECK_OFFSET(ThreadInfo, 0x44, nextThread);
CHECK_OFFSET(ThreadInfo, 0x48, initialPriority);
CHECK_OFFSET(ThreadInfo, 0x4C, priority);
CHECK_OFFSET(ThreadInfo, 0x50, threadState);
CHECK_OFFSET(ThreadInfo, 0x54, processId);
CHECK_OFFSET(ThreadInfo, 0x58, isDetached);
CHECK_OFFSET(ThreadInfo, 0x5C, returnValue);
CHECK_OFFSET(ThreadInfo, 0x60, joinQueue);
CHECK_OFFSET(ThreadInfo, 0x64, threadQueue);
CHECK_OFFSET(ThreadInfo, 0x68, userContext);
CHECK_OFFSET(ThreadInfo, 0xAC, defaultThreadStack);
CHECK_SIZE(ThreadInfo, 0xB0);

typedef struct ThreadQueue
{
	ThreadInfo* nextThread;
} ThreadQueue;

extern ThreadInfo threads[MAX_THREADS];
extern ThreadInfo* currentThread;
extern ThreadQueue* mainQueuePtr;

void InitializeThreadContext(void);
void ScheduleYield( void );
void YieldThread( void );
void YieldCurrentThread( ThreadQueue* threadQueue );
void UnblockThread(ThreadQueue* threadQueue, s32 returnValue);
s32 CreateThread(s32 main, void *arg, u32 *stack_top, u32 stacksize, s32 priority, u32 detached);
s32 CancelThread(u32 threadId, u32 return_value);
s32 JoinThread(s32 threadId, u32* returnedValue);
s32 StartThread(s32 threadId);
s32 GetThreadID(void);
s32 GetProcessID(void);
s32 GetThreadPriority(u32 threadId);
s32 SetThreadPriority(u32 threadId, s32 priority);
s32 GetUID(void);
s32 SetUID(u32 pid, u32 uid);
s32 GetGID(void);
s32 SetGID(u32 pid, u32 gid);
#endif