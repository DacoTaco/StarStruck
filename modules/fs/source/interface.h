/*
	StarStruck - a Free Software reimplementation for the Nintendo/BroadOn IOS.
	Copyright (C) 2022	DacoTaco

# This code is licensed to you under the terms of the GNU GPL, version 2;
# see file COPYING or http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt
*/

#ifndef __INTERFACE_H_
#define __INTERFACE_H_

#include <types.h>

#define IRQ_NAND				1

typedef enum {
	EccFlag = 0x1000,
	ReadFlag = 0x2000,
	WriteFlag = 0x4000,
	WaitFlag = 0x8000,
	ErrorFlag = 0x20000000,
	IrqFlag = 0x40000000,
} NandFlags;

typedef struct {
    u16 ChipId;
	u8 Unknown[2];
    u8 Reset;
    u8 ReadPrefix;
    u8 Read;
    u8 ReadAlternative;
	u8 ReadPost;
	u8 ReadCopyBack;
    u8 Unknown2;
    u8 WritePrefix;
    u8 Write;
	u8 WriteCopyBack;
	u8 Unknown3[2];
	u8 WriteCopyBackPrefix;
    u8 DeletePrefix;
	u8 Unknown4;
    u8 Delete;
	u8 RandomDataOutputPrefix;
	u8 RandomDataOutput;
	u8 RandomDataInput;
    u8 ReadStatusPrefix;
    u8 Unknown6[32];
	u8 ChipType;
	u8 ChipAttributes1;
	u8 ChipAttributes2;
	u8 ChipAttributes3;
	u8 ChipAttributes4;
    u8 Padding[3];
} NandInformationBase;
CHECK_SIZE(NandInformationBase, 0x40);
CHECK_OFFSET(NandInformationBase, 0x00, ChipId);
CHECK_OFFSET(NandInformationBase, 0x02, Unknown);
CHECK_OFFSET(NandInformationBase, 0x04, Reset);
CHECK_OFFSET(NandInformationBase, 0x05, ReadPrefix);
CHECK_OFFSET(NandInformationBase, 0x06, Read);
CHECK_OFFSET(NandInformationBase, 0x07, ReadAlternative);
CHECK_OFFSET(NandInformationBase, 0x08, ReadPost);
CHECK_OFFSET(NandInformationBase, 0x09, ReadCopyBack);
CHECK_OFFSET(NandInformationBase, 0x0B, WritePrefix);
CHECK_OFFSET(NandInformationBase, 0x0C, Write);
CHECK_OFFSET(NandInformationBase, 0x0D, WriteCopyBack);
CHECK_OFFSET(NandInformationBase, 0x10, WriteCopyBackPrefix);
CHECK_OFFSET(NandInformationBase, 0x11, DeletePrefix);
CHECK_OFFSET(NandInformationBase, 0x13, Delete);
CHECK_OFFSET(NandInformationBase, 0x14, RandomDataOutputPrefix);
CHECK_OFFSET(NandInformationBase, 0x15, RandomDataOutput);
CHECK_OFFSET(NandInformationBase, 0x16, RandomDataInput);
CHECK_OFFSET(NandInformationBase, 0x17, ReadStatusPrefix);
CHECK_OFFSET(NandInformationBase, 0x38, ChipType);
CHECK_OFFSET(NandInformationBase, 0x39, ChipAttributes1);
CHECK_OFFSET(NandInformationBase, 0x3A, ChipAttributes2);
CHECK_OFFSET(NandInformationBase, 0x3B, ChipAttributes3);
CHECK_OFFSET(NandInformationBase, 0x3C, ChipAttributes4);

typedef struct {
		u8 Unknown;
		u8 UnknownRegister;
		u8 Padding[2];
} NandInformationExtension;
CHECK_SIZE(NandInformationExtension, 0x04);
CHECK_OFFSET(NandInformationExtension, 0x01, UnknownRegister);

//earlier IOS' had this information without the unknown register stuff, hence why it is split up in 2 structs, with a merged struct
typedef struct {
    NandInformationBase Info;
    NandInformationExtension Extension;
} NandInformation;
CHECK_SIZE(NandInformation, 0x44);
CHECK_OFFSET(NandInformation, 0x00, Info);
CHECK_OFFSET(NandInformation, 0x40, Extension);

extern u8 _nandInitialized;
extern u32 _irqMessageQueueId;
extern u32 _ioscMessageQueueId;

s32 InitializeNand();
s32 SendCommand(u8 command, u32 bitmask, u32 flags, u32 dataLength);
#endif