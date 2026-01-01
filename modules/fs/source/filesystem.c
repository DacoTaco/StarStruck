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

#include "filesystem.h"
#include "commands.h"
#include "inode.h"
#include "nand.h"
#include "cluster.h"
#include "errors.h"
#include "handles.h"

extern u32 _superblockOffset;
extern u32 _fileSystemMetadataSizeShift;
extern u32 _fileSystemDataSize;

// Superblock globals
bool _superblockInitialized = false;
SuperBlockInfo _superblockStorage __attribute__((aligned(64)));
SuperBlockInfo *_selectedSuperBlock = NULL;
u32 _selectedSuperblockIndex;
SaltData _saltData __attribute__((aligned(64)));

// Statistics global
SFFSStatistics _sffStats;

s32 TryWriteSuperblock()
{
	if (!_superblockInitialized)
		return FS_ENOENT;

	s32 ret = IPC_SUCCESS;
	bool successful = false;
	u32 superBlockIndex;

	_selectedSuperBlock->Version++;

 //find free superblock, which wraps around after 16 attempts
	//with each loop we increase superBlockIndex and update _selectedSuperblockIndex
	for (superBlockIndex = 0; !successful && superBlockIndex < 0x10; superBlockIndex++,
	    _selectedSuperblockIndex = (_selectedSuperblockIndex + 1) & 0x0F)
	{
		bool allClustersReserved = true;

		// Calculate shift difference between metadata size and block size
		const u32 shiftDiff =
		    _fileSystemMetadataSizeShift - SelectedNandSizeInfo.BlockSizeBitShift;

		// Calculate the offset within the superblock region (mask with region size - 1)
		u32 regionOffset = _selectedSuperblockIndex &
		                   (u32)((1 << (shiftDiff & 0xFF)) - 1);

		// Calculate FAT cluster offset based on whether superblock spans multiple blocks
		u32 fatClusterOffset =
		    (SelectedNandSizeInfo.BlockSizeBitShift < _fileSystemMetadataSizeShift - 4) ?
		        (regionOffset << ((shiftDiff - 4) & 0xFF))
		            << ((SelectedNandSizeInfo.BlockSizeBitShift - CLUSTER_SIZE_SHIFT) & 0xFF) :
		        regionOffset
		            << ((SelectedNandSizeInfo.BlockSizeBitShift - CLUSTER_SIZE_SHIFT) & 0xFF);

		// Calculate superblock size in clusters (shift from 256KB base)
		u32 superblockSizeShift = (_fileSystemMetadataSizeShift - 0x12) & 0xFF;

		// Calculate the starting cluster index for this superblock slot
		u32 superblockClusterIndex =
		    ((_superblockOffset + _fileSystemDataSize) >> CLUSTER_SIZE_SHIFT) +
		    fatClusterOffset +
		    ((_selectedSuperblockIndex >> (shiftDiff & 0xFF)) << superblockSizeShift);

		// Number of clusters per block
		u32 clustersPerBlock =
		    1 << ((SelectedNandSizeInfo.BlockSizeBitShift - CLUSTER_SIZE_SHIFT) & 0xFF);

		// Total clusters in superblock
		u32 superblockClusterCount = 1 << superblockSizeShift;

		// Check if all FAT entries for this superblock region are reserved
		for (u32 clusterOffset = 0; clusterOffset < superblockClusterCount;
		     clusterOffset += clustersPerBlock)
		{
			if (_selectedSuperBlock->FatEntries[superblockClusterIndex + clusterOffset] != SFFSReservedNode)
			{
				allClustersReserved = false;
				break;
			}
		}

		if (!allClustersReserved)
			continue;

		// Store cluster index in salt data for HMAC calculation
		_saltData.ChainIndex = superblockClusterIndex;

		// Write the superblock with HMAC signature
		ret = WriteClusters((u16)superblockClusterIndex, superblockClusterCount, ClusterFlagsVerify,
		                    &_saltData, (u8 *)_selectedSuperBlock, NULL);

		if (ret == IPC_SUCCESS)
		{
			successful = true;
			continue;
		}

		if (ret != FS_BADBLOCK)
			continue;

		// Mark all clusters in the bad block(s) as bad
		for (u32 clusterOffset = 0; clusterOffset < superblockClusterCount;
		     clusterOffset += clustersPerBlock)
		{
			// Get the block-aligned cluster index
			u32 blockBaseCluster = (superblockClusterIndex + clusterOffset) &
			                       ~(clustersPerBlock - 1);

			// Mark all clusters in this block as bad
			for (u32 i = 0; i < clustersPerBlock; i++)
			{
				_selectedSuperBlock->FatEntries[blockBaseCluster + i] = SFFSBadNode;
			}
		}

		// Increment version to try again with updated FAT
		_selectedSuperBlock->Version++;
	}

	if (superBlockIndex >= 0x10)
		return FS_ECORRUPT;

	return ret;
}

