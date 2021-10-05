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

typedef struct
{
	void** queueHeap;
	s32 queueSize;
	s32 used;
	s32 first;
	u32 processId;
	ThreadInfo* receiveThreadQueue;
	ThreadInfo* sendThreadQueue;
} message_queue;

typedef enum
{
	BlockThread = 0,
	ReturnCall = 1
} MessageQueueFlags;

s32 CreateMessageQueue(void **ptr, u32 numberOfMessages);
s32 DestroyMessageQueue(s32 queueId);
s32 SendMessage(s32 queueId, void* message, u32 flags);
s32 ReceiveMessage(s32 queueid, void **message, u32 flags);

#endif