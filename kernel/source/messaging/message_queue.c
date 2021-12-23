/*
	starstruck - a Free Software reimplementation for the Nintendo/BroadOn IOS.
	message_queue - queue management of ipc messages

	Copyright (C) 2021	DacoTaco

# This code is licensed to you under the terms of the GNU GPL, version 2;
# see file COPYING or http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt
*/

#include <ios/errno.h>

#include "core/defines.h"
#include "interrupt/irq.h"
#include "interrupt/threads.h"
#include "memory/memory.h"
#include "messaging/message_queue.h"

#define MAX_QUEUE 0x100

static message_queue queues[MAX_QUEUE];

s32 CreateMessageQueue(void** ptr, u32 numberOfMessages)
{
	s32 irq_state = irq_kill();
	s16 queueId = 0;
	
	if(ptr == NULL || *ptr == NULL)
	{
		queueId = IPC_EINVAL;
		goto restore_and_return;
	}

	if(CheckMemoryPointer(ptr, numberOfMessages << 2, 4, currentThread->processId, 0) < 0)
	{
		queueId = IPC_EINVAL;
		goto restore_and_return;
	}	
	
	while(queueId < MAX_QUEUE)
	{	
		if(queues[queueId].queueSize == 0)
			break;
		queueId++;
	}
	
	if(queueId >= MAX_QUEUE)
	{
		queueId = IPC_EMAX;
		goto restore_and_return;
	}
	
	queues[queueId].receiveThreadQueue = mainQueuePtr->nextThread;
	queues[queueId].sendThreadQueue = mainQueuePtr->nextThread;
	queues[queueId].queueHeap = ptr;
	queues[queueId].queueSize = numberOfMessages;
	queues[queueId].used = 0;
	queues[queueId].first = 0;
	queues[queueId].processId = currentThread->processId;
	
restore_and_return:
	irq_restore(irq_state);
	return queueId;
}

s32 SendMessage(s32 queueId, void* message, u32 flags)
{
	s32 irq_state = irq_kill();
	s32 ret = 0;
	s32 used = 0;
	s32 msgCount = 0;
	s32 heapIndex = 0;
	
	if(queueId >= MAX_QUEUE || flags > 2)
	{
		ret = IPC_EINVAL;
		goto restore_and_return;
	}

	if(queues[queueId].processId != currentThread->processId)
	{
		ret = IPC_EACCES;
		goto restore_and_return;
	}
	
	used = queues[queueId].used;
	msgCount = queues[queueId].queueSize;
	if (msgCount <= used) 
	{
		if (flags != BlockThread) 
		{
			ret = IPC_EQUEUEFULL;
			goto restore_and_return;
		}
				
		while(msgCount <= used)
		{
			currentThread->threadState = Waiting;
			ThreadQueue* threadQueue = (queues[queueId].sendThreadQueue == NULL)
				? NULL
				: (ThreadQueue*)&queues[queueId].sendThreadQueue;
			YieldCurrentThread(threadQueue);
			
			if(threadQueue == NULL)
				goto restore_and_return;
			
			used = queues[queueId].used;
			msgCount = queues[queueId].queueSize;
		}
	}
	
	heapIndex = queues[queueId].first + used;
	if(msgCount <= heapIndex)
		heapIndex = heapIndex - msgCount;
	
	set_dacr(DomainAccessControlTable[0]);
	queues[queueId].queueHeap[heapIndex] = message;
	set_dacr(DomainAccessControlTable[currentThread->processId]);
	queues[queueId].used++;
	if(queues[queueId].receiveThreadQueue == NULL || queues[queueId].receiveThreadQueue->nextThread != NULL )
		UnblockThread((ThreadQueue*)&queues[queueId].receiveThreadQueue, 0);
  
restore_and_return:
	irq_restore(irq_state);
	return ret;
}

s32 ReceiveMessage(s32 queueId, void** message, u32 flags)
{
	s32 irq_state = irq_kill();
	s32 ret = 0;
	s32 used = 0;
	s32 first = 0;

	if(queueId >= MAX_QUEUE || flags > 2)
	{
		ret = IPC_EINVAL;
		goto restore_and_return;
	}
	
	ret = CheckMemoryPointer(message, 4, 4, currentThread->processId, 0);
	if(ret < 0)
		goto restore_and_return;
	
	if(queues[queueId].processId != currentThread->processId)
	{
		ret = IPC_EACCES;
		goto restore_and_return;
	}
	
	//_Receive_Message itself
	used = queues[queueId].used;
	if(used == 0 && flags != BlockThread)
	{
		ret = -7;
		goto restore_and_return;
	}
	
	while(used == 0)
	{
		currentThread->threadState = Waiting;
		YieldCurrentThread((ThreadQueue*)&queues[queueId].receiveThreadQueue);
		if(currentThread->threadContext.registers[0] != 0)
		{
			ret = currentThread->threadContext.registers[0];
			goto restore_and_return;
		}
		
		used = queues[queueId].used;
	}
	
	if(message != NULL)
	{
		*message = queues[queueId].queueHeap[queues[queueId].first];
		used = queues[queueId].used;
	}
	
	first = queues[queueId].first + 1;
	if(queues[queueId].queueSize <= first)
		first = first - queues[queueId].queueSize;
	
	queues[queueId].first = first;
	queues[queueId].used = used-1;
	
	if(queues[queueId].sendThreadQueue == NULL || queues[queueId].sendThreadQueue->nextThread != NULL )
		UnblockThread((ThreadQueue*)&queues[queueId].sendThreadQueue, 0);

restore_and_return:
	irq_restore(irq_state);
	return ret;
}

s32 DestroyMessageQueue(s32 queueId)
{
	s32 irq_state = irq_kill();
	s32 ret = 0;
	
	if(queueId < 0 || queueId > MAX_QUEUE)
	{
		ret = IPC_EINVAL;
		goto restore_and_return;
	}
	
	if(queues[queueId].processId != currentThread->processId)
	{
		ret = IPC_EACCES;
		goto restore_and_return;
	}
	
	while(queues[queueId].sendThreadQueue != NULL && queues[queueId].sendThreadQueue->nextThread != NULL )
		UnblockThread((ThreadQueue*)&queues[queueId].sendThreadQueue, -3);
	
	while(queues[queueId].receiveThreadQueue != NULL && queues[queueId].receiveThreadQueue->nextThread != NULL )
		UnblockThread((ThreadQueue*)&queues[queueId].receiveThreadQueue, -3);
	
	queues[queueId].queueSize = 0;

restore_and_return:
	irq_restore(irq_state);
	return ret;
}