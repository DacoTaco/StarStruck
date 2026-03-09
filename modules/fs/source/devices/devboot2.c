/*
	StarStruck - a Free Software reimplementation for the Nintendo/BroadOn IOS.
	Copyright (C) 2025	DacoTaco

# This code is licensed to you under the terms of the GNU GPL, version 2;
# see file COPYING or http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt
*/

#include <string.h>
#include <ios/errno.h>
#include <ios/ipc.h>

#include "../hardware/nand_helpers.h"
#include "../errors.h"
#include "devboot2.h"

typedef enum
{
	IOCTL_WRITE_BLOCKMAP = 1,
	IOCTL_WRITE_BLOCKMAP_COPY = 2,
	IOCTL_WRITE_BOOT2_COPY = 3
} Boot2IoctlCommands;

Boot2Handle _boot2Handle = { 0 };

bool IsBoot2FileHandle(s32 fd)
{
	return (fd == (s32)&_boot2Handle.BlockMap);
}

s32 CloseBoot2FileHandle(void)
{
	_boot2Handle.IsOpen = false;
	return IPC_SUCCESS;
}

s32 OpenBoot2FileHandle(void)
{
	u8 blockSignature[8];
	u32 generation = 0;

 //abuse the pagebuffer to contain the copies of maps.
	Boot2BlockMap *maps = (Boot2BlockMap *)_boot2Handle.PageBuffer;

	if (_boot2Handle.IsOpen)
		return NAND_RESULT_BUSY;

	memcpy(blockSignature, &(u64){ BOOT2_BLOCKMAP_SIGNATURE }, 8);

	const u32 pagesPerBlock = GetPagesPerBlock();
	u32 pageNumber = pagesPerBlock * 2 - 1;
	u32 maxPage = 0x100000 >> SelectedNandSizeInfo.PageSizeBitShift;

	while (pageNumber < maxPage)
	{
		s32 ret = ReadNandPage(pageNumber, _boot2Handle.PageBuffer, NULL, false);
		if (ret != IPC_SUCCESS)
			return ret;

		Boot2BlockMap *validMap = NULL;

  // Check maps[0] - if it has valid signature and matches maps[1]
		if (memcmp(&maps[0], blockSignature, 8) == 0)
		{
			if (memcmp(&maps[0], &maps[1], 0x4C) == 0 ||
			    memcmp(&maps[0], &maps[2], 0x4C) == 0)
				validMap = &maps[0];
		}

  // Check maps[1] - if it has valid signature and matches maps[2]
		if (validMap == NULL && memcmp(&maps[1], blockSignature, 8) == 0)
		{
			if (memcmp(&maps[1], &maps[2], 0x4C) == 0)
				validMap = &maps[1];
		}

  // If we found a valid map with newer generation, use it
		if (validMap != NULL && validMap->Generation > generation)
		{
			memcpy(&_boot2Handle.BlockMap, validMap, 0x4C);
			generation = validMap->Generation;
		}

		pageNumber += pagesPerBlock;
	}

 // Initialize or update blockmap
	if (generation < 2)
	{
  // Set generation to 2
		_boot2Handle.BlockMap.Generation = 2;

  // If no valid blockmap found, initialize signature
		if (generation == 0)
			memcpy(&_boot2Handle.BlockMap.Signature, blockSignature, 8);

  // Mark block 0 as bad (reserved)
		_boot2Handle.BlockMap.Blocks[0] = 1;

  // Scan blocks 1+ to check which are good/bad, within first 1MB
		u32 blocksInFirstMB = 0x100000 >> SelectedNandSizeInfo.BlockSizeBitShift;
		for (u32 blockIdx = 1; blockIdx < blocksInFirstMB; blockIdx++)
		{
			u32 blockPage = blockIdx << (SelectedNandSizeInfo.BlockSizeBitShift - CLUSTER_SIZE_SHIFT);
			s32 checkRet = CheckClusterBlocks(blockPage);
			_boot2Handle.BlockMap.Blocks[blockIdx] = (checkRet == 0) ? 0 : 1;
		}

		for (u32 blockIdx = blocksInFirstMB; blockIdx < BOOT2_BLOCKS_COUNT; blockIdx++)
			_boot2Handle.BlockMap.Blocks[blockIdx] = 1;
	}
	else
	{
		_boot2Handle.BlockMap.Generation++;
	}

 // Initialize handle state
	_boot2Handle.PageIndex = 0;
	_boot2Handle.PageCursor = 0;
	_boot2Handle.IsOpen = true;
	_boot2Handle.Finished = false;

 // Return pointer to blockmap as file descriptor
	return (s32)&_boot2Handle.BlockMap;
}

