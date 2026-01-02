/*
	StarStruck - a Free Software reimplementation for the Nintendo/BroadOn IOS.
	Copyright (C) 2022	DacoTaco

# This code is licensed to you under the terms of the GNU GPL, version 2;
# see file COPYING or http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt
*/

#include <errno.h>
#include <ios/errno.h>
#include <ios/processor.h>
#include <ios/syscalls.h>
#include <ios/printk.h>

#include "nand.h"
#include "errors.h"
#include "cluster.h"
#include "fs.h"
#include "commands.h"
#include "cache.h"

int main(void)
{
	u32 messageQueueMessages[8] ALIGNED(0x10);
	u32 *message;
	printk("$IOSVersion:  FFSP: %s %s 64M $", __DATE__, __TIME__);
	s32 ret = OSCreateMessageQueue((void **)&messageQueueMessages, 8);
	s32 messageQueueId = ret;
	if (ret >= 0)
		ret = OSRegisterResourceManager("/dev/boot2", messageQueueId);
	if (ret >= 0)
		ret = OSRegisterResourceManager("/", messageQueueId);
	if (ret < IPC_SUCCESS)
		return ret;

	ret = InitializeNand();
	if (ret != IPC_SUCCESS)
		goto _Main_Loop;

	ret = InitializeSFFS(1);
	printk("InitializeSFFS=%d\n", ret);
	switch (ret)
	{
		case IPC_SUCCESS: {
   // Get usage info for /tmp directory
			u32 tmpClusters;
			u32 tmpInodes;
			ret = GetPathUsage("/tmp", &tmpClusters, &tmpInodes);
			if (ret == IPC_SUCCESS)
			{
				// If /tmp exists but has less than 2 inodes (just the dir itself),
				// it's empty so we can skip deletion/recreation
				if (tmpInodes < 2)
					goto _ClearClusterCache;
			}
			//all error are ignored
			else if (ret != FS_ENOENT)
				break;

			// Delete existing /tmp if it exists
			ret = DeletePath(0, 0, "/tmp");
			if (ret != IPC_SUCCESS && ret != FS_ENOENT)
				break;

			// Create fresh /tmp directory with full permissions (rwx for owner/group/other)
			//TODO
			//ret = CreateDirectory(0, 0, "/tmp", 0, 3, 3, 3);
			if (ret != IPC_SUCCESS)
				break;

			// Fall through to clear cluster cache
			goto _ClearClusterCache;
		}
		case FS_NOFILESYSTEM:
_ClearClusterCache:
			for (s32 i = 0; i < FS_CLUSTER_CACHE_ENTRIES; i++)
				ClusterCacheEntries[i].FileHandle = NULL;
			break;
		default:
			// Other errors, fall through to main loop
			break;
	}

_Main_Loop:
	while (1)
	{
		s32 ret = OSReceiveMessage(messageQueueId, &message, 0);
		if (ret < 0)
			break;
	}
	return 0;
}