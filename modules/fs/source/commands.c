/*
	StarStruck - a Free Software reimplementation for the Nintendo/BroadOn IOS.
	Copyright (C) 2025	DacoTaco

# This code is licensed to you under the terms of the GNU GPL, version 2;
# see file COPYING or http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt
*/
#include <ios/errno.h>
#include <ios/syscalls.h>
#include <ios/keyring.h>
#include <string.h>

#include "commands.h"
#include "filesystem.h"
#include "inode.h"
#include "cluster.h"
#include "errors.h"

u32 _superblockOffset;
u32 _fileSystemMetadataSizeShift = 0;
u32 _fileSystemDataSize = 0;

// Cluster pair typedef - used for relocation maps and write chain tracking
// [0] = source/head cluster, [1] = dest/tail cluster
typedef u16 SFFSClusterPair[2];
static SFFSClusterPair _writeChain = { SFFSLastNode, SFFSBadNode };

// Scan FAT to find blocks marked as reserved for block relocation
static u16 FindReservedCluster(SuperBlockInfo *superblock)
{
	if (superblock == NULL)
		return SFFSBadNode;

	u32 start = _superblockOffset >> CLUSTER_SIZE_SHIFT;
	u32 end = start + (_fileSystemDataSize >> CLUSTER_SIZE_SHIFT);
	u32 clustersPerBlock =
	    1 << ((SelectedNandSizeInfo.BlockSizeBitShift - CLUSTER_SIZE_SHIFT) & 0xFF);

 // Scan block-aligned (not individual clusters)
	while (start < end)
	{
		start &= 0xFFFF; // Keep within u16 range
		if (superblock->FatEntries[start] == SFFSReservedNode)
			return (u16)start;

		start += clustersPerBlock;
	}

	return SFFSBadNode;
}

// Update all FAT chains and FST entries to reflect relocated clusters
static void ClusterRelocationUpdate(SuperBlockInfo *superblock,
                                    SFFSClusterPair *relocationMap, u32 count)
{
	u32 start = _superblockOffset >> CLUSTER_SIZE_SHIFT;
	u32 end = start + (_fileSystemDataSize >> CLUSTER_SIZE_SHIFT);

 // Step 1: Update FAT chain pointers
	for (u32 i = start; i < end; i++)
	{
		// Only check valid cluster pointers (not special values above SFFSLastNode)
		if (superblock->FatEntries[i] > SFFSLastNode)
			continue;

		for (u32 j = 0; j < count; j++)
		{
			if (superblock->FatEntries[i] != relocationMap[j][0])
				continue;

			superblock->FatEntries[i] = relocationMap[j][1];
			break;
		}
	}

	// Step 2: Update FST StartCluster fields
	u32 fstEntryCount = GetFstEntryCount();
	for (u32 inode = 0; inode < fstEntryCount; inode++)
	{
		FileSystemTableEntry *entry = GetFstEntry(superblock, inode);

		// Only process files
		if (entry->Mode.Fields.Type != S_IFREG)
			continue;

		for (u32 j = 0; j < count; j++)
		{
			if (entry->StartCluster != relocationMap[j][0])
				continue;

			// Update using byte-wise writes (endianness handling)
			u8 *startCluster = (u8 *)&entry->StartCluster;
			startCluster[0] = (u8)(relocationMap[j][1] >> 8);
			startCluster[1] = (u8)(relocationMap[j][1]);
			break;
		}
	}

	// Step 3: Update write chain head pointer
	// Tracks first cluster of chain being written during file write operations
	for (u32 i = 0; i < count; i++)
	{
		if (_writeChain[0] != relocationMap[i][0])
			continue;

		_writeChain[0] = relocationMap[i][1];
		break;
	}

	// Step 4: Update write chain tail pointer
	// Tracks last cluster of chain being written during file write operations
	for (u32 i = 0; i < count; i++)
	{
		if (_writeChain[1] != relocationMap[i][0])
			continue;

		_writeChain[1] = relocationMap[i][1];
		return;
	}
}

// Helper: Mark all clusters in a block with a specific status
static inline void MarkBlockStatus(SuperBlockInfo *superblock, u32 blockCluster,
                                   u32 clustersPerBlock, u16 status)
{
	u32 blockStart = blockCluster & (u32)(~(clustersPerBlock - 1));
	for (u32 i = 0; i < clustersPerBlock; i++)
		superblock->FatEntries[blockStart + i] = status;
}

