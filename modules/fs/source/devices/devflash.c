/*
	StarStruck - a Free Software reimplementation for the Nintendo/BroadOn IOS.
	Copyright (C) 2025	DacoTaco

# This code is licensed to you under the terms of the GNU GPL, version 2;
# see file COPYING or http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt
*/

#include <ios/errno.h>
#include <ios/ipc.h>
#include <string.h>

#include "../handles.h"
#include "../errors.h"
#include "../hardware/nand_helpers.h"
#include "devflash.h"

// Flash interface handle structure - tracks state of /dev/flash file handle
typedef struct
{
	u32 NandPosition;  // Current page position for read/write/seek
	bool IsActive;    // Handle open/closed state (1 = open, 0 = closed)
} FlashInterfaceHandle;
CHECK_SIZE(FlashInterfaceHandle, 8);
CHECK_OFFSET(FlashInterfaceHandle, 0x00, NandPosition);
CHECK_OFFSET(FlashInterfaceHandle, 0x04, IsActive);

// IOCTL commands for /dev/flash (raw NAND access)
#define IOCTL_GET_STATS       1  // Get NAND size info (0x1C bytes)
#define IOCTL_GET_LOG         2  // Get NAND error log (0x198 bytes)
#define IOCTL_ERASE_BLOCK     3  // Erase block at current position
#define IOCTL_CHECK_BAD_BLOCK 4  // Check if block is bad (-13 = bad)

// Interface handles for /dev/flash
#define MAX_FLASH_HANDLES     2
FlashInterfaceHandle _interfaceHandles[MAX_FLASH_HANDLES] = { { 0 } };

// Check if file descriptor belongs to /dev/flash device
// IOS 16 checks if fd is an address within the _interfaceHandles array
// The fd is literally a pointer to one of the FlashInterfaceHandle slots
bool IsDevFlashFileHandle(s32 fd)
{
	FlashInterfaceHandle *handle = (FlashInterfaceHandle *)fd;

  // Check if fd points within our _interfaceHandles array
	if (handle >= &_interfaceHandles[0] &&
	    handle <= &_interfaceHandles[MAX_FLASH_HANDLES - 1])
		return true;

	return false;
}

#ifdef ENABLE_DEV_FLASH
// Open /dev/flash handle - allocates a handle slot (IOS 16 approach)
// There is an actual IOS mistake here, as it does not reset the FilePosition.
// meaning when a handle is reused it will continue from the last position instead of starting at 0 lawl
s32 OpenFlashHandle(void)
{
	// Loop through the available handle slots
	for (u32 i = 0; i < MAX_FLASH_HANDLES; i++)
	{
		FlashInterfaceHandle *handle = &_interfaceHandles[i];

		// Check if slot is free
		if (handle->IsActive == 0)
		{
			// Mark as active
			handle->IsActive = 1;

			// Return the address of this handle as the file descriptor
			return (s32)handle;
		}
	}

	// All slots are occupied
	return IPC_ENOMEM; // -5
}
#endif

// Handle IOS_READ for /dev/flash
static inline s32 HandleReadMessage(FlashInterfaceHandle *handle, const ReadMessage *readMsg,
                                    const u32 pageSize, const u32 pageWithEcc)
{
	u32 readLen = readMsg->Length;
	void *data = readMsg->Data;
	void *eccBuf;

	if (readLen == pageWithEcc)
		eccBuf = (u8 *)data + pageSize;
	else if (readLen == pageSize)
		eccBuf = NULL;
	else
		return IPC_EINVAL;

	s32 ret = ReadNandPage(handle->NandPosition, data, eccBuf, true);
	if (ret == IPC_SUCCESS)
	{
		handle->NandPosition++;
		return (s32)readLen;
	}

	return ret;
}

// Handle IOS_WRITE for /dev/flash
static inline s32 HandleWriteMessage(FlashInterfaceHandle *handle,
                                     const WriteMessage *writeMsg,
                                     const u32 pageSize, const u32 pageWithEcc)
{
	u32 writeLen = writeMsg->Length;
	u8 *data = (u8 *)writeMsg->Data;
	void *eccBuf;

	if (writeLen == pageWithEcc)
		eccBuf = (u8 *)data + pageSize;
	else if (writeLen == pageSize)
		eccBuf = NULL;
	else
		return IPC_EINVAL;

	s32 ret = WriteNandPage(handle->NandPosition, data, eccBuf, 0, true);
	if (ret == IPC_SUCCESS)
	{
		handle->NandPosition++;
		return (s32)writeLen;
	}

	return ret;
}

