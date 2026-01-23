/*
	StarStruck - a Free Software reimplementation for the Nintendo/BroadOn IOS.
	Copyright (C) 2025	DacoTaco

# This code is licensed to you under the terms of the GNU GPL, version 2;
# see file COPYING or http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt
*/
#include <ios/errno.h>
#include <string.h>

#include "inode.h"
#include "filesystem.h"
#include "commands.h"
#include "errors.h"
#include "handles.h"

s32 ProcessInodeAction(SuperBlockInfo *superblock, u32 inode, InodeAction action)
{
	s32 result = 0;
	u32 clusterCount = 0;
	u32 inodeCount = 1; // Start at 1 to count the current directory

  // Get the first child inode from the parent's StartCluster field
	FileSystemTableEntry *parentEntry = GetFstEntry(superblock, inode);
	u32 currentInode = parentEntry->StartCluster;

  // Traverse the sibling chain
	while (currentInode != SFFSErasedNode)
	{
		FileSystemTableEntry *entry = GetFstEntry(superblock, currentInode);
		FileSystemEntryType entryType = entry->Mode.Fields.Type;

		switch (action)
		{
			case MarkInodePending:
      // Set high bit of FileSize to mark as pending delete
    // FileSize is stored big-endian, so set 0x80 on first byte
			case UnlinkInode:
				break;
			case CheckIfOpenInode:
    // if its not a file, skip
				for (int i = 0; i < FS_MAX_FILE_HANDLES; i++)
				{
					FSHandle *handle = &_fileHandles[i];
					if (handle->InUse && handle->Inode == currentInode)
						return FS_EFDOPEN;
				}
				break;

			case GetUsedClusters:
    // if its not a file, skip
				if (entryType != S_IFREG)
					break;

    // Check if file is open - use handle's size if so
				u32 handleIndex = 0;
				for (; handleIndex < FS_MAX_FILE_HANDLES; handleIndex++)
				{
					FSHandle *handle = &_fileHandles[handleIndex];
					if (handle->InUse == 1 && handle->Inode == currentInode)
					{
      // File is open, use handle's size
						// Add 0x3FFF to round up, then shift by 14 (divide by cluster size 0x4000)
						break;
					}
				}
				// File not open, use FST entry's file size
				if (handleIndex == FS_MAX_FILE_HANDLES)
					clusterCount += (entry->FileSize + 0x3FFF) >> 14;

				break;

			case GetUsedInodes:
				// if its not a file, skip
				if (entryType != S_IFREG)
					break;

				inodeCount++;
				break;

			default:
				break;
		}

		// If this is a directory, recurse into it
		if (entryType == S_IFDIR)
		{
			s32 recursiveResult = ProcessInodeAction(superblock, currentInode, action);
			if (recursiveResult < 0)
				return recursiveResult;

			if (action == GetUsedClusters)
			{
				clusterCount += (u32)recursiveResult;
				result = 0;
			}
			else if (action == GetUsedInodes)
			{
				inodeCount += (u32)recursiveResult;
				result = 0;
			}
		}

		// Unlink action: free FAT chain and clear FST entry
		// This is done AFTER recursion so subdirectories are cleaned first
		if (action == UnlinkInode)
		{
			if (entryType == S_IFREG)
			{
				u16 cluster = entry->StartCluster;
				while (cluster != SFFSLastNode)
				{
					u16 nextCluster = superblock->FatEntries[cluster];
					superblock->FatEntries[cluster] = SFFSFreeNode;
					RemoveUsedClusterStats(1);
					cluster = nextCluster;
				}
			}

			// Clear the FST entry attributes
			entry->Mode.Value = 0;
			RemoveUsedInodeStats(1);
		}

		// Move to the next sibling
		currentInode = entry->Sibling;
	}

	// Return appropriate value based on action
	if (action == GetUsedClusters)
		return (s32)clusterCount;
	if (action == GetUsedInodes)
		return (s32)inodeCount;
	return result;
}

