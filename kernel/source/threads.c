/*
	starstruck - a Free Software reimplementation for the Nintendo/BroadOn IOS.
	threads - manage threads on the starlet

	Copyright (C) 2021	DacoTaco

# This code is licensed to you under the terms of the GNU GPL, version 2;
# see file COPYING or http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt
*/

#include <irqcore.h>
#include "defines.h"
#include "gecko.h"
#include "threads.h"
#include "syscallcore.h"
#include "utils.h"

u8 active_thread MEM2_BSS = 0;
threadInfo threads[MAX_THREADS] ALIGNED(0x20) MEM2_BSS;
threadInfo* threadQueue[MAX_THREADS] MEM2_BSS;
threadInfo* currentThread = NULL;
s32 irq_state = 0;

void _thread_end()
{
	u32 ret = 0;
	__asm__ volatile ("mov\t%0, r0" : "=l" (ret));
	//we use a syscall here so our context switching actually does something :)
	os_stopThread( 0, ret );
}

void InitializeThreadContext()
{
	//Initilize thread structures & set current thread as thread 0
	
	threads[0].threadQueue = threadQueue;
	threads[0].threadState = Running;
	threads[0].priority = 0x80;
	threads[0].nextThread = &threads[0];
	currentThread = &threads[0];
	threadQueue[0] = currentThread;
}

//save & restore current thread state. called by the IRQ, SVC/SWI & UDF handlers
void SaveThreadInfo(Registers* input)
{
	if(currentThread == NULL || input == NULL)
		return;
	
	memcpy32(&currentThread->registers, input, sizeof(Registers));
}

void RestoreThreadInfo(Registers* input)
{
	if(currentThread == NULL || input == NULL)
		return;
	
	memcpy32(input, &currentThread->registers, sizeof(Registers));
}

//Scheduler
void UnQueueThread( threadInfo** threadQueue, threadInfo* thread )
{  
	threadInfo* nextThread = threadQueue[0];
	threadInfo* prevThread = threadQueue[0];
	
	while(1)
	{
		if(nextThread == NULL)
			return;
		
		if(nextThread == thread)
			break;
		
		prevThread = nextThread;
		nextThread = nextThread->nextThread;
	}
	
	prevThread->nextThread = nextThread->nextThread;
	return;
}

void QueueNextThread( threadInfo** threadQueue, threadInfo* thread )
{
	threadInfo* nextThread = threadQueue[0];
	s16 threadPriority = thread->priority;
	s16 nextPriority = nextThread->priority;
	s16 newPriority = nextPriority - threadPriority;
	threadInfo** previousThreadLink = threadQueue;
	
	while(nextThread != NULL && ((newPriority <= 0) == ( threadPriority > nextPriority ) ))
	{
		previousThreadLink = &nextThread->nextThread;
		//if the next thread is null or itself (aka loop), we need to insert in our thread here
		if(nextThread->nextThread != NULL && nextThread != nextThread->nextThread)
		{		
			nextThread = nextThread->nextThread;
			nextPriority = nextThread->priority;
			newPriority = nextPriority - threadPriority;
		}
		else
		{
			nextPriority = threadPriority-1;
			newPriority = 1;
		}	
	}
	
	if(nextThread == NULL)
	{
		gecko_printf("not queue'ing - %p - %p\n", nextThread, *previousThreadLink);
		return;
	}

	*previousThreadLink = thread;
	thread->nextThread = nextThread;
	thread->threadQueue = threadQueue;
	return;
}

void ScheduleYield( void )
{
	if(currentThread == NULL || currentThread->nextThread == NULL)
		return;
	
	if(currentThread->threadState == Running)
		currentThread->threadState = Ready;
	
	currentThread = currentThread->nextThread;
	currentThread->threadState = Running;
	return;
}

void YieldThread( void )
{
	//TODO : add queue support. IOS requeue's the current thread back into the given queue?
	/*if(queueToQueueIn != NULL)
		QueueNextThread(queueToQueueIn, currentThread);*/
	
	ScheduleYield();
	return;
}

//IOS Handlers
s32 CreateThread(s32 main, void *arg, u32 *stack_top, u32 stacksize, s32 priority, u32 detached)
{
	int threadId = 0;
	irq_state = irq_kill();
	
	if(priority >= 0x80 || (currentThread != NULL && priority > currentThread->priority))
	{
		threadId = -4;
		goto restore_and_return;
	}
	
	
	threadInfo* selectedThread;
	
	while(threadId < 100)
	{
		selectedThread = &threads[threadId];
		if(selectedThread->threadState == Unset)
			break;
		
		threadId++;
	}
	
	if(threadId >= 100)
	{
		threadId = -5;
		goto restore_and_return;
	}

	selectedThread->threadQueue = currentThread->threadQueue;
	selectedThread->threadId = threadId;
	selectedThread->processId = (currentThread == NULL) ? 0 : currentThread->processId;
	selectedThread->threadState = Stopped;
	selectedThread->priority = priority;
	selectedThread->initialPriority = priority;
	selectedThread->registers.programCounter = main;
	selectedThread->registers.registers[0] = (u32)arg;
	selectedThread->registers.linkRegister = (u32)_thread_end;

	//gcc works with a decreasing stack, meaning our SP should start high and go down.
	selectedThread->registers.stackPointer = (u32)((stack_top == NULL) ? selectedThread->stackAddress : stack_top ) + stacksize;
	//unsure what this is all about tbh, but its probably to set the thread state correctly.
	selectedThread->registers.statusRegister = (((s32)(main << 0x1f)) < 0) ? 0x30 : 0x10;
	selectedThread->isDetached = detached;	
	
restore_and_return:
	irq_restore(irq_state);
	return threadId;	
}

