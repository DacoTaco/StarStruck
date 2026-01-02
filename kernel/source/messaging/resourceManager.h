/*
	starstruck - a Free Software reimplementation for the Nintendo/BroadOn IOS.
	resourceManager - manager to maintain all device resources

	Copyright (C) 2021	DacoTaco

# This code is licensed to you under the terms of the GNU GPL, version 2;
# see file COPYING or http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt
*/

#ifndef __RESOURCEMANAGER_H__
#define __RESOURCEMANAGER_H__

#include <types.h>

#define MAX_RESOURCES 0x26
#define MAX_PATHLEN   0x40

typedef struct
{
	char DevicePath[MAX_PATHLEN];
	u32 PathLength;
	MessageQueue *Queue;
	u32 ProcessId;
	u32 PpcHasAccessRights;
} ResourceManager;

extern ResourceManager ResourceManagers[MAX_RESOURCES];

CHECK_SIZE(ResourceManager, 0x50);
CHECK_OFFSET(ResourceManager, 0x00, DevicePath);
CHECK_OFFSET(ResourceManager, 0x40, PathLength);
CHECK_OFFSET(ResourceManager, 0x44, Queue);
CHECK_OFFSET(ResourceManager, 0x48, ProcessId);
CHECK_OFFSET(ResourceManager, 0x4C, PpcHasAccessRights);

s32 RegisterResourceManager(const char *devicePath, const s32 queueid);

#endif
