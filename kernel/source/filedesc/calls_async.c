/*
	starstruck - a Free Software reimplementation for the Nintendo/BroadOn IOS.
	resourceManager - manager to maintain all device resources

	Copyright (C) 2021	DacoTaco

# This code is licensed to you under the terms of the GNU GPL, version 2;
# see file COPYING or http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt
*/

#include <ios/errno.h>
#include "calls_async.h"
#include "memory/memory.h"

#define WRAP_INNER_CALL(rettype, name, arguments) \
rettype name ## FDAsync(ARGEXTRACT_END( ARGEXTRACT_LOOP_FULL_A arguments ), u32 messageQueueId, IpcMessage* message) { \
	const s32 state = DisableInterrupts(); \
	rettype ret = IPC_EACCES; \
	if(messageQueueId < 256) { \
		MessageQueue* queue = &MessageQueues[messageQueueId]; \
		if(queue->ProcessId == GetProcessID()) { \
			if((ret = CheckMemoryPointer(message, sizeof(IpcRequest), 4, queue->ProcessId, 0)) == IPC_SUCCESS) { \
				ret = name ## FD_Inner(ARGEXTRACT_END( ARGEXTRACT_LOOP_EVEN_A arguments ), queue, message); \
			} \
		} \
	} \
	else { \
		ret = IPC_EINVAL; \
	} \
	RestoreInterrupts(state); \
	return ret; \
}

#include "calls_inner.h"

s32 OpenFDAsync(const char* path, int mode, u32 messageQueueId, IpcMessage* message)
{
	s32 ret = IPC_EACCES;

	const s32 state = DisableInterrupts();
	if (messageQueueId < 256) {
		MessageQueue* queue = &MessageQueues[messageQueueId];
		if(queue->ProcessId == GetProcessID()) {
			if((ret = CheckMemoryPointer(message, sizeof(IpcRequest), 4, queue->ProcessId, 0)) == IPC_SUCCESS) {
				if((ret = OpenFD_Inner(path, mode)) >= 0) {
					message->Request.Command = IOS_REPLY;
					message->Request.Result = ret;
					ret = SendMessageToQueue(queue, message, RegisteredEventHandler);
				}
			}
		}
	}
	else {
		ret = IPC_EINVAL;
	}

	RestoreInterrupts(state);
	return ret;
}
