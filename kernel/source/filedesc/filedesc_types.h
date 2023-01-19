/*
	starstruck - a Free Software reimplementation for the Nintendo/BroadOn IOS.
	resourceManager - manager to maintain all device resources

	Copyright (C) 2021	DacoTaco

# This code is licensed to you under the terms of the GNU GPL, version 2;
# see file COPYING or http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt
*/

#ifndef __FILEDESC_TYPES_H__
#define __FILEDESC_TYPES_H__

#include "messaging/ipc.h"
#include "messaging/message_queue.h"
#include "messaging/resourceManager.h"

typedef union {
	char DevicePath[MAX_PATHLEN];
	u32 DevicePathUINT[MAX_PATHLEN / sizeof(u32)];
} FileDescriptorPath;

typedef struct {
	s32 id;
	ResourceManager* belongs_to_resource;
} FileDescriptor;

#define MAX_PROCESS_FDS 0x18

typedef FileDescriptor ProcessFileDescriptors_t[MAX_PROCESS_FDS];
typedef ProcessFileDescriptors_t AllProcessesFileDescriptors_t[MAX_PROCESSES];

extern FileDescriptor AesFileDescriptor;
extern FileDescriptor ShaFileDescriptor;
extern FileDescriptorPath* fd_path_array;

#endif
