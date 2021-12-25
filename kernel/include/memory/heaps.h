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

typedef enum HeapBlockState
{
	HeapBlockInit = 0xBABE0000,
	HeapBlockInUse = 0xBABE0001,
	HeapBlockAligned = 0xBABE0002
} HeapBlockState;

typedef struct HeapBlock
{
	HeapBlockState blockState;
	u32 size;
	struct HeapBlock* prevBlock;
	struct HeapBlock* nextBlock;
} HeapBlock;
CHECK_SIZE(HeapBlock, 0x10);
CHECK_OFFSET(HeapBlock, 0x00, blockState);
CHECK_OFFSET(HeapBlock, 0x04, size);
CHECK_OFFSET(HeapBlock, 0x08, prevBlock);
CHECK_OFFSET(HeapBlock, 0x0C, nextBlock);

typedef struct
{
	void* heap;
	u32 size;
	HeapBlock* firstBlock;
} HeapInfo;

s32 CreateHeap(void *ptr, u32 size);
s32 DestroyHeap(s32 heapid);
void* MallocateOnHeap(s32 heapid, u32 size, u32 alignment);
void* AllocateOnHeap(s32 heapid, u32 size);
s32 FreeOnHeap(s32 heapid, void* ptr);

#endif