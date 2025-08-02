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
#include "messaging/messageQueue.h"

#ifndef MIOS
extern u32 *MemoryTranslationTable;
extern u32 DomainAccessControlTable[MAX_PROCESSES];
extern u32 *HardwareRegistersAccessTable[MAX_PROCESSES];
#endif

MessageQueue MessageQueues[MAX_MESSAGEQUEUES] SRAM_DATA;

s32 CreateMessageQueue(void **ptr, u32 numberOfMessages)
{
	u32 irqState = DisableInterrupts();
	s16 queueId = 0;

	if (ptr == NULL)
	{
		queueId = IPC_EINVAL;
		goto restore_and_return;
	}

#ifndef MIOS
	if (CheckMemoryPointer(ptr, numberOfMessages * sizeof(u32), 4,
	                       CurrentThread->ProcessId, 0) < 0)
	{
		queueId = IPC_EINVAL;
		goto restore_and_return;
	}
#endif

	while (queueId < MAX_MESSAGEQUEUES)
	{
		if (MessageQueues[queueId].QueueSize == 0)
			break;
		queueId++;
	}

	if (queueId >= MAX_MESSAGEQUEUES)
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

s32 JamMessage(const s32 queueId, void *message, u32 flags)
{
	u32 irqState = DisableInterrupts();
	s32 ret = 0;
	MessageQueue *messageQueue;

	if (queueId < 0 || queueId >= MAX_MESSAGEQUEUES || flags > Invalid)
	{
		ret = IPC_EINVAL;
		goto restore_and_return;
	}

	messageQueue = &MessageQueues[queueId];
	if (messageQueue->ProcessId != CurrentThread->ProcessId)
	{
		ret = IPC_EACCES;
		goto restore_and_return;
	}

	if (messageQueue->QueueSize <= messageQueue->Used)
	{
		if (flags != None)
		{
			ret = IPC_EQUEUEFULL;
			goto restore_and_return;
		}

		do
		{
			CurrentThread->ThreadState = Waiting;
			ret = YieldCurrentThread(&messageQueue->SendThreadQueue);
			if (ret != 0)
				goto restore_and_return;
		}
		while (messageQueue->QueueSize <= messageQueue->Used);
	}

	if (messageQueue->First == 0)
		messageQueue->First = messageQueue->QueueSize - 1;
	else
		messageQueue->First--;

#ifndef MIOS
	SetDomainAccessControlRegister(DomainAccessControlTable[0]);
#endif
	messageQueue->QueueHeap[messageQueue->First] = message;
	DCFlushRange(messageQueue->QueueHeap[messageQueue->First], 4);
#ifndef MIOS
	SetDomainAccessControlRegister(DomainAccessControlTable[CurrentThread->ProcessId]);
#endif
	messageQueue->Used++;
	if (messageQueue->ReceiveThreadQueue.NextThread != NULL)
		UnblockThread(&messageQueue->ReceiveThreadQueue, 0);

restore_and_return:
	RestoreInterrupts(irqState);
	return ret;
}

s32 SendMessage(const s32 queueId, void *message, u32 flags)
{
	u32 irqState = DisableInterrupts();
	s32 ret = 0;

	if (queueId < 0 || queueId >= MAX_MESSAGEQUEUES || flags > Invalid)
	{
		ret = IPC_EINVAL;
		goto restore_and_return;
	}

	if (MessageQueues[queueId].ProcessId != CurrentThread->ProcessId)
	{
		ret = IPC_EACCES;
		goto restore_and_return;
	}

	ret = SendMessageToQueue(&MessageQueues[queueId], message, flags);

restore_and_return:
	RestoreInterrupts(irqState);
	return ret;
}

s32 SendMessageToQueue(MessageQueue *messageQueue, void *message, u32 flags)
{
	if (messageQueue == NULL)
		return IPC_EINVAL;

	u32 used = messageQueue->Used;
	u32 queueSize = messageQueue->QueueSize;

	if (queueSize <= used)
	{
		if (flags != None)
			return IPC_EQUEUEFULL;

		while (queueSize <= used)
		{
			CurrentThread->ThreadState = Waiting;
			const s32 yieldReturn = YieldCurrentThread(&messageQueue->SendThreadQueue);
			if (yieldReturn != IPC_SUCCESS)
				return yieldReturn;

			used = messageQueue->Used;
			queueSize = messageQueue->QueueSize;
		}
	}

	u32 heapIndex = messageQueue->First + used;
	if (queueSize <= heapIndex)
		heapIndex = heapIndex - queueSize;

#ifndef MIOS
	SetDomainAccessControlRegister(DomainAccessControlTable[0]);
#endif
	messageQueue->QueueHeap[heapIndex] = message;
	DCFlushRange(&messageQueue->QueueHeap[heapIndex], 4);
	messageQueue->Used++;
#ifndef MIOS
	SetDomainAccessControlRegister(DomainAccessControlTable[CurrentThread->ProcessId]);
#endif
	if (messageQueue->ReceiveThreadQueue.NextThread != NULL)
		UnblockThread(&messageQueue->ReceiveThreadQueue, 0);

	return 0;
}

s32 ReceiveMessage(const s32 queueId, void **message, u32 flags)
{
	u32 irqState = DisableInterrupts();
	s32 ret = 0;

	if (queueId < 0 || queueId >= MAX_MESSAGEQUEUES || flags >= Invalid)
	{
		ret = IPC_EINVAL;
		goto restore_and_return;
	}

#ifndef MIOS
	ret = CheckMemoryPointer(message, 4, 4, CurrentThread->ProcessId, 0);
	if (ret < 0)
		goto restore_and_return;
#endif

	if (MessageQueues[queueId].ProcessId != CurrentThread->ProcessId)
	{
		ret = IPC_EACCES;
		goto restore_and_return;
	}

	ret = ReceiveMessageFromQueue(&MessageQueues[queueId], message, flags);

restore_and_return:
	RestoreInterrupts(irqState);
	return ret;
}

s32 ReceiveMessageFromQueue(MessageQueue *messageQueue, void **message, u32 flags)
{
	if (messageQueue == NULL)
		return IPC_EINVAL;

	u32 used = messageQueue->Used;
	if (used == 0 && flags != None)
		return IPC_EQUEUEEMPTY;

	while (used == 0)
	{
		CurrentThread->ThreadState = Waiting;
		const s32 yieldReturn = YieldCurrentThread(&messageQueue->ReceiveThreadQueue);
		if (yieldReturn != IPC_SUCCESS)
			return yieldReturn;

		used = messageQueue->Used;
	}

	if (message != NULL)
	{
		*message = messageQueue->QueueHeap[messageQueue->First];
		used = messageQueue->Used;
	}

	u32 first = messageQueue->First + 1;
	if (messageQueue->QueueSize <= first)
		first = first - messageQueue->QueueSize;

	messageQueue->First = first;
	messageQueue->Used = used - 1;

	if (messageQueue->SendThreadQueue.NextThread->NextThread != NULL)
		UnblockThread(&messageQueue->SendThreadQueue, 0);

	return IPC_SUCCESS;
}

s32 DestroyMessageQueue(const s32 queueId)
{
	u32 irqState = DisableInterrupts();
	s32 ret = 0;

	if (queueId < 0 || queueId > MAX_MESSAGEQUEUES)
	{
		ret = IPC_EINVAL;
		goto restore_and_return;
	}

	if (MessageQueues[queueId].ProcessId != CurrentThread->ProcessId)
	{
		ret = IPC_EACCES;
		goto restore_and_return;
	}

	while (MessageQueues[queueId].SendThreadQueue.NextThread != NULL)
		UnblockThread(&MessageQueues[queueId].SendThreadQueue, IPC_EINTR);

	while (MessageQueues[queueId].ReceiveThreadQueue.NextThread != NULL)
		UnblockThread(&MessageQueues[queueId].ReceiveThreadQueue, IPC_EINTR);

	MessageQueues[queueId].QueueSize = 0;

restore_and_return:
	RestoreInterrupts(irqState);
	return ret;
}

s32 SendMessageUnsafe(const s32 queueId, void *message, u32 flags)
{
	const u32 irqState = DisableInterrupts();
	s32 ret = IPC_EINVAL;

	if (queueId >= 0 && queueId < MAX_MESSAGEQUEUES && flags < Invalid)
		ret = SendMessageToQueue(&MessageQueues[queueId], message, flags);

	RestoreInterrupts(irqState);
	return ret;
}
s32 ReceiveMessageUnsafe(const s32 queueId, void **message, u32 flags)
{
	const u32 irqState = DisableInterrupts();
	s32 ret = IPC_EINVAL;

	if (queueId >= 0 && queueId < MAX_MESSAGEQUEUES && flags < Invalid)
		ret = ReceiveMessageFromQueue(&MessageQueues[queueId], message, flags);

	RestoreInterrupts(irqState);
	return ret;
}
