/*
	starstruck - a Free Software reimplementation for the Nintendo/BroadOn IOS.
	message_queue - queue management of ipc messages

	Copyright (C) 2021	DacoTaco

# This code is licensed to you under the terms of the GNU GPL, version 2;
# see file COPYING or http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt
*/

#include "core/defines.h"
#include "interrupt/irq.h"
#include "interrupt/threads.h"
#include "messaging/message_queue.h"

extern u8 __modules_area_start[];
extern u8 __mem2_area_start[];
#define MEM_MODULES_START	((u32) __modules_area_start)
#define MEM_MODULES_END		((u32) __mem2_area_start)

#define MAX_QUEUE 0x100

static message_queue queues[MAX_QUEUE] MEM2_BSS;

s32 CreateMessageQueue(void** ptr, u32 numberOfMessages)
{
	s32 irq_state = irq_kill();
	s16 queueId = 0;
	
	if(ptr == NULL || *ptr == NULL || numberOfMessages < 0x30)
	{
		queueId = -4;
		goto restore_and_return;
	}

	u32 queue_addr = (u32)ptr;	
	if(queue_addr < MEM_MODULES_START || queue_addr > MEM_MODULES_END || queue_addr+numberOfMessages > MEM_MODULES_END)
	{
		queueId = -4;
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
		queueId = -5;
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
		ret = -4;
		goto restore_and_return;
	}

	if(queues[queueId].processId != currentThread->processId)
	{
		ret = -1;
		goto restore_and_return;
	}
	
	used = queues[queueId].used;
	msgCount = queues[queueId].queueSize;
	if (msgCount <= used) 
	{
		if (flags != BlockThread) 
		{
			ret = -8;
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
	
	queues[queueId].queueHeap[heapIndex] = message;
	queues[queueId].used++;
	if(queues[queueId].receiveThreadQueue == NULL || queues[queueId].receiveThreadQueue->nextThread != NULL )
	{
		UnblockThread((ThreadQueue*)&queues[queueId].receiveThreadQueue, 0);
	}
  
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
		ret = -4;
		goto restore_and_return;
	}
	
	if(queues[queueId].processId != currentThread->processId)
	{
		ret = -1;
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
		if(currentThread->registers.registers[0] != 0)
		{
			ret = currentThread->registers.registers[0];
			goto restore_and_return;
		}
		
		used = queues[queueId].used;
	}
	
	if(message != NULL && (u32)message >= MEM_MODULES_START && (u32)message <= MEM_MODULES_END)
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
		ret = -4;
		goto restore_and_return;
	}
	
	if(queues[queueId].processId != currentThread->processId)
	{
		ret = -1;
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