// Helper: Check if a block has mixed usage (both free and used clusters)
static bool IsBlockMixed(SuperBlockInfo *superblock, u32 blockCluster, u32 clustersPerBlock)
{
	bool foundFreeCluster = false;
	bool hasUsedClusters = false;

	for (u32 i = 0; i < clustersPerBlock; i++)
	{
		u16 fatEntry = superblock->FatEntries[blockCluster + i];

		// Skip reserved, bad, or erased blocks entirely
		if (fatEntry == SFFSReservedNode || fatEntry == SFFSBadNode || fatEntry == SFFSErasedNode)
			return false;

		if (fatEntry == SFFSFreeNode)
		{
			if (hasUsedClusters)
				return true; // Mixed: found free after used
			foundFreeCluster = true;
		}
		else
		{
			if (foundFreeCluster)
				return true; // Mixed: found used after free
			hasUsedClusters = true;
		}
	}

	return false;
}

// Helper: Rollback failed relocations and mark destination block as bad
static void RollbackRelocations(SuperBlockInfo *superblock,
                                SFFSClusterPair *relocationMap, u32 relocationCount,
                                u32 failedDestBlock, u32 clustersPerBlock)
{
	// Restore original FAT pointers for all relocated clusters
	for (u32 i = 0; i < relocationCount; i++)
	{
		u16 srcCluster = relocationMap[i][0];
		u16 dstCluster = relocationMap[i][1];

		// Restore source cluster's original chain pointer
		superblock->FatEntries[srcCluster] = superblock->FatEntries[dstCluster];
	}

	// Clean up destination block - convert erased clusters back to free
	u32 destBlockStart = failedDestBlock & (u32)(-clustersPerBlock);
	for (u32 j = 0; j < clustersPerBlock; j++)
	{
		u32 clusterIdx = destBlockStart + j;
		if (superblock->FatEntries[clusterIdx] == SFFSErasedNode)
			superblock->FatEntries[clusterIdx] = SFFSFreeNode;
	}

	// Mark the failed destination block as bad
	MarkBlockStatus(superblock, failedDestBlock, clustersPerBlock, SFFSBadNode);

	// Update statistics
	AddBadBlockStats(clustersPerBlock);
}

