/*
	StarStruck - a Free Software reimplementation for the Nintendo/BroadOn IOS.
	Copyright (C) 2025	DacoTaco

# This code is licensed to you under the terms of the GNU GPL, version 2;
# see file COPYING or http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt
*/

#pragma once
#include <types.h>

#include "../hardware/nand.h"

// Maximum directory nesting depth for path validation
#define MAX_PATH_DEPTH     8
#define MAX_FILE_PATH      0x40
#define MAX_FILE_SIZE      0x0C

#define MAX_SUPERBLOCK_CNT 0x10

// POSIX-style file type constants
typedef enum __attribute__((__packed__))
{
	S_IFZERO = 0,  // Entry not in use
	S_IFREG = 1,   // Regular file
	S_IFDIR = 2,   // Directory
	S_IFMT = 3     // Mask for file type bits
} FileSystemEntryType;

// Custom st_mode type for SFFS (IOS filesystem mode field)
// Similar to POSIX st_mode but simplified to 8 bits with 2-bit permission fields
typedef union
{
	//DO NOT CHANGE THE ORDER OF THIS STRUCTURE
	//IT WILL BREAK THINGS AS IT IS WRITTEN TO NAND
	struct
	{
		u8 OwnerPermissions : 2;
		u8 GroupPermissions : 2;
		u8 OtherPermissions : 2;
		FileSystemEntryType Type : 2;
	} Fields;
	u8 Value;
} st_mode;
CHECK_SIZE(st_mode, 1);
CHECK_OFFSET(st_mode, 0x00, Value);
CHECK_OFFSET(st_mode, 0x00, Fields);

typedef struct
{
	char Name[12];
	st_mode Mode;
	u8 Attributes;
	u16 StartCluster;
	u16 Sibling;
	u32 FileSize;
	u32 UserId;
	u16 GroupId;
	u32 SFFSGeneration;
} __attribute__((packed)) FileSystemTableEntry;
CHECK_SIZE(FileSystemTableEntry, 0x20);
CHECK_OFFSET(FileSystemTableEntry, 0x00, Name);
CHECK_OFFSET(FileSystemTableEntry, 0x0C, Mode);
CHECK_OFFSET(FileSystemTableEntry, 0x0D, Attributes);
CHECK_OFFSET(FileSystemTableEntry, 0x0E, StartCluster);
CHECK_OFFSET(FileSystemTableEntry, 0x10, Sibling);
CHECK_OFFSET(FileSystemTableEntry, 0x12, FileSize);
CHECK_OFFSET(FileSystemTableEntry, 0x16, UserId);
CHECK_OFFSET(FileSystemTableEntry, 0x1A, GroupId);
CHECK_OFFSET(FileSystemTableEntry, 0x1C, SFFSGeneration);

//the FS superblock is a special section of the fileSystem that contains information about the filesystem
//it contains the file allocation table (FAT) and the file system table (FST) together with a version
#define SuperblockIdentifier 0x53464653 //"SFFS"
typedef struct
{
	u32 Identifier; // 0x53464653 or "SFFS"
	u32 Version; // Version Number, used to pick the newest version
	u32 Generation;
	u16 FatEntries[0x8000]; // File Allocation Table, existing of 0x8000 16 bit identifiers
	FileSystemTableEntry FstEntries[0x17FF]; // File System Table, used to store the file information
	u8 Padding[0x14]; // Padding to 0x40000
} SuperBlockInfo;
CHECK_OFFSET(SuperBlockInfo, 0x00, Identifier);
CHECK_OFFSET(SuperBlockInfo, 0x04, Version);
CHECK_OFFSET(SuperBlockInfo, 0x08, Generation);
CHECK_OFFSET(SuperBlockInfo, 0x0C, FatEntries);
CHECK_OFFSET(SuperBlockInfo, 0x1000C, FstEntries);
CHECK_OFFSET(SuperBlockInfo, 0x3FFEC, Padding);
CHECK_SIZE(SuperBlockInfo, 0x40000);

