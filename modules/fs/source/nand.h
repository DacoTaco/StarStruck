/*
	StarStruck - a Free Software reimplementation for the Nintendo/BroadOn IOS.
	Copyright (C) 2022	DacoTaco

# This code is licensed to you under the terms of the GNU GPL, version 2;
# see file COPYING or http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt
*/

#pragma once

#include <types.h>

//for more info :
//https://wiibrew.org/wiki/Hardware/NAND_Interface

#define IRQ_NAND      1

/* NAND Registers */

#define NAND_REG_BASE 0xd010000

#define NAND_CMD      (NAND_REG_BASE + 0x000)
#define NAND_STATUS   NAND_CMD
#define NAND_CONF     (NAND_REG_BASE + 0x004)
#define NAND_ADDR0    (NAND_REG_BASE + 0x008)
#define NAND_ADDR1    (NAND_REG_BASE + 0x00c)
#define NAND_DATA     (NAND_REG_BASE + 0x010)
#define NAND_ECC      (NAND_REG_BASE + 0x014)
#define NAND_CLKDIV   (NAND_REG_BASE + 0x018)
#define NAND_UNK2     (NAND_REG_BASE + 0x01c)

typedef enum
{
	EccFlag = 0x1000,
	ReadFlag = 0x2000,
	WriteFlag = 0x4000,
	WaitFlag = 0x8000,
	ErrorFlag = 0x20000000,
	IrqFlag = 0x40000000,
} NandFlags;

typedef union
{
	struct __attribute__((packed))
	{
		u8 Execute : 1;
		u8 GenerateIrq : 1;
		u8 HasError : 1;
		u8 Address : 5;
		u8 Command : 8;
		u8 Wait : 1;
		u8 WriteData : 1;
		u8 ReadData : 1;
		u8 CalculateEEC : 1;
		u32 DataLength : 12;
	} Fields;
	u32 Value;
} NandCommand;
CHECK_SIZE(NandCommand, 4);

#define READ_CMD() ((NandCommand)read32(NAND_CMD))

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

typedef struct
{
	u32 NandSizeBitShift;
	u32 BlockSizeBitShift;
	u32 PageSizeBitShift;
	u32 EccSizeBitShift;
	u32 HMACSizeShift;
	u16 PageCopyMask;
	u16 SupportPageCopy;
	u16 EccDataCheckByteOffset;
	u8 Padding[2];
} NandSizeInformation;
CHECK_SIZE(NandSizeInformation, 0x1C);
CHECK_OFFSET(NandSizeInformation, 0x00, NandSizeBitShift);
CHECK_OFFSET(NandSizeInformation, 0x04, BlockSizeBitShift);
CHECK_OFFSET(NandSizeInformation, 0x08, PageSizeBitShift);
CHECK_OFFSET(NandSizeInformation, 0x0C, EccSizeBitShift);
CHECK_OFFSET(NandSizeInformation, 0x10, HMACSizeShift);
CHECK_OFFSET(NandSizeInformation, 0x14, PageCopyMask);
CHECK_OFFSET(NandSizeInformation, 0x16, SupportPageCopy);
CHECK_OFFSET(NandSizeInformation, 0x18, EccDataCheckByteOffset);
CHECK_OFFSET(NandSizeInformation, 0x1a, Padding);

typedef struct
{
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

typedef struct
{
	u8 Unknown;
	u8 ClockDivisorValue;
	u8 Padding[2];
} NandInformationExtension;
CHECK_SIZE(NandInformationExtension, 0x04);
CHECK_OFFSET(NandInformationExtension, 0x01, ClockDivisorValue);

// Earlier IOS versions had this information without the unknown register stuff,
// hence why it is split up in 2 structs, with a merged struct
typedef struct
{
	NandInformationBase Info;
	NandInformationExtension Extension;
} NandInformation;
CHECK_SIZE(NandInformation, 0x44);
CHECK_OFFSET(NandInformation, 0x00, Info);
CHECK_OFFSET(NandInformation, 0x40, Extension);

typedef enum
{
	DeleteCommand = 0,
	WriteCommand = 1,
	ReadCommand = 2,
	CopyCommand = 3,
	UnknownCommandType2 = 4,
} CommandType;

typedef struct
{
	u32 Page;
	u32 CommandType;
	s32 Return;
} NandErrorEntry;
CHECK_SIZE(NandErrorEntry, 0x0C);
CHECK_OFFSET(NandErrorEntry, 0x00, Page);
CHECK_OFFSET(NandErrorEntry, 0x04, CommandType);
CHECK_OFFSET(NandErrorEntry, 0x08, Return);

#define ERROR_LOG_SIZE 0x21
typedef struct
{
	u32 SuccessfulDeletes;
	u32 SuccessfulWrites;
	u32 SuccessfulReads;
	u32 Unknown4;
	u32 ErrorOverflowIndex;
	u32 ErrorIndex;
	NandErrorEntry Errors[ERROR_LOG_SIZE];
} NandCommandLog;
CHECK_SIZE(NandCommandLog, 0x1A4);
CHECK_OFFSET(NandCommandLog, 0x00, SuccessfulDeletes);
CHECK_OFFSET(NandCommandLog, 0x04, SuccessfulWrites);
CHECK_OFFSET(NandCommandLog, 0x08, SuccessfulReads);
CHECK_OFFSET(NandCommandLog, 0x0C, Unknown4);
CHECK_OFFSET(NandCommandLog, 0x10, ErrorOverflowIndex);
CHECK_OFFSET(NandCommandLog, 0x14, ErrorIndex);

// Cluster size is always 16 KiB (0x4000 bytes)
#define CLUSTER_SIZE       0x4000
#define CLUSTER_SIZE_SHIFT 0x0E

/* Global state - defined in nand.c */
extern s32 IrqMessageQueueId;
extern s32 IoscMessageQueueId;
extern NandInformation SelectedNandChip;
extern NandSizeInformation SelectedNandSizeInfo;
extern u8 EccBuffer[];

/* NAND Interface Functions */
bool IsNandInitialized(void);
s32 InitializeNand(void);
void SetNandData(void *data, void *ecc);
void SetNandAddress(u32 pageOffset, u32 pageNumber);
s32 CorrectNandData(void *dataBuffer, void *eccBuffer);
s32 SelectNandSize(bool selectNandSize);
s32 ReadNandPage(u32 pageNumber, void *data, void *ecc, bool readEcc);
s32 WriteNandPage(u32 pageNumber, void *data, void *ecc, u8 unknownWriteflag, bool writeEcc);
s32 DeleteCluster(u16 cluster);
s32 CopyCluster(u16 srcCluster, u16 dstCluster);
s32 CopyPage(u32 srcPage, u32 dstPage);
s32 CheckNandBlock(u8 block);
