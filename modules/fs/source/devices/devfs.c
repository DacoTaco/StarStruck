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
#include "../hardware/nand.h"
#include "../sffs/cache.h"
#include "../sffs/commands.h"
#include "../sffs/inode.h"
#include "devfs.h"

FSHandle _fileHandles[FS_MAX_FILE_HANDLES];
s32 _fsShutdown = 0;

typedef enum
{
	IOCTLV_READDIR = 0x04,
	IOCTLV_GETUSAGE = 0x0C,
	IOCTLV_MASSCREATE = 0x0E,
} FSIoctlvCommands;

typedef enum
{
	IOCTL_FORMAT = 0x01,
	IOCTL_GETSTATS = 0x02,
	IOCTL_CREATEDIR = 0x03,
	IOCTL_SETATTR = 0x05,
	IOCTL_GETATTR = 0x06,
	IOCTL_DELETE = 0x07,
	IOCTL_RENAME = 0x08,
	IOCTL_CREATEFILE = 0x09,
	IOCTL_SETFILEVERCTRL = 0x0A,
	IOCTL_GETFILESTATS = 0x0B,
	IOCTL_SHUTDOWN = 0x0D,
} FSIoctlCommands;

// Allocate and initialize a file handle
s32 GetFSHandle(u32 userId, u16 groupId, u32 inode, u32 mode, u32 size)
{
	s32 fd = NAND_RESULT_ACCESS;
	u32 index = 0;

 // Find first free handle
	for (index = 0; index < FS_MAX_FILE_HANDLES; index++)
	{
		if (_fileHandles[index].InUse != 0)
			continue;
  // Initialize the handle
		_fileHandles[index].InUse = 1;
		_fileHandles[index].UserId = userId;
		_fileHandles[index].GroupId = groupId;
		_fileHandles[index].Inode = inode;
		_fileHandles[index].Mode = mode;
		_fileHandles[index].FilePosition = 0;
		_fileHandles[index].FilePointer = 0;
		_fileHandles[index].Size = size;
		_fileHandles[index].ShouldFlushSuperblock = 0;
		_fileHandles[index].Error = 0;
		break;
	}

 // Return pointer to handle as fd if found
	if (index != FS_MAX_FILE_HANDLES)
		fd = (s32)&_fileHandles[index];
	return fd;
}

// Open a file and return its handle
s32 GetFileHandle(u32 userId, u16 groupId, const char *path, u32 mode)
{
 // Validate path length
	u32 pathLen = GetPathLength(path);
	if (pathLen == 0)
		return FS_EINVAL;

 // Get superblock
	SuperBlockInfo *superblock = SelectSuperBlock();
	if (superblock == NULL)
		return FS_NOFILESYSTEM;

 // Find inode for path
	u32 inode = FindInodeByPath(superblock, path);
	if (inode == SFFSErasedNode)
		return FS_ENOENT;

 // Get FST entry and check if it's a file (not directory)
	FileSystemTableEntry *fstEntry = GetFstEntry(superblock, inode);
	if ((fstEntry->Mode.Fields.Type & S_IFMT) != S_IFREG)
		return FS_EINVAL;

 // Check permissions
	s32 ret = CheckUserPermissions(superblock, inode, userId, groupId, mode);
	if (ret != IPC_SUCCESS)
		return ret;

 // Allocate handle and return as fd
	s32 fd = GetFSHandle(userId, groupId, inode, mode, fstEntry->FileSize);
	if (fd < 0)
		return FS_EFDEXHAUSTED;

	return fd;
}

// Close a file handle and flush superblock if needed
s32 CloseHandle(FSHandle *handle)
{
	s32 ret = IPC_SUCCESS;

 // Validate handle pointer
	if ((s32)handle < 0)
		return FS_EINVAL;

 // Get superblock
	SuperBlockInfo *superblock = SelectSuperBlock();
	if (superblock == NULL)
		return FS_NOFILESYSTEM;

 // Flush superblock if needed
	if (handle->ShouldFlushSuperblock != 0)
	{
		ret = TryWriteSuperblock();
		if (ret != IPC_SUCCESS)
			return ret;
	}

 // Mark handle as inactive
	handle->InUse = 0;

	return ret;
}

