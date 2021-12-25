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
#include "scheduler/threads.h"
#include "memory/memory.h"
#include "messaging/message_queue.h"

MessageQueue messageQueues[MAX_MESSAGEQUEUES];

s32 CreateMessageQueue(void** ptr, u32 numberOfMessages)
{
	s32 irqState = DisableInterrupts();
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
	
	while(queueId < MAX_MESSAGEQUEUES)
	{	
		if(messageQueues[queueId].queueSize == 0)
			break;
		queueId++;
	}
	
	if(queueId >= MAX_MESSAGEQUEUES)
	{
		queueId = IPC_EMAX;
		goto restore_and_return;
	}
	
	messageQueues[queueId].receiveThreadQueue = mainQueuePtr;
	messageQueues[queueId].sendThreadQueue = mainQueuePtr;
	messageQueues[queueId].queueHeap = ptr;
	messageQueues[queueId].queueSize = numberOfMessages;
	messageQueues[queueId].used = 0;
	messageQueues[queueId].first = 0;
	messageQueues[queueId].processId = currentThread->processId;
	
restore_and_return:
	RestoreInterrupts(irqState);
	return queueId;
}

s32 SendMessage(s32 queueId, void* message, u32 flags)
{
	s32 irqState = DisableInterrupts();
	s32 ret = 0;
	s32 used = 0;
	s32 queueSize = 0;
	s32 heapIndex = 0;
	
	if(queueId >= MAX_MESSAGEQUEUES || flags > 2)
	{
		ret = IPC_EINVAL;
		goto restore_and_return;
	}

	if(messageQueues[queueId].processId != currentThread->processId)
	{
		ret = IPC_EACCES;
		goto restore_and_return;
	}
	
	used = messageQueues[queueId].used;
	queueSize = messageQueues[queueId].queueSize;
	if (queueSize <= used) 
	{
		if (flags != BlockThread) 
		{
			ret = IPC_EQUEUEFULL;
			goto restore_and_return;
		}
				
		while(queueSize <= used)
		{
			currentThread->threadState = Waiting;
			ThreadQueue* threadQueue = messageQueues[queueId].sendThreadQueue;
			YieldCurrentThread(threadQueue);
			
			if(threadQueue != NULL)
				goto restore_and_return;
			
			used = messageQueues[queueId].used;
			queueSize = messageQueues[queueId].queueSize;
		}
	}
	
	heapIndex = messageQueues[queueId].first + used;
	if(queueSize <= heapIndex)
		heapIndex = heapIndex - queueSize;
	
	SetDomainAccessControlRegister(DomainAccessControlTable[0]);
	messageQueues[queueId].queueHeap[heapIndex] = message;
	DCFlushRange(&messageQueues[queueId].queueHeap[heapIndex], 4);
	messageQueues[queueId].used++;
	SetDomainAccessControlRegister(DomainAccessControlTable[currentThread->processId]);
	if(messageQueues[queueId].receiveThreadQueue == NULL || messageQueues[queueId].receiveThreadQueue->nextThread != NULL )
		UnblockThread(messageQueues[queueId].receiveThreadQueue, 0);
  
restore_and_return:
	RestoreInterrupts(irqState);
	return ret;
}

s32 ReceiveMessage(s32 queueId, void** message, u32 flags)
{
	s32 irqState = DisableInterrupts();
	s32 ret = 0;
	s32 used = 0;
	s32 first = 0;

	if(queueId >= MAX_MESSAGEQUEUES || flags > 2)
	{
		ret = IPC_EINVAL;
		goto restore_and_return;
	}
	
	ret = CheckMemoryPointer(message, 4, 4, currentThread->processId, 0);
	if(ret < 0)
		goto restore_and_return;
	
	if(messageQueues[queueId].processId != currentThread->processId)
	{
		ret = IPC_EACCES;
		goto restore_and_return;
	}
	
	//_Receive_Message itself
	used = messageQueues[queueId].used;
	if(used == 0 && flags != BlockThread)
	{
		ret = -7;
		goto restore_and_return;
	}
	
	while(used == 0)
	{
		currentThread->threadState = Waiting;
		YieldCurrentThread(messageQueues[queueId].receiveThreadQueue);
		if(currentThread->threadContext.registers[0] != 0)
		{
			ret = currentThread->threadContext.registers[0];
			goto restore_and_return;
		}
		
		used = messageQueues[queueId].used;
	}
	
	if(message != NULL)
	{
		*message = messageQueues[queueId].queueHeap[messageQueues[queueId].first];
		used = messageQueues[queueId].used;
	}
	
	first = messageQueues[queueId].first + 1;
	if(messageQueues[queueId].queueSize <= first)
		first = first - messageQueues[queueId].queueSize;
	
	messageQueues[queueId].first = first;
	messageQueues[queueId].used = used-1;
	
	if(messageQueues[queueId].sendThreadQueue == NULL || messageQueues[queueId].sendThreadQueue->nextThread != NULL )
		UnblockThread(messageQueues[queueId].sendThreadQueue, 0);

restore_and_return:
	RestoreInterrupts(irqState);
	return ret;
}

s32 DestroyMessageQueue(s32 queueId)
{
	s32 irqState = DisableInterrupts();
	s32 ret = 0;
	
	if(queueId < 0 || queueId > MAX_MESSAGEQUEUES)
	{
		ret = IPC_EINVAL;
		goto restore_and_return;
	}
	
	if(messageQueues[queueId].processId != currentThread->processId)
	{
		ret = IPC_EACCES;
		goto restore_and_return;
	}
	
	while(messageQueues[queueId].sendThreadQueue != NULL && messageQueues[queueId].sendThreadQueue->nextThread != NULL )
		UnblockThread(messageQueues[queueId].sendThreadQueue, -3);
	
	while(messageQueues[queueId].receiveThreadQueue != NULL && messageQueues[queueId].receiveThreadQueue->nextThread != NULL )
		UnblockThread(messageQueues[queueId].receiveThreadQueue, -3);
	
	messageQueues[queueId].queueSize = 0;

restore_and_return:
	RestoreInterrupts(irqState);
	return ret;
}