// Wear leveling and bad block handling
// Finds blocks with mixed usage (both free and used clusters),
// relocates used clusters to reserved blocks, and marks source blocks as reserved
s32 ReclaimBlocks(SuperBlockInfo *superblock)
{
	u32 clustersPerBlock =
	    1 << ((SelectedNandSizeInfo.BlockSizeBitShift - CLUSTER_SIZE_SHIFT) & 0xFF);

	// Early exit if only 1 cluster per block (can't do wear leveling)
	if (clustersPerBlock == 1)
		return FS_ENOENT;

	// VLA - matches IOS dynamic stack allocation: (clustersPerBlock * 4) + 4 bytes
	SFFSClusterPair relocationMap[clustersPerBlock + 1];

	u32 startCluster = _superblockOffset >> CLUSTER_SIZE_SHIFT;
	u32 clusterCount = _fileSystemDataSize >> CLUSTER_SIZE_SHIFT;

	// Main retry loop - continues until success or unrecoverable error
	while (true)
	{
		u16 reclaimedBlocks[8] = { 0 };
		u16 reservedBlocks[8] = { 0 };
		u32 relocationCount = 0;
		u32 reclaimedCount = 0;
		u32 currentReservedIdx = 0;
		s32 returnCode = 0;

		// Step 1: Find and prepare reserved blocks for relocation
		u32 reservedBlockCount = 0;
		for (u32 i = 0; i < 8; i++)
		{
			u16 foundCluster = FindReservedCluster(superblock);
			reservedBlocks[i] = foundCluster;

			if (foundCluster == SFFSBadNode)
				break;

			// Erase the reserved block to prepare it for use
			MarkBlockStatus(superblock, foundCluster, clustersPerBlock, SFFSErasedNode);
			reservedBlockCount++;
		}

		// No reserved blocks available - cannot proceed
		if (reservedBlockCount == 0)
			return FS_ENOENT;

		// Step 2: Scan filesystem for mixed-usage blocks and relocate them
		u32 currentReservedBlock = reservedBlocks[0];
		bool encounteredError = false;

		for (u32 blockCluster = startCluster;
		     blockCluster < startCluster + clusterCount && !encounteredError;
		     blockCluster += clustersPerBlock)
		{
			// Check if this block has mixed usage
			if (!IsBlockMixed(superblock, blockCluster, clustersPerBlock))
				continue;

			// Step 3: Relocate all used clusters from this mixed block
			for (u32 i = 0; i < clustersPerBlock && !encounteredError; i++)
			{
				u16 srcCluster = (u16)(blockCluster + i);
				u16 fatEntry = superblock->FatEntries[srcCluster];

				// Only relocate valid cluster chains (not special values)
				if (fatEntry > SFFSLastNode)
					continue;

				// Switch to next reserved block if current one is full
				if (relocationCount == clustersPerBlock)
				{
					ClusterRelocationUpdate(superblock, relocationMap, relocationCount);
					currentReservedIdx++;
					currentReservedBlock = reservedBlocks[currentReservedIdx];
					relocationCount = 0;
				}

				u16 dstCluster = (u16)(currentReservedBlock + relocationCount);

				// Attempt to copy cluster data
				returnCode = CopyClusters(srcCluster, dstCluster, 1);
				if (returnCode != IPC_SUCCESS)
				{
					// Copy failed - rollback and retry with different reserved block
					RollbackRelocations(superblock, relocationMap, relocationCount,
					                    currentReservedBlock, clustersPerBlock);

					// Mark partial progress as reserved before retrying
					for (u32 j = 0; j < currentReservedIdx; j++)
						MarkBlockStatus(superblock, reclaimedBlocks[j],
						                clustersPerBlock, SFFSReservedNode);

					currentReservedIdx++;
					for (u32 j = currentReservedIdx;
					     j < 8 && reservedBlocks[j] != SFFSBadNode; j++)
						MarkBlockStatus(superblock, reservedBlocks[j],
						                clustersPerBlock, SFFSReservedNode);

					TryWriteSuperblock();
					encounteredError = true;
					break; // Break to retry loop
				}

				// Relocation successful - update FAT and tracking structures
				superblock->FatEntries[dstCluster] = fatEntry;
				superblock->FatEntries[srcCluster] = SFFSFreeNode;

				relocationMap[relocationCount][0] = srcCluster;
				relocationMap[relocationCount][1] = dstCluster;
				relocationCount++;
			}

			if (encounteredError)
				break;

			// Step 4: Erase and record the now-empty source block
			MarkBlockStatus(superblock, blockCluster, clustersPerBlock, SFFSErasedNode);
			reclaimedBlocks[reclaimedCount++] = (u16)blockCluster;

			// Stop if we've reclaimed enough or ran out of reserved blocks
			if (reclaimedCount == 8 || reservedBlocks[currentReservedIdx + 1] == SFFSBadNode)
				break;
		}

		// If error occurred, retry from the beginning
		if (encounteredError)
			continue;

		// Step 5: Finalize successful relocation
		if (reclaimedCount == 0)
			return FS_ENOENT; // No blocks could be reclaimed

		// Apply final batch of relocations
		if (relocationCount != 0)
		{
			ClusterRelocationUpdate(superblock, relocationMap, relocationCount);
			currentReservedIdx++;
		}

		// Mark reclaimed blocks as reserved for future wear leveling
		for (u32 i = 0; i < currentReservedIdx; i++)
			MarkBlockStatus(superblock, reclaimedBlocks[i], clustersPerBlock, SFFSReservedNode);

		// Mark any unused reserved blocks as reserved
		for (u32 i = currentReservedIdx; i < 8 && reservedBlocks[i] != SFFSBadNode; i++)
			MarkBlockStatus(superblock, reservedBlocks[i], clustersPerBlock, SFFSReservedNode);

		// Success - flush changes and return
		return TryWriteSuperblock();
	}
}

