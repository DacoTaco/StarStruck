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

#include "../hardware/cluster.h"
#include "../hardware/nand_helpers.h"
#include "../errors.h"
#include "commands.h"
#include "filesystem.h"
#include "inode.h"

extern u32 _selectedSuperblockIndex;
extern SuperBlockInfo *_selectedSuperBlock;
extern SaltData _saltData;

u32 _superblockOffset;
u32 _fileSystemDataSize = 0;

// Cluster pair typedef - used for relocation maps and write chain tracking
// [0] = source/head cluster, [1] = dest/tail cluster
typedef u16 SFFSClusterPair[2];
static SFFSClusterPair _writeChain = { SFFSLastNode, SFFSBadNode };

// Allocation tracking for wear leveling
static u16 _lastAllocatedCluster = 0;
static bool _lastAllocatedClusterInitialized = false;

// Scan FAT to find blocks marked as reserved for block relocation
static u16 FindReservedCluster(SuperBlockInfo *superblock)
{
	if (superblock == NULL)
		return SFFSBadNode;

	u32 start = _superblockOffset >> CLUSTER_SIZE_SHIFT;
	u32 end = start + (_fileSystemDataSize >> CLUSTER_SIZE_SHIFT);
	u32 clustersPerBlock = GetClustersPerBlock();

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

// Rescue valid data from a failing block by relocating it to a reserved block
// Called when a write operation encounters a bad block
// Returns IPC_SUCCESS if rescue succeeded, error code otherwise
s32 RescueFailingBlock(SuperBlockInfo *superblock, u16 failingCluster)
{
	const u32 clustersPerBlock = GetClustersPerBlock();

	// Calculate block-aligned start of the failing block
	u32 failingBlock = (u32)failingCluster & (u32)(~(clustersPerBlock - 1));

	// VLA for relocation map
	SFFSClusterPair relocationMap[clustersPerBlock];
	u32 validClusterCount = 0;

	// Step 1: Scan the failing block to find all valid clusters
	for (u32 i = 0; i < clustersPerBlock; i++)
	{
		// Skip special values (not part of a valid chain)
		if (superblock->FatEntries[failingBlock + i] >= SFFSReservedNode)
			continue;

		relocationMap[validClusterCount][0] = (u16)(failingBlock + i);
		validClusterCount++;
	}

	// Step 2: Retry loop to find a working reserved block
	while (true)
	{
		// Find a reserved block for relocation
		u16 reservedBlock = FindReservedCluster(superblock) & 0xFFFF;

		// No reserved blocks available - cannot rescue
		if (reservedBlock == SFFSBadNode)
			return FS_EFBIG;

		// Erase the reserved block to prepare it for use
		MarkBlockStatus(superblock, reservedBlock, clustersPerBlock, SFFSErasedNode);

		// Update statistics: reserved block becomes free space temporarily
		_sffStats.ReservedClusters -= clustersPerBlock;

		// If no valid clusters to rescue, just mark the block bad and return
		if (validClusterCount == 0)
		{
			MarkBlockStatus(superblock, failingBlock, clustersPerBlock, SFFSBadNode);
			_sffStats.BadClusters += clustersPerBlock;
			return IPC_SUCCESS;
		}

		// Step 3: Copy all valid clusters to the reserved block
		// Original uses while loop that continues while copies succeed
		u32 index = 0;
		while (CopyClusters(relocationMap[index][0], (u16)(reservedBlock + index), 1) == IPC_SUCCESS)
		{
			// Update FAT during copy (matches original behavior)
			superblock->FatEntries[reservedBlock + index] =
			    superblock->FatEntries[relocationMap[index][0]];
			relocationMap[index][1] = (u16)(reservedBlock + index);
			index++;

			// All clusters copied successfully
			if (index >= validClusterCount)
			{
				// Step 4: Update FAT chains to point to new locations
				ClusterRelocationUpdate(superblock, relocationMap, validClusterCount);

				// Step 5: Mark the failing block as bad
				MarkBlockStatus(superblock, failingBlock, clustersPerBlock, SFFSBadNode);
				_sffStats.BadClusters += clustersPerBlock;
				return IPC_SUCCESS;
			}
		}

		// Copy failed - mark this reserved block as bad and retry with another
		MarkBlockStatus(superblock, reservedBlock, clustersPerBlock, SFFSBadNode);
		_sffStats.BadClusters += clustersPerBlock;
	}
}

// Format the filesystem: initialize superblock, FAT and FST, persist to NAND
s32 Format(u32 userId, FSHandle *fileHandles, u32 fileHandleCount)
{
	//Root-only operation
	if (userId != 0 || fileHandles == NULL || fileHandleCount == 0)
		return FS_EACCESS;

	s32 ret = SelectNandSize(true);
	if (ret != IPC_SUCCESS)
		return ret;

	ret = InitSuperblockInfo(true);
	if (ret != IPC_SUCCESS)
	{
		SelectNandSize(false);
		return ret;
	}

	//Create an in-memory superblock and initialize FAT/FST
	SuperBlockInfo *superblock = CreateSuperBlock();
	if (superblock == NULL)
	{
		ret = FS_NOFILESYSTEM;
		goto format_cleanup;
	}

	//Initialize root inode (inode 0) as an empty directory
	FileSystemTableEntry *root = GetFstEntry(superblock, 0);
	strncpy(root->Name, "/", sizeof(root->Name));
	root->Mode.Value = 0;
	root->Mode.Fields.Type = S_IFDIR;
	root->Mode.Fields.OwnerPermissions = 3; /* rw */
	root->Mode.Fields.GroupPermissions = 1; /* r */
	root->Mode.Fields.OtherPermissions = 1; /* r */
	root->Attributes = 0;
	root->StartCluster = SFFSErasedNode;
	root->Sibling = SFFSErasedNode;
	root->FileSize = 0;
	root->UserId = 0;
	root->GroupId = 0;
	root->SFFSGeneration = 0;

	//Clear file handle table passed in by caller (usually the module-global array)
	for (u32 i = 0; i < fileHandleCount; i++)
		((FSHandle *)fileHandles)[i].InUse = 0;

	//Recompute filesystem statistics
	ret = StatSuperblock(superblock);
	if (ret != IPC_SUCCESS)
		goto format_cleanup;

	//Mark block-aligned regions as reserved before writing
	ret = MarkBlocksReserved(superblock);
	if (ret != IPC_SUCCESS)
		goto format_cleanup;

	//Persist the new superblock to NAND
	ret = TryWriteSuperblock();
	if (ret != IPC_SUCCESS)
		goto format_cleanup;

	goto format_return;

format_cleanup:
	InitSuperblockInfo(false);
	SelectNandSize(false);

format_return:
	return ret;
}

// Wear leveling and bad block handling
// Finds blocks with mixed usage (both free and used clusters),
// relocates used clusters to reserved blocks, and marks source blocks as reserved
s32 ReclaimBlocks(SuperBlockInfo *superblock)
{
	u32 clustersPerBlock = GetClustersPerBlock();

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

s32 DeletePath(const u32 uid, const u16 gid, const char *path)
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

static inline bool IsValidPath(const char *path, const u32 pathLen)
{
	for (u32 i = 0; i < (pathLen); i++)
	{
		//only allow characters >= 0x20 (space) and <= 0x7E (~)
		if ((path)[i] < ' ' || (path)[i] > '~')
			return false;
	}
	return true;
}

s32 Rename(const u32 userId, const u16 groupId, const char *source, const char *destination)
{
	if (GetPathLength(source) == 0 || GetPathLength(destination) == 0)
		return FS_EINVAL;

	SuperBlockInfo *superblock = SelectSuperBlock();
	if (superblock == NULL)
		return FS_NOFILESYSTEM;

	char sourceDir[MAX_FILE_PATH];
	char sourceName[MAX_FILE_SIZE + 4];
	char destinationDir[MAX_FILE_PATH];
	char destinationName[MAX_FILE_SIZE + 4];

	if (SplitPath(source, sourceDir, sourceName) != IPC_SUCCESS)
		return FS_EINVAL;
	if (SplitPath(destination, destinationDir, destinationName) != IPC_SUCCESS)
		return FS_EINVAL;

	u32 sourceDirectoryInode = FindInodeByPath(superblock, sourceDir);
	if (sourceDirectoryInode == SFFSErasedNode)
		return FS_ENOENT;

	u32 destinationDirectoryInode = FindInodeByPath(superblock, destinationDir);
	if (destinationDirectoryInode == SFFSErasedNode)
		return FS_ENOENT;

	// Require write permission on both source parent and destination parent
	s32 ret = CheckUserPermissions(superblock, sourceDirectoryInode, userId, groupId, Write);
	if (ret != IPC_SUCCESS)
		return ret;
	ret = CheckUserPermissions(superblock, destinationDirectoryInode, userId, groupId, Write);
	if (ret != IPC_SUCCESS)
		return ret;

	u16 sourceInode = (u16)FindInode(superblock, sourceDirectoryInode, sourceName);
	if (sourceInode == SFFSErasedNode)
		return FS_ENOENT;

	FileSystemTableEntry *srcEntry = GetFstEntry(superblock, sourceInode);
	st_mode sourceMode = srcEntry->Mode;
	switch (sourceMode.Fields.Type)
	{
		//dont allow file basename to change. filename is used in the file encryption so if that changes...
		//moving it is fine though, just no file rename
		case S_IFREG:
			if (strncmp(sourceName, destinationName, MAX_FILE_SIZE) != 0)
				return FS_EINVAL;
			break;
		//directories are fine if they are not open down the tree
		case S_IFDIR:
			ret = ProcessInodeAction(superblock, sourceInode, CheckIfOpenInode);
			break;
		default:
			ret = CheckIfFileOpen(sourceInode);
			break;
	}

	if (ret != IPC_SUCCESS)
		return ret;

	// If destination exists, handle according to type
	u32 destinationInode = FindInode(superblock, destinationDirectoryInode, destinationName);
	bool unlinkedInodes = false;
	if (destinationInode != SFFSErasedNode)
	{
		FileSystemTableEntry *destinationEntry = GetFstEntry(superblock, destinationInode);
		FileSystemEntryType destinationType = destinationEntry->Mode.Fields.Type;

		//if the types differ or its the same entry return error
		if (destinationType != sourceMode.Fields.Type || sourceInode == destinationInode)
			return FS_EINVAL;

		switch (destinationType)
		{
			case S_IFDIR:
				if (destinationEntry->StartCluster == SFFSErasedNode)
					break;

				ret = ProcessInodeAction(superblock, sourceInode, CheckIfOpenInode);
				if (ret != IPC_SUCCESS)
					return ret;

				ret = ProcessInodeAction(superblock, destinationInode, UnlinkInode);
				if (ret != IPC_SUCCESS)
					return ret;

				unlinkedInodes = true;
				break;
			default:
				ret = CheckIfFileOpen(sourceInode);
				if (ret != IPC_SUCCESS)
					return ret;

				u16 cluster = destinationEntry->StartCluster;
				if (cluster == SFFSLastNode)
					break;

				unlinkedInodes = true;
				while (cluster != SFFSLastNode)
				{
					superblock->FatEntries[cluster] = SFFSFreeNode;
					RemoveUsedClusterStats(1);
					cluster = superblock->FatEntries[cluster];
				}
				break;
		}

		ret = UnlinkTargetInode(superblock, destinationDirectoryInode, destinationInode);
		if (ret != IPC_SUCCESS)
			return ret;

		RemoveUsedInodeStats(1);
	}

	// Unlink from source parent
	ret = UnlinkTargetInode(superblock, sourceDirectoryInode, sourceInode);
	if (ret != IPC_SUCCESS)
		return ret;

	// Update data of the source entry to the new data
	FileSystemTableEntry *parentEntry = GetFstEntry(superblock, destinationDirectoryInode);
	strncpy(srcEntry->Name, destinationName, MAX_FILE_SIZE);
	srcEntry->Mode = sourceMode;
	srcEntry->Sibling = parentEntry->StartCluster;
	srcEntry->StartCluster = sourceInode;

	bool flushSuperBlock = false;
	if (unlinkedInodes)
	{
		ret = ReclaimBlocks(superblock);
		flushSuperBlock = ret == IPC_SUCCESS;
		ret = IPC_SUCCESS;
	}

	return flushSuperBlock ? TryWriteSuperblock() : ret;
}

s32 ReadDirectory(const u32 uid, const u16 gid, const char *path, char *files, u32 *numberOfEntries)
{
	if (!path || *path != '/' || !numberOfEntries || strnlen(path, MAX_FILE_PATH) == MAX_FILE_PATH)
		return FS_EINVAL;

	SuperBlockInfo *superblock = SelectSuperBlock();
	if (!superblock)
		return FS_NOFILESYSTEM;

	u32 inode = FindInodeByPath(superblock, path);
	if (inode == SFFSErasedNode)
		return FS_ENOENT;

	FileSystemTableEntry *entry = GetFstEntry(superblock, inode);
	if ((entry->Mode.Fields.Type & S_IFMT) != S_IFDIR)
		return FS_EINVAL;

	s32 ret = CheckUserPermissions(superblock, inode, uid, gid, Read);
	if (ret != IPC_SUCCESS)
		return ret;

	// If files is NULL, set numberOfEntries to max possible
	if (!files)
		*numberOfEntries = GetFstEntryCount();

	// Iterate directory entries
	char filename[MAX_FILE_SIZE + 1] = { 0 };
	for (u32 i = 0; i < *numberOfEntries;
	     entry = GetFstEntry(superblock, entry->Sibling), i++)
	{
		inode = entry->StartCluster;
		if (inode == SFFSErasedNode)
		{
			*numberOfEntries = i;
			return IPC_SUCCESS;
		}

		if (!files)
			continue;

		FileSystemTableEntry *childEntry = GetFstEntry(superblock, inode);
		strncpy(filename, childEntry->Name, MAX_FILE_SIZE);
		memcpy(files, filename, MAX_FILE_SIZE + 1);

		while (*files != '\0') files++;
		files++;
	}

	return IPC_SUCCESS;
}

s32 CreateDirectory(const u32 uid, const u16 gid, const char *path,
                    u8 attributes, u8 ownerPerm, u8 groupPerm, u8 otherPerm)
{
	// Validate path length and characters (0x20-0x7E)
	u32 pathLen = GetPathLength(path);
	if (pathLen == 0)
		return FS_EINVAL;

	if (!IsValidPath(path, pathLen))
		return FS_EINVAL;

	// Get superblock and split path
	SuperBlockInfo *superblock = SelectSuperBlock();
	if (superblock == NULL)
		return FS_NOFILESYSTEM;

	char directory[MAX_FILE_PATH];
	char fileName[MAX_FILE_SIZE + 4];
	s32 ret = SplitPath(path, directory, fileName);
	if (ret != IPC_SUCCESS)
		return FS_EINVAL;

	// Find and verify parent directory
	u32 parentInode = FindInodeByPath(superblock, directory);
	if (parentInode == SFFSErasedNode)
		return FS_ENOENT;

	ret = CheckUserPermissions(superblock, parentInode, uid, gid, Write);
	if (ret != IPC_SUCCESS)
		return ret;

	// Check if directory already exists
	u32 existingInode = FindInode(superblock, parentInode, fileName);
	if (existingInode != SFFSErasedNode)
		return FS_EEXIST;

	// Allocate new inode
	u32 newInode = GetFreeInode(superblock);
	if (newInode >= SFFSErasedNode)
		return FS_NO_INODES;

	// Initialize FST entry for new directory
	FileSystemTableEntry *newEntry = GetFstEntry(superblock, newInode);
	strncpy(newEntry->Name, fileName, MAX_FILE_SIZE);
	newEntry->Mode =
	    (st_mode){ .Fields = { .Type = S_IFDIR,
		                       .OwnerPermissions = (u8)(ownerPerm & 0x3),
		                       .GroupPermissions = (u8)(groupPerm & 0x3),
		                       .OtherPermissions = (u8)(otherPerm & 0x3) } };
	newEntry->Attributes = attributes;
	newEntry->StartCluster = SFFSErasedNode; // Empty directory
	newEntry->FileSize = 0;
	newEntry->UserId = uid;
	newEntry->GroupId = gid;
	newEntry->SFFSGeneration = 0;

	// Insert into parent's child list at head
	FileSystemTableEntry *parentEntry = GetFstEntry(superblock, parentInode);
	newEntry->Sibling = parentEntry->StartCluster;
	parentEntry->StartCluster = (u16)newInode;

	// Update statistics and flush
	AddUsedInodeStats(1);
	return TryWriteSuperblock();
}

s32 CreateFileInner(SuperBlockInfo *superBlock, u32 userId, u16 groupId,
                    const char *path, u8 attributes, u32 ownerPermissions,
                    u32 groupPermissions, u32 otherPermissions, u16 *inodeOutput)
{
	u32 pathLen = GetPathLength(path);
	if (pathLen == 0)
		return FS_EINVAL;

	if (!IsValidPath(path, pathLen))
		return FS_EINVAL;

	char directory[MAX_FILE_PATH];
	char fileName[MAX_FILE_SIZE + 4];
	if (SplitPath(path, directory, fileName) != IPC_SUCCESS)
		return FS_EINVAL;

	u32 parentInode = FindInodeByPath(superBlock, directory);
	if (parentInode == SFFSErasedNode)
		return FS_ENOENT;

	s32 ret = CheckUserPermissions(superBlock, parentInode, userId, groupId, Write);
	if (ret != IPC_SUCCESS)
		return ret;

	u32 existingInode = FindInode(superBlock, parentInode, fileName);
	if (existingInode != SFFSErasedNode)
		return FS_EEXIST;

	u32 newInode = GetFreeInode(superBlock);
	if (newInode == SFFSErasedNode)
		return FS_NO_INODES;

	FileSystemTableEntry *newEntry = GetFstEntry(superBlock, newInode);
	strncpy(newEntry->Name, fileName, MAX_FILE_SIZE);
	newEntry->Mode =
	    (st_mode){ .Fields = { .Type = S_IFREG,
		                       .OwnerPermissions = (u8)(ownerPermissions & 0x3),
		                       .GroupPermissions = (u8)(groupPermissions & 0x3),
		                       .OtherPermissions = (u8)(otherPermissions & 0x3) } };
	newEntry->Attributes = attributes;
	newEntry->StartCluster = SFFSLastNode; // No data yet
	newEntry->FileSize = 0;
	newEntry->UserId = userId;
	newEntry->GroupId = groupId;
	newEntry->SFFSGeneration = 0;

	// Insert into parent's child list at head
	FileSystemTableEntry *parentEntry = GetFstEntry(superBlock, parentInode);
	newEntry->Sibling = parentEntry->StartCluster;
	parentEntry->StartCluster = (u16)newInode;

	AddUsedInodeStats(1);
	if (inodeOutput)
		*inodeOutput = (u16)newInode;
	return IPC_SUCCESS;
}

s32 CreateFile(const u32 userId, const u16 groupId, const char *path, u8 attributes,
               u8 ownerPermissions, u8 groupPermissions, u8 otherPermissions)
{
	SuperBlockInfo *superblock = SelectSuperBlock();
	if (superblock == NULL)
		return FS_NOFILESYSTEM;

	s32 ret = CreateFileInner(superblock, userId, groupId, path, attributes,
	                          (u32)ownerPermissions, (u32)groupPermissions,
	                          (u32)otherPermissions, (u16 *)NULL);
	if (ret == IPC_SUCCESS)
		ret = TryWriteSuperblock();
	return ret;
}

// Seek to a position in a file
s32 SeekFile(FSHandle *handle, s32 offset, SeekMode whence)
{
	// Validate handle pointer
	if ((s32)handle < 0)
		return FS_EINVAL;

	// Get superblock to access file metadata
	SuperBlockInfo *superblock = SelectSuperBlock();
	if (superblock == NULL)
		return FS_NOFILESYSTEM;

	// Determine base position based on whence parameter
	u32 basePosition;
	switch (whence)
	{
		case SeekSet: //from beginning
			basePosition = 0;
			break;
		case SeekCur: //from current position
			basePosition = handle->FilePosition;
			break;
		case SeekEnd: //from end of file
		{
			FileSystemTableEntry *fstEntry = GetFstEntry(superblock, handle->Inode);
			// Round file size up to cluster boundary (0x4000 aligned) and mask off lower bits?
			basePosition = (fstEntry->FileSize + 0x3FFF) & CLUSTER_MASK;
			break;
		}
		default:
			return FS_EINVAL;
	}

	// Calculate new position
	u32 newPosition = (u32)((s32)basePosition + offset);

	// Validate new position
	if ((s32)newPosition < 0)
		return FS_EINVAL;

	// Check if position exceeds file size (rounded to cluster boundary)
	FileSystemTableEntry *fstEntry = GetFstEntry(superblock, handle->Inode);
	u32 maxPosition = (fstEntry->FileSize + 0x3FFF) & CLUSTER_MASK;
	if (newPosition > maxPosition)
		return FS_EINVAL;

	// Ensure position is cluster-aligned (must be multiple of 0x4000)
	if ((newPosition & 0x3FFF) != 0)
		return FS_EINVAL;

	// Update file pointer
	handle->FilePosition = newPosition;

	return IPC_SUCCESS;
}

// Allocate a free cluster from the FAT with wear leveling
// Implements generation-based allocation distribution, block-aligned scanning,
// preference for erased blocks, and automatic reclamation when exhausted
static u16 AllocateCluster(SuperBlockInfo *superblock)
{
	// Initialize last allocated position on first call using generation-based distribution
	if (!_lastAllocatedClusterInitialized)
	{
		u32 totalRange = _superblockOffset + _fileSystemDataSize;
		u32 generationCount = 1 << (_fileSystemMetadataSizeShift & 0xFF);
		u32 generationOffset = (superblock->Version << 20) &
		                       ((totalRange + generationCount) - 1);

		// Clamp to valid data region
		if (generationOffset < totalRange)
		{
			if (generationOffset < _superblockOffset)
				generationOffset = generationOffset + _superblockOffset;
		}
		else
		{
			generationOffset = generationOffset - generationCount;
		}

		// Align to block boundary and convert to cluster index
		u32 blockAlignMask = (u32)(-(GetClustersPerBlock()));
		_lastAllocatedCluster =
		    (u16)((generationOffset >> CLUSTER_SIZE_SHIFT) & blockAlignMask);
		_lastAllocatedClusterInitialized = true;
	}

	s32 retryCount = 1;
	u16 resultCluster = SFFSBadNode;
	u16 firstFreeCluster;

	u32 dataStart = _superblockOffset >> CLUSTER_SIZE_SHIFT;
	u32 dataSize = _fileSystemDataSize >> CLUSTER_SIZE_SHIFT;

	while (true)
	{
		u16 currentCluster = _lastAllocatedCluster;
		u16 index = 0;
		firstFreeCluster = resultCluster;
		if (dataSize == 0)
			goto reclaimBlocks;

		while (true)
		{
			firstFreeCluster = SFFSBadNode;
			bool isBlockFullyErased = true;
			u16 blockCluster = (_lastAllocatedCluster + index) & 0xFFFF;
			if (dataStart + dataSize <= blockCluster)
				blockCluster = (blockCluster - dataSize) & 0xFFFF;

			const u32 clustersPerBlock = GetClustersPerBlock();
			for (u32 clusterIndex = 0; clusterIndex < clustersPerBlock; clusterIndex++)
			{
				u16 fatEntry = superblock->FatEntries[blockCluster + clusterIndex];
				if (fatEntry == SFFSErasedNode)
				{
					if (firstFreeCluster == SFFSBadNode)
						firstFreeCluster = (u16)(blockCluster + clusterIndex) & 0xFFFF;
				}
				else if (fatEntry != SFFSFreeNode)
					isBlockFullyErased = false;
			}

			currentCluster = blockCluster;
			if (isBlockFullyErased)
			{
				// Refresh entire block as erased
				for (index = 0; index < clustersPerBlock; index++)
					superblock->FatEntries[(blockCluster & -clustersPerBlock) + index] =
					    SFFSErasedNode;

				firstFreeCluster = blockCluster;
				goto reclaimBlocks;
			}

			// Check if we found a free cluster or exhausted search space
			if (firstFreeCluster != SFFSBadNode)
				goto reclaimBlocks;

			// Advance to next block
			index = (u16)(index + clustersPerBlock);
			currentCluster = _lastAllocatedCluster;
			firstFreeCluster = resultCluster;

			// If we've scanned all blocks, give up and try reclamation
			if (dataSize <= index)
				goto reclaimBlocks;
		}

reclaimBlocks:
		resultCluster = firstFreeCluster;
		_lastAllocatedCluster = currentCluster;

		// Check if we found a cluster, ran out of retries, or reclamation failed
		if (resultCluster != SFFSBadNode)
			return resultCluster;

		if (retryCount == 0)
			return resultCluster;

		retryCount--;
		s32 reclaimResult = ReclaimBlocks(superblock);
		if (reclaimResult != IPC_SUCCESS)
			return resultCluster;
	}
}

// Create multiple files in a single call (IOS-style batch creation)
s32 MassCreateFiles(u32 userId, u16 groupId, IoctlvMessageData *paths, u32 *sizes, u32 numberOfFiles)
{
	SuperBlockInfo *superblock = SelectSuperBlock();
	if (!superblock)
		return FS_NOFILESYSTEM;
	if (_sffStats.FreeInodes < numberOfFiles)
		return FS_NO_INODES;

	s32 ret = IPC_SUCCESS;
	//Pre-calculate total clusters needed for all files
	u32 clusterCount = 0;
	for (u32 i = 0; i < numberOfFiles; ++i)
		clusterCount += (sizes[i] + CLUSTER_SIZE - 1) / CLUSTER_SIZE;

	if (_sffStats.FreeClusters < clusterCount)
		return FS_EFBIG;

	u16 selectedCluster = (u16)(_fileSystemDataSize / CLUSTER_SIZE);
	u16 maxCluster = (u16)(selectedCluster + (_fileSystemDataSize >> CLUSTER_SIZE_SHIFT));
	for (u32 i = 0; i < numberOfFiles; ++i)
	{
		u16 inode = 0;
		ret = CreateFileInner(superblock, userId, groupId,
		                      (const char *)paths[i].Data, 0, 3, 0, 0, &inode);
		if (ret != IPC_SUCCESS)
			break;

		u32 fileClusters = (sizes[i] + CLUSTER_SIZE - 1) / CLUSTER_SIZE;
		FileSystemTableEntry *entry = GetFstEntry(superblock, inode);
#pragma GCC diagnostic ignored "-Waddress-of-packed-member"
		u16 *clusterPointer = &entry->StartCluster;
#pragma GCC diagnostic pop
		for (u32 cluster = 0; cluster < fileClusters;)
		{
			// FAT operations: allocate and chain clusters
			if (superblock->FatEntries[selectedCluster] < SFFSBadNode)
			{
				*clusterPointer = selectedCluster;
				AddUsedClusterStats(1);
				cluster++;
				clusterPointer = &superblock->FatEntries[selectedCluster];
			}

			selectedCluster++;
			if (selectedCluster == maxCluster)
			{
				ret = FS_EFBIG;
				*clusterPointer = SFFSLastNode;
				goto cleanup_mass_create;
			}
		}

		*clusterPointer = SFFSLastNode;
	}
cleanup_mass_create:
	// If an error occurred during allocation, reset any clusters that were marked as used but not actually assigned to files.
	// This prevents cluster leaks and ensures the FAT is consistent.
	const u32 clustersPerBlock = GetClustersPerBlock();
	const u32 blockEnd = (u32)(selectedCluster & ~(clustersPerBlock - 1)) + clustersPerBlock;
	for (; selectedCluster < blockEnd; selectedCluster++)
	{
		// If the cluster is marked as used for this operation, reset it to free
		if (superblock->FatEntries[selectedCluster] == SFFSErasedNode)
			superblock->FatEntries[selectedCluster] = SFFSFreeNode;
	}

	s32 flushRet = TryWriteSuperblock();
	return ret != IPC_SUCCESS ? ret : flushRet;
}

// Write data to a file
s32 ReadFile(FSHandle *handle, u8 *data, u32 length)
{
	// Validate arguments: handle must be non-NULL, data non-NULL,
	// length non-zero and cluster-aligned (FS_ReadFileInner_ enforces this too)
	if ((s32)handle < 0 || data == NULL || (length & (CLUSTER_SIZE - 1)) != 0 || length == 0)
		return FS_EINVAL;

	SuperBlockInfo *superblock = SelectSuperBlock();
	if (superblock == NULL)
		return FS_NOFILESYSTEM;

	// Require read permission (mode bit 0)
	if ((handle->Mode & Read) == 0)
		return FS_EACCESS;

	u32 inode = handle->Inode;
	u32 filePosition = handle->FilePosition;
	FileSystemTableEntry *fstEntry = GetFstEntry(superblock, inode);

	// Validate the requested range fits within the cluster-rounded file size
	u32 clusterRoundedSize = (fstEntry->FileSize + (CLUSTER_SIZE - 1)) & CLUSTER_MASK;
	if (clusterRoundedSize < filePosition + length)
		return FS_EINVAL;

	// Navigate FAT chain to the starting cluster
	u32 chainIndex = filePosition >> CLUSTER_SIZE_SHIFT;
	u16 cluster = fstEntry->StartCluster;
	for (u32 i = 0; i < chainIndex; i++)
		cluster = superblock->FatEntries[cluster];

	// Prepare per-file salt data for AES/HMAC operations
	_saltData.Uid = fstEntry->UserId;
	memcpy(_saltData.Filename, fstEntry->Name, sizeof(_saltData.Filename));
	_saltData.Inode = inode;
	_saltData.Miscellaneous = fstEntry->SFFSGeneration;
	memset(_saltData.Unknown, 0, sizeof(_saltData.Unknown));

	s32 ret = IPC_SUCCESS;
	bool isSuccessfulRead = true;
	bool needsSuperblockFlush = false;

	for (u32 bytesRead = 0; bytesRead < length; bytesRead += CLUSTER_SIZE, chainIndex++)
	{
		_saltData.ChainIndex = chainIndex;
		ret = ReadClusters(cluster, 1, ClusterFlagsEncryptDecrypt | ClusterFlagsVerify,
		                   &_saltData, data + bytesRead, NULL);

		if (ret == IPC_SUCCESS)
		{
			// Nominal read – advance to the next cluster in the chain
			cluster = superblock->FatEntries[cluster];
			continue;
		}

		if (ret != FS_EAGAIN)
		{
			isSuccessfulRead = false;
			break;
		}

		// ECC-corrected read: schedule a superblock flush and try to
		// relocate the weakening cluster to a freshly-erased one
		needsSuperblockFlush = true;
		u16 newCluster = AllocateCluster(superblock);
		if (newCluster != SFFSBadNode)
		{
			_saltData.ChainIndex = chainIndex;
			ret = WriteClusters(newCluster, 1, ClusterFlagsEncryptDecrypt | ClusterFlagsVerify,
			                    &_saltData, data + bytesRead, NULL);

			bool writeSucceeded;
			if (ret == IPC_SUCCESS)
				writeSucceeded = true;
			else if (ret == FS_BADBLOCK)
			{
				// New cluster turned out bad; rescue and skip relocation
				ret = RescueFailingBlock(superblock, newCluster);
				if (ret != IPC_SUCCESS)
				{
					isSuccessfulRead = false;
					break;
				}
				writeSucceeded = false;
			}
			else
			{
				isSuccessfulRead = false;
				break;
			}

			// Re-traverse the chain from StartCluster to re-establish the current
			// cluster position after WriteClusters/RescueFailingBlock may have
			// modified FAT entries, making our local `cluster` stale.
			// decrementing by CLUSTER_SIZE each step until zero.
			u16 reTraversedCluster = fstEntry->StartCluster;
			for (u32 byteOffset = bytesRead + handle->FilePosition;
			     byteOffset != 0; byteOffset -= CLUSTER_SIZE)
				reTraversedCluster = superblock->FatEntries[reTraversedCluster];

			if (writeSucceeded)
			{
				// Relocation succeeded – splice new cluster into the chain
				SFFSClusterPair pair = { reTraversedCluster, newCluster };
				ClusterRelocationUpdate(superblock, &pair, 1);
				superblock->FatEntries[newCluster] = superblock->FatEntries[reTraversedCluster];
				superblock->FatEntries[reTraversedCluster] = SFFSFreeNode;
				cluster = newCluster;
			}
		}

		// ECC correction handled; advance the chain and continue
		cluster = superblock->FatEntries[cluster];
		ret = IPC_SUCCESS;
	}

	if (isSuccessfulRead)
		handle->FilePosition = filePosition + length;

	if (needsSuperblockFlush)
		ret = TryWriteSuperblock();

	return ret;
}

s32 WriteFile(FSHandle *handle, const void *data, u32 length)
{
	s32 ret = IPC_SUCCESS;
	u32 bytesWritten = 0;
	u32 clustersWritten = 0;
	bool flushSuperBlock = false;

	// Validate arguments
	if ((s32)handle < 0 || data == NULL || length == 0)
	{
		ret = FS_EINVAL;
		goto cleanup;
	}

	// Round length up to cluster boundary for allocation
	u32 lengthAligned = (length + (CLUSTER_SIZE - 1)) & CLUSTER_MASK;
	SuperBlockInfo *superblock = SelectSuperBlock();
	if (superblock == NULL)
	{
		ret = FS_NOFILESYSTEM;
		goto cleanup;
	}

	// Check if handle has write permission (mode bit 1)
	if ((handle->Mode & 0x02) == 0)
	{
		ret = FS_EACCESS;
		goto cleanup;
	}

	u32 inode = handle->Inode;
	u32 filePosition = handle->FilePosition;
	FileSystemTableEntry *fstEntry = GetFstEntry(superblock, inode);

	// Exit early if not entering write path (not cluster-aligned AND not extending file)
	if (length != lengthAligned && filePosition + length < fstEntry->FileSize)
	{
		ret = FS_EINVAL;
		goto cleanup;
	}

	u32 clusterIndex = filePosition >> CLUSTER_SIZE_SHIFT;

	// Reset write chain tracking
	_writeChain[0] = SFFSLastNode;
	_writeChain[1] = SFFSBadNode;

	// Allocate and write clusters
	while (bytesWritten < lengthAligned)
	{
		// Allocate a new cluster
		u16 newCluster = AllocateCluster(superblock);
		if (newCluster == SFFSBadNode)
		{
			ret = FS_EFBIG;
			goto cleanup;
		}

		// Prepare salt data for encryption (per cluster)
		_saltData.Uid = fstEntry->UserId;
		memcpy(_saltData.Filename, fstEntry->Name, sizeof(_saltData.Filename));
		_saltData.ChainIndex = clusterIndex;
		_saltData.Inode = inode;
		_saltData.Miscellaneous = fstEntry->SFFSGeneration;
		memset(_saltData.Unknown, 0, sizeof(_saltData.Unknown));

		// Write cluster with encryption and HMAC
		ret = WriteClusters(newCluster, 1, ClusterFlagsEncryptDecrypt | ClusterFlagsVerify,
		                    &_saltData, (u8 *)data + bytesWritten, NULL);

		if (ret == FS_BADBLOCK)
		{
			flushSuperBlock = true;
			// Try to rescue any valid data from the failing block
			ret = RescueFailingBlock(superblock, newCluster);

			// Rescue succeeded, retry allocation
			if (ret == IPC_SUCCESS)
				continue;
		}

		// Fatal error - free all allocated clusters in write chain
		if (ret != IPC_SUCCESS)
		{
			while (_writeChain[0] != SFFSLastNode)
			{
				u16 nextCluster = superblock->FatEntries[_writeChain[0]];
				superblock->FatEntries[_writeChain[0]] = SFFSFreeNode;
				_writeChain[0] = nextCluster;
				RemoveUsedClusterStats(1);
			}
			goto cleanup;
		}

		// Update statistics - cluster moved from free to used
		AddUsedClusterStats(1);
		if (bytesWritten == 0)
			_writeChain[0] = newCluster;

		// Mark cluster as EOF initially
		superblock->FatEntries[newCluster] = SFFSLastNode;

		if (_writeChain[1] != SFFSBadNode)
			superblock->FatEntries[_writeChain[1]] = newCluster;

		clusterIndex++;
		clustersWritten++;
		bytesWritten += CLUSTER_SIZE;
		_writeChain[1] = newCluster;
	}

	// Integrate new chain into file's cluster chain
	u16 nextCluster;
	if (filePosition == 0)
	{
		// Replace file's start cluster
		nextCluster = fstEntry->StartCluster;
		fstEntry->StartCluster = _writeChain[0];
	}
	else
	{
		// Find the cluster at filePosition by traversing the chain
		u16 currentCluster = fstEntry->StartCluster;
		for (u32 remainingOffset = filePosition - CLUSTER_SIZE;
		     remainingOffset != 0; remainingOffset -= CLUSTER_SIZE)
			currentCluster = superblock->FatEntries[currentCluster];

		nextCluster = superblock->FatEntries[currentCluster];
		superblock->FatEntries[currentCluster] = _writeChain[0];
	}

	// Free old clusters that were replaced (count limited by number of new clusters)
	u16 currentCluster = nextCluster;
	for (; currentCluster != SFFSLastNode && clustersWritten != 0; clustersWritten--)
	{
		nextCluster = superblock->FatEntries[currentCluster];
		superblock->FatEntries[currentCluster] = SFFSFreeNode;
		RemoveUsedClusterStats(1);
		currentCluster = nextCluster;
	}

	// Link end of new chain to rest of file (after freeing)
	superblock->FatEntries[_writeChain[1]] = nextCluster;

	// Update file position and size
	filePosition = lengthAligned + handle->FilePosition;
	handle->FilePosition = filePosition;

	if (fstEntry->FileSize < filePosition)
	{
		u32 newSize = filePosition - (lengthAligned - length);
		fstEntry->FileSize = newSize;
	}

	handle->ShouldFlushSuperblock = 1;

cleanup:
	// Reset write chain
	_writeChain[0] = SFFSBadNode;
	_writeChain[1] = SFFSBadNode;

	if (flushSuperBlock)
		ret = TryWriteSuperblock();

	return ret;
}

// Enable or disable SFFS version control (generation tracking) for a regular file.
s32 SetFileVersionControl(u32 userId, const char *path, u32 enable)
{
	if (GetPathLength(path) == 0)
		return FS_EINVAL;

	SuperBlockInfo *superblock = SelectSuperBlock();
	if (superblock == NULL)
		return FS_NOFILESYSTEM;

	u32 inode = FindInodeByPath(superblock, path);
	if (inode == SFFSErasedNode)
		return FS_ENOENT;

	FileSystemTableEntry *entry = GetFstEntry(superblock, inode);

	// Only the file owner or root may change version control settings
	if (userId != 0 && entry->UserId != userId)
		return FS_EACCESS;

	// Version control is only meaningful for regular files
	if ((entry->Mode.Value & S_IFMT) != S_IFREG)
		return FS_EINVAL;

	// The file must be empty before toggling version control
	if (entry->FileSize != 0)
		return FS_ENOTEMPTY;

	// If the state doesn't need changing -> error
	if ((entry->SFFSGeneration != 0) == (enable != 0))
		return FS_EINVAL;

	if (enable)
	{
		// Assign the next global generation number to track this file's version
		superblock->Generation++;
		entry->SFFSGeneration = superblock->Generation;
	}
	else
	{
		// Clear the generation field, disabling version tracking
		entry->SFFSGeneration = 0;
	}

	s32 ret = TryWriteSuperblock();

	// After enabling, persist the updated global generation to the IOSC keyring
	if (ret == IPC_SUCCESS && enable)
		ret = OSSetIOSCData(KEYRING_CONST_NAND_GEN, superblock->Generation);

	return ret;
}