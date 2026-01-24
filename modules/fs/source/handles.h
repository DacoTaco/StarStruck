/*
	StarStruck - a Free Software reimplementation for the Nintendo/BroadOn IOS.
	Copyright (C) 2025	DacoTaco

# This code is licensed to you under the terms of the GNU GPL, version 2;
# see file COPYING or http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt
*/

#pragma once
#include <types.h>

#define FS_MAX_FILE_HANDLES 0x10

typedef struct
{
	u16 InUse;
	u16 GroupId;
	u32 UserId;
	u32 Inode;
	u32 Mode;
	u32 FilePosition;
	u32 FilePointer;
	u32 Size;
	u32 ShouldFlushSuperblock;
	u32 Error;
} FSHandle;

CHECK_SIZE(FSHandle, 0x24);
CHECK_OFFSET(FSHandle, 0x00, InUse);
CHECK_OFFSET(FSHandle, 0x02, GroupId);
CHECK_OFFSET(FSHandle, 0x04, UserId);
CHECK_OFFSET(FSHandle, 0x08, Inode);
CHECK_OFFSET(FSHandle, 0x0C, Mode);
CHECK_OFFSET(FSHandle, 0x10, FilePosition);
CHECK_OFFSET(FSHandle, 0x14, FilePointer);
CHECK_OFFSET(FSHandle, 0x18, Size);
CHECK_OFFSET(FSHandle, 0x1C, ShouldFlushSuperblock);
CHECK_OFFSET(FSHandle, 0x20, Error);

extern FSHandle _fileHandles[FS_MAX_FILE_HANDLES];
