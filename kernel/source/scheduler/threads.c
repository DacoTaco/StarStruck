/*
	starstruck - a Free Software reimplementation for the Nintendo/BroadOn IOS.
	threads - manage threads on the starlet

	Copyright (C) 2021	DacoTaco

# This code is licensed to you under the terms of the GNU GPL, version 2;
# see file COPYING or http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt
*/

#include <types.h>
#include <ios/processor.h>
#include <ios/gecko.h>
#include <ios/errno.h>

#include "core/defines.h"
#include "interrupt/irq.h"
#include "scheduler/threads.h"
#include "memory/memory.h"

extern const void* __thread_stacks_area_start;
extern const void* __thread_stacks_area_end;
extern const u32 __kernel_heap_size;
void EndThread();
#define STACK_SIZE		0x400

u32 ProcessUID[MAX_PROCESSES] = { 0 };
u16 ProcessGID[MAX_PROCESSES] = { 0 };
ThreadInfo threads[MAX_THREADS] SRAM_DATA ALIGNED(0x10);
ThreadQueue runningQueue ALIGNED(0x04) = { &threadStartingState };
ThreadInfo threadStartingState ALIGNED(0x04) = {.priority = -1};
ThreadInfo* currentThread ALIGNED(0x10) = NULL;
void* ThreadEndFunction = NULL;

static inline s32 _GetThreadID(ThreadInfo* thread)
{
	u32 offset = (u32)thread - (u32)(&threads[0]);
	return offset == 0 
		? 0 
		: offset / sizeof(ThreadInfo);
}

void InitializeThreadContext()
{
	//copy function to mem2 where everything can access it
	ThreadEndFunction = KMalloc(0x10);
	memcpy32(ThreadEndFunction, EndThread, 0x10);

	//Initilize thread structures & set stack pointers
	for(s16 i = 0; i < MAX_PROCESSES; i++)
	{
		ProcessUID[i] = i;
		ProcessGID[i] = i;
	}

	memset8((u8*)&__thread_stacks_area_start, 0xA5, (u32)&__thread_stacks_area_end - (u32)&__thread_stacks_area_start);

	for(s16 i = 0; i < MAX_THREADS; i++)
	{
		//IOS clears the thread structure on startup, we do it on initalize
		memset32(&threads[i], 0 , sizeof(ThreadInfo));

		//gcc works by having a downwards stack, hence setting the stack to the upper limit
		threads[i].defaultThreadStack = ((u32)&__thread_stacks_area_start) + (STACK_SIZE*(i+1));
	}
}

//save current thread state. called by the IRQ, SVC/SWI & UDF handlers
void SaveThreadInfo(ThreadContext* input)
{
	if(currentThread == NULL || input == NULL)
		return;
	
	memcpy32(&currentThread->threadContext, input, sizeof(ThreadContext));
}

//Scheduler
void ThreadQueue_RemoveThread( ThreadQueue* threadQueue, ThreadInfo* threadToRemove )
{
	if(threadQueue == NULL || threadToRemove == NULL)
		return;
	
	ThreadInfo* thread = threadQueue->nextThread;
	while(thread)
	{
		if(thread == threadToRemove)
		{
			threadQueue->nextThread = threadToRemove->nextThread;
			break;
		}

		threadQueue = (ThreadQueue*)thread->nextThread;
		thread = thread->nextThread;
	}

	return;
}

void ThreadQueue_PushThread( ThreadQueue* threadQueue, ThreadInfo* thread )
{
	//not sure if this is correct. it works, and seems to be what the asm in ios kinda looks like
	//however, looking in ghidra it looks completely different. what is ghidra thinking, and why?
	if(threadQueue == NULL || thread == NULL)
		return;

	ThreadInfo* nextThread = threadQueue->nextThread;	
	s16 threadPriority = thread->priority;
	s16 nextPriority = nextThread->priority;
	ThreadQueue* previousThread = threadQueue;

	while(threadPriority < nextPriority)
	{
		previousThread = (ThreadQueue*)&nextThread->nextThread;
		nextThread = nextThread->nextThread;
		nextPriority = nextThread->priority;
	}

	previousThread->nextThread = thread;
	thread->threadQueue = threadQueue;
	thread->nextThread = nextThread;
	return;
}

