/*
	starstruck - a Free Software reimplementation for the Nintendo/BroadOn IOS.
	calls_async - async filedescriptor syscalls

	Copyright (C) 2021	DacoTaco

# This code is licensed to you under the terms of the GNU GPL, version 2;
# see file COPYING or http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt
*/

#pragma once

#include "filedesc_types.h"

// these functions are implemented through calls_inner.h via a template in calls_async.c
// the actual work done is in calls_inner.c by the <name>FD_Inner functions
s32 OpenFDAsync(const char* path, s32 mode, u32 messageQueueId, IpcMessage* message);
s32 CloseFDAsync(s32 fd, u32 messageQueueId, IpcMessage* message);
s32 ReadFDAsync(s32 fd, void *buf, u32 len, u32 messageQueueId, IpcMessage* message);
s32 WriteFDAsync(s32 fd, const void *buf, u32 len, u32 messageQueueId, IpcMessage* message);
s32 SeekFDAsync(s32 fd, s32 offset, s32 origin, u32 messageQueueId, IpcMessage* message);
s32 IoctlFDAsync(s32 fd, u32 requestId, void *inputBuffer, u32 inputBufferLength, void *outputBuffer, u32 outputBufferLength, u32 messageQueueId, IpcMessage* message);
s32 IoctlvFDAsync(s32 fd, u32 requestId, u32 vectorInputCount, u32 vectorIOCount, IoctlvMessageData *vectors, u32 messageQueueId, IpcMessage* message);
