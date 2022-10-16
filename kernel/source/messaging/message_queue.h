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
	ThreadQueue receiveThreadQueue;
	ThreadQueue sendThreadQueue;
	u32 processId;
	s32 used;
	s32 first;
	s32 queueSize;
	void** queueHeap;
} MessageQueue;
CHECK_SIZE(MessageQueue, 0x1C);
CHECK_OFFSET(MessageQueue, 0x00, receiveThreadQueue);
CHECK_OFFSET(MessageQueue, 0x04, sendThreadQueue);
CHECK_OFFSET(MessageQueue, 0x08, processId);
CHECK_OFFSET(MessageQueue, 0x0C, used);
CHECK_OFFSET(MessageQueue, 0x10, first);
CHECK_OFFSET(MessageQueue, 0x14, queueSize);
CHECK_OFFSET(MessageQueue, 0x18, queueHeap);
typedef enum
{
	None = 0,
	RegisteredEventHandler = 1,
	Invalid = 2
} MessageQueueFlags;

#define MAX_MESSAGEQUEUES 0x100
extern MessageQueue messageQueues[MAX_MESSAGEQUEUES];

s32 CreateMessageQueue(void **ptr, u32 numberOfMessages);
s32 DestroyMessageQueue(s32 queueId);
s32 SendMessage(s32 queueId, void* message, u32 flags);
s32 SendMessageToQueue(MessageQueue* messageQueue, void* message, u32 flags);
s32 ReceiveMessage(s32 queueid, void **message, u32 flags);
s32 ReceiveMessageFromQueue(MessageQueue* messageQueue, void **message, u32 flags);

#endif