s32 InitSuperblockInfo(bool clearInfo)
{
	if (!clearInfo)
	{
		_selectedSuperBlock = NULL;
		return IPC_SUCCESS;
	}

	_superblockOffset = 0x100000;
	switch (SelectedNandSizeInfo.NandSizeBitShift)
	{
		case 0x1A:
			_fileSystemMetadataSizeShift = 0x12;
			break;
		case 0x1B:
		case 0x1C:
			_fileSystemMetadataSizeShift = 0x14;
			break;
		case 0x1D:
		case 0x1E:
			_fileSystemMetadataSizeShift = 0x16;
			break;
			//invalid or unsupported nand size
		default:
			return FS_NOTIMPL;
	}

	//IOS does this but i don't understand why. if its > it means the shift is > 0x16, and thats our maximum value lol
	if ((1 << ((_fileSystemMetadataSizeShift - 4) & 0xFF)) > 0x40000)
		return FS_NOTIMPL;

	//calculate size of the nand that can contain data: nandsize - 1MB - superblock size
	_fileSystemDataSize =
	    (u32)((1 << (u8)(SelectedNandSizeInfo.NandSizeBitShift & 0xFF)) - 0x100000) -
	    (1 << _fileSystemMetadataSizeShift);
	//setup superblock storage and salt data
	_selectedSuperBlock = &_superblockStorage;
	memset(&_saltData, 0, sizeof(SaltData));

	return IPC_SUCCESS;
}

//doing the superblock selection
SuperBlockInfo *SelectSuperBlock()
{
	SuperBlockInfo *returnedSuperblock = NULL;
	s32 superBlockIndex;
	u32 superBlockGeneration;
	u32 lowestFailedGeneration = 0xFFFFFFFF;
	bool rewriteSuperblock = false;
	s32 ret;
	u32 sffsGeneration;

	if (_selectedSuperBlock == NULL)
		goto _selectSuperBlockEnd;

	if (_superblockInitialized)
	{
		returnedSuperblock = _selectedSuperBlock;
		goto _selectSuperBlockEnd;
	}

	ret = OSGetIOSCData(KEYRING_CONST_NAND_GEN, (u32 *)&sffsGeneration);
	if (ret != IPC_SUCCESS)
		goto _selectSuperBlockEnd;

	while (true)
	{
		superBlockGeneration = 0;
		superBlockIndex = -1;

		for (u32 index = 0; index < 0x10; index++)
		{
			const u32 shiftDiff = _fileSystemMetadataSizeShift -
			                      SelectedNandSizeInfo.BlockSizeBitShift;

			// Calculate the offset within the superblock region
			u32 regionOffset = (u32)((1 << (shiftDiff & 0xFF)) - 1) & index;

			// If a superblock is bigger than 1 block...
			u16 fatClusterOffset =
			    (SelectedNandSizeInfo.BlockSizeBitShift < _fileSystemMetadataSizeShift - 4) ?
			        (u16)((regionOffset << ((shiftDiff - 4) & 0xFF))
			              << ((SelectedNandSizeInfo.BlockSizeBitShift - CLUSTER_SIZE_SHIFT) & 0xFF)) :
			        (u16)(regionOffset << ((SelectedNandSizeInfo.BlockSizeBitShift - CLUSTER_SIZE_SHIFT) &
			                               0xFF));

			// Calculate the cluster index for this superblock candidate
			u32 clusterIndex =
			    (u32)((_superblockOffset + _fileSystemDataSize) >> CLUSTER_SIZE_SHIFT) +
			    fatClusterOffset +
			    (u32)((index >> (shiftDiff & 0xFF))
			          << ((_fileSystemMetadataSizeShift - 0x12) & 0xFF));

			// Read the first cluster of the superblock (unencrypted, no HMAC)
			ret = ReadClusters((u16)clusterIndex, 1, ClusterFlagsNone, NULL,
			                   (u8 *)_selectedSuperBlock, NULL);

			// Check if read was successful (or had correctable ECC error)
			// and validate superblock identifier and generation
			if ((ret == IPC_SUCCESS || ret == FS_EAGAIN) &&
			    _selectedSuperBlock->Identifier == SuperblockIdentifier &&
			    sffsGeneration <= _selectedSuperBlock->Unknown + 1)
			{
				// Update generation if needed
				if (_selectedSuperBlock->Unknown < sffsGeneration)
					_selectedSuperBlock->Unknown = sffsGeneration;

				u32 version = _selectedSuperBlock->Version;
				// Select superblock with highest version that hasn't failed HMAC before
				if (version < lowestFailedGeneration && superBlockGeneration <= version)
				{
					superBlockIndex = (s32)index;
					superBlockGeneration = version;
				}
			}
		}

		if (superBlockIndex < 0)
			goto _selectSuperBlockEnd;

		// Now read the full superblock with HMAC verification
		const u32 shiftDiff =
		    _fileSystemMetadataSizeShift - SelectedNandSizeInfo.BlockSizeBitShift;
		u32 regionOffset = (u32)((1 << (shiftDiff & 0xFF)) - 1) & (u32)superBlockIndex;

		u32 fatClusterOffset =
		    (SelectedNandSizeInfo.BlockSizeBitShift < _fileSystemMetadataSizeShift - 4) ?
		        ((regionOffset << ((shiftDiff - 4) & 0xFF))
		         << ((SelectedNandSizeInfo.BlockSizeBitShift - CLUSTER_SIZE_SHIFT) & 0xFF)) :
		        (regionOffset
		         << ((SelectedNandSizeInfo.BlockSizeBitShift - CLUSTER_SIZE_SHIFT) & 0xFF));

		u32 superblockSizeShift = _fileSystemMetadataSizeShift - 0x12;
		u32 clusterIndex =
		    ((_superblockOffset + _fileSystemDataSize) >> CLUSTER_SIZE_SHIFT) +
		    fatClusterOffset +
		    (((u32)superBlockIndex >> (shiftDiff & 0xFF)) << (superblockSizeShift & 0xFF));

		// Store cluster index in salt data for HMAC calculation
		_saltData.ChainIndex = clusterIndex;

		// Read full superblock with HMAC verification
		ret = ReadClusters((u16)clusterIndex, 1 << (superblockSizeShift & 0xFF), ClusterFlagsVerify,
		                   &_saltData, (u8 *)_selectedSuperBlock, NULL);

		if (ret == IPC_SUCCESS)
			goto _selectSuperBlockWithoutRewrite;

		if (ret == FS_EAGAIN)
			break;

		// HMAC verification failed, mark this generation as failed and retry
		lowestFailedGeneration = superBlockGeneration;
	}

	rewriteSuperblock = true;
_selectSuperBlockWithoutRewrite:
	returnedSuperblock = _selectedSuperBlock;
	_selectedSuperblockIndex = (superBlockIndex + 1) & 0x0F;

_selectSuperBlockEnd:
	if (returnedSuperblock != NULL)
		_superblockInitialized = true;

	if (rewriteSuperblock && _superblockInitialized)
		TryWriteSuperblock();

	return returnedSuperblock;
}