static s32 EraseNextBoot2Block(void)
{
	const u32 blockSizeShift =
	    (SelectedNandSizeInfo.BlockSizeBitShift - SelectedNandSizeInfo.PageSizeBitShift) & 0xFF;
	u32 block = _boot2Handle.PageIndex >> blockSizeShift;
	bool found = false;

	while (true)
	{
		if (block > 0x3F)
		{
			if (!found)
				return IPC_EMAX;
			block = _boot2Handle.PageIndex >> blockSizeShift;
		}

		if (_boot2Handle.BlockMap.Blocks[block] == 0)
		{
			_boot2Handle.PageIndex = block << blockSizeShift;
			found = true;

			if (DeleteCluster(block) == IPC_SUCCESS)
				return IPC_SUCCESS;

			_boot2Handle.BlockMap.Blocks[block] = 1;
		}

		block++;
	}
}

static s32 WriteBoot2Page(bool writeEcc)
{
	const u32 pagesPerBlock = GetPagesPerBlock();
	u32 pageSize = GetPageSize();
	u32 remaining = pageSize - _boot2Handle.PageCursor;
	u32 oldBlockStart;
	s32 ret;

	if (remaining > 0)
		memset(_boot2Handle.PageBuffer + _boot2Handle.PageCursor, 0, remaining);

	if ((_boot2Handle.PageIndex & (pagesPerBlock - 1)) == 0)
	{
		ret = EraseNextBoot2Block();
		if (ret != IPC_SUCCESS)
			return ret;
	}

	while (true)
	{
		ret = WriteNandPage(_boot2Handle.PageIndex, _boot2Handle.PageBuffer,
		                    NULL, 0, writeEcc);
		if (ret == IPC_SUCCESS)
			break;

		oldBlockStart = -pagesPerBlock & _boot2Handle.PageIndex;
		_boot2Handle.BlockMap.Blocks[GetBlockIndexFromPage(_boot2Handle.PageIndex)] = 1;

		ret = EraseNextBoot2Block();
		if (ret != IPC_SUCCESS)
			return ret;

		for (; oldBlockStart < _boot2Handle.PageIndex; oldBlockStart++)
		{
			ret = CopyPage(oldBlockStart, _boot2Handle.PageIndex);
			if (ret != IPC_SUCCESS)
				return ret;
			_boot2Handle.PageIndex++;
		}
	}

	_boot2Handle.PageCursor = 0;
	_boot2Handle.PageIndex++;
	return IPC_SUCCESS;
}

static s32 FinishBoot2Write(void)
{
 // If already finished, return EINVAL
	if (_boot2Handle.Finished)
		return IPC_EINVAL;

	const u32 pagesPerBlock = GetPagesPerBlock();
	s32 ret = IPC_SUCCESS;

  // If there is a partial page, flush it
	if (_boot2Handle.PageCursor != 0)
	{
		ret = WriteBoot2Page(true);
		if (ret != IPC_SUCCESS)
			return ret;
	}

 //nothing written yet
	if (_boot2Handle.PageIndex == 0)
		return IPC_SUCCESS;

	if (((_boot2Handle.PageIndex & (pagesPerBlock - 1)) == 0))
	{
		ret = EraseNextBoot2Block();
		if (ret != IPC_SUCCESS)
			return ret;
	}
  // Always set PageIndex to end of block (IOS 58 logic)
	_boot2Handle.PageIndex =
	    (_boot2Handle.PageIndex & ~(pagesPerBlock - 1)) + pagesPerBlock - 1;

 // Write blockmap in triplicate at last page (0x4C bytes each, total 0xE4)
	memcpy(_boot2Handle.PageBuffer, &_boot2Handle.BlockMap, sizeof(Boot2BlockMap));
	memcpy(_boot2Handle.PageBuffer + sizeof(Boot2BlockMap),
	       &_boot2Handle.BlockMap, sizeof(Boot2BlockMap));
	memcpy(_boot2Handle.PageBuffer + (sizeof(Boot2BlockMap) * 2),
	       &_boot2Handle.BlockMap, sizeof(Boot2BlockMap));
	_boot2Handle.PageCursor = sizeof(Boot2BlockMap) * 3;
	ret = WriteBoot2Page(false);
	if (ret == IPC_SUCCESS)
		_boot2Handle.Finished = true;

	return ret;
}

