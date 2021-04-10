/*
	starstruck - a Free Software reimplementation for the Nintendo/BroadOn IOS.
	message_queue - queue management of ipc messages

	Copyright (C) 2021	DacoTaco

# This code is licensed to you under the terms of the GNU GPL, version 2;
# see file COPYING or http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt
*/

#include <utils.h>
#include "defines.h"
#include "message_queue.h"

extern u8 __modules_area_start[];
extern u8 __mem2_area_start[];
#define MEM_MODULES_START	((u32) __modules_area_start)
#define MEM_MODULES_END		((u32) __mem2_area_start)

#define MAX_QUEUE 0x100

static message_queue queues[MAX_QUEUE] MEM2_BSS;

s32 CreateMessageQueue(void* ptr, u32 numberOfMessages)
{
	if(ptr == NULL || numberOfMessages < 0x30)
		return -4;

	u32 queue_addr = (u32)ptr;	
	if(queue_addr < MEM_MODULES_START || queue_addr > MEM_MODULES_END || queue_addr+numberOfMessages > MEM_MODULES_END)
		return -4;
	
	message_queue* queue = NULL;
	s16 queueid = 0;
	while(queueid < MAX_QUEUE)
	{
		queue = queues[queueid].queue;
		
		if(queue == NULL && queues[queueid].numberOfMessages == 0)
			break;
		queueid++;
	}
	
	if(queueid >= MAX_QUEUE)
		return -5;
	
	queues[queueid].queue = ptr;
	queues[queueid].numberOfMessages = numberOfMessages;
	
	return queueid;
}

s32 DestroyMessageQueue(s32 queueId)
{
	if(queueId < 0 || queueId > MAX_QUEUE)
		return -4;
	
	return -4;
}