/*
	starstruck - a Free Software reimplementation for the Nintendo/BroadOn IOS.
	heaps - management of memory heaps

	Copyright (C) 2021	DacoTaco

# This code is licensed to you under the terms of the GNU GPL, version 2;
# see file COPYING or http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt
*/

#ifndef __HEAPS_H__
#define __HEAPS_H__

#include <types.h>

#define HEAP_INIT_FLAG		0xBABE0000
#define HEAP_INUSE_FLAG		0xBABE0001
#define HEAP_ALIGNED_FLAG	0xBABE0002

typedef struct heap_block
{
	u32 blockFlag;
	struct heap_block* prevBlock;
	u32 size;
	struct heap_block* nextBlock;
} heap_block;

typedef struct
{
	void* heap;
	u32 size;
	heap_block* firstBlock;
} heap_info;

s32 CreateHeap(void *ptr, u32 size);
s32 DestroyHeap(s32 heapid);
void* MallocateOnHeap(s32 heapid, u32 size, u32 alignment);
void* AllocateOnHeap(s32 heapid, u32 size);
s32 FreeOnHeap(s32 heapid, void* ptr);

#endif