// Handle IOS_CLOSE for /dev/fs files
s32 HandleDevFsClose(IpcMessage *message)
{
	FSHandle *handle = (FSHandle *)message->Request.FileDescriptor;
	s32 ret = IPC_SUCCESS;

 // Check if handle has an error stored
	if (handle->Error != 0)
		ret = (s32)handle->Error;

 // Find and flush any cached cluster data for this handle
	ClusterCacheEntry *cache = FindCachedCluster(handle);
	if (cache != NULL)
	{
		s32 flushRet = FlushCachedCluster(cache);
		if (flushRet != IPC_SUCCESS)
			ret = flushRet;

  // Clear the cache entry
		cache->FileHandle = NULL;
	}

 // Check if this is the special /dev/fs handle (Inode = 0xFFFF)
 // If so, just mark as closed without further action
	if (handle->Inode == SFFSErasedNode)
		handle->InUse = 0;
	else // Close regular file handle
	{
		s32 closeRet = CloseHandle(handle);
		if (closeRet != IPC_SUCCESS)
			ret = closeRet;
	}

	return ret;
}

// Handle IOS_READ for /dev/fs files
s32 HandleDevFsRead(IpcMessage *message)
{
	FSHandle *handle = (FSHandle *)message->Request.FileDescriptor;

 // Propagate any deferred error stored on the handle
	s32 ret = (s32)handle->Error;
	if (ret != 0)
		return ret;

	// The /dev/fs control handle (Inode == SFFSErasedNode) cannot be directly read
	if (handle->Inode == SFFSErasedNode)
		return FS_EINVAL;

	// Require read permission (AccessMode::Read, bit 0)
	if ((handle->Mode & Read) == 0)
		return FS_EACCESS;

	u8 *output = (u8 *)message->Request.Data.Read.Data;
	u32 readLen = message->Request.Data.Read.Length;
	s32 progress = 0;

	// Clamp to the bytes actually remaining in the file
	if (handle->Size < readLen + handle->FilePointer)
		readLen = handle->Size - handle->FilePointer;

	ClusterCacheEntry *cache;
	while (readLen != 0)
	{
		cache = FindCachedCluster(handle);
		if (cache != NULL && cache->DataOffset == (handle->FilePointer & CLUSTER_MASK))
		{
			// Cache hit: copy the required bytes from the buffered cluster
			u32 offsetInCluster = handle->FilePointer - cache->DataOffset;
			u32 bytesToCopy = CLUSTER_SIZE - offsetInCluster;
			if (readLen < bytesToCopy)
				bytesToCopy = readLen;

			memcpy(output + progress, cache->Data + offsetInCluster, bytesToCopy);
			readLen -= bytesToCopy;
			progress += (s32)bytesToCopy;
			handle->FilePointer += bytesToCopy;
			continue;
		}

		// Cache miss – determine how to load the next cluster
		u32 clusterAlignedPos = handle->FilePointer & CLUSTER_MASK;
		u8 *outputBuffer;
		if ((handle->FilePointer & (CLUSTER_SIZE - 1)) == 0 &&
		    readLen >= CLUSTER_SIZE && ((u32)(output + progress) & 0x3F) == 0)
		{
			// Fast path: fptr is cluster-aligned, caller buffer is 64-byte aligned,
			// and at least one full cluster remains – read directly into output buffer
			// (no intermediate copy through the cache).
			outputBuffer = output + progress;
			readLen -= CLUSTER_SIZE;
			progress += (s32)CLUSTER_SIZE;
			ret = SeekFile(handle, (s32)clusterAlignedPos, SeekSet);
			if (ret != IPC_SUCCESS)
				return ret;

			handle->FilePointer += CLUSTER_SIZE;
		}
		else
		{
			// Normal path: load the cluster into a cache entry; the data will be
			// served byte-by-byte on the next loop iteration(s).
			ClusterCacheEntry *entry;
			if (cache == NULL)
			{
				entry = GetClusterCacheEntry(handle);
			}
			else
			{
				ret = FlushCachedCluster(cache);
				if (ret != IPC_SUCCESS)
					return ret;

				entry = cache;
			}

			entry->DataOffset = clusterAlignedPos;
			u32 remaining = handle->Size - clusterAlignedPos;
			entry->DataSize = remaining < CLUSTER_SIZE ? remaining : CLUSTER_SIZE;

			ret = SeekFile(handle, (s32)clusterAlignedPos, SeekSet);
			if (ret != IPC_SUCCESS)
				return ret;

			outputBuffer = &entry->Data[0];
		}

		ret = ReadFile(handle, outputBuffer, CLUSTER_SIZE);
		if (ret != IPC_SUCCESS)
			return ret;
	}

	return progress;
}