ThreadInfo* ThreadQueue_PopThread(ThreadQueue* queue)
{
	ThreadInfo* ret = queue->nextThread;
	queue->nextThread = ret->nextThread;

	return ret;
}

__attribute__((target("arm")))
void ScheduleYield( void )
{
	currentThread = ThreadQueue_PopThread(&runningQueue);
	currentThread->threadState = Running;

	SetDomainAccessControlRegister(DomainAccessControlTable[currentThread->processId]);
	MemoryTranslationTable[0xD0] = (u32)HardwareRegistersAccessTable[currentThread->processId];
	TlbInvalidate();
	FlushMemory();

	register void* threadContext	__asm__("r0") = (void*)currentThread;
	__asm__ volatile (
		"\
#ios loads the threads' state buffer back in to sp, resetting the exception's stack\n\
		msr		cpsr_c, #0xd3\n\
		add		sp, %[threadContext], %[stackOffset]\n\
		msr		cpsr_c, #0xdb\n\
		add		sp, %[threadContext], %[stackOffset]\n\
#move pointer to the context\n\
		add		%[threadContext], %[threadContext], %[threadContextOffset]\n\
#restore the status register\n\
		ldmia	%[threadContext]!, {r4}\n\
		msr		spsr_cxsf, r4\n\
#restore the rest of the state\n\
		mov		lr, %[threadContext] \n\
		ldmia	lr, {r0-r12, sp, lr}^\n\
		add		lr, lr, #0x3C\n\
#jump to thread\n\
		ldmia	lr, {pc}^\n"
		:
		: [threadContext] "r" (threadContext), 
		  [threadContextOffset] "J" (offsetof(ThreadInfo, threadContext)),
		  [stackOffset] "J" (offsetof(ThreadInfo, userContext) + sizeof(ThreadContext))
	);
}

//Called syscalls.
void YieldThread( void )
{
	s32 state = DisableInterrupts();
	currentThread->threadState = Ready;

	YieldCurrentThread(&runningQueue);

	RestoreInterrupts(state);
}

void UnblockThread(ThreadQueue* threadQueue, s32 returnValue)
{
	ThreadInfo* nextThread = ThreadQueue_PopThread(threadQueue);
	nextThread->threadContext.registers[0] = returnValue;
	nextThread->threadState = Ready;
	
	ThreadQueue_PushThread(&runningQueue, nextThread);
	nextThread = currentThread;
	
	if(nextThread->priority < runningQueue.nextThread->priority)
	{
		currentThread->threadState = Ready;
		YieldCurrentThread(&runningQueue);
	}
}

//IOS Handlers
s32 CreateThread(s32 main, void *arg, u32 *stack_top, u32 stacksize, s32 priority, u32 detached)
{
	int threadId = 0;
	s32 irqState = DisableInterrupts();

	if(priority >= 0x80 || (stack_top != NULL && stacksize == 0) || (currentThread != NULL && priority > currentThread->initialPriority))
	{
		threadId = IPC_EINVAL;
		goto restore_and_return;
	}
		
	ThreadInfo* selectedThread;	
	while(threadId < MAX_THREADS)
	{
		selectedThread = &threads[threadId];
		if(selectedThread->threadState == Unset)
			break;
		
		threadId++;
	}
	
	if(threadId >= MAX_THREADS)
	{
		threadId = IPC_EMAX;
		goto restore_and_return;
	}

	selectedThread->processId = (currentThread == NULL) ? 0 : currentThread->processId;
	selectedThread->threadState = Stopped;
	selectedThread->priority = priority;
	selectedThread->initialPriority = priority;
	selectedThread->threadContext.programCounter = main;
	selectedThread->threadContext.registers[0] = (u32)arg;
	selectedThread->threadContext.linkRegister = (u32)ThreadEndFunction;
	selectedThread->threadContext.stackPointer = stack_top == NULL
		? selectedThread->defaultThreadStack 
		: (u32)stack_top;
		
	//set thread state correctly
	selectedThread->threadContext.statusRegister = ((main & 0x01) == 1)
		? (SPSR_USER_MODE | SPSR_THUMB_MODE)
		: SPSR_USER_MODE ;
	selectedThread->nextThread = NULL;
	selectedThread->threadQueue = NULL;
	selectedThread->joinQueue = NULL;
	selectedThread->isDetached = detached;
	
restore_and_return:
	RestoreInterrupts(irqState);
	return threadId;	
}

