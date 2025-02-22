/*
	starstruck - a Free Software reimplementation for the Nintendo/BroadOn IOS.
	threads - manage threads on the starlet

	Copyright (C) 2021	DacoTaco

# This code is licensed to you under the terms of the GNU GPL, version 2;
# see file COPYING or http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt
*/

#pragma once
#include <types.h>

#ifdef MIOS
#define MAX_PROCESSES		4
#define MAX_THREADS 		8
#else
#define MAX_PROCESSES		20
#define MAX_THREADS 		100
#endif

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
	u32 StatusRegister;
	u32 Registers[13];
	u32 StackPointer;
	u32 LinkRegister;
	u32 ProgramCounter;
} ThreadContext;

CHECK_OFFSET(ThreadContext, 0x00, StatusRegister);
CHECK_OFFSET(ThreadContext, 0x04, Registers);
CHECK_OFFSET(ThreadContext, 0x38, StackPointer);
CHECK_OFFSET(ThreadContext, 0x3C, LinkRegister);
CHECK_OFFSET(ThreadContext, 0x40, ProgramCounter);
CHECK_SIZE(ThreadContext, 0x44);

typedef struct ThreadInfo
{
	ThreadContext ThreadContext;
	struct ThreadInfo* NextThread;
#ifdef MIOS
	u32 Priority;
	u32 ThreadState;
	u32 Unknown;
#else
	u32 InitialPriority;
	u32 Priority;
	u32 ThreadState;
#endif
	u32 ProcessId;
	u32 IsDetached;	
	u32 ReturnValue;
	struct ThreadQueue* JoinQueue;
	struct ThreadQueue* ThreadQueue;
	ThreadContext UserContext;
#ifndef MIOS
	u32 DefaultThreadStack;
#endif
} ThreadInfo;

CHECK_OFFSET(ThreadInfo, 0x00, ThreadContext);
CHECK_OFFSET(ThreadInfo, 0x44, NextThread);
CHECK_OFFSET(ThreadInfo, 0x54, ProcessId);
CHECK_OFFSET(ThreadInfo, 0x58, IsDetached);
CHECK_OFFSET(ThreadInfo, 0x5C, ReturnValue);
CHECK_OFFSET(ThreadInfo, 0x60, JoinQueue);
CHECK_OFFSET(ThreadInfo, 0x64, ThreadQueue);
CHECK_OFFSET(ThreadInfo, 0x68, UserContext);
#ifdef MIOS
CHECK_OFFSET(ThreadInfo, 0x48, Priority);
CHECK_OFFSET(ThreadInfo, 0x4C, ThreadState);
CHECK_OFFSET(ThreadInfo, 0x50, Unknown);
CHECK_SIZE(ThreadInfo, 0xAC);
#else
CHECK_OFFSET(ThreadInfo, 0x48, InitialPriority);
CHECK_OFFSET(ThreadInfo, 0x4C, Priority);
CHECK_OFFSET(ThreadInfo, 0x50, ThreadState);
CHECK_OFFSET(ThreadInfo, 0xAC, DefaultThreadStack);
CHECK_SIZE(ThreadInfo, 0xB0);
#endif

typedef struct ThreadQueue
{
	ThreadInfo* NextThread;
} ThreadQueue;

extern ThreadInfo Threads[MAX_THREADS];
extern ThreadInfo* CurrentThread;
extern ThreadInfo ThreadStartingState;
extern ThreadQueue SchedulerQueue;

void InitializeThreadContext(void);
void ScheduleYield( void );
void YieldThread( void );
s32 YieldCurrentThread( ThreadQueue* threadQueue );
void UnblockThread(ThreadQueue* threadQueue, s32 returnValue);
ThreadInfo* ThreadQueue_PopThread(ThreadQueue* queue);
void ThreadQueue_PushThread( ThreadQueue* threadQueue, ThreadInfo* thread );
s32 CreateThread(u32 main, void *arg, u32 *stack_top, u32 stacksize, u32 priority, u32 detached);
s32 CancelThread(const u32 threadId, u32 return_value);
s32 JoinThread(const u32 threadId, u32* returnedValue);
s32 SuspendThread(const u32 threadId);
s32 StartThread(const u32 threadId);
u32 GetThreadID(void);
u32 GetProcessID(void);
s32 GetThreadPriority(const u32 threadId);
s32 SetThreadPriority(const u32 threadId, u32 priority);
u32 GetUID(void);
s32 SetUID(u32 pid, u32 uid);
u16 GetGID(void);
s32 SetGID(u32 pid, u16 gid);

#ifndef MIOS
s32 LaunchRM(const char* path);
#endif