// Find a child inode within a directory by name
u32 FindInode(SuperBlockInfo *superblock, u32 parentInode, const char *name)
{
	s32 pathLen = (s32)strnlen(name, MAX_FILE_PATH);
	char component[64];
	char *componentPtr = component;
	u32 currentInode = parentInode;
	s32 error = 0;

	for (s32 index = 0; index <= pathLen; index++)
	{
		if (name[index] != '/' && name[index] != '\0')
		{
			*componentPtr = name[index];
			componentPtr++;
			continue;
		}

		*componentPtr = '\0';
		componentPtr = component;

		// Check if component name is too long (max 12 chars for SFFS)
		if (strnlen(componentPtr, MAX_FILE_SIZE) == MAX_FILE_SIZE)
		{
			error = FS_EINVAL;
			break;
		}

		// Get the StartCluster of the current directory (first child inode)
		FileSystemTableEntry *entry = GetFstEntry(superblock, currentInode);
		currentInode = entry->StartCluster;

		// Search through siblings for matching name
		for (; currentInode != SFFSErasedNode; currentInode = entry->Sibling)
		{
			FileSystemTableEntry *entry = GetFstEntry(superblock, currentInode);
			if (strncmp(entry->Name, componentPtr, 0x0C) == 0)
				break;
		}

		if (currentInode == SFFSErasedNode)
		{
			error = FS_EINVAL;
			break;
		}
	}

	if (error != 0)
		return SFFSErasedNode;
	else
		return currentInode;
}

// Find an inode by full path starting from root
// Path must start with '/'
u32 FindInodeByPath(SuperBlockInfo *superblock, const char *path)
{
	// If path is just "/", return root inode (0)
	// otherwise, skip leading '/' and search from root
	return path[1] == '\0' ? 0 : FindInode(superblock, 0, path + 1);
}

// Remove target inode from parent directory's sibling linked list
s32 UnlinkTargetInode(SuperBlockInfo *superblock, u32 parentInode, u32 targetInode)
{
	FileSystemTableEntry *parentEntry = GetFstEntry(superblock, parentInode);
	FileSystemTableEntry *targetEntry = GetFstEntry(superblock, targetInode);

	// Check if target is first child (parent's StartCluster points to target)
	if (parentEntry->StartCluster == targetInode)
	{
		// Update parent's StartCluster to point to target's sibling
		u16 targetSibling = targetEntry->Sibling;
		u8 *parentStartCluster = (u8 *)&parentEntry->StartCluster;
		parentStartCluster[0] = (u8)(targetSibling >> 8);
		parentStartCluster[1] = (u8)(targetSibling);
	}
	else
	{
		// Traverse sibling chain to find the inode that points to target
		bool found = false;
		u32 currentInode = parentEntry->StartCluster;

		while (currentInode != SFFSErasedNode)
		{
			FileSystemTableEntry *currentEntry = GetFstEntry(superblock, currentInode);

			if (currentEntry->Sibling == targetInode)
			{
				// Found it - update current's sibling to skip target
				u16 targetSibling = targetEntry->Sibling;
				u8 *currentSibling = (u8 *)&currentEntry->Sibling;
				currentSibling[0] = (u8)(targetSibling >> 8);
				currentSibling[1] = (u8)(targetSibling);
				found = true;
				break;
			}

			currentInode = currentEntry->Sibling;
		}

		if (!found)
			return FS_ENOENT;
	}

	// Clear target's attributes byte to mark as unused
	targetEntry->Mode.Fields.Type = S_IFZERO;

	return IPC_SUCCESS;
}

// Check if a file inode is currently open in any file handle
s32 CheckIfFileOpen(u32 inode)
{
	for (u32 i = 0; i < FS_MAX_FILE_HANDLES; i++)
	{
		FSHandle *handle = &_fileHandles[i];
		// Check both InUse flag and Inode match
		if (handle->InUse != 0 && handle->Inode == inode)
		{
			return FS_EFDOPEN;
		}
	}
	return IPC_SUCCESS;
}

s32 CheckUserPermissions(SuperBlockInfo *superblock, u32 inode, u32 uid, u16 gid, AccessMode mode)
{
	if (uid == 0)
		return IPC_SUCCESS;

	FileSystemTableEntry *entry = GetFstEntry(superblock, inode);
	u8 permissions = 0;
	if (entry->UserId == uid)
		permissions = entry->Mode.Fields.OwnerPermissions;
	else if (entry->GroupId == gid)
		permissions = entry->Mode.Fields.GroupPermissions;
	else
		permissions = entry->Mode.Fields.OtherPermissions;

	if ((mode & permissions) != mode)
		return FS_EACCESS;

	return IPC_SUCCESS;
}
