/*
	starstruck - a Free Software reimplementation for the Nintendo/BroadOn IOS.
	threads - manage threads on the starlet

	Copyright (C) 2021	DacoTaco

# This code is licensed to you under the terms of the GNU GPL, version 2;
# see file COPYING or http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt
*/

#include <ios/processor.h>
#include <ios/gecko.h>

#include "core/defines.h"
#include "interrupt/irq.h"
#include "interrupt/threads.h"

#define BASE_STACKADDR 	0x13AC0400
#define STACK_SIZE		0x400

ThreadInfo threads[MAX_THREADS] ALIGNED(0x10);
ThreadQueue mainQueue ALIGNED(0x10);
ThreadQueue* mainQueuePtr = &mainQueue;
ThreadInfo* currentThread ALIGNED(0x10);

extern void RestoreAndReturnToUserMode(Registers* registers, u32 swi_mode);

void _thread_end()
{
	u32 ret = 0;
	__asm__ volatile ("mov\t%0, r0" : "=l" (ret));
	CancelThread( 0, ret );
}

void InitializeThreadContext()
{
	//Initilize thread structures & set stack pointers
	
	mainQueuePtr->nextThread = &threads[0];
	for(int i = 0; i < MAX_THREADS; i++)
	{
		memset32(&threads[i], 0 , sizeof(ThreadInfo));
		threads[i].defaultThreadStack = BASE_STACKADDR + (STACK_SIZE*i);
	}
}

//save current thread state. called by the IRQ, SVC/SWI & UDF handlers
void SaveThreadInfo(Registers* input)
{
	if(currentThread == NULL || input == NULL)
		return;
	
	memcpy32(&currentThread->registers, input, sizeof(Registers));
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

void ScheduleYield( void )
{
	currentThread = PopNextThreadFromQueue(mainQueuePtr);
	
	currentThread->threadState = Running;
	RestoreAndReturnToUserMode(&currentThread->registers, ((u32)currentThread->exceptionStack) + sizeof(currentThread->exceptionStack) );
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
	nextThread->registers.registers[0] = returnValue;
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
	
	if(priority >= 0x80 || (currentThread != NULL && priority > currentThread->initialPriority))
	{
		threadId = -4;
		goto restore_and_return;
	}
		
	ThreadInfo* selectedThread;	
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

	selectedThread->threadId = threadId;
	selectedThread->threadQueue = currentThread->threadQueue;
	selectedThread->processId = (currentThread == NULL) ? 0 : currentThread->processId;
	selectedThread->threadState = Stopped;
	selectedThread->priority = priority;
	selectedThread->initialPriority = priority;
	selectedThread->registers.programCounter = main;
	selectedThread->registers.registers[0] = (u32)arg;
	selectedThread->registers.linkRegister = (u32)_thread_end;

	//gcc works with a decreasing stack, meaning our SP should start high and go down.
	selectedThread->registers.stackPointer = (u32)((stack_top == NULL) ? selectedThread->defaultThreadStack : (u32)stack_top ) + stacksize;
	//unsure what this is all about tbh, but its probably to set the thread state correctly
	//apparently it disables either FIQ&IRQ interrupts or just the IRQ interrupt
	selectedThread->registers.statusRegister = (((s32)(main << 0x1f)) < 0) ? 0x30 : 0x10;
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
		ret = -4;
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
		ret = -4;
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
	
	if(threadId > 100)
	{
		ret = -4;
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
		ret = -4;
		goto restore_and_return;
	}
	
	threadToCancel->returnValue = return_value;	
	if(threadToCancel->threadState != Stopped)
		UnQueueThread(mainQueuePtr, threadToCancel);
	
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
	
	if(threadId > 100)
	{
		ret = -4;
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
		ret = -4;
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
	s32 irq_state = irq_kill();
	
	ThreadInfo* thread;
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
	ret = -4;
restore_and_return:
	irq_restore(irq_state);
	return ret;
}