// Handle IOS_WRITE for /dev/fs files
s32 HandleDevFsWrite(IpcMessage *message)
{
	FSHandle *handle = (FSHandle *)message->Request.FileDescriptor;
	const u8 *writeData = (const u8 *)message->Request.Data.Write.Data;
	u32 writeLen = message->Request.Data.Write.Length;
	s32 progress = 0;

	// Propagate any deferred error stored on the handle
	s32 ret = (s32)handle->Error;
	if (ret != 0)
		return ret;

	// The /dev/fs control handle (Inode == SFFSErasedNode) cannot be written
	if (handle->Inode == SFFSErasedNode)
		return FS_EINVAL;

	// Require write permission (AccessMode::Write, bit 1)
	if ((handle->Mode & Write) == 0)
		return FS_EACCESS;

	ClusterCacheEntry *cache;
	while (writeLen != 0)
	{
		cache = FindCachedCluster(handle);
		if (cache == NULL || cache->DataOffset != (handle->FilePointer & CLUSTER_MASK))
		{
			// Cache miss: the desired cluster is not in the cache
			u32 clusterAlignedPos = handle->FilePointer & CLUSTER_MASK;

			if ((handle->FilePointer & (CLUSTER_SIZE - 1)) == 0 &&
			    writeLen >= CLUSTER_SIZE && ((u32)(writeData + progress) & 0x3F) == 0)
			{
				// Fast path: fptr is cluster-aligned, at least one full cluster remains,
				// and the caller's buffer is 64-byte aligned – write straight to NAND
				// without staging through the cluster cache.
				ret = CheckFreeClustersCached();
				if (ret != IPC_SUCCESS)
					return ret;

				ret = SeekFile(handle, (s32)clusterAlignedPos, SeekSet);
				if (ret != IPC_SUCCESS)
					return ret;

				ret = WriteFile(handle, writeData + progress, CLUSTER_SIZE);
				if (ret != IPC_SUCCESS)
					return ret;

				writeLen -= CLUSTER_SIZE;
				progress += (s32)CLUSTER_SIZE;
				u32 newFptr = handle->FilePointer + CLUSTER_SIZE;
				handle->FilePointer = newFptr;
				if (handle->Size < newFptr)
					handle->Size = newFptr;

				continue;
			}

			// Slow path: stage the cluster through the cache so sub-cluster and
			// unaligned writes can be merged with existing file data.
			if (cache == NULL)
				cache = GetClusterCacheEntry(handle);
			else
			{
				ret = FlushCachedCluster(cache);
				if (ret != IPC_SUCCESS)
					return ret;
			}

			cache->DataOffset = clusterAlignedPos;

			// Pre-load how much existing file data lives in this cluster slot.
			// If the write position is at or beyond EOF, DataSize == 0 and we
			// can skip reading – the cluster will be entirely new data.
			u32 remaining = handle->Size - clusterAlignedPos;
			cache->DataSize = remaining < CLUSTER_SIZE ? remaining : CLUSTER_SIZE;
			if (cache->DataSize == 0)
				continue;

			// Read the existing cluster content so that we can do a partial overwrite
			ret = SeekFile(handle, (s32)clusterAlignedPos, SeekSet);
			if (ret != IPC_SUCCESS)
				return ret;

			// Temporarily grant read permission if the handle was opened write-only
			bool needsReadPerm = (handle->Mode & Read) == 0;
			if (needsReadPerm)
				handle->Mode |= Read;

			ret = ReadFile(handle, cache->Data, CLUSTER_SIZE);
			if (needsReadPerm)
				handle->Mode &= ~(u32)Read;

			// On next iteration the cache hit branch takes over
			if (ret != IPC_SUCCESS)
				return ret;
		}
		else
		{
			// Cache hit: fptr is within the already-loaded cluster
			if (!cache->Unallocated)
			{
				// First write to this cache entry: verify space is available before
				// marking it dirty to avoid leaving the cache in an inconsistent state.
				ret = CheckFreeClustersCached();
				if (ret != IPC_SUCCESS)
					return ret;

				cache->Unallocated = true;
			}

			u32 offsetInCluster = handle->FilePointer - cache->DataOffset;
			u32 bytesToCopy = CLUSTER_SIZE - offsetInCluster;
			if (writeLen < bytesToCopy)
				bytesToCopy = writeLen;

			memcpy(cache->Data + offsetInCluster, writeData + progress, bytesToCopy);

			writeLen -= bytesToCopy;
			progress += (s32)bytesToCopy;
			u32 newFptr = handle->FilePointer + bytesToCopy;
			handle->FilePointer = newFptr;
			if (handle->Size < newFptr)
				handle->Size = newFptr;

			// Extend DataSize if the write moved fptr past the previously tracked end
			if (cache->DataOffset + cache->DataSize < handle->FilePointer)
				cache->DataSize = handle->FilePointer - cache->DataOffset;

			// If fptr is still inside the cluster, keep accumulating writes
			if ((handle->FilePointer & (CLUSTER_SIZE - 1)) != 0)
				continue;

			// Cluster boundary reached – flush to NAND
			ret = FlushCachedCluster(cache);
			if (ret != IPC_SUCCESS)
				return ret;
		}
	}

	return progress;
}

