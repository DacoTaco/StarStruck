/*
	starstruck - a Free Software reimplementation for the Nintendo/BroadOn IOS.
	calls_async - async filedescriptor syscalls

	Copyright (C) 2021	DacoTaco

# This code is licensed to you under the terms of the GNU GPL, version 2;
# see file COPYING or http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt
*/

#include <ios/errno.h>
#include "calls_async.h"
#include "memory/memory.h"

#ifdef MIOS
#define IOSFDAsync_CheckPerformInner() (queue->ProcessId == GetProcessID())
#else
#define IOSFDAsync_CheckPerformInner() ( \
		(queue->ProcessId == GetProcessID()) \
		&& \
		((ret = CheckMemoryPointer(message, sizeof(IpcRequest), 4, queue->ProcessId, 0)) == IPC_SUCCESS) \
	)
#endif

// tells calls_inner.h to produce the <name>FDAsync syscall functions in its include, with this template
// they all share this exact shape, except Open
#define WRAP_INNER_CALL(rettype, name, arguments) \
rettype name ## FDAsync(ARGEXTRACT_DO( ARGEXTRACT_FULL arguments ), u32 messageQueueId, IpcMessage* message) { \
	const u32 state = DisableInterrupts(); \
	rettype ret = IPC_EACCES; \
	if(messageQueueId < MAX_MESSAGEQUEUES) { \
		MessageQueue* queue = &MessageQueues[messageQueueId]; \
		if(IOSFDAsync_CheckPerformInner()) { \
			ret = name ## FD_Inner(ARGEXTRACT_DO( ARGEXTRACT_EVEN arguments ), queue, message); \
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

	const u32 state = DisableInterrupts();
	if (messageQueueId < MAX_MESSAGEQUEUES) {
		MessageQueue* queue = &MessageQueues[messageQueueId];
		if(IOSFDAsync_CheckPerformInner()) {
			if((ret = OpenFD_Inner(path, mode)) >= 0) {
				message->Request.Command = IOS_REPLY;
				message->Request.Result = ret;
				ret = SendMessageToQueue(queue, message, RegisteredEventHandler);
			}
		}
	}
	else {
		ret = IPC_EINVAL;
	}

	RestoreInterrupts(state);
	return ret;
}