s32 InitializeSFFS(s32 mode)
{
	s32 ret = 0;
	bool flushSuperBlock = false;
	SuperBlockInfo *fetchedSuperBlock;

	if (mode == 0)
	{
		InitSuperblockInfo(false);
		SelectNandSize(false);
		goto _initSSFSEnd;
	}

	ret = SelectNandSize(true);
	if (ret != IPC_SUCCESS)
		goto _initSSFSEnd;

	ret = InitSuperblockInfo(true);
	if (ret != IPC_SUCCESS)
		goto _initSSFSCleanupNandSize;

	fetchedSuperBlock = SelectSuperBlock();
	if (fetchedSuperBlock == NULL)
	{
		ret = FS_NOFILESYSTEM;
		goto _initSSFSCleanupSuperblock;
	}

	// Initialize FST entries and check for orphaned files
	ret = InitializeFstEntries(fetchedSuperBlock, &flushSuperBlock);
	if (ret != IPC_SUCCESS)
		goto _initSSFSCleanupSuperblock;

	// Clear reserved markers in the boot area FAT entries
	// The first N clusters (where N = clusters in 1MB boot area) may have
	// stale reserved markers that need to be freed
	u32 bootAreaClusters =
	    1 << ((SelectedNandSizeInfo.NandSizeBitShift - CLUSTER_SIZE_SHIFT) & 0xFF);
	for (u32 i = 0; i < bootAreaClusters; i++)
	{
		if (fetchedSuperBlock->FatEntries[i] == SFFSReservedNode)
			fetchedSuperBlock->FatEntries[i] = SFFSFreeNode;
	}

	// Clear all file handles
	for (u32 i = 0; i < FS_MAX_FILE_HANDLES; i++)
	{
		_fileHandles[i].InUse = 0;
	}

	// Calculate filesystem statistics
	ret = StatSuperblock(fetchedSuperBlock);
	if (ret == IPC_SUCCESS)
		goto _initSSFSEnd;

_initSSFSCleanupSuperblock:
	InitSuperblockInfo(false);
_initSSFSCleanupNandSize:
	SelectNandSize(false);

_initSSFSEnd:
	if (flushSuperBlock)
		ret = TryWriteSuperblock();

	return ret;
}

