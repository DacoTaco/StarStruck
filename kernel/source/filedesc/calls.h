/*
	starstruck - a Free Software reimplementation for the Nintendo/BroadOn IOS.
	resourceManager - manager to maintain all device resources

	Copyright (C) 2021	DacoTaco

# This code is licensed to you under the terms of the GNU GPL, version 2;
# see file COPYING or http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt
*/

#ifndef __FILEDESC_CALLS_H__
#define __FILEDESC_CALLS_H__

#include "filedesc_types.h"

// these functions are implemented through calls_inner.h via a template in calls.c
// the actual work done is in calls_inner.c by the <name>FD_Inner functions
s32 OpenFD(const char* path, int mode);
s32 CloseFD(s32 fd);
s32 ReadFD(s32 fd, void *buf, u32 len);
s32 WriteFD(s32 fd, const void *buf, u32 len);
s32 SeekFD(s32 fd, s32 offset, s32 origin);
s32 IoctlFD(s32 fd, u32 requestId, void *inputBuffer, u32 inputBufferLength, void *outputBuffer, u32 outputBufferLength);
s32 IoctlvFD(s32 fd, u32 requestId, u32 vectorInputCount, u32 vectorIOCount, IoctlvMessageData *vectors);

#endif