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

#define MAX_THREADS 100

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

//Note : do -NOT- mess with these variables. this is the order of the registers and how the irq asm code pushes them on the stack
//messing with these without the asm WILL break everything.
typedef struct 
{	
	u32 statusRegister;
	u32 registers[13];
	u32 stackPointer;
	u32 linkRegister;
	u32 programCounter;
} Registers;

typedef struct threadInfo
{
	Registers registers;
	u32 threadId;
	u32 processId;
	u8 threadState;
	void* arguments;
	u32* stackAddress;
	s32 initialPriority;
	s32 priority;
	s32 isDetached;	
	u32 returnValue;
	struct threadInfo* nextThread;
	struct threadInfo** threadQueue;
} threadInfo;

extern u8 active_thread;
extern threadInfo threads[MAX_THREADS];
extern threadInfo* threadQueue[MAX_THREADS];
extern threadInfo* currentThread;

void InitializeThreadContext(void);
void ScheduleYield( void );
void YieldThread( void );
s32 CreateThread(s32 main, void *arg, u32 *stack_top, u32 stacksize, s32 priority, u32 detached);
s32 CancelThread(u32 threadId, u32 return_value);
s32 JoinThread(s32 threadId, u32* returnedValue);
s32 StartThread(s32 threadId);
s32 GetThreadID(void);
s32 GetProcessID(void);
s32 GetThreadPriority(u32 threadId);
s32 SetThreadPriority(u32 threadId, s32 priority);

#endif