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
#define MAX_PROCESSES 4
#define MAX_THREADS   8
#else
#define MAX_PROCESSES 20
#define MAX_THREADS   100
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

typedef enum
{
	KernelId = 0,
	ESId = 1,
	FSId = 2,
	DIId = 3,
	OH0Id = 4,
	OH1Id = 5,
	EHCIId = 6,
	SDIId = 7,
	USBEthId = 8,
	NetId = 9,
	WDId = 10,
	WLId = 11,
	KDId = 12,
	NCDId = 13,
	STMId = 14,
	PPCBOOTId = 15,
	SSLId = 16,
	USBId = 17,
	P2PId = 18,
	WFSId = 19,
} ProcessorIds;

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
	ThreadContext Context;
	struct ThreadInfo *NextThread;
#ifdef MIOS
	s32 Priority;
	u32 ThreadState;
	u32 Unknown;
#else
	s32 InitialPriority;
	s32 Priority;
	u32 ThreadState;
#endif
	u32 ProcessId;
	u32 IsDetached;
	u32 ReturnValue;
	struct ThreadQueue *JoinQueue;
	struct ThreadQueue *ThreadQueue;
	ThreadContext UserContext;
#ifndef MIOS
	u32 DefaultThreadStack;
#endif
} ThreadInfo;

CHECK_OFFSET(ThreadInfo, 0x00, Context);
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
	ThreadInfo *NextThread;
} ThreadQueue;

extern ThreadInfo Threads[MAX_THREADS];
extern ThreadInfo *CurrentThread;
extern ThreadInfo ThreadStartingState;
extern ThreadQueue SchedulerQueue;

void InitializeThreadContext(void);
void ScheduleYield(void);
void YieldThread(void);
s32 YieldCurrentThread(ThreadQueue *threadQueue);
void UnblockThread(ThreadQueue *threadQueue, s32 returnValue);
ThreadInfo *ThreadQueue_PopThread(ThreadQueue *queue);
void ThreadQueue_PushThread(ThreadQueue *threadQueue, ThreadInfo *thread);
s32 CreateThread(u32 main, void *arg, u32 *stack_top, u32 stacksize,
                 s32 priority, u32 detached);
s32 CancelThread(const s32 threadId, u32 return_value);
s32 JoinThread(const s32 threadId, u32 *returnedValue);
s32 SuspendThread(const s32 threadId);
s32 StartThread(const s32 threadId);
s32 GetThreadID(void);
u32 GetProcessID(void);
s32 GetThreadPriority(const s32 threadId);
s32 SetThreadPriority(const s32 threadId, s32 priority);
u32 GetUID(void);
s32 SetUID(u32 pid, u32 uid);
u16 GetGID(void);
s32 SetGID(u32 pid, u16 gid);

#ifndef MIOS
s32 LaunchModule(const char *path);
#endif