s32	StartThread(s32 threadId)
{
	irq_state = irq_kill();
	s32 ret = 0;

	if(threadId > MAX_THREADS || threadId < 0)
	{
		ret = -4;
		goto restore_and_return;
	}
	
	threadInfo* threadToStart = NULL;
	if( threadId == 0 )
		threadToStart = currentThread;
	
	if(threadToStart == NULL)
		threadToStart = &threads[threadId];

	//does the current thread even own the thread?
	if( currentThread != NULL && currentThread->processId != 0 && threadToStart->processId != currentThread->processId)
	{
		ret = -4;
		goto restore_and_return;
	}
	
	if(threadToStart->threadState != Stopped)
		goto restore_and_return;
	
	//TODO : if the thread's queue is null OR the current queue = the thread's queue -> State = Ready
	//Else State = Waiting
	//-> followed by queue'ing it in the right queue
	threadToStart->threadState = Ready;
	QueueNextThread(threadQueue, threadToStart);
	
	currentThread->registers.registers[0] = ret;
	threadToStart = currentThread;
	if(threadToStart == NULL)
		ScheduleYield();
	else
	{
		threadToStart->threadState = Ready;
		YieldThread();
	}
	
restore_and_return:
	irq_restore(irq_state);
	return ret;	
}

s32 CancelThread(u32 threadId, u32 return_value)
{
	irq_state = irq_kill();
	s32 ret = 0;
	
	if(threadId > 100)
	{
		ret = -4;
		goto restore_and_return;
	}
	
	threadInfo* threadToCancel = NULL;
	if( threadId == 0 )
		threadToCancel = currentThread;

	if(threadToCancel == NULL)
		threadToCancel = &threads[threadId];

	//does the current thread even own the thread?
	if( currentThread != NULL && currentThread->processId != 0 && threadToCancel->processId != currentThread->processId)
	{
		ret = -4;
		goto restore_and_return;
	}
	
	threadToCancel->returnValue = return_value;	
	if(threadToCancel->threadState != Stopped)
		UnQueueThread(threadQueue, threadToCancel);
	
	if(!threadToCancel->isDetached)
		threadToCancel->threadState = Dead;
	else
		threadToCancel->threadState = Unset;
	
	currentThread->registers.registers[0] = ret;
	if(threadToCancel == currentThread)
		ScheduleYield();
	else
	{
		currentThread->threadState = Ready;
		YieldThread();
	}
	
restore_and_return:
	irq_restore(irq_state);
	return ret;
}

s32 JoinThread(s32 threadId, u32* returnedValue)
{
	irq_state = irq_kill();
	s32 ret = 0;
	
	if(threadId > 100)
	{
		ret = -4;
		goto restore_and_return;
	}
	
	threadInfo* threadToJoin = NULL;
	if( threadId == 0 )
		threadToJoin = currentThread;

	if(threadToJoin == NULL)
		threadToJoin = &threads[threadId];

	//does the current thread even own the thread?
	if( currentThread != NULL && currentThread->processId != 0 && threadToJoin->processId != currentThread->processId)
	{
		ret = -4;
		goto restore_and_return;
	}
	
	if(threadToJoin == currentThread || threadToJoin->isDetached)
		goto restore_and_return;
	
	ThreadState threadState = threadToJoin->threadState;	
	if(threadState != Dead)
	{
		currentThread->registers.registers[0] = ret;
		currentThread->threadState = Waiting;
		YieldThread();
		threadState = threadToJoin->threadState;
	}
	
	if(returnedValue != NULL)
		*returnedValue = threadToJoin->returnValue;
	
	if(threadState != Dead)
		gecko_printf("thread %d is not dead, but join from %d resumed\n", threadToJoin->threadId, currentThread->threadId );

	threadToJoin->threadState = Unset;	
restore_and_return:
	irq_restore(irq_state);
	return ret;
}

s32 GetThreadID()
{
	return currentThread->threadId;
}

s32 GetProcessID()
{
	return currentThread->processId;
}

s32 GetThreadPriority( u32 threadId )
{
	irq_state = irq_kill();
	
	threadInfo* thread;
	s32 ret = -4;
	
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
	ret = -4;
restore_and_return:
	irq_restore(irq_state);
	return ret;
}

s32 SetThreadPriority( u32 threadId, s32 priority )
{
	irq_state = irq_kill();

	threadInfo* thread = NULL;
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
		UnQueueThread(threadQueue, thread);
		QueueNextThread(threadQueue, thread);
	}
	
	if( currentThread->priority < threadQueue[0]->priority )
	{
		currentThread->registers.registers[0] = ret;
		currentThread->threadState = Ready;
		YieldThread();
	}
	goto restore_and_return;
	
return_error:
	ret = -4;
restore_and_return:
	irq_restore(irq_state);
	return ret;
}