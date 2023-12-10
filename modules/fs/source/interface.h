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

typedef struct 
{
	u8 Reset;
    u8 ReadPrefix;
    u8 Read;
    u8 ReadAlternative;
	u8 ReadPost;
	u8 ReadCopyBack;
    u8 Unknown;
    u8 WritePrefix;
    u8 Write;
	u8 WriteCopyBack;
	u8 Unknown2;
	u8 WriteUnknown;
	u8 WriteCopyBackPrefix;
    u8 DeletePrefix;
	u8 Unknown3;
    u8 Delete;
	u8 RandomDataOutputPrefix;
	u8 RandomDataOutput;
	u8 RandomDataInput;
    u8 ReadStatusPrefix;
	u8 Unknown4;
	u8 InputAddress;
	u8 Padding[2];
} NandCommandInformation;
CHECK_SIZE(NandCommandInformation, 0x18);
CHECK_OFFSET(NandCommandInformation, 0x00, Reset);
CHECK_OFFSET(NandCommandInformation, 0x01, ReadPrefix);
CHECK_OFFSET(NandCommandInformation, 0x02, Read);
CHECK_OFFSET(NandCommandInformation, 0x03, ReadAlternative);
CHECK_OFFSET(NandCommandInformation, 0x04, ReadPost);
CHECK_OFFSET(NandCommandInformation, 0x05, ReadCopyBack);
CHECK_OFFSET(NandCommandInformation, 0x07, WritePrefix);
CHECK_OFFSET(NandCommandInformation, 0x08, Write);
CHECK_OFFSET(NandCommandInformation, 0x09, WriteCopyBack);
CHECK_OFFSET(NandCommandInformation, 0x0B, WriteUnknown);
CHECK_OFFSET(NandCommandInformation, 0x0C, WriteCopyBackPrefix);
CHECK_OFFSET(NandCommandInformation, 0x0D, DeletePrefix);
CHECK_OFFSET(NandCommandInformation, 0x0F, Delete);
CHECK_OFFSET(NandCommandInformation, 0x10, RandomDataOutputPrefix);
CHECK_OFFSET(NandCommandInformation, 0x11, RandomDataOutput);
CHECK_OFFSET(NandCommandInformation, 0x12, RandomDataInput);
CHECK_OFFSET(NandCommandInformation, 0x13, ReadStatusPrefix);
CHECK_OFFSET(NandCommandInformation, 0x15, InputAddress);

typedef struct {
	u32 NandSizeBitShift;
	u8 Unknown[4];
	u32 PageSizeBitShift;
	u32 EccSizeBitShift;
    u8 Unknown2[12];
} NandSizeInformation;
CHECK_SIZE(NandSizeInformation, 0x1C);
CHECK_OFFSET(NandSizeInformation, 0x00, NandSizeBitShift);
CHECK_OFFSET(NandSizeInformation, 0x08, PageSizeBitShift);
CHECK_OFFSET(NandSizeInformation, 0x0C, EccSizeBitShift);
CHECK_OFFSET(NandSizeInformation, 0x10, Unknown2);

typedef struct {
    u16 ChipId;
	u8 Padding[2];
	NandCommandInformation Commands;
	NandSizeInformation SizeInfo;
	u8 ChipType;
	u8 ChipAttributes1;
	u8 ChipAttributes2;
	u8 ChipAttributes3;
	u8 ChipAttributes4;
    u8 Padding4[3];
} NandInformationBase;
CHECK_SIZE(NandInformationBase, 0x40);
CHECK_OFFSET(NandInformationBase, 0x00, ChipId);
CHECK_OFFSET(NandInformationBase, 0x02, Padding);
CHECK_OFFSET(NandInformationBase, 0x04, Commands);
CHECK_OFFSET(NandInformationBase, 0x1C, SizeInfo);
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

typedef enum
{
	DeleteCommand = 0,
	UnknownCommandType = 1,
	ReadCommand = 2,
	UnknownCommandType2 = 3,
} CommandType;
typedef struct 
{
	u32 Unknown;
	u32 CommandType;
	s32 Return;
} NandErrorEntry;
CHECK_SIZE(NandErrorEntry, 0x0C);
CHECK_OFFSET(NandErrorEntry, 0x00, Unknown);
CHECK_OFFSET(NandErrorEntry, 0x04, CommandType);
CHECK_OFFSET(NandErrorEntry, 0x08, Return);

#define ERROR_LOG_SIZE		0x21
typedef struct {
	u32 SuccessfulDeletes;
	u32 Unknown2;
	u32 SuccessfulReads;
	u32 Unknown4;
	u32 ErrorOverflowIndex;
	u32 ErrorIndex;
	NandErrorEntry Errors[ERROR_LOG_SIZE];
} NandCommandLog;
CHECK_SIZE(NandCommandLog, 0x1A4);
CHECK_OFFSET(NandCommandLog, 0x00, SuccessfulDeletes);
CHECK_OFFSET(NandCommandLog, 0x04, Unknown2);
CHECK_OFFSET(NandCommandLog, 0x08, SuccessfulReads);
CHECK_OFFSET(NandCommandLog, 0x0C, Unknown4);
CHECK_OFFSET(NandCommandLog, 0x10, ErrorOverflowIndex);
CHECK_OFFSET(NandCommandLog, 0x14, ErrorIndex);

extern u8 _nandInitialized;
extern u32 IrqMessageQueueId;
extern u32 IoscMessageQueueId;
extern NandInformation SelectedNandChip;

s32 InitializeNand();
void SetNandAddress(u32 pageOffset, u32 pageNumber);
void SetNandData(void* data, void* ecc);
s32 ReadNandStatus(void);
s32 CorrectNandData(void* data, void* ecc);
s32 SendNandCommand(u8 command, u32 bitmask, u32 flags, u32 dataLength);
#endif