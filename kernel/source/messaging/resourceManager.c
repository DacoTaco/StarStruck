/*
	starstruck - a Free Software reimplementation for the Nintendo/BroadOn IOS.
	resourceManager - manager to maintain all device resources

	Copyright (C) 2021	DacoTaco

# This code is licensed to you under the terms of the GNU GPL, version 2;
# see file COPYING or http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt
*/

#include <string.h>
#include <ios/errno.h>

#include "core/defines.h"
#include "crypto/aes.h"
#include "crypto/sha.h"
#include "interrupt/irq.h"
#include "memory/memory.h"
#include "messaging/resourceManager.h"
#include "filedesc/filedesc_types.h"

static u8 hashTable[0x10] = { 0 };
static u32 hashTableSalt = 0;
static u32 hashTableCount = 0;
ResourceManager ResourceManagers[MAX_RESOURCES] SRAM_BSS;

u32 GetPpcAccessRights(const char* resourcePath)
{
	u32 salt = hashTableSalt;
	for(; *resourcePath != '\0'; resourcePath++)
		salt = salt ^ (((salt * 128) + (u32)(*resourcePath)) + (salt >> 5));

	u32 hashIndex = salt % hashTableCount;
	return ((hashTableCount == 0) || (1 << hashTable[hashIndex] & salt) != 0)
		? 1
		: 0;
}

s32 RegisterResourceManager(const char* devicePath, const u32 queueid)
{
	u32 interrupts = DisableInterrupts();
	s32 ret = 0;
	u32 devicePathLen = strnlen(devicePath, MAX_PATHLEN);
	s32 resourceManagerId;

	if(devicePathLen >= MAX_PATHLEN)
	{
		ret = IPC_EINVAL;
		goto returnRegisterResource;
	}

	if(CheckMemoryPointer(devicePath, devicePathLen, 3, CurrentThread->ProcessId, CurrentThread->ProcessId) != 0 || queueid >= MAX_MESSAGEQUEUES)
	{
		ret = IPC_EINVAL;
		goto returnRegisterResource;
	}

	if(MessageQueues[queueid].ProcessId != CurrentThread->ProcessId)
	{
		ret = IPC_EACCES;
		goto returnRegisterResource;
	}

	for(resourceManagerId = 0; resourceManagerId < MAX_DEVICES; resourceManagerId++)
	{
		if(strncmp(devicePath, ResourceManagers[resourceManagerId].DevicePath, MAX_PATHLEN-1) == 0)
		{
			ret = IPC_EEXIST;
			goto returnRegisterResource;
		}

		if(ResourceManagers[resourceManagerId].DevicePath[0] == '\0')
			break;
	}

	if(resourceManagerId >= MAX_DEVICES)
	{
		ret = IPC_EMAX;
		goto returnRegisterResource;
	}

	memcpy(ResourceManagers[resourceManagerId].DevicePath, devicePath, devicePathLen + 1);
	ResourceManagers[resourceManagerId].PathLength = devicePathLen;
	ResourceManagers[resourceManagerId].MessageQueue = &MessageQueues[queueid];
	ResourceManagers[resourceManagerId].ProcessId = CurrentThread->ProcessId;
	ResourceManagers[resourceManagerId].PpcHasAccessRights = GetPpcAccessRights(devicePath);

	if(!memcmp(devicePath, AES_DEVICE_NAME, AES_DEVICE_NAME_SIZE))
		AesFileDescriptor.BelongsToResource = &ResourceManagers[resourceManagerId];

	if(!memcmp(devicePath, SHA_DEVICE_NAME, SHA_DEVICE_NAME_SIZE))
		ShaFileDescriptor.BelongsToResource = &ResourceManagers[resourceManagerId];

returnRegisterResource:
	RestoreInterrupts(interrupts);
	return ret;
}