// Initialize FST entries by marking reachable files and clearing unreachable ones
// This cleans up any files that were marked as pending delete but not fully removed
s32 InitializeFstEntries(SuperBlockInfo *superblock, bool *flushNeeded)
{
	*flushNeeded = false;

	// Mark all reachable inodes starting from root (inode 0)
	s32 ret = ProcessInodeAction(superblock, 0, MarkInodePending);
	if (ret != IPC_SUCCESS)
		return ret;

	// Calculate number of FST entries based on NAND size
	u32 fstEntryCount = GetFstEntryCount();

	// Iterate through all FST entries (skip inode 0 which is root)
	for (u32 inode = 1; inode < fstEntryCount; inode++)
	{
		FileSystemTableEntry *entry = GetFstEntry(superblock, inode);

		// Skip unused entries
		if (entry->Mode.Fields.Type == S_IFZERO)
			continue;

		// Check if pending delete bit is set (bit 31 of FileSize)
		if ((entry->FileSize & 0x80000000) == 0)
			continue;

		// Clear the pending delete bit by shifting left then right
		// This effectively clears bit 31
		entry->FileSize = entry->FileSize & 0x7FFFFFFF;
	}

	return ret;
}

// Calculate filesystem statistics by scanning FAT and FST entries
s32 StatSuperblock(SuperBlockInfo *superblock)
{
	// Initialize statistics
	_sffStats.ClusterSize = 0x4000;
	_sffStats.FreeClusters = 0;
	_sffStats.UsedClusters = 0;
	_sffStats.BadClusters = 0;
	_sffStats.ReservedClusters = 0;
	_sffStats.FreeInodes = 0;
	_sffStats.UsedInodes = 0;

	// Calculate data cluster range
	u32 dataClusterStart = _superblockOffset >> CLUSTER_SIZE_SHIFT;
	u32 dataClusterCount = _fileSystemDataSize >> CLUSTER_SIZE_SHIFT;

	// Scan FAT entries in the data region
	for (u32 i = dataClusterStart; i < dataClusterStart + dataClusterCount; i++)
	{
		u16 fatEntry = superblock->FatEntries[i];

		if (fatEntry == SFFSBadNode)
		{
			_sffStats.BadClusters++;
		}
		else if (fatEntry == SFFSFreeNode || fatEntry == SFFSErasedNode)
		{
			_sffStats.FreeClusters++;
		}
		else if (fatEntry == SFFSReservedNode)
		{
			_sffStats.ReservedClusters++;
		}
		else
		{
			// Any other value (including SFFSLastNode or chain pointers) means used
			_sffStats.UsedClusters++;
		}
	}

	// Calculate number of FST entries
	u32 fstEntryCount = GetFstEntryCount();

	// Scan FST entries to count used/free inodes
	for (u32 inode = 0; inode < fstEntryCount; inode++)
	{
		FileSystemTableEntry *entry = GetFstEntry(superblock, inode);

		if (entry->Mode.Fields.Type == S_IFZERO)
		{
			_sffStats.FreeInodes++;
		}
		else
		{
			_sffStats.UsedInodes++;
		}
	}

	return IPC_SUCCESS;
}

s32 GetPathUsage(const char *path, u32 *clusters, u32 *inodes)
{
	// Validate arguments
	if (path == NULL || clusters == NULL || inodes == NULL)
		return FS_EINVAL;

	// Path must start with '/' and not be too long
	if (*path != '/' || strnlen(path, MAX_FILE_PATH) == MAX_FILE_PATH)
		return FS_EINVAL;

	// Get the superblock
	SuperBlockInfo *superblock = SelectSuperBlock();
	if (superblock == NULL)
		return FS_NOFILESYSTEM;

	// Find the inode for the path
	u32 inode = FindInodeByPath(superblock, path);
	if (inode == SFFSErasedNode)
		return FS_ENOENT;

	// Verify it's a directory
	FileSystemTableEntry *entry = GetFstEntry(superblock, inode);
	if (entry->Mode.Fields.Type != S_IFDIR)
		return FS_EINVAL;

	// Get cluster count
	s32 ret = ProcessInodeAction(superblock, inode, GetUsedClusters);
	if (ret < 0)
		return FS_NOTIMPL;
	*clusters = (u32)ret;

	// Get inode count
	ret = ProcessInodeAction(superblock, inode, GetUsedInodes);
	if (ret < 0)
		return FS_NOTIMPL;
	*inodes = (u32)ret;

	return IPC_SUCCESS;
}
