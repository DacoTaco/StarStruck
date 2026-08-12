/*
	starstruck - a Free Software reimplementation for the Nintendo/BroadOn IOS.
	iosc_helpers - iosc's helper functions

	Copyright (C) 2021	DacoTaco

# This code is licensed to you under the terms of the GNU GPL, version 2;
# see file COPYING or http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt
*/

#include <types.h>

#include "memory/memory.h"
#include "filedesc/calls_inner.h"
#include "crypto/iosc_helpers.h"

s32 DispatchIoctlv(s32 fd, u32 requestId, u32 vectorInputCount, u32 vectorIOCount, IoctlvMessageData* vectors)
{
	u32 flags = DisableInterrupts();
	s32 ret = IoctlvFD_InnerWithFlag(fd, requestId, vectorInputCount, vectorIOCount, vectors, NULL, NULL, 0);

	RestoreInterrupts(flags);
	return ret;
}

s32 DispatchIoctlvAsync(s32 fd, u32 requestId, u32 vectorInputCount, u32 vectorIOCount,
                        IoctlvMessageData* vectors, s32 messageQueueId, IpcMessage* message)
{
	u32 flags = DisableInterrupts();
	s32 ret = 0;
	if (messageQueueId >= MAX_MESSAGEQUEUES)
	{
		ret = -1;
		goto _return_cleanup_dispatchIoctlvAsync;
	}

	if (MessageQueues[messageQueueId].ProcessId != CurrentThread->ProcessId)
	{
		return -4;
		goto _return_cleanup_dispatchIoctlvAsync;
	}

	ret = CheckMemoryPointer(message, 0x20, 4, MessageQueues[messageQueueId].ProcessId, 0);
	if (ret != 0)
		goto _return_cleanup_dispatchIoctlvAsync;

	ret = IoctlvFD_InnerWithFlag(fd, requestId, vectorInputCount, vectorIOCount, vectors,
	                             &MessageQueues[messageQueueId], message, 0);

_return_cleanup_dispatchIoctlvAsync:
	RestoreInterrupts(flags);
	return ret;
}