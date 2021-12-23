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
#include "interrupt/threads.h"
#include "memory/memory.h"

extern const void* __thread_stacks_area_start;
extern const void* __thread_stacks_area_end;
extern const u32 __kernel_heap_size;
#define STACK_SIZE		0x400

u32 ProcessUID[MAX_PROCESSES] = { 0 };
u16 ProcessGID[MAX_PROCESSES] = { 0 };
ThreadInfo threads[MAX_THREADS] ALIGNED(0x10);
ThreadQueue mainQueue ALIGNED(0x10) = { &threads[0] };
ThreadQueue* mainQueuePtr = &mainQueue;
ThreadInfo* currentThread ALIGNED(0x10);

static inline s32 _GetThreadID(ThreadInfo* thread)
{
	u32 offset = (u32)thread - (u32)(&threads[0]);
	return offset == 0 
		? 0 
		: offset / sizeof(ThreadInfo);
}

void _thread_end()
{
	u32 ret = 0;
	__asm__ volatile ("mov\t%0, r0" : "=l" (ret));
	CancelThread( 0, ret );
}

void InitializeThreadContext()
{
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
void UnQueueThread( ThreadQueue* threadQueue, ThreadInfo* thread )
{
	ThreadInfo* nextThread = (threadQueue == NULL)
		? NULL
		: threadQueue->nextThread;
	
	while(1)
	{
		if(nextThread == NULL)
			return;
		
		if(nextThread == thread)
			break;
		
		//place pointer to the next thread pointer into the queue variable.
		//since threadQueue starts with a pointer this... works.
		//wtf nintendo? :)
		threadQueue = (ThreadQueue*)&nextThread->nextThread;
		nextThread = nextThread->nextThread;
	}
	
	threadQueue->nextThread = thread->nextThread;
	return;
}

void QueueNextThread( ThreadQueue* threadQueue, ThreadInfo* thread )
{
	ThreadInfo* nextThread = threadQueue->nextThread;
	s16 threadPriority = thread->priority;
	s16 nextPriority = nextThread->priority;
	s16 newPriority = nextPriority - threadPriority;
	ThreadQueue* previousThreadLink = threadQueue;

	while((newPriority <= 0) == ( threadPriority > nextPriority ) )
	{
		if(nextThread->nextThread == NULL || nextThread == nextThread->nextThread)
			break;
		
		//place pointer to the next thread pointer into the queue variable.
		//since threadQueue starts with a pointer this... works.
		//wtf nintendo? :)
		previousThreadLink = (ThreadQueue*)&nextThread->nextThread;
		nextThread = nextThread->nextThread;
		nextPriority = nextThread->priority;
		newPriority = nextPriority - threadPriority;
	}
	
	previousThreadLink->nextThread = thread;
	thread->threadQueue = threadQueue;
	thread->nextThread = nextThread;
	return;
}

ThreadInfo* PopNextThreadFromQueue(ThreadQueue* queue)
{
	ThreadInfo* ret = queue->nextThread;
	queue->nextThread = ret->nextThread;
	return ret;
}

__attribute__((target("arm")))
void ScheduleYield( void )
{
	currentThread = PopNextThreadFromQueue(mainQueuePtr);
	currentThread->threadState = Running;

	set_dacr(DomainAccessControlTable[currentThread->processId]);
	MemoryTranslationTable[0xD0] = (u32)HardwareRegistersAccessTable[currentThread->processId];
	tlb_invalidate();
	flush_memory();

	register void* threadContext	__asm__("r0") = (void*)&currentThread->threadContext;
	register u32 stackPointer	__asm__("r1") = ((u32)&currentThread->userContext) + sizeof(ThreadContext);
	__asm__ volatile (
		"\
#ios loads the threads' state buffer back in to sp, resetting the exception's stack\n\
		msr		cpsr_c, #0xd2\n\
		ldr		sp, =__irqstack_addr\n\
		msr		cpsr_c, #0xd3\n\
		mov		sp, %[stackPointer]\n\
		msr		cpsr_c, #0xdb\n\
		mov		sp, %[stackPointer]\n\
#restore the status register\n\
		ldmia	%[threadContext]!, {r4}\n\
		msr		spsr_cxsf, r4\n\
#restore the rest of the state\n\
		ldmia	%[threadContext]!, {r0-r12, sp, lr}^\n\
		ldmia	%[threadContext]!, {lr}\n\
#jump to thread\n\
		movs	pc, lr\n"
		:
		: [threadContext] "r" (threadContext), [stackPointer] "r" (stackPointer)
	);
	
	return;
}

//Called syscalls.
void YieldThread( void )
{
	s32 state = irq_kill();
	if(currentThread != NULL)
		currentThread->threadState = Ready;

	YieldCurrentThread(mainQueuePtr);

	irq_restore(state);
}

void UnblockThread(ThreadQueue* threadQueue, s32 returnValue)
{
	ThreadInfo* nextThread = PopNextThreadFromQueue(threadQueue);
	nextThread->threadContext.registers[0] = returnValue;
	nextThread->threadState = Ready;
	
	QueueNextThread(mainQueuePtr, nextThread);
	nextThread = currentThread;
	
	if(nextThread->priority < mainQueuePtr->nextThread->priority)
	{
		currentThread->threadState = Ready;
		YieldCurrentThread(&mainQueue);
	}
}

//IOS Handlers
s32 CreateThread(s32 main, void *arg, u32 *stack_top, u32 stacksize, s32 priority, u32 detached)
{
	int threadId = 0;
	s32 irq_state = irq_kill();
	
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

	selectedThread->threadQueue = currentThread->threadQueue;
	selectedThread->processId = (currentThread == NULL) ? 0 : currentThread->processId;
	selectedThread->threadState = Stopped;
	selectedThread->priority = priority;
	selectedThread->initialPriority = priority;
	selectedThread->threadContext.programCounter = main;
	selectedThread->threadContext.registers[0] = (u32)arg;
	selectedThread->threadContext.linkRegister = (u32)_thread_end;
	selectedThread->threadContext.stackPointer = stack_top == NULL
		? selectedThread->defaultThreadStack 
		: (u32)stack_top;
		
	//set thread state correctly: things like arm or thumb mode etc
	selectedThread->threadContext.statusRegister = (((s32)(main << 0x1f)) < 0) ? 0x30 : 0x10;
	selectedThread->nextThread = NULL;
	selectedThread->threadQueue = NULL;
	selectedThread->isDetached = detached;
	
restore_and_return:
	irq_restore(irq_state);
	return threadId;	
}

s32	StartThread(s32 threadId)
{
	s32 irq_state = irq_kill();
	s32 ret = 0;
	ThreadQueue* threadQueue = NULL;

	if(threadId > MAX_THREADS || threadId < 0)
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
		goto restore_and_return;
	
	threadQueue = threadToStart->threadQueue;
	if(threadQueue == NULL || threadQueue == mainQueuePtr)
	{
		threadToStart->threadState = Ready;
		QueueNextThread(mainQueuePtr, threadToStart);
		threadToStart = currentThread;
	}
	else
	{
		threadToStart->threadState = Waiting;
		QueueNextThread(threadQueue, threadToStart);
		threadToStart = currentThread;
	}

	if(threadToStart == NULL)
		ScheduleYield();
	else if(threadToStart->priority < mainQueuePtr->nextThread->priority)
	{
		threadToStart->threadState = Ready;
		YieldCurrentThread(mainQueuePtr);
	}
	
restore_and_return:
	irq_restore(irq_state);
	return ret;	
}

s32 CancelThread(u32 threadId, u32 return_value)
{
	s32 irq_state = irq_kill();
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
		UnQueueThread(mainQueuePtr, threadToCancel);
	
	if(!threadToCancel->isDetached)
		threadToCancel->threadState = Dead;
	else
		threadToCancel->threadState = Unset;
	
	currentThread->threadContext.registers[0] = ret;
	if(threadToCancel == currentThread)
		ScheduleYield();
	else
	{
		currentThread->threadState = Ready;
		YieldCurrentThread(mainQueuePtr);
	}
	
restore_and_return:
	irq_restore(irq_state);
	return ret;
}

s32 JoinThread(s32 threadId, u32* returnedValue)
{
	s32 irq_state = irq_kill();
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
		YieldCurrentThread(mainQueuePtr);
		threadState = threadToJoin->threadState;
	}
	
	if(returnedValue != NULL)
		*returnedValue = threadToJoin->returnValue;
	
	if(threadState != Dead)
		gecko_printf("thread %d is not dead, but join from %d resumed\n", _GetThreadID(threadToJoin), _GetThreadID(currentThread) );

	threadToJoin->threadState = Unset;	
restore_and_return:
	irq_restore(irq_state);
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
	s32 irq_state = irq_kill();
	
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
	irq_restore(irq_state);
	return ret;
}

s32 SetThreadPriority( u32 threadId, s32 priority )
{
	s32 irq_state = irq_kill();

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
		UnQueueThread(mainQueuePtr, thread);
		QueueNextThread(mainQueuePtr, thread);
	}
	
	if( currentThread->priority < mainQueuePtr->nextThread->priority )
	{
		currentThread->threadState = Ready;
		YieldCurrentThread(mainQueuePtr);
	}
	goto restore_and_return;
	
return_error:
	ret = IPC_EINVAL;
restore_and_return:
	irq_restore(irq_state);
	return ret;
}

s32 GetUID(void)
{
	return ProcessUID[currentThread->processId];
}

s32 SetUID(u32 pid, u32 uid)
{
	s32 ret = IPC_SUCCESS;
	s32 irq_state = irq_kill();
	
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
	irq_restore(irq_state);
	return ret;
}

s32 GetGID(void)
{
	return ProcessGID[currentThread->processId];
}

s32 SetGID(u32 pid, u32 gid)
{
	s32 ret = IPC_SUCCESS;
	s32 irq_state = irq_kill();
	
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
	irq_restore(irq_state);
	return ret;
}
