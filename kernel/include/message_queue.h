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
	void* queue;
	s32 numberOfMessages;
} message_queue;

s32 CreateMessageQueue(void *ptr, u32 numberOfMessages);
s32 DestroyMessageQueue(s32 queueId);

#endif