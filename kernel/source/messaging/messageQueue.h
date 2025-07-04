/*
	starstruck - a Free Software reimplementation for the Nintendo/BroadOn IOS.
	message_queue - queue management of ipc messages

	Copyright (C) 2021	DacoTaco

# This code is licensed to you under the terms of the GNU GPL, version 2;
# see file COPYING or http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt
*/

#ifndef __MESSAGE_QUEUE_H__
#define __MESSAGE_QUEUE_H__

#include <types.h>
#include "scheduler/threads.h"

typedef struct
{
	ThreadQueue ReceiveThreadQueue;
	ThreadQueue SendThreadQueue;
	u32 ProcessId;
	u32 Used;
	u32 First;
	u32 QueueSize;
	void** QueueHeap;
} MessageQueue;
CHECK_SIZE(MessageQueue, 0x1C);
CHECK_OFFSET(MessageQueue, 0x00, ReceiveThreadQueue);
CHECK_OFFSET(MessageQueue, 0x04, SendThreadQueue);
CHECK_OFFSET(MessageQueue, 0x08, ProcessId);
CHECK_OFFSET(MessageQueue, 0x0C, Used);
CHECK_OFFSET(MessageQueue, 0x10, First);
CHECK_OFFSET(MessageQueue, 0x14, QueueSize);
CHECK_OFFSET(MessageQueue, 0x18, QueueHeap);
typedef enum
{
	None = 0,
	RegisteredEventHandler = 1,
	Invalid = 2
} MessageQueueFlags;

#define MAX_MESSAGEQUEUES 0x100
extern MessageQueue MessageQueues[MAX_MESSAGEQUEUES];

s32 CreateMessageQueue(void **ptr, u32 numberOfMessages);
s32 DestroyMessageQueue(const s32 queueId);
s32 JamMessage(const s32 queueId, void* message, u32 flags);
s32 SendMessage(const s32 queueId, void* message, u32 flags);
s32 SendMessageToQueue(MessageQueue* messageQueue, void* message, u32 flags);
s32 ReceiveMessage(const s32 queueId, void **message, u32 flags);
s32 ReceiveMessageFromQueue(MessageQueue* messageQueue, void **message, u32 flags);
s32 SendMessageUnsafe(const s32 queueId, void* message, u32 flags);
s32 ReceiveMessageUnsafe(const s32 queueId, void **message, u32 flags);

#endif
