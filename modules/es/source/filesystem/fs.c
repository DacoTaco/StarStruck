/*
	StarStruck - a Free Software reimplementation for the Nintendo/BroadOn IOS.
	ES Module - /dev/fs thin wrappers

	Copyright (C) 2026	DacoTaco

# This code is licensed to you under the terms of the GNU GPL, version 2;
# see file COPYING or http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt
*/

#include <ios/errno.h>
#include <ios/syscalls.h>
#include <fs/errors.h>
#include <fs/fs.h>
#include <string.h>

#include "fs.h"

#define KERNEL_HEAPID 0
#define SYSTEM_USERID 0x1000

s32 GetTitleUserId(TitleType titleType, u32 titleId, u32 *userIdOutput)
{
	TitleUIDEntry *entries = NULL;

 //read the UID from the file
 //if it doesn't exist, we will create it with the System menu's UserId
	s32 fd = OpenFile("/sys/uid.sys", Read);
	if (fd == FS_ENOENT)
	{
		entries = (TitleUIDEntry *)OSAllocateMemory(KERNEL_HEAPID, sizeof(TitleUIDEntry));
		if (entries == NULL)
			return IPC_ENOMEM;

		entries->Title = (TitleID){ .Type = SystemTitle, .Id = 0x00000002 };
		entries->UserId = SYSTEM_USERID;

		*userIdOutput = SYSTEM_USERID;
		s32 ret = CreateAndWriteFile("/sys/uid.sys", 0, 3, 3, 0, (u8 *)entries,
		                             sizeof(TitleUIDEntry));
		OSFreeMemory(KERNEL_HEAPID, entries);
		return ret;
	}

	if (fd < 0)
		return fd;

	//get the file content and allocate space for the file + 1 entry in case we need to append one
	FileStatistics stats;
	s32 ret = GetFileStats(fd, &stats);
	if (ret != 0)
		goto cleanup_get_uid;

	entries = (TitleUIDEntry *)OSAllocateMemory(
	    KERNEL_HEAPID, stats.FileLength + sizeof(TitleUIDEntry));
	if (entries == NULL)
	{
		ret = IPC_ENOMEM;
		goto cleanup_get_uid;
	}

	s32 read = ReadFile(fd, entries, stats.FileLength);
	OSCloseFD(fd);
	fd = -1;
	if ((u32)read != stats.FileLength)
	{
		ret = (read < 0) ? read : IOS_EIO;
		goto cleanup_get_uid;
	}

	u32 entryCount = stats.FileLength / sizeof(TitleUIDEntry);
	u32 lastUserId = SYSTEM_USERID;
	bool found = false;

	for (u32 i = 0; i < entryCount; i++)
	{
		if (entries[i].UserId > lastUserId)
			lastUserId = entries[i].UserId;

		if (entries[i].Title.Type == titleType && entries[i].Title.Id == titleId)
		{
			*userIdOutput = entries[i].UserId;
			found = true;
		}
	}

	if (found)
	{
		ret = 0;
		goto cleanup_get_uid;
	}

	//append new UserId for the title and write it back to nand
	u32 newUserId = lastUserId + 1;
	entries[entryCount].Title = (TitleID){ .Type = titleType, .Id = titleId };
	entries[entryCount].UserId = newUserId;

	ret = CreateAndWriteFile("/sys/uid.sys", 0, 3, 3, 0, (u8 *)entries,
	                         stats.FileLength + sizeof(TitleUIDEntry));
	if (ret == 0)
		*userIdOutput = newUserId;

cleanup_get_uid:
	if (fd >= 0)
		OSCloseFD(fd);
	if (entries != NULL)
		OSFreeMemory(KERNEL_HEAPID, entries);
	return ret;
}

s32 DeleteDirectoryIfEmpty(const char *path)
{
	s32 ret = IPC_SUCCESS;
	char buffer[MAX_FILE_PATH] = { 0 };
	strncpy(buffer, path, MAX_FILE_PATH - 1);

	while (1)
	{
		char *seperator = strrchr(buffer, '/');
		if (seperator == NULL || seperator <= buffer)
			break;

		//check if directory is empty, if its already gone lets bail :)
		u32 entriesCount = 0;
		ret = ReadDirectory(buffer, NULL, &entriesCount);
		if (ret == FS_ENOENT)
			break;
		if (ret != IPC_SUCCESS)
			return ret;

		//if its not empty we bail out
		if (entriesCount != 0)
			return 0;

		ret = DeletePath(buffer);
		if (ret != IPC_SUCCESS)
			return ret;

		//trim string and repeat
		*seperator = '\0';
	}

	return ret;
}

s32 ReadDirectoryEntries(const char *path, char **entriesOutput, u32 *count)
{
	*entriesOutput = NULL;

	//read the entries count first
	s32 ret = ReadDirectory(path, NULL, count);
	if (ret != 0 || *count == 0)
		return ret;

	char *buffer = (char *)OSAllocateMemory(KERNEL_HEAPID, *count * (MAX_FILE_SIZE + 1));
	if (buffer == NULL)
		return IPC_ENOMEM;

	memset(buffer, 0, *count * (MAX_FILE_SIZE + 1));
	*entriesOutput = buffer;

	//fetch the entries
	ret = ReadDirectory(path, *entriesOutput, count);
	if (ret != IPC_SUCCESS)
	{
		OSFreeMemory(KERNEL_HEAPID, buffer);
		*entriesOutput = NULL;
	}
	return ret;
}