// Handle IOS_SEEK for /dev/fs files
s32 HandleDevFsSeek(IpcMessage *message)
{
	FSHandle *handle = (FSHandle *)message->Request.FileDescriptor;
	if (handle->Error != 0)
		return (s32)handle->Error;

	if (handle->Inode == SFFSErasedNode)
		return FS_EINVAL;

	u32 start = 0;
	switch (message->Request.Data.Seek.Whence)
	{
		case SeekSet:
			start = 0;
			break;
		case SeekCur:
			start = handle->FilePointer;
			break;
		case SeekEnd:
			start = handle->Size;
			break;
		default:
			return FS_EINVAL;
	}

	start = (u32)((s32)start + message->Request.Data.Seek.Where);
	if (handle->Size < start)
		return FS_EINVAL;

	handle->FilePointer = start;
	return (s32)start;
}

// Handle IOS_IOCTL for /dev/fs
s32 HandleDevFsIoctl(IpcMessage *message)
{
	FSHandle *handle = (FSHandle *)message->Request.FileDescriptor;
	IoctlMessage *ioctl = &message->Request.Data.Ioctl;
	s32 ret;

	switch (ioctl->Ioctl)
	{
		case IOCTL_FORMAT:
			return Format(handle->UserId, _fileHandles, FS_MAX_FILE_HANDLES);

		case IOCTL_GETSTATS:
			if (ioctl->IoLength < sizeof(SFFSStatistics))
				return FS_EINVAL;

			SFFSStatistics *out = (SFFSStatistics *)ioctl->IoBuffer;
			ret = GetStats(out);
			// Adjust free/used cluster counts to account for dirty cache entries
			// that are not yet flushed to NAND
			u32 dirtyCount = 0;
			for (u32 i = 0; i < FS_CLUSTER_CACHE_ENTRIES; i++)
			{
				if (ClusterCacheEntries[i].FileHandle != NULL &&
				    ClusterCacheEntries[i].Unallocated)
					dirtyCount++;
			}

			out->FreeClusters -= dirtyCount;
			out->UsedClusters += dirtyCount;
			return ret;

		case IOCTL_CREATEDIR:
			if (ioctl->IoLength < (s32)sizeof(FileOperationsParameter))
				return FS_EINVAL;

			FileOperationsParameter *attributes =
			    (FileOperationsParameter *)ioctl->IoBuffer;
			return CreateDirectory(handle->UserId, handle->GroupId, attributes->Path,
			                       attributes->Attributes, attributes->OwnerPermissions,
			                       attributes->GroupPermissions,
			                       attributes->OtherPermissions);

		case IOCTL_SETATTR:
			if (ioctl->InputLength < (s32)sizeof(FileOperationsParameter))
				return FS_EINVAL;

			FileOperationsParameter *inAttr = (FileOperationsParameter *)ioctl->InputBuffer;
			return SetAttributes(handle->UserId, inAttr->Path, inAttr->UserId,
			                     inAttr->GroupId, inAttr->Attributes, inAttr->OwnerPermissions,
			                     inAttr->GroupPermissions, inAttr->OtherPermissions);

		case IOCTL_GETATTR:
			if (ioctl->InputLength < MAX_FILE_PATH ||
			    ioctl->IoLength < (s32)sizeof(FileOperationsParameter))
				return FS_EINVAL;

			const char *inPath = (const char *)ioctl->InputBuffer;
			FileOperationsParameter outAttr = { 0 };
			u32 userId = 0;
			u16 groupId = 0;

			ret = GetAttributes(handle->UserId, handle->GroupId, inPath, &userId,
			                    &groupId, &outAttr.Attributes, &outAttr.OwnerPermissions,
			                    &outAttr.GroupPermissions, &outAttr.OtherPermissions);
			outAttr.UserId = userId;
			outAttr.GroupId = groupId;
			memcpy(ioctl->IoBuffer, &outAttr, sizeof(FileOperationsParameter));
			return ret;

		case IOCTL_DELETE:
			if (ioctl->InputLength < MAX_FILE_PATH)
				return FS_EINVAL;

			return DeletePath(handle->UserId, handle->GroupId,
			                  (const char *)ioctl->InputBuffer);

		case IOCTL_RENAME:
			// Expect two MAX_FILE_PATH buffers concatenated: source path then destination path
			const FileRenameParameter *paths =
			    (const FileRenameParameter *)ioctl->InputBuffer;
			return Rename(handle->UserId, handle->GroupId, paths->Source, paths->Destination);

		case IOCTL_CREATEFILE:
			if (ioctl->IoLength < (s32)sizeof(FileOperationsParameter))
				return FS_EINVAL;

			FileOperationsParameter *createAttr =
			    (FileOperationsParameter *)ioctl->IoBuffer;
			return CreateFile(handle->UserId, handle->GroupId, createAttr->Path,
			                  createAttr->Attributes, createAttr->OwnerPermissions,
			                  createAttr->GroupPermissions, createAttr->OtherPermissions);

		case IOCTL_SETFILEVERCTRL:
			const FileOperationsParameter *versionControlParameters =
			    (const FileOperationsParameter *)ioctl->InputBuffer;
			return SetFileVersionControl(handle->UserId,
			                             versionControlParameters->Path,
			                             (u32)versionControlParameters->Attributes);

		case IOCTL_GETFILESTATS:
			// Propagate any error recorded on the handle (e.g. from a failed open)
			if (handle->Error != 0)
				return (s32)handle->Error;

			// Stats are only available when the file was opened for reading
			if ((handle->Mode & Read) == 0)
				return FS_EACCESS;

			// Control/directory handles (no backing inode) don't carry file stats
			if (handle->Inode == SFFSErasedNode)
				return FS_EINVAL;

			FileStatistics *stats = (FileStatistics *)ioctl->IoBuffer;
			stats->FileLength = handle->Size;
			stats->FilePosition = handle->FilePointer;
			return IPC_SUCCESS;

		case IOCTL_SHUTDOWN:
			_fsShutdown = 1;
			return DisableNand();

		default:
			return FS_EINVAL;
	}
}

