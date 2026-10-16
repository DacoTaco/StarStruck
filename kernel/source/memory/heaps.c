/*
	starstruck - a Free Software reimplementation for the Nintendo/BroadOn IOS.
	heaps - management of memory heaps

	Copyright (C) 2021	DacoTaco

# This code is licensed to you under the terms of the GNU GPL, version 2;
# see file COPYING or http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt
*/

#include <errno.h>
#include <ios/processor.h>
#include <string.h>
#include <ios/gecko.h>
#include <ios/errno.h>

#include "core/defines.h"
#include "memory/heaps.h"
#include "memory/memory.h"
#include "interrupt/irq.h"
#include "scheduler/threads.h"

#define MAX_HEAP 0x10

s32 KernelHeapId = -1;
static HeapInfo heaps[MAX_HEAP];

s32 CreateHeap(void* ptr, u32 size)
{
	u32 irqState = DisableInterrupts();
	s8 heap_index = 0;

#ifdef MIOS
	if (ptr == NULL || ((u32)ptr & 0x1f) != 0 || size < 0x30)
	{
		heap_index = IPC_EINVAL;
		goto restore_and_return;
	}
#else
	if (ptr == NULL || ((u32)ptr & 0x1f) != 0 || size < 0x30 ||
	    CheckMemoryPointer(ptr, size, 4, CurrentThread->ProcessId, 0) < 0)
	{
		heap_index = IPC_EINVAL;
		goto restore_and_return;
	}
#endif

	while (heap_index < MAX_HEAP && heaps[heap_index].Heap != NULL) heap_index++;

	if (heap_index >= MAX_HEAP)
	{
		heap_index = IPC_EMAX;
		goto restore_and_return;
	}

	HeapBlock* firstBlock = (HeapBlock*)ptr;
	firstBlock->BlockState = HeapBlockInit;
	firstBlock->Size = size - sizeof(HeapBlock);
	firstBlock->PreviousBlock = NULL;
	firstBlock->NextBlock = NULL;

	heaps[heap_index].Heap = ptr;
	heaps[heap_index].ProcessId = CurrentThread->ProcessId;
	heaps[heap_index].Size = size;
	heaps[heap_index].FirstBlock = firstBlock;

restore_and_return:
	RestoreInterrupts(irqState);
	return heap_index;
}

s32 DestroyHeap(s32 heapid)
{
	u32 irqState = DisableInterrupts();
	s32 ret = 0;

	if (heapid < 0 || heapid >= MAX_HEAP || heaps[heapid].Heap == NULL)
	{
		ret = IPC_EINVAL;
		goto restore_and_return;
	}

	if (heaps[heapid].ProcessId != CurrentThread->ProcessId)
	{
		ret = IPC_EACCES;
		goto restore_and_return;
	}

	heaps[heapid].Heap = NULL;
	heaps[heapid].Size = 0;
	heaps[heapid].ProcessId = 0;
	heaps[heapid].FirstBlock = NULL;

restore_and_return:
	RestoreInterrupts(irqState);
	return ret;
}

void* AllocateOnHeap(s32 heapid, u32 size)
{
	return MallocateOnHeap(heapid, size, 0x20);
}

void* MallocateOnHeap(s32 heapid, u32 size, u32 alignment)
{
	u32 irqState = DisableInterrupts();
	u32 ret = 0;

	//the alignment has to be a power of 2, an alignment smaller than the minimum gets raised to it (like IOS does)
	if (size == 0 || alignment == 0 || ((alignment - 1) & alignment) != 0 || heapid < 0 ||
	    heapid >= MAX_HEAP || heaps[heapid].Heap == NULL)
		goto restore_and_return;

	if (alignment < 0x20)
		alignment = 0x20;

	//align size by 0x20
	u32 alignedSize = (size + 0x1F) & 0xFFFFFFE0;
	HeapBlock* currentBlock = heaps[heapid].FirstBlock;
	HeapBlock* blockToAllocate = NULL;
	u32 blockSize = 0;
	u32 alignedOffset = 0;

	//find the best fitting block that is free
	while (currentBlock != NULL)
	{
		blockSize = currentBlock->Size;
		alignedOffset = (alignment - ((u32)(currentBlock + 1) & (alignment - 1))) & (alignment - 1);

		//an exact fit is perfect
		if (blockSize == alignedSize && alignedOffset == 0)
		{
			blockToAllocate = currentBlock;
			break;
		}

		if (alignedSize + alignedOffset <= blockSize &&
		    (blockToAllocate == NULL || blockSize < blockToAllocate->Size))
			blockToAllocate = currentBlock;

		currentBlock = currentBlock->NextBlock;
	}

	if (blockToAllocate == NULL)
		goto restore_and_return;

	//set values of selected block
	blockSize = blockToAllocate->Size;
	alignedOffset = (alignment - ((u32)(blockToAllocate + 1) & (alignment - 1))) & (alignment - 1);

	HeapBlock* freeBlock = NULL;
	//split up the block if its big enough to do so
	if (alignedSize + alignedOffset + sizeof(HeapBlock) < blockSize)
	{
		blockToAllocate->Size = alignedSize + alignedOffset;
		freeBlock = (HeapBlock*)(((u32)blockToAllocate) + sizeof(HeapBlock) + blockToAllocate->Size);
		freeBlock->BlockState = HeapBlockInit;
		freeBlock->Size = blockSize - blockToAllocate->Size - sizeof(HeapBlock);
		freeBlock->NextBlock = blockToAllocate->NextBlock;
		if (freeBlock->NextBlock != NULL)
			freeBlock->NextBlock->PreviousBlock = freeBlock;

		blockToAllocate->NextBlock = freeBlock;
	}
	else
		freeBlock = blockToAllocate->NextBlock;

	//remove from heap list
	if (blockToAllocate->PreviousBlock == NULL)
		heaps[heapid].FirstBlock = freeBlock;
	else
		blockToAllocate->PreviousBlock->NextBlock = freeBlock;

	if (blockToAllocate->NextBlock != NULL)
		blockToAllocate->NextBlock->PreviousBlock = blockToAllocate->PreviousBlock;

	//mark block as in use & remove it from our available heap
	blockToAllocate->BlockState = HeapBlockInUse;
	blockToAllocate->NextBlock = NULL;
	blockToAllocate->PreviousBlock = NULL;

	//add the block header infront of the allocated space if needed (because of alignment)
	currentBlock = (HeapBlock*)(((u32)blockToAllocate) + alignedOffset);
	if (alignedOffset != 0)
	{
		currentBlock->BlockState = HeapBlockAligned;
		currentBlock->NextBlock = blockToAllocate;
	}

	//get the pointer
	ret = (u32)(currentBlock + 1);

restore_and_return:
	RestoreInterrupts(irqState);
	return (void*)ret;
}