s32	StartThread(s32 threadId)
{
	s32 irqState = DisableInterrupts();
	s32 ret = 0;
	ThreadQueue* threadQueue = NULL;

	if(threadId >= MAX_THREADS || threadId < 0)
	{
		ret = IPC_EINVAL;
		goto restore_and_return;
	}
	
	ThreadInfo* threadToStart = NULL;
	if( threadId == 0 )
		threadToStart = currentThread;
	
	if(threadToStart == NULL)
		threadToStart = &threads[threadId];

	//does the current thread even own the thread?
	if( currentThread != NULL && currentThread->processId != 0 && threadToStart->processId != currentThread->processId)
	{
		ret = IPC_EINVAL;
		goto restore_and_return;
	}
	
	if(threadToStart->threadState != Stopped)
	{
		ret = IPC_EINVAL;
		goto restore_and_return;
	}
	
	threadQueue = threadToStart->threadQueue;
	if(threadQueue == NULL || threadQueue == &runningQueue)
	{
		threadToStart->threadState = Ready;
		ThreadQueue_PushThread(&runningQueue, threadToStart);
	}
	else
	{
		threadToStart->threadState = Waiting;
		ThreadQueue_PushThread(threadQueue, threadToStart);
	}

	if(currentThread == NULL)
		ScheduleYield();
	else if(currentThread->priority < runningQueue.nextThread->priority)
	{
		currentThread->threadState = Ready;
		YieldCurrentThread(&runningQueue);
	}
	
restore_and_return:
	RestoreInterrupts(irqState);
	return ret;	
}

s32 CancelThread(u32 threadId, u32 return_value)
{
	s32 irqState = DisableInterrupts();
	s32 ret = 0;
	
	if(threadId > MAX_THREADS)
	{
		ret = IPC_EINVAL;
		goto restore_and_return;
	}
	
	ThreadInfo* threadToCancel = NULL;
	if( threadId == 0 )
		threadToCancel = currentThread;

	if(threadToCancel == NULL)
		threadToCancel = &threads[threadId];

	//does the current thread even own the thread?
	if( currentThread != NULL && currentThread->processId != 0 && threadToCancel->processId != currentThread->processId)
	{
		ret = IPC_EINVAL;
		goto restore_and_return;
	}
	
	threadToCancel->returnValue = return_value;	
	if(threadToCancel->threadState != Stopped)
		ThreadQueue_RemoveThread(&runningQueue, threadToCancel);		
	
	if(!threadToCancel->isDetached)
		threadToCancel->threadState = Dead;
	else
		threadToCancel->threadState = Unset;
	
	currentThread->threadContext.registers[0] = ret;
	if(threadToCancel == currentThread)
		ScheduleYield();
	else if(currentThread->priority < runningQueue.nextThread->priority)
	{
		currentThread->threadState = Ready;
		YieldCurrentThread(&runningQueue);
	}
	
restore_and_return:
	RestoreInterrupts(irqState);
	return ret;
}

s32 JoinThread(s32 threadId, u32* returnedValue)
{
	s32 irqState = DisableInterrupts();
	s32 ret = 0;
	
	if(threadId > MAX_THREADS)
	{
		ret = IPC_EINVAL;
		goto restore_and_return;
	}
	
	ThreadInfo* threadToJoin = NULL;
	if( threadId == 0 )
		threadToJoin = currentThread;

	if(threadToJoin == NULL)
		threadToJoin = &threads[threadId];

	//does the current thread even own the thread?
	if( currentThread != NULL && currentThread->processId != 0 && threadToJoin->processId != currentThread->processId)
	{
		ret = IPC_EINVAL;
		goto restore_and_return;
	}
	
	if(threadToJoin == currentThread || threadToJoin->isDetached)
		goto restore_and_return;
	
	ThreadState threadState = threadToJoin->threadState;	
	if(threadState != Dead)
	{
		currentThread->threadState = Waiting;
		YieldCurrentThread(&runningQueue);
		threadState = threadToJoin->threadState;
	}
	
	if(returnedValue != NULL)
		*returnedValue = threadToJoin->returnValue;
	
	if(threadState != Dead)
		gecko_printf("thread %d is not dead, but join from %d resumed\n", _GetThreadID(threadToJoin), _GetThreadID(currentThread) );

	threadToJoin->threadState = Unset;	
restore_and_return:
	RestoreInterrupts(irqState);
	return ret;
}