// Handle IOS_IOCTLV for /dev/fs
s32 HandleDevFsIoctlv(IpcMessage *message)
{
	FSHandle *handle = (FSHandle *)message->Request.FileDescriptor;
	IoctlvMessage *ioctlvMessage = &message->Request.Data.Ioctlv;
	switch (ioctlvMessage->Ioctl)
	{
		case IOCTLV_READDIR: {
			//parameters are :
			// in: [filepath, num_entries (optional)],
			//out: [name_list (optional), num_entries]
			if (ioctlvMessage->Data[0].Length != MAX_FILE_PATH)
				return FS_EINVAL;

			if (ioctlvMessage->InputArgc < 1 || ioctlvMessage->InputArgc > 2 ||
			    ioctlvMessage->IoArgc != ioctlvMessage->InputArgc ||
			    ioctlvMessage->Data[1].Length != sizeof(u32))
				return FS_EINVAL;

			const char *filePath = (char *)ioctlvMessage->Data[0].Data;
			char *files = NULL;
			u32 *numberOfEntries;
			if (ioctlvMessage->InputArgc == 2)
			{
				const u32 bufferSize =
				    (*(u32 *)ioctlvMessage->Data[1].Data) * (MAX_FILE_SIZE + 1);
				if (ioctlvMessage->Data[3].Length != 4 ||
				    ioctlvMessage->Data[2].Length != bufferSize)
					return FS_EINVAL;

				files = (char *)ioctlvMessage->Data[2].Data;
				numberOfEntries = (u32 *)ioctlvMessage->Data[3].Data;
			}
			else
				numberOfEntries = (u32 *)ioctlvMessage->Data[1].Data;

			*numberOfEntries = *((u32 *)ioctlvMessage->Data[1].Data);
			return ReadDirectory(handle->UserId, handle->GroupId, filePath,
			                     files, numberOfEntries);
		}
		case IOCTLV_GETUSAGE: {
			// parameters are:
			// in: [filepath]
			// out: [clusters, inodes]
			if (ioctlvMessage->InputArgc != 1 || ioctlvMessage->IoArgc != 2)
				return FS_EINVAL;

			if (ioctlvMessage->Data[0].Length != MAX_FILE_PATH ||
			    ioctlvMessage->Data[1].Length != sizeof(u32) ||
			    ioctlvMessage->Data[2].Length != sizeof(u32))
				return FS_EINVAL;

			return GetPathUsage((const char *)ioctlvMessage->Data[0].Data,
			                    (u32 *)ioctlvMessage->Data[1].Data,
			                    (u32 *)ioctlvMessage->Data[2].Data);
		}
		case IOCTLV_MASSCREATE: {
			// parameters are:
			// in: [filepath_0, ..., filepath_N-1, sizes_array]
			// out: none
			if (ioctlvMessage->InputArgc < 2 || ioctlvMessage->IoArgc != 0)
				return FS_EINVAL;

			const u32 numberOfFiles = ioctlvMessage->InputArgc - 1;
			IoctlvMessageData *sizesVector = &ioctlvMessage->Data[numberOfFiles];
			u32 *sizes = (u32 *)sizesVector->Data;

			// The last input vector holds the sizes array: one u32 per file
			if (sizesVector->Length != numberOfFiles * sizeof(u32))
				return FS_EINVAL;

			// Validate each file path
			for (u32 i = 0; i < numberOfFiles; i++)
			{
				u32 pathLen = strnlen((const char *)ioctlvMessage->Data[i].Data, MAX_FILE_PATH);
				if (pathLen == MAX_FILE_PATH ||
				    pathLen + 1 != ioctlvMessage->Data[i].Length)
					return FS_EINVAL;
			}

			return MassCreateFiles(handle->UserId, handle->GroupId,
			                       ioctlvMessage->Data, sizes, numberOfFiles);
		}
		default:
			// Unknown/unsupported IOCTLV
			return FS_EINVAL;
	}
}
