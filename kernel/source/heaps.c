/*
	starstruck - a Free Software reimplementation for the Nintendo/BroadOn IOS.
	heaps - management of memory heaps

	Copyright (C) 2021	DacoTaco

# This code is licensed to you under the terms of the GNU GPL, version 2;
# see file COPYING or http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt
*/

#include <errno.h>
#include <utils.h>
#include "heaps.h"
#include "gecko.h"

extern u8 __modules_area_start[];
extern u8 __mem2_area_start[];
#define MEM_MODULES_START	((u32) __modules_area_start)
#define MEM_MODULES_END		((u32) __mem2_area_start)

#define ALIGNED_BLOCK_HEADER_SIZE	((sizeof(heap_block) + 0x0F) & -0x10)

#define MAX_HEAP 0x10
static heap_info heaps[MAX_HEAP] MEM2_BSS;

s32 CreateHeap(void *ptr, u32 size)
{
	if(ptr == NULL || size < 0x30)
		return -4;

	u32 heap = (u32)ptr;	
	if(heap < MEM_MODULES_START || heap > MEM_MODULES_END || heap+size > MEM_MODULES_END)
		return -4;
	
	s8 heap_index = 0;
	while(heap_index < MAX_HEAP && heaps[heap_index].heap != NULL)
	{
		heap_index++;
	}
	
	if(heap_index >= MAX_HEAP)
		return -5;

	heap_block* firstBlock = (heap_block*)ptr;
	firstBlock->blockFlag = HEAP_INIT_FLAG;
	firstBlock->size = size - ALIGNED_BLOCK_HEADER_SIZE;
	firstBlock->prevBlock = NULL;
	firstBlock->nextBlock = NULL;
	
	heaps[heap_index].heap = ptr;
	heaps[heap_index].size = size;
	heaps[heap_index].firstBlock = firstBlock;
	
	return heap_index;
}

s32 DestroyHeap(s32 heapid)
{
	if(heapid < 0 || heapid > MAX_HEAP)
		return -4;
	
	if(heaps[heapid].heap != NULL)
	{
		heaps[heapid].heap = NULL;
		heaps[heapid].size = 0;
		heaps[heapid].firstBlock = NULL;
	}
	
	return 0;
}

void* AllocateOnHeap(s32 heapid, u32 size, u32 alignment)
{
	if(	heapid < 0 || heapid > MAX_HEAP || !heaps[heapid].heap || 
		size == 0 || heaps[heapid].size < size || alignment < 0x20)
		return NULL;
		
	if(heaps[heapid].heap == NULL)
		return NULL;
	
	//align size by 0x20
	u32 alignedSize = (size + 0x1F) & -0x20;
	heap_block* currentBlock = heaps[heapid].firstBlock;
	heap_block* blockToAllocate = NULL;
	u32 blockSize = 0;
	u32 alignedOffset = 0;
	
	//find the best fitting block that is free
	while(currentBlock != NULL)
	{
		blockSize = currentBlock->size;
		alignedOffset = (alignment - ((u32)(currentBlock + 1) & (alignment - 1))) & (alignment -1);
		
		if( alignedSize + alignedOffset <= blockSize && (blockToAllocate == NULL || blockSize < blockToAllocate->size))
			blockToAllocate = currentBlock;
		
		currentBlock = currentBlock->nextBlock;
		
		if( blockToAllocate != NULL && blockToAllocate->size == alignedSize + alignedOffset)
			break;
	}
	
	if(blockToAllocate == NULL)
		return NULL;
	
	heap_block* freeBlock = NULL;
	//split up the block if its big enough to do so
	if (alignedSize + alignedOffset + ALIGNED_BLOCK_HEADER_SIZE < blockSize) 
	{
		blockToAllocate->size = alignedSize + alignedOffset + ALIGNED_BLOCK_HEADER_SIZE;		
		freeBlock = (heap_block*)(((u32)blockToAllocate) + blockToAllocate->size);
		freeBlock->blockFlag = HEAP_INIT_FLAG;
		freeBlock->size = blockSize - (blockToAllocate->size);
		freeBlock->prevBlock = blockToAllocate->prevBlock;
		freeBlock->nextBlock = blockToAllocate->nextBlock;
	}
	else
		freeBlock = blockToAllocate->nextBlock;
	
	//remove from heap list
	currentBlock = blockToAllocate->prevBlock;
	if(currentBlock == NULL )
	{
		heaps[heapid].firstBlock = freeBlock;
		currentBlock = freeBlock;
	}
	else
		currentBlock->nextBlock = freeBlock;
	
	if(currentBlock->nextBlock != NULL)
		currentBlock->nextBlock->prevBlock = currentBlock;
	
	//mark block as in use & remove it from our available heap
	blockToAllocate->blockFlag = HEAP_INUSE_FLAG;
	blockToAllocate->nextBlock = NULL;
	blockToAllocate->prevBlock = NULL;
	
	//add the block header infront of the allocated space if needed (because of alignment)
	currentBlock = (heap_block*)(((u32)blockToAllocate) + alignedOffset);
	if(alignedOffset != 0)
	{
		currentBlock->blockFlag = HEAP_ALIGNED_FLAG;
		currentBlock->nextBlock = blockToAllocate;
	}
	
	//get pointer and clear it!
	s8* ptr = (s8*)(currentBlock + 1);
	if(ptr)
		memset8(ptr, 0, size);

	return ptr;
}

