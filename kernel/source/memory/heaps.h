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
	HeapBlockState BlockState;
	u32 Size;
	struct HeapBlock *PreviousBlock;
	struct HeapBlock *NextBlock;
} HeapBlock;
CHECK_SIZE(HeapBlock, 0x10);
CHECK_OFFSET(HeapBlock, 0x00, BlockState);
CHECK_OFFSET(HeapBlock, 0x04, Size);
CHECK_OFFSET(HeapBlock, 0x08, PreviousBlock);
CHECK_OFFSET(HeapBlock, 0x0C, NextBlock);

typedef struct
{
	void *Heap;
	u32 ProcessId;
	u32 Size;
	HeapBlock *FirstBlock;
} HeapInfo;
CHECK_SIZE(HeapInfo, 0x10);
CHECK_OFFSET(HeapInfo, 0x00, Heap);
CHECK_OFFSET(HeapInfo, 0x04, ProcessId);
CHECK_OFFSET(HeapInfo, 0x08, Size);
CHECK_OFFSET(HeapInfo, 0x0C, FirstBlock);

extern s32 KernelHeapId;

s32 CreateHeap(void *ptr, u32 size);
s32 DestroyHeap(s32 heapid);
void *MallocateOnHeap(s32 heapid, u32 size, u32 alignment);
void *AllocateOnHeap(s32 heapid, u32 size);
s32 FreeOnHeap(s32 heapid, void *ptr);

#endif