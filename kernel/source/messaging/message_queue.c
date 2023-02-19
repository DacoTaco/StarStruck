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

extern u32* MemoryTranslationTable;
extern u32 DomainAccessControlTable[MAX_PROCESSES];
extern u32* HardwareRegistersAccessTable[MAX_PROCESSES];

MessageQueue MessageQueues[MAX_MESSAGEQUEUES] SRAM_DATA;

s32 CreateMessageQueue(void** ptr, u32 numberOfMessages)
{
	s32 irqState = DisableInterrupts();
	s16 queueId = 0;
	
	if(ptr == NULL)
	{
		queueId = IPC_EINVAL;
		goto restore_and_return;
	}

	if(CheckMemoryPointer(ptr, numberOfMessages*sizeof(u32), 4, CurrentThread->ProcessId, 0) < 0)
	{
		queueId = IPC_EINVAL;
		goto restore_and_return;
	}	
	
	while(queueId < MAX_MESSAGEQUEUES)
	{	
		if(MessageQueues[queueId].QueueSize == 0)
			break;
		queueId++;
	}
	
	if(queueId >= MAX_MESSAGEQUEUES)
	{
		queueId = IPC_EMAX;
		goto restore_and_return;
	}

	//ios assigns &ThreadStartingState ? that makes no sense, but it somehow got the value of the thread the message queue belongs too.
	MessageQueues[queueId].ReceiveThreadQueue.NextThread = &ThreadStartingState;
	MessageQueues[queueId].SendThreadQueue.NextThread = &ThreadStartingState;
	MessageQueues[queueId].QueueHeap = ptr;
	MessageQueues[queueId].QueueSize = numberOfMessages;
	MessageQueues[queueId].Used = 0;
	MessageQueues[queueId].First = 0;
	MessageQueues[queueId].ProcessId = CurrentThread->ProcessId;
	
restore_and_return:
	RestoreInterrupts(irqState);
	return queueId;
}

s32 JamMessage(s32 queueId, void* message, u32 flags)
{
	s32 irqState = DisableInterrupts();
	s32 ret = 0;
	MessageQueue* messageQueue;

	if(queueId >= MAX_MESSAGEQUEUES || flags > Invalid)
	{
		ret = IPC_EINVAL;
		goto restore_and_return;
	}

	messageQueue = &MessageQueues[queueId];
	if(messageQueue->ProcessId != CurrentThread->ProcessId)
	{
		ret = IPC_EACCES;
		goto restore_and_return;
	}

	if(messageQueue->QueueSize <= messageQueue->Used)
	{
		if(flags != None)
		{
			ret = IPC_EQUEUEFULL;
			goto restore_and_return;
		}

		do
		{
			CurrentThread->ThreadState = Waiting;
			//wth ios?
			ret = (s32)&messageQueue->SendThreadQueue;
			YieldCurrentThread((ThreadQueue*)ret);
			if(ret != 0)
				goto restore_and_return;
		} while (messageQueue->QueueSize <= messageQueue->Used);		
	}

	if (messageQueue->First == 0) 
		messageQueue->First = messageQueue->QueueSize - 1;
   	else
		messageQueue->First--;

	SetDomainAccessControlRegister(DomainAccessControlTable[0]);
	messageQueue->QueueHeap[messageQueue->First] = message;
	DCFlushRange(messageQueue->QueueHeap[messageQueue->First], 4);
	SetDomainAccessControlRegister(DomainAccessControlTable[CurrentThread->ProcessId]);
	messageQueue->Used++;
	if(messageQueue->ReceiveThreadQueue.NextThread != NULL )
		UnblockThread(&messageQueue->ReceiveThreadQueue, 0);

restore_and_return:
	RestoreInterrupts(irqState);
	return ret;
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

	if(MessageQueues[queueId].ProcessId != CurrentThread->ProcessId)
	{
		ret = IPC_EACCES;
		goto restore_and_return;
	}
	
	ret = SendMessageToQueue(&MessageQueues[queueId], message, flags);
  
restore_and_return:
	RestoreInterrupts(irqState);
	return ret;
}