int MergeNextBlockIfUnused(heap_block* parentBlock)
{
	if(parentBlock == NULL || parentBlock->nextBlock == NULL)
		return 0;
	
	u32 blockSize = parentBlock->size;
	heap_block * blockToMerge = parentBlock->nextBlock;

	if(blockToMerge != (heap_block*)(((u32)parentBlock) + blockSize))
		return 0;

	//link parent block with the tomerge's next block and vice versa
	heap_block* nextBlock = blockToMerge->nextBlock;
	parentBlock->nextBlock = nextBlock;
	if(nextBlock != NULL)
		nextBlock->prevBlock = parentBlock;
	
	//merge sizes
	parentBlock->size = blockSize + blockToMerge->size;
	return 1;
}

s32 FreeOnHeap(s32 heapid, void* ptr)
{
	//verify incoming parameters & if the heap is in use
	if(heapid < 0 || heapid >= 0x10 || ptr == NULL || heaps[heapid].heap == NULL)
		return -4;
	
	//verify the pointer address
	if( ptr < (heaps[heapid].heap + sizeof(heap_block)) || ptr >= (heaps[heapid].heap + heaps[heapid].size) )
		return -4;
	
	//verify the block that the pointer belongs to
	heap_block* blockToFree = (heap_block*)(ptr-sizeof(heap_block));
	
	if(blockToFree->blockFlag == HEAP_ALIGNED_FLAG)
		blockToFree = blockToFree->nextBlock;
	
	if(blockToFree == NULL || blockToFree->blockFlag != HEAP_INUSE_FLAG)
		return -4;
	
	heap_block* firstBlock = heaps[heapid].firstBlock;
	heap_block* currBlock = firstBlock;
	heap_block* nextBlock = NULL;
	blockToFree->blockFlag = HEAP_INIT_FLAG;
	
	while(currBlock != NULL)
	{
		nextBlock = currBlock->nextBlock;
		if(nextBlock == NULL || blockToFree < nextBlock)
			break;
		
		currBlock = nextBlock;
	}

	//move block to the front
	if (currBlock == NULL || blockToFree <= firstBlock)
	{
		blockToFree->nextBlock = firstBlock;
		heaps[heapid].firstBlock = blockToFree;
		blockToFree->prevBlock = NULL;
	}
	//just place the block infront of the block closest to us
	else
	{
		blockToFree->prevBlock = currBlock;
		blockToFree->nextBlock = currBlock->nextBlock;
		currBlock->nextBlock = blockToFree;
	}
	
	//link the next block if needed
	if(blockToFree->nextBlock != NULL)
		blockToFree->nextBlock->prevBlock = blockToFree;
	
	//merge blocks if we can
	MergeNextBlockIfUnused(blockToFree);
	MergeNextBlockIfUnused(blockToFree->prevBlock);

	return 0;
}