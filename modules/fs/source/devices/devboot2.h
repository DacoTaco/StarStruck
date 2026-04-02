/*
	StarStruck - a Free Software reimplementation for the Nintendo/BroadOn IOS.
	Copyright (C) 2025	DacoTaco

# This code is licensed to you under the terms of the GNU GPL, version 2;
# see file COPYING or http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt
*/

#pragma once

#include <ios/ipc.h>
#include <types.h>

// Boot2 blockmap signature - identifies a valid blockmap page
#define BOOT2_BLOCKMAP_SIGNATURE 0x26f29a401ee684cfULL
#define BOOT2_BLOCKS_COUNT       0x40

// Boot2 blockmap structure (0x4C bytes)
// Found at the last page of boot2 blocks in the first 1MB of NAND
typedef struct Boot2BlockMap
{
	u64 Signature;
	u32 Generation;
	u8 Blocks[BOOT2_BLOCKS_COUNT];
} __attribute__((packed)) Boot2BlockMap;
CHECK_SIZE(Boot2BlockMap, 0x4C);
CHECK_OFFSET(Boot2BlockMap, 0x00, Signature);
CHECK_OFFSET(Boot2BlockMap, 0x08, Generation);
CHECK_OFFSET(Boot2BlockMap, 0x0C, Blocks);

typedef struct
{
	bool IsOpen;
	u8 _Padding[3];
	Boot2BlockMap BlockMap;
	u32 PageIndex;
	bool Finished;
	u8 _Padding2[3];
	u8 Unknown[0x28];
	u8 PageBuffer[0x800];
	u32 PageCursor;
} Boot2Handle;
CHECK_SIZE(Boot2Handle, 0x884);
CHECK_OFFSET(Boot2Handle, 0x00, IsOpen);
CHECK_OFFSET(Boot2Handle, 0x04, BlockMap);
CHECK_OFFSET(Boot2Handle, 0x50, PageIndex);
CHECK_OFFSET(Boot2Handle, 0x54, Finished);
CHECK_OFFSET(Boot2Handle, 0x80, PageBuffer);
CHECK_OFFSET(Boot2Handle, 0x880, PageCursor);

// Handle IPC messages for /dev/boot2 device
bool IsBoot2FileHandle(s32 fd);
s32 OpenBoot2FileHandle(void);
s32 CloseBoot2FileHandle(void);
s32 HandleDevBoot2Message(IpcMessage *message);