// Handle IOS_CLOSE for /dev/flash
static inline s32 HandleCloseMessage(FlashInterfaceHandle *handle)
{
	handle->IsActive = 0;
	return IPC_SUCCESS;
}

static inline s32 HandleIoctlMessage(FlashInterfaceHandle *handle,
                                     const IoctlMessage *ioctlMsg,
                                     const NandSizeInformation *nandSizeInfo)
{
	void *const outBuffer = ioctlMsg->IoBuffer;
	const u32 outSize = ioctlMsg->IoLength;

	switch (ioctlMsg->Ioctl)
	{
		case IOCTL_GET_STATS:
			if (!outBuffer || outSize < sizeof(NandSizeInformation))
				return IPC_EINVAL;

			memcpy(outBuffer, nandSizeInfo, sizeof(NandSizeInformation));
			return IPC_SUCCESS;

		case IOCTL_GET_LOG:
			if (!outBuffer || outSize < COPY_COMMAND_LOG_SIZE)
				return IPC_EINVAL;

			return GetNandCommandLog(outBuffer);

		case IOCTL_ERASE_BLOCK:
		case IOCTL_CHECK_BAD_BLOCK:
			const u32 blockShift =
			    ((CLUSTER_SIZE_SHIFT - nandSizeInfo->PageSizeBitShift) & 0xFF);

			return ioctlMsg->Ioctl == IOCTL_ERASE_BLOCK ?
			           DeleteCluster(handle->NandPosition >> blockShift) :
			           CheckClusterBlocks(handle->NandPosition >> blockShift);

		default:
			return IPC_EINVAL;
	}
}

// Handle IOS_SEEK for /dev/flash
static inline s32 HandleSeekMessage(FlashInterfaceHandle *handle, const SeekMessage *seekMsg,
                                    const NandSizeInformation *nandSizeInfo)
{
	// Calculate total pages in NAND
	u32 totalPages = GetNandMaxPages(nandSizeInfo);

	// Calculate new position based on whence
	u32 newPos;
	switch (seekMsg->Whence)
	{
		case SeekSet:
			newPos = (u32)seekMsg->Where;
			break;
		case SeekCur:
			newPos = (u32)((s32)handle->NandPosition + seekMsg->Where);
			break;
		case SeekEnd:
			newPos = (u32)((s32)totalPages + seekMsg->Where);
			break;
		default:
			return IPC_EINVAL;
	}

	// Validate new position is within bounds
	if (newPos >= totalPages)
		return IPC_EINVAL;

	handle->NandPosition = newPos;
	return (s32)newPos;
}

// Handle all IPC messages for /dev/flash device
s32 HandleDevFlashMessage(IpcMessage *message)
{
	// Get handle from file descriptor
	FlashInterfaceHandle *handle = (FlashInterfaceHandle *)message->Request.FileDescriptor;

	// Get NAND size info (IOS 58 calls FS_GetNandSizeInfo_ at function start)
	NandSizeInformation nandSizeInfo;
	s32 ret = GetNandSizeInfo(&nandSizeInfo);
	if (ret != IPC_SUCCESS)
		return ret;

	// Calculate page size with ECC for read/write operations
	const u32 pageSize = GetNandPageSize(&nandSizeInfo);
	const u32 pageWithEcc = pageSize + GetNandEccSize(&nandSizeInfo);

	// Route to appropriate handler based on command type
	switch (message->Request.Command)
	{
		case IOS_READ:
			return HandleReadMessage(handle, &message->Request.Data.Read, pageSize, pageWithEcc);
		case IOS_WRITE:
			return HandleWriteMessage(handle, &message->Request.Data.Write,
			                          pageSize, pageWithEcc);
		case IOS_SEEK:
			return HandleSeekMessage(handle, &message->Request.Data.Seek, &nandSizeInfo);
		case IOS_IOCTL:
			return HandleIoctlMessage(handle, &message->Request.Data.Ioctl, &nandSizeInfo);
		case IOS_CLOSE:
			return HandleCloseMessage(handle);
		default:
			return IPC_EINVAL;
	}
}