// SFFS filesystem statistics
typedef struct
{
	u32 ClusterSize;
	u32 FreeClusters;
	u32 UsedClusters;
	u32 BadClusters;
	u32 ReservedClusters;
	u32 FreeInodes;
	u32 UsedInodes;
} SFFSStatistics;
CHECK_SIZE(SFFSStatistics, 0x1C);
CHECK_OFFSET(SFFSStatistics, 0x00, ClusterSize);
CHECK_OFFSET(SFFSStatistics, 0x04, FreeClusters);
CHECK_OFFSET(SFFSStatistics, 0x08, UsedClusters);
CHECK_OFFSET(SFFSStatistics, 0x0C, BadClusters);
CHECK_OFFSET(SFFSStatistics, 0x10, ReservedClusters);
CHECK_OFFSET(SFFSStatistics, 0x14, FreeInodes);
CHECK_OFFSET(SFFSStatistics, 0x18, UsedInodes);

extern SFFSStatistics _sffStats;
extern u32 _fileSystemMetadataSizeShift;

static inline void RemoveUsedInodeStats(u32 inodeCount)
{
	_sffStats.FreeInodes += inodeCount;
	_sffStats.UsedInodes -= inodeCount;
}

static inline void AddUsedInodeStats(u32 inodeCount)
{
	_sffStats.FreeInodes -= inodeCount;
	_sffStats.UsedInodes += inodeCount;
}

static inline void RemoveUsedClusterStats(u32 clusterCount)
{
	_sffStats.FreeClusters += clusterCount;
	_sffStats.UsedClusters -= clusterCount;
}

static inline void AddUsedClusterStats(u32 clusterCount)
{
	_sffStats.FreeClusters -= clusterCount;
	_sffStats.UsedClusters += clusterCount;
}

static inline void AddBadBlockStats(u32 clusterCount)
{
	_sffStats.ReservedClusters -= clusterCount;
	_sffStats.BadClusters += clusterCount;
}

static inline u32 GetSuperBlockSize()
{
	return (1 << (_fileSystemMetadataSizeShift & 0xFF)) / (u32)MAX_SUPERBLOCK_CNT;
}

static inline u32 GetFatArraySize()
{
	return 2 << ((SelectedNandSizeInfo.NandSizeBitShift - CLUSTER_SIZE_SHIFT) & 0xFF);
}

static inline FileSystemTableEntry *GetFstEntry(SuperBlockInfo *superblock, u32 inode)
{
	// FAT size = 2 << (NandSizeBitShift - CLUSTER_SIZE_SHIFT) bytes (when using 512MB nand)
	// FST base = FAT base + FAT size
	// Each FST entry is 0x20 (32) bytes

	//its basically comes down to `return &superblock->FstEntries[inode];` but dynamically calculating the fst base
	FileSystemTableEntry *fstBase =
	    (FileSystemTableEntry *)((u8 *)superblock->FatEntries + GetFatArraySize());
	return &fstBase[inode];
}

// Helper to calculate total number of FST entries
static inline u32 GetFstEntryCount(void)
{
	return (GetSuperBlockSize() - GetFatArraySize() - offsetof(SuperBlockInfo, FatEntries)) /
	       sizeof(FileSystemTableEntry);
}

// Path utilities
u32 GetPathLength(const char *path);
s32 SplitPath(const char *path, char *directory, char *fileName);

s32 TryWriteSuperblock(void);
s32 InitSuperblockInfo(bool clearInfo);
SuperBlockInfo *SelectSuperBlock(void);
s32 InitializeSFFS(s32 mode);

s32 InitializeFstEntries(SuperBlockInfo *superblock, bool *flushNeeded);
s32 StatSuperblock(SuperBlockInfo *superblock);
s32 GetStats(SFFSStatistics *stats);
s32 GetPathUsage(const char *path, u32 *clusters, u32 *inodes);

// Retrieve file attributes for the specified path.
s32 GetAttributes(u32 userId, u16 groupId, const char *path, u32 *userIdOut,
                  u16 *groupIdOut, u8 *attributesOut, u8 *ownerPermOut,
                  u8 *groupPermOut, u8 *otherPermOut);

//Set Attributes of a file or directory
s32 SetAttributes(u32 callerUserId, const char *path, u32 userId, u16 groupId, u8 attributes,
                  u8 ownerPermissions, u8 groupPermissions, u8 otherPermissions);

// Create a fresh superblock in memory and initialize FAT/FST structures
SuperBlockInfo *CreateSuperBlock(void);
s32 MarkBlocksReserved(SuperBlockInfo *superblock);
