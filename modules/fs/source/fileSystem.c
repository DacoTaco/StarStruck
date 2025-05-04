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

#include "fileSystem.h"
#include "interface.h"
#include "errors.h"

bool _superblockInitialized = false;
SuperBlockInfo _superblockStorage;
SuperBlockInfo *_selectedSuperBlock = NULL;
u32 _selectedSuperblockIndex;
SaltData _saltData;
u32 _superblockOffset;
u32 _fileSystemMetadataSizeShift = 0;
//the size of the file system data, this is the size of the nand minus the metadata & superblock size
u32 _fileSystemDataSize = 0;

s32 TryWriteSuperblock()
{
	if (!_superblockInitialized)
		return FS_ENOENT;

	bool flushed = false;
	u32 superBlockIndex;
	s32 ret;
	s32 superblockClusterIndex;
	_selectedSuperBlock->Version++;
	for (superBlockIndex = 0; !flushed && superBlockIndex < 0x10; superBlockIndex++)
	{
		bool allClustersReserved;

		//first step of getting the metadata offset within the superblock region: get the superblocks region size
		u32 superblockMetadataOffset =
		    1 << ((_fileSystemMetadataSizeShift - SelectedNandSizeInfo.BlockSizeBitShift) & 0xFF);

		//reduce that by 1 for the current superblock offset within the region(mask)
		superblockMetadataOffset--;

		//now that we have the mask, let us get the actual index/offset
		superblockMetadataOffset = _selectedSuperblockIndex & superblockMetadataOffset;

		//get fat offset
		u32 fatClusterOffset = ((SelectedNandSizeInfo.BlockSizeBitShift - 0x0E) & 0xFF);
		u32 fatClusterOffset =
		    (SelectedNandSizeInfo.BlockSizeBitShift < (_fileSystemMetadataSizeShift - 4)) ?
		        (superblockMetadataOffset
		             << (_fileSystemMetadataSizeShift - SelectedNandSizeInfo.BlockSizeBitShift) - 4 &
		         0xFF)
		            << fatClusterOffset :
		        superblockMetadataOffset << fatClusterOffset;

		/* 
      superblockClusterIndex =
           ((uint)(FS_SFFSFileDataStartBytes? + FS_SFFSFileDataSizeBytes?) >> 0xe) +
           fatClusterOffset +
           ((FS_CurrentSuperblockIndex? >>
            (FS_SFFSMetadataSizeShift - Selected_Nand_Size?.BlockSizeBitShift & 0xff)) <<
           (FS_SFFSMetadataSizeShift - 0x12U & 0xff));
      superblockMetadataOffset = 0;*/

		/*
		//get the start of the data offset of the metadata region, and devide by 16kB to get the cluster index
		superblockClusterIndex = (_superblockOffset + _fileSystemDataSize) / 16;
		superblockClusterIndex = ;

		uVar6 = ((uint)(FS_SFFSFileDataStartBytes? + FS_SFFSFileDataSizeBytes?) >> 0xe) + iVar3 +
        ((FS_CurrentSuperblockIndex? >>
        (FS_SFFSMetadataSizeShift - Selected_Nand_Size?.BlockSizeBitShift & 0xff)) <<
        (FS_SFFSMetadataSizeShift - 0x12U & 0xff));*/
	}

	if (superBlockIndex >= 0x10)
		return FS_ECORRUPT;
}

s32 InitSuperblockInfo(bool clearInfo)
{
	if (!clearInfo)
	{
		_selectedSuperBlock = NULL;
		return 0;
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
			return FS_EINVAL;
	}

	//IOS does this but i don't understand why. if its >= it means the shift is > 0x16, and thats our maximum value lol
	if (1 << (_fileSystemMetadataSizeShift - 4 & 0xFF) >= 0x40000)
		return FS_EINVAL;

	//calculate size of the nand that can contain data: nandsize - 1MB - superblock size
	_fileSystemDataSize = ((1 << (SelectedNandSizeInfo.NandSizeBitShift & 0xFF)) - 0x100000) -
	                      (1 << _fileSystemMetadataSizeShift);
	//setup superblock storage and salt data
	_selectedSuperBlock = &_superblockStorage;
	memset(&_saltData, 0, sizeof(SaltData));
}

//doing the superblock selection
SuperBlockInfo *SelectSuperBlock()
{
	SuperBlockInfo *returnedSuperblock;
	u32 superBlockIndex;
	u32 superBlockGeneration;
	bool rewriteSuperblock = false;
	s32 ret;
	u32 sffsGeneration;

	if (_selectedSuperBlock != NULL)
		goto _selectSuperBlockEnd;

	if (_superblockInitialized)
	{
		.returnedSuperblock = _selectedSuperBlock;
		goto _selectSuperBlockEnd;
	}

	ret = OSGetIOSCData(KEYRING_CONST_NAND_GEN, (u32 *)&sffsGeneration);
	if (ret != IPC_SUCCESS)
		goto _selectSuperBlockEnd;

	while (true)
	{
		//we were here
		superBlockGeneration = 0;
		superBlockIndex = -1;
		u32 index;

		for (index = 0; index < 0x10; index++)
		{
			//TODO
		}

		if (superBlockIndex < 0)
			goto _selectSuperBlockEnd;
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

s32 InitializeSuperBlockInfo(s32 mode)
{
	s32 ret = 0;
	bool flushSuperBlock;
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
		goto _initSSFSEnd;

	//we were here
	fetchedSuperBlock = SelectSuperBlock();

_initSSFSEnd:
	if (flushSuperBlock)
		ret = TryWriteSuperblock();

	return ret;
}