s32 GetThreadID()
{
	return _GetThreadID(currentThread);
}

s32 GetProcessID()
{
	return currentThread->processId;
}

s32 GetThreadPriority( u32 threadId )
{
	s32 irqState = DisableInterrupts();
	
	ThreadInfo* thread;
	s32 ret;
	
	if(threadId == 0 && currentThread != NULL)
	{
		ret = currentThread->priority;
		goto restore_and_return;
	}
	
	if(threadId >= MAX_THREADS)
		goto return_error;
	
	thread = &threads[threadId];
	//does the current thread even own the thread?
	if( currentThread != NULL && currentThread->processId != 0 && thread->processId != currentThread->processId)
		goto return_error;
	
	ret = thread->priority;
	goto restore_and_return;
	
return_error:
	ret = IPC_EINVAL;
restore_and_return:
	RestoreInterrupts(irqState);
	return ret;
}

s32 SetThreadPriority( u32 threadId, s32 priority )
{
	s32 irqState = DisableInterrupts();

	ThreadInfo* thread = NULL;
	s32 ret = 0;
	
	if(threadId > MAX_THREADS || priority >= 0x80 )
		goto return_error;
	
	if( threadId == 0 )
		thread = currentThread;

	if(thread == NULL)
		thread = &threads[threadId];

	//does the current thread even own the thread?
	if( currentThread != NULL && currentThread->processId != 0 && thread->processId != currentThread->processId)
		goto return_error;
	
	if(priority >= thread->initialPriority)
		goto return_error;
	
	if(thread->priority == priority)
		goto restore_and_return;
	
	thread->priority = priority;
	if(thread != currentThread && thread->threadState != Stopped)
	{
		ThreadQueue_RemoveThread(&runningQueue, thread);
		ThreadQueue_PushThread(&runningQueue, thread);
	}
	
	if( currentThread->priority < runningQueue.nextThread->priority )
	{
		currentThread->threadState = Ready;
		YieldCurrentThread(&runningQueue);
	}
	goto restore_and_return;
	
return_error:
	ret = IPC_EINVAL;
restore_and_return:
	RestoreInterrupts(irqState);
	return ret;
}

s32 GetUID(void)
{
	return ProcessUID[currentThread->processId];
}

s32 SetUID(u32 pid, u32 uid)
{
	s32 ret = IPC_SUCCESS;
	s32 irqState = DisableInterrupts();
	
	if(pid >= MAX_PROCESSES)
	{
		ret = IPC_EINVAL;
		goto restore_and_return;
	}
	
	if(currentThread->processId >= 2)
	{
		ret = IPC_EACCES;
		goto restore_and_return;
	}
	
	ProcessUID[pid] = uid;
	
restore_and_return:
	RestoreInterrupts(irqState);
	return ret;
}

s32 GetGID(void)
{
	return ProcessGID[currentThread->processId];
}

s32 SetGID(u32 pid, u32 gid)
{
	s32 ret = IPC_SUCCESS;
	s32 irqState = DisableInterrupts();
	
	if(pid >= MAX_PROCESSES)
	{
		ret = IPC_EINVAL;
		goto restore_and_return;
	}
	
	if(currentThread->processId >= 2)
	{
		ret = IPC_EACCES;
		goto restore_and_return;
	}
	
	ProcessGID[pid] = gid;
	
restore_and_return:
	RestoreInterrupts(irqState);
	return ret;
}
