/*
	StarStruck - a Free Software reimplementation for the Nintendo/BroadOn IOS.
	Copyright (C) 2025	DacoTaco

# This code is licensed to you under the terms of the GNU GPL, version 2;
# see file COPYING or http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt
*/

#pragma once
#include <types.h>

#include "../handles.h"
#include "filesystem.h"

typedef struct
{
	u8 Data[0x4000];      // 0x0000: Cluster data buffer
	FSHandle *FileHandle; // 0x4000: Associated file handle (NULL = free)
	bool Unallocated;          // 0x4004: Needs write back
	u8 Padding[3];
	u32 DataOffset;     // 0x4008: File position for cached data
	u32 DataSize;       // 0x400C: Size of cached data to write
} ClusterCacheEntry;
CHECK_SIZE(ClusterCacheEntry, 0x4010);
CHECK_OFFSET(ClusterCacheEntry, 0x0000, Data);
CHECK_OFFSET(ClusterCacheEntry, 0x4000, FileHandle);
CHECK_OFFSET(ClusterCacheEntry, 0x4004, Unallocated);
CHECK_OFFSET(ClusterCacheEntry, 0x4008, DataOffset);
CHECK_OFFSET(ClusterCacheEntry, 0x400C, DataSize);

// Cluster cache entry for buffering file data
#define FS_CLUSTER_CACHE_ENTRIES 1
extern ClusterCacheEntry ClusterCacheEntries[FS_CLUSTER_CACHE_ENTRIES];

// Cache operations
ClusterCacheEntry *FindCachedCluster(FSHandle *handle);
ClusterCacheEntry *GetClusterCacheEntry(FSHandle *handle);
s32 FlushCachedCluster(ClusterCacheEntry *cache);
s32 CheckFreeClustersCached(void);