s32 SendMessageToQueue(MessageQueue* messageQueue, void* message, u32 flags)
{
	if(messageQueue == NULL)
		return IPC_EINVAL;
	
	s32 used = messageQueue->Used;
	s32 queueSize = messageQueue->QueueSize;

	if (queueSize <= used) 
	{
		if (flags != None) 
			return IPC_EQUEUEFULL;
				
		while(queueSize <= used)
		{
			CurrentThread->ThreadState = Waiting;
			ThreadQueue* threadQueue = &messageQueue->SendThreadQueue;
			YieldCurrentThread(threadQueue);
			
			if(threadQueue != NULL)
				return (s32)threadQueue;
			
			used = messageQueue->Used;
			queueSize = messageQueue->QueueSize;
		}
	}
	
	s32 heapIndex = messageQueue->First + used;
	if(queueSize <= heapIndex)
		heapIndex = heapIndex - queueSize;
	
	SetDomainAccessControlRegister(DomainAccessControlTable[0]);
	messageQueue->QueueHeap[heapIndex] = message;
	DCFlushRange(&messageQueue->QueueHeap[heapIndex], 4);
	messageQueue->Used++;
	SetDomainAccessControlRegister(DomainAccessControlTable[CurrentThread->ProcessId]);
	if(messageQueue->ReceiveThreadQueue.NextThread != NULL )
		UnblockThread(&messageQueue->ReceiveThreadQueue, 0);

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
	
	ret = CheckMemoryPointer(message, 4, 4, CurrentThread->ProcessId, 0);
	if(ret < 0)
		goto restore_and_return;
	
	if(MessageQueues[queueId].ProcessId != CurrentThread->ProcessId)
	{
		ret = IPC_EACCES;
		goto restore_and_return;
	}
	
	ret = ReceiveMessageFromQueue(&MessageQueues[queueId], message, flags);

restore_and_return:
	RestoreInterrupts(irqState);
	return ret;
}

s32 ReceiveMessageFromQueue(MessageQueue* messageQueue, void **message, u32 flags)
{
	if(messageQueue == NULL)
		return IPC_EINVAL;
	
	s32 used = messageQueue->Used;
	if(used == 0 && flags != None)
		return IPC_EQUEUEEMPTY;

	while(used == 0)
	{
		register s32 yieldReturn	__asm__("r0");
		CurrentThread->ThreadState = Waiting;
		YieldCurrentThread(&messageQueue->ReceiveThreadQueue);
		if(yieldReturn != IPC_SUCCESS)
			return yieldReturn;

		used = messageQueue->Used;
	}
	
	if(message != NULL)
	{
		*message = messageQueue->QueueHeap[messageQueue->First];
		used = messageQueue->Used;
	}
	
	s32 first = messageQueue->First + 1;
	if(messageQueue->QueueSize <= first)
		first = first - messageQueue->QueueSize;
	
	messageQueue->First = first;
	messageQueue->Used = used-1;
	
	if(messageQueue->SendThreadQueue.NextThread->NextThread != NULL )
		UnblockThread(&messageQueue->SendThreadQueue, 0);

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
	
	if(MessageQueues[queueId].ProcessId != CurrentThread->ProcessId)
	{
		ret = IPC_EACCES;
		goto restore_and_return;
	}
	
	while(MessageQueues[queueId].SendThreadQueue.NextThread != NULL )
		UnblockThread(&MessageQueues[queueId].SendThreadQueue, IPC_EINTR);
	
	while(MessageQueues[queueId].ReceiveThreadQueue.NextThread != NULL )
		UnblockThread(&MessageQueues[queueId].ReceiveThreadQueue, IPC_EINTR);
	
	MessageQueues[queueId].QueueSize = 0;

restore_and_return:
	RestoreInterrupts(irqState);
	return ret;
}