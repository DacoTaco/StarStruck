/*
	StarStruck - a Free Software reimplementation for the Nintendo/BroadOn IOS.
	Copyright (C) 2025	DacoTaco

# This code is licensed to you under the terms of the GNU GPL, version 2;
# see file COPYING or http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt
*/

#include <ios/errno.h>
#include <ios/ipc.h>

#include "../errors.h"
#include "cache.h"
#include "commands.h"

// Cluster cache storage
ClusterCacheEntry ClusterCacheEntries[FS_CLUSTER_CACHE_ENTRIES] ALIGNED(32);

// Find cached cluster entry for a given file handle
ClusterCacheEntry *FindCachedCluster(FSHandle *handle)
{
	for (s32 i = 0; i < FS_CLUSTER_CACHE_ENTRIES; i++)
	{
		if (ClusterCacheEntries[i].FileHandle == handle)
			return &ClusterCacheEntries[i];
	}
	return NULL;
}

// Get a cache entry for the given handle, evicting the oldest entry if all slots are occupied.
ClusterCacheEntry *GetClusterCacheEntry(FSHandle *handle)
{
	// Find a free (unassociated) cache slot
	u32 cacheSlot = 1;
	u32 index;
	for (index = 0; index < FS_CLUSTER_CACHE_ENTRIES; index++)
	{
		if (ClusterCacheEntries[index].FileHandle == NULL)
			break;

		if (!ClusterCacheEntries[index].Unallocated)
			cacheSlot = index;
	}

	if (index == FS_CLUSTER_CACHE_ENTRIES && cacheSlot == FS_CLUSTER_CACHE_ENTRIES)
	{
		FlushCachedCluster(&ClusterCacheEntries[0]);
		FSHandle *evictedHandle = ClusterCacheEntries[0].FileHandle;
		if (evictedHandle != NULL)
			evictedHandle->Error = cacheSlot;
		index = 0;
	}

	//allocate cache entry
	ClusterCacheEntry *entry = &ClusterCacheEntries[index];

	entry->FileHandle = handle;
	entry->Unallocated = false;
	entry->DataOffset = 0;
	entry->DataSize = 0;
	return entry;
}

// Flush cached cluster data to storage
s32 FlushCachedCluster(ClusterCacheEntry *cache)
{
	FSHandle *handle = cache->FileHandle;

	// Early return if no handle or no data needs writing
	if (handle == NULL || !cache->Unallocated)
		return IPC_SUCCESS;

	// Seek to the cached data's file position
	s32 ret = SeekFile(handle, (s32)cache->DataOffset, SeekSet);
	if (ret != IPC_SUCCESS)
		return ret;

	// Write the cached data
	ret = WriteFile(handle, cache->Data, cache->DataSize);
	if (ret != IPC_SUCCESS)
		return ret;

	// Clear the dirty flag
	cache->Unallocated = false;
	return IPC_SUCCESS;
}

// Ensure at least one more free cluster exists than the number of dirty cache entries.
// Called before any write that would require a new cluster allocation, so that cache
// flushes cannot exhaust all available NAND space.
s32 CheckFreeClustersCached(void)
{
	SFFSStatistics stats;
	s32 ret = GetStats(&stats);
	if (ret != IPC_SUCCESS)
		return ret;

	u32 dirtyCount = 0;
	for (s32 i = 0; i < FS_CLUSTER_CACHE_ENTRIES; i++)
	{
		if (ClusterCacheEntries[i].FileHandle != NULL && ClusterCacheEntries[i].Unallocated)
			dirtyCount++;
	}

	if (stats.FreeClusters <= dirtyCount)
		return FS_EFBIG;

	return IPC_SUCCESS;
}