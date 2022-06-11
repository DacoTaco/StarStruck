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

MessageQueue messageQueues[MAX_MESSAGEQUEUES] SRAM_DATA;

s32 CreateMessageQueue(void** ptr, u32 numberOfMessages)
{
	s32 irqState = DisableInterrupts();
	s16 queueId = 0;
	
	if(ptr == NULL)
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

	//ios assigns &runningQueue ? that makes no sense, but it somehow got the value of the thread the message queue belongs too.
	messageQueues[queueId].receiveThreadQueue.nextThread = currentThread;
	messageQueues[queueId].sendThreadQueue.nextThread = currentThread;
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
	
	if(queueId >= MAX_MESSAGEQUEUES || flags > Invalid)
	{
		ret = IPC_EINVAL;
		goto restore_and_return;
	}

	if(messageQueues[queueId].processId != currentThread->processId)
	{
		ret = IPC_EACCES;
		goto restore_and_return;
	}
	
	ret = SendMessageToQueue(&messageQueues[queueId], message, flags);
  
restore_and_return:
	RestoreInterrupts(irqState);
	return ret;
}

s32 SendMessageToQueue(MessageQueue* messageQueue, void* message, u32 flags)
{
	if(messageQueue == NULL)
		return IPC_EINVAL;
	
	s32 used = messageQueue->used;
	s32 queueSize = messageQueue->queueSize;

	if (queueSize <= used) 
	{
		if (flags != None) 
			return IPC_EQUEUEFULL;
				
		while(queueSize <= used)
		{
			currentThread->threadState = Waiting;
			ThreadQueue* threadQueue = &messageQueue->sendThreadQueue;
			YieldCurrentThread(threadQueue);
			
			if(threadQueue != NULL)
				return (s32)threadQueue;
			
			used = messageQueue->used;
			queueSize = messageQueue->queueSize;
		}
	}
	
	s32 heapIndex = messageQueue->first + used;
	if(queueSize <= heapIndex)
		heapIndex = heapIndex - queueSize;
	
	SetDomainAccessControlRegister(DomainAccessControlTable[0]);
	messageQueue->queueHeap[heapIndex] = message;
	DCFlushRange(&messageQueue->queueHeap[heapIndex], 4);
	messageQueue->used++;
	SetDomainAccessControlRegister(DomainAccessControlTable[currentThread->processId]);
	if(messageQueue->receiveThreadQueue.nextThread != NULL )
		UnblockThread(&messageQueue->receiveThreadQueue, 0);

	return 0;
}

s32 ReceiveMessage(s32 queueId, void** message, u32 flags)
{
	s32 irqState = DisableInterrupts();
	s32 ret = 0;

	if(queueId >= MAX_MESSAGEQUEUES || flags > Invalid)
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
	
	ret = ReceiveMessageFromQueue(&messageQueues[queueId], message, flags);

restore_and_return:
	RestoreInterrupts(irqState);
	return ret;
}

s32 ReceiveMessageFromQueue(MessageQueue* messageQueue, void **message, u32 flags)
{
	if(messageQueue == NULL)
		return IPC_EINVAL;
	
	s32 used = messageQueue->used;
	if(used == 0 && flags != None)
		return -7;

	while(used == 0)
	{
		currentThread->threadState = Waiting;
		YieldCurrentThread(&messageQueue->receiveThreadQueue);

		if(currentThread->threadContext.registers[0] != 0)
			return 0;

		used = messageQueue->used;
	}
	
	if(message != NULL)
	{
		*message = messageQueue->queueHeap[messageQueue->first];
		used = messageQueue->used;
	}
	
	s32 first = messageQueue->first + 1;
	if(messageQueue->queueSize <= first)
		first = first - messageQueue->queueSize;
	
	messageQueue->first = first;
	messageQueue->used = used-1;
	
	if(messageQueue->sendThreadQueue.nextThread != NULL )
		UnblockThread(&messageQueue->sendThreadQueue, 0);

	return 0;
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
	
	while(messageQueues[queueId].sendThreadQueue.nextThread != NULL )
		UnblockThread(&messageQueues[queueId].sendThreadQueue, -3);
	
	while(messageQueues[queueId].receiveThreadQueue.nextThread != NULL )
		UnblockThread(&messageQueues[queueId].receiveThreadQueue, -3);
	
	messageQueues[queueId].queueSize = 0;

restore_and_return:
	RestoreInterrupts(irqState);
	return ret;
}