static s32 WriteBoot2Copy(u32 size)
{
	if (_boot2Handle.PageCursor != 0)
		return IPC_EINVAL;

 // Calculate NAND geometry
	const u32 pagesPerBlock = GetPagesPerBlock();
	const u32 pageSize = GetPageSize();
	const u32 blockSize = pagesPerBlock * pageSize;
	u32 minimumPage = 0;
	s32 ret = IPC_SUCCESS;

 // Find the last block for the main copy, skipping bad and used blocks
	u32 index = 0;
	for (; index < BOOT2_BLOCKS_COUNT; index++)
	{
		if (_boot2Handle.BlockMap.Blocks[index] != 0)
			continue;

		if (size < blockSize)
		{
			minimumPage = (index * pagesPerBlock) +
			              (((size + pageSize) - 1) >>
			               (SelectedNandSizeInfo.PageSizeBitShift & 0xFF)) -
			              1;
			break;
		}
		size -= blockSize;
	}

	if (index == BOOT2_BLOCKS_COUNT)
		return IPC_EMAX;

	u32 pageIndex = (((minimumPage + 1) % pagesPerBlock) == 0) ?
	                    minimumPage + pagesPerBlock :
	                    ((-pagesPerBlock & minimumPage) + pagesPerBlock) - 1;

	const u32 blockIndex = GetBlockIndexFromPage(pageIndex);
	int copies = 0;
	for (index = 0; index < BOOT2_BLOCKS_COUNT; index++)
	{
		if (_boot2Handle.BlockMap.Blocks[index] != 0)
			continue;

		copies += blockIndex < index ? 1 : -1;
	}

	// Iterate over available boot2 copies, check for bad blocks, copy pages, handle errors/retries
	u32 maxBlocks = BOOT2_BLOCKS_COUNT;
	for (index = 0; index <= blockIndex; index++)
	{
		if (copies < 0)
			return IPC_EMAX;

		// Skip bad blocks (marked as 1 in blockmap)
		if (_boot2Handle.BlockMap.Blocks[index] == 1)
			continue;

		while (_boot2Handle.BlockMap.Blocks[maxBlocks] != 0) maxBlocks--;

		ret = DeleteCluster(
		    maxBlocks
		    << ((SelectedNandSizeInfo.BlockSizeBitShift - CLUSTER_SIZE_SHIFT) & 0xFF));

		for (u32 i = 0; ret == IPC_SUCCESS && i < pagesPerBlock; i++)
		{
			u32 srcPage = (index * pagesPerBlock) + i;
			u32 dstPage = (maxBlocks * pagesPerBlock) + i;

			if (minimumPage < srcPage && srcPage == pageIndex)
			{
				_boot2Handle.BlockMap.Generation++;
				memcpy(_boot2Handle.PageBuffer, &_boot2Handle.BlockMap, sizeof(Boot2BlockMap));
				memcpy(_boot2Handle.PageBuffer + sizeof(Boot2BlockMap),
				       &_boot2Handle.BlockMap, sizeof(Boot2BlockMap));
				memcpy(_boot2Handle.PageBuffer + (sizeof(Boot2BlockMap) * 2),
				       &_boot2Handle.BlockMap, sizeof(Boot2BlockMap));
				memset(_boot2Handle.PageBuffer + sizeof(Boot2BlockMap) * 3, 0,
				       GetPageSize() - (sizeof(Boot2BlockMap) * 3));

				ret = WriteNandPage(dstPage, _boot2Handle.PageBuffer, NULL, 0, false);
			}
			else if (minimumPage >= srcPage)
			{
				ret = CopyPage(srcPage, dstPage);
			}

			if (ret != IPC_SUCCESS)
				break;
		}

		if (ret != IPC_SUCCESS)
		{
			_boot2Handle.BlockMap.Blocks[maxBlocks] = 1;
			maxBlocks--;
			copies--;
		}
	}

	return ret;
}

static s32 HandleBoot2Write(const void *data, u32 length)
{
	u32 pageSize;
	u32 spaceLeft;
	u32 copySize;
	u32 originalLength;
	s32 ret;

	if (_boot2Handle.Finished)
		return IPC_EINVAL;

	pageSize = GetPageSize();
	originalLength = length;

	while (length > 0)
	{
		spaceLeft = pageSize - _boot2Handle.PageCursor;
		copySize = (length < spaceLeft) ? length : spaceLeft;

		memcpy(_boot2Handle.PageBuffer + _boot2Handle.PageCursor, data, copySize);
		_boot2Handle.PageCursor += copySize;
		data = (const u8 *)data + copySize;
		length -= copySize;

		if (_boot2Handle.PageCursor == pageSize)
		{
			ret = WriteBoot2Page(true);
			if (ret != IPC_SUCCESS)
				return ret;
		}
	}

	return (s32)originalLength;
}

static inline s32 HandleBoot2Ioctl(IpcMessage *message)
{
	switch (message->Request.Data.Ioctl.Ioctl)
	{
		case IOCTL_WRITE_BLOCKMAP:
		case IOCTL_WRITE_BLOCKMAP_COPY:
			return FinishBoot2Write();

		case IOCTL_WRITE_BOOT2_COPY:
			if (message->Request.Data.Ioctl.InputLength != 4)
				return IPC_EINVAL;

			return WriteBoot2Copy(*(u32 *)message->Request.Data.Ioctl.InputBuffer);

		default:
			return IPC_EINVAL;
	}
}

s32 HandleDevBoot2Message(IpcMessage *message)
{
	switch (message->Request.Command)
	{
		case IOS_WRITE:
			return HandleBoot2Write(message->Request.Data.Write.Data,
			                        message->Request.Data.Write.Length);
		case IOS_CLOSE:
			return CloseBoot2FileHandle();
		case IOS_IOCTL:
			return HandleBoot2Ioctl(message);
		default:
			return IPC_EINVAL;
	}
}
