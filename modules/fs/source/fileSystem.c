/*
	StarStruck - a Free Software reimplementation for the Nintendo/BroadOn IOS.
	Copyright (C) 2025	DacoTaco

# This code is licensed to you under the terms of the GNU GPL, version 2;
# see file COPYING or http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt
*/

#include "fileSystem.h"
#include "interface.h"
#include "errors.h"

bool _superblockInitialized = false;
SuperBlockInfo* _selectedSuperBlock = NULL;
void* _superblockLocation = NULL;
u32 _fileSystemMetadataSizeShift = 0;
//the size of the file system data, this is the size of the nand minus the metadata & superblock size
u32 _fileSystemDataSize = 0;

s32 FlushSuperBlock()
{
	if(!_superblockInitialized)
		return FS_ENOENT;

	//TODO: implement the rest
}

s32 ChangeSelectedSuperblock(bool findSuperblock)
 {
	if(!findSuperblock)
	{
		_selectedSuperBlock = NULL;
		return 0;
	}

	_superblockLocation = (void*)0x100000;
	switch(SelectedNandSizeInfo.NandSizeBitShift)
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
	if(1 << (_fileSystemMetadataSizeShift - 4 & 0xFF) >= 0x40000)
		return FS_EINVAL;

	//calculate size of the nand that can contain data: nandsize - 1MB - superblock size
	_fileSystemDataSize = ((1 << (SelectedNandSizeInfo.NandSizeBitShift & 0xFF)) -0x100000) - (1 << _fileSystemMetadataSizeShift);
	//we were here
	_superblockLocation = ;
	memset(, 0, 0x40);
	/*    if (1 << (FS_SFFSMetadataSizeShift - 4U & 0xff) < 0x40001) {
      FS_SFFSFileDataSizeBytes? =
           ((1 << (Selected_Nand_Size?.NandSizeBitShift & 0xff)) + -0x100000) -
           (1 << FS_SFFSMetadataSizeShift);
      FS_SuperblockPtr? = (FSSuperblock *)BYTE_ARRAY_20009940;
      FS_memset((byte *)&FSSaltData_20049940,0,0x40);
    }
    else {
      ret = -0x75;
    }*/
 }

s32 InitializeSuperBlockInfo(s32 mode)
{
	s32 ret = 0;
	s32 flushSuperBlock = 0;
	if(mode == 0)
	{
		ChangeSelectedSuperblock(false);
    	SelectNandSize(false);
		goto _initSSFSEnd;
	}

	ret = SelectNandSize(true);
	if( ret != 0 )
		goto _initSSFSEnd;

	//we were here
	ret = ChangeSelectedSuperblock(true);


_initSSFSEnd:
	if(flushSuperBlock)
		ret = FlushSuperBlock();
	
	return ret;
}