s32 DeletePath(u32 uid, u16 gid, const char *path)
{
	u32 length = GetPathLength(path);
	if (length == 0)
		return FS_EINVAL;

	SuperBlockInfo *superblock = SelectSuperBlock();
	if (superblock == NULL)
		return FS_NOFILESYSTEM;

	char directory[MAX_FILE_PATH];
	char fileName[0x10];

	if (SplitPath(path, directory, fileName) != IPC_SUCCESS)
		return FS_EINVAL;

	u32 directoryNode = FindInodeByPath(superblock, directory);
	if (directoryNode == SFFSErasedNode)
		return FS_ENOENT;

	s32 ret = CheckUserPermissions(superblock, directoryNode, uid, gid, Write);
	if (ret != IPC_SUCCESS)
		return ret;

	u32 fileNode = FindInode(superblock, directoryNode, fileName);
	if (fileNode == SFFSErasedNode)
		return FS_ENOENT;

	// Track if we need to flush and if clusters were freed
	bool clustersFreed = false;
	bool superblockFlushed = false;

	// Get FST entry for the target
	FileSystemTableEntry *entry = GetFstEntry(superblock, fileNode);
	FileSystemEntryType entryType = entry->Mode.Fields.Type;

	// Check if it's a directory
	if (entryType == S_IFDIR)
	{
		// Check if directory has children
		if (entry->StartCluster != SFFSErasedNode)
		{
			// Check if any files in directory tree are open
			ret = ProcessInodeAction(superblock, fileNode, CheckIfOpenInode);
			if (ret != IPC_SUCCESS)
				return ret;

			// Recursively delete directory contents
			ret = ProcessInodeAction(superblock, fileNode, UnlinkInode);
			if (ret != IPC_SUCCESS)
				return ret;

			clustersFreed = true;
		}
	}
	else
	{
		// It's a file - check if currently open
		ret = CheckIfFileOpen(fileNode);
		if (ret != IPC_SUCCESS)
			return ret;

		// Get first cluster of file
		u16 cluster = entry->StartCluster;

		// Free the file's cluster chain
		while (cluster != SFFSLastNode)
		{
			clustersFreed = true;

			u16 nextCluster = superblock->FatEntries[cluster];
			superblock->FatEntries[cluster] = SFFSFreeNode;
			RemoveUsedClusterStats(1);
			cluster = nextCluster;
		}
	}

	// Remove inode from parent's sibling chain
	ret = UnlinkTargetInode(superblock, directoryNode, fileNode);
	if (ret != IPC_SUCCESS)
		return ret;

	// Update inode statistics
	RemoveUsedInodeStats(1);

	if (clustersFreed)
	{
		// Attempt block reclamation, if success mark superblock as flushed
		if (ReclaimBlocks(superblock) == IPC_SUCCESS)
			superblockFlushed = true;

		ret = IPC_SUCCESS;
	}

	// Flush superblock to persist changes
	if (!superblockFlushed)
		ret = TryWriteSuperblock();

	return ret;
}

u32 GetPathLength(const char *path)
{
	if (path == NULL || *path != '/')
		return 0;

	u32 length = strnlen(path, MAX_FILE_PATH);
	if (length == MAX_FILE_PATH || length < 2 || path[length - 1] == '/')
		return 0;

	return length;
}

s32 SplitPath(const char *path, char *directory, char *fileName)
{
	u32 pathLength = GetPathLength(path);
	if (pathLength == 0 || directory == NULL || fileName == NULL)
		return FS_EINVAL;

	// Find the last '/' in the path by scanning backwards
	u32 lastSlashIndex = pathLength;
	for (; path[lastSlashIndex] != '/' && lastSlashIndex > 0; lastSlashIndex--)
	{
	}

	// Check if filename is too long (max 12 chars for SFFS)
	u32 fileNameLength = pathLength - lastSlashIndex - 1;
	if (fileNameLength >= MAX_FILE_SIZE)
		return FS_EINVAL;

	// Copy directory part
	if (lastSlashIndex == 0)
	{
		// Root directory case
		directory[0] = '/';
		directory[1] = '\0';
	}
	else
	{
		strncpy(directory, path, (size_t)lastSlashIndex);
		directory[lastSlashIndex] = '\0';
	}

	// Copy filename part (skip the '/')
	strncpy(fileName, path + lastSlashIndex + 1, (size_t)fileNameLength);
	fileName[fileNameLength] = '\0';

	return IPC_SUCCESS;
}