void MergeNextBlockIfUnused(HeapBlock* parentBlock)
{
	if (parentBlock == NULL || parentBlock->NextBlock == NULL)
		return;

	u32 blockSize = parentBlock->Size;
	HeapBlock* blockToMerge = parentBlock->NextBlock;

	if (blockToMerge != (HeapBlock*)(((u32)parentBlock) + sizeof(HeapBlock) + blockSize))
		return;

	//link parent block with the tomerge's next block and vice versa
	HeapBlock* nextBlock = blockToMerge->NextBlock;
	parentBlock->NextBlock = nextBlock;
	if (nextBlock != NULL)
		nextBlock->PreviousBlock = parentBlock;

	//merge sizes
	parentBlock->Size = blockSize + blockToMerge->Size + sizeof(HeapBlock);
}

s32 FreeOnHeap(s32 heapid, void* ptr)
{
	u32 irqState = DisableInterrupts();
	s32 ret = 0;

	//verify incoming parameters & if the heap is in use
	if (heapid < 0 || heapid >= 0x10 || ptr == NULL || heaps[heapid].Heap == NULL)
	{
		ret = IPC_EINVAL;
		goto restore_and_return;
	}

	//verify the pointer address
	if ((u8*)ptr < ((u8*)heaps[heapid].Heap + sizeof(HeapBlock)) ||
	    (u8*)ptr >= ((u8*)heaps[heapid].Heap + heaps[heapid].Size))
	{
		ret = IPC_EINVAL;
		goto restore_and_return;
	}

	//verify the block that the pointer belongs to
	HeapBlock* blockToFree = (HeapBlock*)((u8*)ptr - sizeof(HeapBlock));

	if (blockToFree->BlockState == HeapBlockAligned)
		blockToFree = blockToFree->NextBlock;

	if (blockToFree == NULL || blockToFree->BlockState != HeapBlockInUse)
	{
		ret = IPC_EINVAL;
		goto restore_and_return;
	}

	HeapBlock* firstBlock = heaps[heapid].FirstBlock;
	HeapBlock* currBlock = firstBlock;
	HeapBlock* nextBlock = NULL;
	blockToFree->BlockState = HeapBlockInit;

	while (currBlock != NULL)
	{
		nextBlock = currBlock->NextBlock;
		if (nextBlock == NULL || blockToFree < nextBlock)
			break;

		currBlock = nextBlock;
	}

	//move block to the front
	if (currBlock == NULL || blockToFree <= firstBlock)
	{
		blockToFree->NextBlock = firstBlock;
		heaps[heapid].FirstBlock = blockToFree;
		blockToFree->PreviousBlock = NULL;
	}
	//just place the block infront of the block closest to us
	else
	{
		blockToFree->PreviousBlock = currBlock;
		blockToFree->NextBlock = currBlock->NextBlock;
		currBlock->NextBlock = blockToFree;
	}

	//link the next block if needed
	if (blockToFree->NextBlock != NULL)
		blockToFree->NextBlock->PreviousBlock = blockToFree;

	//merge blocks if we can
	MergeNextBlockIfUnused(blockToFree);
	MergeNextBlockIfUnused(blockToFree->PreviousBlock);

restore_and_return:
	RestoreInterrupts(irqState);
	return ret;
}