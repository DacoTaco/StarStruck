/*
	starstruck - a Free Software reimplementation for the Nintendo/BroadOn IOS.
	iosc_helpers - iosc's helper functions

	Copyright (C) 2021	DacoTaco

# This code is licensed to you under the terms of the GNU GPL, version 2;
# see file COPYING or http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt
*/
#pragma once

#include <types.h>
#include <ios/ipc.h>

extern void IOSC_SwapStack(u32 currentStackBase, u32 newStackBase);

s32 DispatchIoctlv(s32 fd, u32 requestId, u32 vectorInputCount, u32 vectorIOCount, IoctlvMessageData* vectors);
s32 DispatchIoctlvAsync(s32 fd, u32 requestId, u32 vectorInputCount, u32 vectorIOCount,
                        IoctlvMessageData* vectors, s32 messageQueueId, IpcMessage* message);