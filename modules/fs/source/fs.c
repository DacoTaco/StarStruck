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
#include <ios/ipc.h>
#include <string.h>
#include <ios/module.h>
#include <ios/gecko.h>

#include "hardware/nand.h"
#include "hardware/cluster.h"
#include "sffs/commands.h"
#include "sffs/cache.h"
#include "sffs/inode.h"
#include "devices/devfs.h"
#include "devices/devflash.h"
#include "devices/devboot2.h"
#include "errors.h"
#include "fs.h"
#include "handles.h"

// Handle IOS_OPEN command - route to appropriate handler based on path
static s32 ProcessOpenMessage(IpcMessage *message)
{
	const char *path = message->Request.Data.Open.Filepath;
	u32 uid = message->Request.Data.Open.UID;
	u16 gid = message->Request.Data.Open.GID;
	AccessMode accessMode = message->Request.Data.Open.Mode;
	bool openFs = false;
	bool openBoot2 = false;
#ifdef ENABLE_DEV_FLASH
	bool openFlash = false;
#endif

 // Check if path starts with "/dev/"
	if (strncmp(path, "/dev/", 5) == 0)
	{
  // Check for "/dev/fs"
		if (strncmp(path + 5, "fs", 3) == 0)
			openFs = true;
#ifdef ENABLE_DEV_FLASH
		else if (strncmp(path + 5, "flash", 6) == 0)
			openFlash = true;
#endif
		else if (strncmp(path + 5, "boot2", 6) == 0)
			openBoot2 = true;
		else //unknown /dev/ device
			return FS_EINVAL;
	}

 // Handle based on detected device type
	if (openFs)
		return GetFSHandle(uid, gid, SFFSErasedNode, 0, 0);
#ifdef ENABLE_DEV_FLASH
	else if (openFlash)
		return uid == 0 ? OpenFlashHandle() : NAND_RESULT_ACCESS;
#endif
	else if (openBoot2)
		return uid == 0 ? OpenBoot2FileHandle() : NAND_RESULT_ACCESS;
	else
		return GetFileHandle(uid, gid, path, accessMode);
}

int main(void)
{
	u32 messageQueueMessages[8] ALIGNED(0x10);
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
			ret = CreateDirectory(0, 0, "/tmp", 0, 3, 3, 3);
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
		IpcMessage *ipcMessage;
		s32 receiveRet = OSReceiveMessage(messageQueueId, (u32 **)&ipcMessage, 0);
		if (receiveRet != IPC_SUCCESS)
			continue;

		s32 ipcRet = ret;
		// If initialization succeeded, process the message
		if (ret == IPC_SUCCESS)
		{
			if (_fsShutdown == 1)
				ipcRet = FS_ESHUTDOWN;
			else if (ipcMessage->Request.Command != IOS_OPEN &&
			         IsDevFlashFileHandle(ipcMessage->Request.FileDescriptor))
				ipcRet = HandleDevFlashMessage(ipcMessage);
			else if (ipcMessage->Request.Command != IOS_OPEN &&
			         IsBoot2FileHandle(ipcMessage->Request.FileDescriptor))
				ipcRet = HandleDevBoot2Message(ipcMessage);
			else
			{
				switch (ipcMessage->Request.Command)
				{
					case IOS_OPEN:
						ipcRet = ProcessOpenMessage(ipcMessage);
						break;
					case IOS_CLOSE:
						ipcRet = HandleDevFsClose(ipcMessage);
						break;
					case IOS_READ:
						ipcRet = HandleDevFsRead(ipcMessage);
						break;
					case IOS_WRITE:
						ipcRet = HandleDevFsWrite(ipcMessage);
						break;
					case IOS_SEEK:
						ipcRet = HandleDevFsSeek(ipcMessage);
						break;
					case IOS_IOCTL:
						ipcRet = HandleDevFsIoctl(ipcMessage);
						break;
					case IOS_IOCTLV:
						ipcRet = HandleDevFsIoctlv(ipcMessage);
						break;
					default:
						ipcRet = IPC_EINVAL;
						break;
				}
			}
		}

		// Reply to the caller
		OSResourceReply(ipcMessage, ipcRet);
	}
	return 0;
}