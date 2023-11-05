/*
	StarStruck - a Free Software reimplementation for the Nintendo/BroadOn IOS.
	Copyright (C) 2022	DacoTaco

# This code is licensed to you under the terms of the GNU GPL, version 2;
# see file COPYING or http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt
*/

#include <ios/errno.h>
#include <string.h>
#include <ios/processor.h>
#include <ios/syscalls.h>
#include <ios/printk.h>
#include <ios/module.h>

#include "nand.h"
#include "errors.h"

//for more info :
//https://wiibrew.org/wiki/Hardware/NAND_Interface

typedef struct
{
	u32 Initialized;
	u32 Opened;
} NandState;

const NandInformation SupportedNandChips[10] MODULE_DATA = {
	// Hynix HY27US0812(1/2)B
	{
		.Info = {
			.ChipId = 0xAD76,
			.Padding = { 0x00, 0x00},
			.Commands = {
				.Reset = 0xFF,
				.ReadPrefix = 0xFE,
				.Read = 0x00,
				.ReadAlternative = 0x01,
				.ReadPost = 0x50,
				.ReadCopyBack = 0xFE,
				.Unknown = 0xFE,
				.WritePrefix = 0x80,
				.Write = 0x10,
				.WriteCopyBack = 0xFE,
				.Unknown2 = 0xfe,
				.WriteUnknown = 0xFE,
				.WriteCopyBackPrefix = 0x8A,
				.DeletePrefix = 0x60,
				.Unknown3 = 0xFE,
				.Delete = 0xD0,
				.RandomDataOutputPrefix = 0xFE,
				.RandomDataOutput = 0xFE,
				.RandomDataInput = 0xFE,
				.ReadStatusPrefix = 0x70,
				.Unknown4 = 0xFE,
				.InputAddress = 0x1D,
				.Padding = { 0x00, 0x00 },
			},
			.SizeInfo = {
				.NandSizeBitShift = 0x0000001A,
				.BlockSizeBitShift = 0x00000e,
				.PageSizeBitShift = 0x00000009,
				.EccSizeBitShift = 0x00000004,
				.HMACSizeShift = 0x00000002,
				.PageCopyMask = 0x0001,
				.SupportPageCopy = 0x0000,
				.EccDataCheckByteOffset = 005,
				.Padding = { 0x00, 0x00 },
			},
			.ChipType = 0x04,
			.ChipAttributes1 = 0x03,
			.ChipAttributes2 = 0x3e,
			.ChipAttributes3 = 0x0e,
			.ChipAttributes4 = 0x7f,
			.Padding4 = { 0x00, 0x00, 0x00 }
		},
		.Extension = {
			.Unknown = 0,
			.ClockDivisorValue = 0x01,
			.Padding = { 0x00, 0x00 }
		}
	},
	// Hynix HY27UF081G2A
	{
		.Info = {
			.ChipId = 0xADF1,
			.Padding = { 0x00, 0x00},
			.Commands = {
				.Reset = 0xFF,
				.ReadPrefix = 0x00,
				.Read = 0x30,
				.ReadAlternative = 0xFE,
				.ReadPost = 0xFE,
				.ReadCopyBack = 0x35,
				.Unknown = 0xFE,
				.WritePrefix = 0x80,
				.Write = 0x10,
				.WriteCopyBack = 0x10,
				.Unknown2 = 0xfe,
				.WriteUnknown = 0xFE,
				.WriteCopyBackPrefix = 0x85,
				.DeletePrefix = 0x60,
				.Unknown3 = 0xFE,
				.Delete = 0xD0,
				.RandomDataOutputPrefix = 0x05,
				.RandomDataOutput = 0xE0,
				.RandomDataInput = 0x85,
				.ReadStatusPrefix = 0x70,
				.Unknown4 = 0xFE,
				.InputAddress = 0x0F,
				.Padding = { 0x00, 0x00 },
			},
			.SizeInfo = {
				.NandSizeBitShift = 0x0000001B,
				.BlockSizeBitShift = 0x000011,
				.PageSizeBitShift = 0x0000000B,
				.EccSizeBitShift = 0x00000006,
				.HMACSizeShift = 0x00000005,
				.PageCopyMask = 0x0001,
				.SupportPageCopy = 0x0000,
				.EccDataCheckByteOffset = 000,
				.Padding = { 0x00, 0x00 },
			},
			.ChipType = 0x03,
			.ChipAttributes1 = 0x03,
			.ChipAttributes2 = 0x3e,
			.ChipAttributes3 = 0x0e,
			.ChipAttributes4 = 0x7f,
			.Padding4 = { 0x00, 0x00, 0x00 }
		},
		.Extension = {
			.Unknown = 0,
			.ClockDivisorValue = 0x01,
			.Padding = { 0x00, 0x00 }
		}
	},
	// Hynix HY27UF084G2M or HY27UG084G2M
	{
		.Info = {
			.ChipId = 0xADDC,
			.Padding = { 0x00, 0x00},
			.Commands = {
				.Reset = 0xFF,
				.ReadPrefix = 0x00,
				.Read = 0x30,
				.ReadAlternative = 0xFE,
				.ReadPost = 0xFE,
				.ReadCopyBack = 0x35,
				.Unknown = 0xFE,
				.WritePrefix = 0x80,
				.Write = 0x10,
				.WriteCopyBack = 0x10,
				.Unknown2 = 0xfe,
				.WriteUnknown = 0xFE,
				.WriteCopyBackPrefix = 0x85,
				.DeletePrefix = 0x60,
				.Unknown3 = 0xFE,
				.Delete = 0xD0,
				.RandomDataOutputPrefix = 0x05,
				.RandomDataOutput = 0xE0,
				.RandomDataInput = 0x85,
				.ReadStatusPrefix = 0x70,
				.Unknown4 = 0xFE,
				.InputAddress = 0x1F,
				.Padding = { 0x00, 0x00 },
			},
			.SizeInfo = {
				.NandSizeBitShift = 0x0000001D,
				.BlockSizeBitShift = 0x000011,
				.PageSizeBitShift = 0x0000000B,
				.EccSizeBitShift = 0x00000006,
				.HMACSizeShift = 0x00000005,
				.PageCopyMask = 0x0001,
				.SupportPageCopy = 0x0000,
				.EccDataCheckByteOffset = 000,
				.Padding = { 0x00, 0x00 },
			},
			.ChipType = 0x07,
			.ChipAttributes1 = 0x04,
			.ChipAttributes2 = 0x3f,
			.ChipAttributes3 = 0x3f,
			.ChipAttributes4 = 0xff,
			.Padding4 = { 0x00, 0x00, 0x00 }
		},
		.Extension = {
			.Unknown = 0,
			.ClockDivisorValue = 0x00,
			.Padding = { 0x00, 0x00 }
		}
	},
	// Samsung K9F1208U0C
	{
		.Info = {
			.ChipId = 0xEC76,
			.Padding = { 0x00, 0x00},
			.Commands = {
				.Reset = 0xFF,
				.ReadPrefix = 0xFE,
				.Read = 0x00,
				.ReadAlternative = 0x01,
				.ReadPost = 0x50,
				.ReadCopyBack = 0xFE,
				.Unknown = 0x03,
				.WritePrefix = 0x80,
				.Write = 0x10,
				.WriteCopyBack = 0x10,
				.Unknown2 = 0x11,
				.WriteUnknown = 0xFE,
				.WriteCopyBackPrefix = 0x8A,
				.DeletePrefix = 0x60,
				.Unknown3 = 0x60,
				.Delete = 0xD0,
				.RandomDataOutputPrefix = 0xFE,
				.RandomDataOutput = 0xFE,
				.RandomDataInput = 0xFE,
				.ReadStatusPrefix = 0x70,
				.Unknown4 = 0x71,
				.InputAddress = 0x1D,
				.Padding = { 0x00, 0x00 },
			},
			.SizeInfo = {
				.NandSizeBitShift = 0x0000001A,
				.BlockSizeBitShift = 0x00000e,
				.PageSizeBitShift = 0x00000009,
				.EccSizeBitShift = 0x00000004,
				.HMACSizeShift = 0x00000002,
				.PageCopyMask = 0x0004,
				.SupportPageCopy = 0x0000,
				.EccDataCheckByteOffset = 005,
				.Padding = { 0x00, 0x00 },
			},
			.ChipType = 0x04,
			.ChipAttributes1 = 0x03,
			.ChipAttributes2 = 0x3e,
			.ChipAttributes3 = 0x0e,
			.ChipAttributes4 = 0x7f,
			.Padding4 = { 0x00, 0x00, 0x00 }
		},
		.Extension = {
			.Unknown = 0,
			.ClockDivisorValue = 0x01,
			.Padding = { 0x00, 0x00 }
		}
	},
	// Samsung K9F1G08U0B
	{
		.Info = {
			.ChipId = 0xECF1,
			.Padding = { 0x00, 0x00},
			.Commands = {
				.Reset = 0xFF,
				.ReadPrefix = 0x00,
				.Read = 0x30,
				.ReadAlternative = 0xfe,
				.ReadPost = 0xFE,
				.ReadCopyBack = 0x35,
				.Unknown = 0xFE,
				.WritePrefix = 0x80,
				.Write = 0x10,
				.WriteCopyBack = 0x10,
				.Unknown2 = 0xfe,
				.WriteUnknown = 0xFE,
				.WriteCopyBackPrefix = 0x85,
				.DeletePrefix = 0x60,
				.Unknown3 = 0xfe,
				.Delete = 0xD0,
				.RandomDataOutputPrefix = 0x05,
				.RandomDataOutput = 0xE0,
				.RandomDataInput = 0x85,
				.ReadStatusPrefix = 0x70,
				.Unknown4 = 0xFE,
				.InputAddress = 0x0F,
				.Padding = { 0x00, 0x00 },
			},
			.SizeInfo = {
				.NandSizeBitShift = 0x0000001B,
				.BlockSizeBitShift = 0x000011,
				.PageSizeBitShift = 0x0000000B,
				.EccSizeBitShift = 0x00000006,
				.HMACSizeShift = 0x00000005,
				.PageCopyMask = 0x0001,
				.SupportPageCopy = 0x0000,
				.EccDataCheckByteOffset = 000,
				.Padding = { 0x00, 0x00 },
			},
			.ChipType = 0x03,
			.ChipAttributes1 = 0x03,
			.ChipAttributes2 = 0x3e,
			.ChipAttributes3 = 0x3e,
			.ChipAttributes4 = 0x7f,
			.Padding4 = { 0x00, 0x00, 0x00 }
		},
		.Extension = {
			.Unknown = 0,
			.ClockDivisorValue = 0x01,
			.Padding = { 0x00, 0x00 }
		}
	},
	// Samsung K9F2G08U0A
	{
		.Info = {
			.ChipId = 0xECDA,
			.Padding = { 0x00, 0x00},
			.Commands = {
				.Reset = 0xFF,
				.ReadPrefix = 0x00,
				.Read = 0x30,
				.ReadAlternative = 0xfe,
				.ReadPost = 0xFE,
				.ReadCopyBack = 0x35,
				.Unknown = 0xFE,
				.WritePrefix = 0x80,
				.Write = 0x10,
				.WriteCopyBack = 0x10,
				.Unknown2 = 0xfe,
				.WriteUnknown = 0xFE,
				.WriteCopyBackPrefix = 0x85,
				.DeletePrefix = 0x60,
				.Unknown3 = 0xfe,
				.Delete = 0xD0,
				.RandomDataOutputPrefix = 0x05,
				.RandomDataOutput = 0xE0,
				.RandomDataInput = 0x85,
				.ReadStatusPrefix = 0x70,
				.Unknown4 = 0xFE,
				.InputAddress = 0x1F,
				.Padding = { 0x00, 0x00 },
			},
			.SizeInfo = {
				.NandSizeBitShift = 0x0000001C,
				.BlockSizeBitShift = 0x000011,
				.PageSizeBitShift = 0x0000000B,
				.EccSizeBitShift = 0x00000006,
				.HMACSizeShift = 0x00000005,
				.PageCopyMask = 0x0001,
				.SupportPageCopy = 0x0000,
				.EccDataCheckByteOffset = 000,
				.Padding = { 0x00, 0x00 },
			},
			.ChipType = 0x04,
			.ChipAttributes1 = 0x03,
			.ChipAttributes2 = 0x3e,
			.ChipAttributes3 = 0x0e,
			.ChipAttributes4 = 0x7f,
			.Padding4 = { 0x00, 0x00, 0x00 }
		},
		.Extension = {
			.Unknown = 0,
			.ClockDivisorValue = 0x01,
			.Padding = { 0x00, 0x00 }
		}
	},
	// Samsung K9F4G08U0A
	{
		.Info = {
			.ChipId = 0xECDC,
			.Padding = { 0x00, 0x00},
			.Commands = {
				.Reset = 0xFF,
				.ReadPrefix = 0x00,
				.Read = 0x30,
				.ReadAlternative = 0xfe,
				.ReadPost = 0xFE,
				.ReadCopyBack = 0x35,
				.Unknown = 0xFE,
				.WritePrefix = 0x80,
				.Write = 0x10,
				.WriteCopyBack = 0x10,
				.Unknown2 = 0xfe,
				.WriteUnknown = 0xFE,
				.WriteCopyBackPrefix = 0x85,
				.DeletePrefix = 0x60,
				.Unknown3 = 0xfe,
				.Delete = 0xD0,
				.RandomDataOutputPrefix = 0x05,
				.RandomDataOutput = 0xE0,
				.RandomDataInput = 0x85,
				.ReadStatusPrefix = 0x70,
				.Unknown4 = 0xFE,
				.InputAddress = 0x1F,
				.Padding = { 0x00, 0x00 },
			},
			.SizeInfo = {
				.NandSizeBitShift = 0x0000001D,
				.BlockSizeBitShift = 0x000011,
				.PageSizeBitShift = 0x0000000B,
				.EccSizeBitShift = 0x00000006,
				.HMACSizeShift = 0x00000005,
				.PageCopyMask = 0x0002,
				.SupportPageCopy = 0x0000,
				.EccDataCheckByteOffset = 000,
				.Padding = { 0x00, 0x00 },
			},
			.ChipType = 0x07,
			.ChipAttributes1 = 0x04,
			.ChipAttributes2 = 0x3f,
			.ChipAttributes3 = 0x3f,
			.ChipAttributes4 = 0xff,
			.Padding4 = { 0x00, 0x00, 0x00 }
		},
		.Extension = {
			.Unknown = 0,
			.ClockDivisorValue = 0x00,
			.Padding = { 0x00, 0x00 }
		}
	},
	// Toshiba TC58NVG0S3AFT05 or TC58NVG0S3ATG05 or TC58NVG0S3BFT00
	{
		.Info = {
			.ChipId = 0x9876,
			.Padding = { 0x00, 0x00},
			.Commands = {
				.Reset = 0xFF,
				.ReadPrefix = 0xFE,
				.Read = 0x00,
				.ReadAlternative = 0x01,
				.ReadPost = 0x50,
				.ReadCopyBack = 0xFE,
				.Unknown = 0xFE,
				.WritePrefix = 0x80,
				.Write = 0x10,
				.WriteCopyBack = 0xFE,
				.Unknown2 = 0x11,
				.WriteUnknown = 0xFE,
				.WriteCopyBackPrefix = 0xFE,
				.DeletePrefix = 0x60,
				.Unknown3 = 0x60,
				.Delete = 0xD0,
				.RandomDataOutputPrefix = 0xFE,
				.RandomDataOutput = 0xFE,
				.RandomDataInput = 0xFE,
				.ReadStatusPrefix = 0x70,
				.Unknown4 = 0x71,
				.InputAddress = 0x1D,
				.Padding = { 0x00, 0x00 },
			},
			.SizeInfo = {
				.NandSizeBitShift = 0x0000001A,
				.BlockSizeBitShift = 0x00000e,
				.PageSizeBitShift = 0x00000009,
				.EccSizeBitShift = 0x00000004,
				.HMACSizeShift = 0x00000002,
				.PageCopyMask = 0x0004,
				.SupportPageCopy = 0x0000,
				.EccDataCheckByteOffset = 005,
				.Padding = { 0x00, 0x00 },
			},
			.ChipType = 0x04,
			.ChipAttributes1 = 0x03,
			.ChipAttributes2 = 0x3e,
			.ChipAttributes3 = 0x0e,
			.ChipAttributes4 = 0x7f,
			.Padding4 = { 0x00, 0x00, 0x00 }
		},
		.Extension = {
			.Unknown = 0,
			.ClockDivisorValue = 0x01,
			.Padding = { 0x00, 0x00 }
		}
	},
	// Toshiba TC58NVG0S3AFT05 or TC58NVG0S3ATG05 or TC58NVG0S3BFT00
	{
		.Info = {
			.ChipId = 0x98F1,
			.Padding = { 0x00, 0x00},
			.Commands = {
				.Reset = 0xFF,
				.ReadPrefix = 0x00,
				.Read = 0x30,
				.ReadAlternative = 0xfe,
				.ReadPost = 0xFE,
				.ReadCopyBack = 0xFE,
				.Unknown = 0xFE,
				.WritePrefix = 0x80,
				.Write = 0x10,
				.WriteCopyBack = 0xFE,
				.Unknown2 = 0xfe,
				.WriteUnknown = 0xFE,
				.WriteCopyBackPrefix = 0xFE,
				.DeletePrefix = 0x60,
				.Unknown3 = 0xfe,
				.Delete = 0xD0,
				.RandomDataOutputPrefix = 0x05,
				.RandomDataOutput = 0xE0,
				.RandomDataInput = 0xFE,
				.ReadStatusPrefix = 0x70,
				.Unknown4 = 0xFE,
				.InputAddress = 0x0F,
				.Padding = { 0x00, 0x00 },
			},
			.SizeInfo = {
				.NandSizeBitShift = 0x0000001B,
				.BlockSizeBitShift = 0x000011,
				.PageSizeBitShift = 0x0000000B,
				.EccSizeBitShift = 0x00000006,
				.HMACSizeShift = 0x00000005,
				.PageCopyMask = 0x0001,
				.SupportPageCopy = 0x0000,
				.EccDataCheckByteOffset = 000,
				.Padding = { 0x00, 0x00 },
			},
			.ChipType = 0x03,
			.ChipAttributes1 = 0x03,
			.ChipAttributes2 = 0x3e,
			.ChipAttributes3 = 0x0e,
			.ChipAttributes4 = 0x7f,
			.Padding4 = { 0x00, 0x00, 0x00 }
		},
		.Extension = {
			.Unknown = 0,
			.ClockDivisorValue = 0x01,
			.Padding = { 0x00, 0x00 }
		}
	},
	// Toshiba TC58NVG1D4BTG00
	{
		.Info = {
			.ChipId = 0x98DA,
			.Padding = { 0x00, 0x00},
			.Commands = {
				.Reset = 0xFF,
				.ReadPrefix = 0x00,
				.Read = 0x30,
				.ReadAlternative = 0xfe,
				.ReadPost = 0xFE,
				.ReadCopyBack = 0xFE,
				.Unknown = 0xFE,
				.WritePrefix = 0x80,
				.Write = 0x10,
				.WriteCopyBack = 0xFE,
				.Unknown2 = 0xfe,
				.WriteUnknown = 0xFE,
				.WriteCopyBackPrefix = 0xFE,
				.DeletePrefix = 0x60,
				.Unknown3 = 0xfe,
				.Delete = 0xD0,
				.RandomDataOutputPrefix = 0x05,
				.RandomDataOutput = 0xE0,
				.RandomDataInput = 0xFE,
				.ReadStatusPrefix = 0x70,
				.Unknown4 = 0xFE,
				.InputAddress = 0x1F,
				.Padding = { 0x00, 0x00 },
			},
			.SizeInfo = {
				.NandSizeBitShift = 0x0000001C,
				.BlockSizeBitShift = 0x000011,
				.PageSizeBitShift = 0x0000000B,
				.EccSizeBitShift = 0x00000006,
				.HMACSizeShift = 0x00000005,
				.PageCopyMask = 0x0001,
				.SupportPageCopy = 0x0000,
				.EccDataCheckByteOffset = 000,
				.Padding = { 0x00, 0x00 },
			},
			.ChipType = 0x04,
			.ChipAttributes1 = 0x03,
			.ChipAttributes2 = 0x3e,
			.ChipAttributes3 = 0x0e,
			.ChipAttributes4 = 0x7f,
			.Padding4 = { 0x00, 0x00, 0x00 }
		},
		.Extension = {
			.Unknown = 0,
			.ClockDivisorValue = 0x01,
			.Padding = { 0x00, 0x00 }
		}
	},
};

#define UNUSED_CMD         0xFE
#define DEFAULT_RESET_CMD  0xFF
#define DEFAULT_READID_CMD 0x90

static NandState NandStates[7] = { 0 };
s32 IrqMessageQueueId = 0;
s32 IoscMessageQueueId = 0;
NandInformation SelectedNandChip;
NandSizeInformation SelectedNandSizeInfo;
NandCommandLog NandInterfaceLog;

static u32 _ioscMessage;
static u32 _irqMessageQueue[4] = { 0 };
static u8 _nandInfoBuffer[0x40] = { 0 };
static u8 _readPageBuffer[0x800] __attribute__((aligned(0x40))) = { 0 };
static u8 _writePageBuffer[0x900] __attribute__((aligned(0x40))) = { 0 };

u8 EccBuffer[0x40 * 6] __attribute__((aligned(0x80))) = { 0 }; // max supported ecc is 0x40 bytes, 6 blocks

bool IsNandInitialized(void)
{
	return NandStates[0].Initialized;
}

static void LogCommand(u32 page, CommandType commandType, s32 returnValue)
{
	if (returnValue == 0)
	{
		if (commandType == DeleteCommand)
			NandInterfaceLog.SuccessfulDeletes++;
		else if (commandType == WriteCommand)
			NandInterfaceLog.SuccessfulWrites++;
		else if (commandType == ReadCommand)
			NandInterfaceLog.SuccessfulReads++;
		else
			NandInterfaceLog.Unknown4++;
	}
	else
	{
		//oh no, an error to log
		u32 index = 0;
		if (NandInterfaceLog.ErrorIndex == (ERROR_LOG_SIZE - 1))
		{
			index = NandInterfaceLog.ErrorOverflowIndex;
			NandInterfaceLog.ErrorOverflowIndex =
			    (NandInterfaceLog.ErrorOverflowIndex + 1) & (ERROR_LOG_SIZE - 2);
		}
		else
		{
			index = NandInterfaceLog.ErrorOverflowIndex + NandInterfaceLog.ErrorIndex;
			NandInterfaceLog.ErrorIndex++;
		}

		NandInterfaceLog.Errors[index].Page = page;
		NandInterfaceLog.Errors[index].CommandType = commandType;
		NandInterfaceLog.Errors[index].Return = returnValue;
	}
}

void SetNandData(void *data, void *ecc)
{
	if ((s32)data != -1)
	{
		write32(NAND_DATA, OSVirtualToPhysical((u32)data));
	}

	if ((s32)ecc != -1)
	{
		u32 addr = OSVirtualToPhysical((u32)ecc);
		if (addr & 0x7f)
			printk("NAND: Spare buffer 0x%08x is not aligned, data will be corrupted\n", addr);
		write32(NAND_ECC, addr);
	}
}

void SetNandAddress(u32 pageOffset, u32 pageNumber)
{
	if ((s32)pageOffset != -1)
		write32(NAND_ADDR0, pageOffset);

	if ((s32)pageNumber != -1)
		write32(NAND_ADDR1, pageNumber);
}

static s32 SendNandCommand(u8 cmd, u8 address, u32 flags, u32 dataLength)
{
	if (cmd == UNUSED_CMD)
		return IPC_EINVAL;

	s32 ret = 0;
	const NandCommand command = { .Fields = { .Execute = 1,
		                                      .Wait = (flags & WaitFlag) > 0,
		                                      .GenerateIrq = (flags & IrqFlag) > 0,
		                                      .CalculateEEC = (flags & EccFlag) > 0,
		                                      .ReadData = (flags & ReadFlag) > 0,
		                                      .WriteData = (flags & WriteFlag) > 0,
		                                      .Command = cmd,
		                                      .Address = (u8)(address & 0x1F),
		                                      .DataLength = dataLength & 0x0FFF } };
	write32(NAND_CMD, command.Value);
	if ((flags & IrqFlag) > 0)
	{
		u32 message;
		ret = OSReceiveMessage(IrqMessageQueueId, &message, 0);
		if (ret != 0 || message != 1)
		{
			ret = IPC_UNKNOWN;
			goto return_error;
		}
	}
	else
	{
		//Wait for command to end
		while (READ_CMD().Fields.Execute)
		{
		}
	}

	if (!READ_CMD().Fields.HasError)
		return IPC_SUCCESS;

	ret = -1;
return_error:
	// Wait for command to end
	while (READ_CMD().Fields.Execute)
	{
	}
	NandCommand waitCommand = { .Fields = { .Execute = 1, .Wait = 1, .Command = DEFAULT_RESET_CMD } };
	write32(NAND_CMD, waitCommand.Value);
	return ret;
}

s32 InitializeNand(void)
{
	if (IsNandInitialized())
		return IPC_SUCCESS;

	// Enable NAND controller
	set32(NAND_CONF, 0x08000000);

	s32 ret = OSCreateMessageQueue(&_irqMessageQueue, 4);
	if (ret < 0)
	{
		ret = IPC_UNKNOWN;
		goto return_init;
	}

	IrqMessageQueueId = ret;
	ret = OSCreateMessageQueue(&_ioscMessage, 1);
	if (ret < 0)
	{
		ret = IPC_UNKNOWN;
		goto destroy_irq_return;
	}

	IoscMessageQueueId = ret;
	ret = OSRegisterEventHandler(IRQ_NAND, IrqMessageQueueId, (void *)1);
	if (ret != 0)
		goto destroy_iosc_return;

	// Reset/init the interface
	ret = SendNandCommand(DEFAULT_RESET_CMD, 0, IrqFlag | WaitFlag, 0);
	if (ret != 0)
		goto destroy_and_return;

	OSDCInvalidateRange(_nandInfoBuffer, ARRAY_LENGTH(_nandInfoBuffer));
	SetNandAddress(0, (u32)-1);
	SetNandData(_nandInfoBuffer, (void *)-1);
	ret = SendNandCommand(DEFAULT_READID_CMD, 1, ReadFlag, 0x40);
	if (ret != 0)
		goto destroy_and_return;

	OSAhbFlushFrom(AHB_NAND);
	OSAhbFlushTo(AHB_STARLET);

	int index = 0;
	for (; index < 10; index++)
	{
		if (SupportedNandChips[index].Info.ChipId != *(u16 *)_nandInfoBuffer)
			continue;

		memcpy(&SelectedNandChip, &SupportedNandChips[index], sizeof(NandInformation));

		// Set config according to the nand information + force enable & 512MB chip
		u32 config = 0x88000000 | (SelectedNandChip.Info.ChipType << 0x1C) |
		             (SelectedNandChip.Info.ChipAttributes1 << 0x18) |
		             (SelectedNandChip.Info.ChipAttributes2 << 0x10) |
		             (SelectedNandChip.Info.ChipAttributes3 << 0x8) |
		             SelectedNandChip.Info.ChipAttributes4;

		write32(NAND_CONF, config);
		write32(NAND_CLKDIV, (read32(NAND_CLKDIV) & 0xFFFFFFFE) |
		                         SelectedNandChip.Extension.ClockDivisorValue);
		break;
	}

	if (index >= 10)
	{
		ret = IPC_UNKNOWN;
		goto destroy_and_return;
	}

	NandInterfaceLog.SuccessfulDeletes = 0;
	NandInterfaceLog.SuccessfulWrites = 0;
	NandInterfaceLog.SuccessfulReads = 0;
	NandInterfaceLog.ErrorIndex = 0;
	NandInterfaceLog.ErrorOverflowIndex = 0;
	NandStates[0].Initialized = 1;
	return 0;

destroy_and_return:
	OSUnregisterEventHandler(IRQ_NAND);
destroy_iosc_return:
	OSDestroyMessageQueue(IoscMessageQueueId);
destroy_irq_return:
	OSDestroyMessageQueue(IrqMessageQueueId);
return_init:
	// Read config & disable its enable pin
	clear32(NAND_CONF, 0x08000000);
	return ret;
}

static s32 ReadNandStatus(void)
{
	OSDCInvalidateRange(_nandInfoBuffer, ARRAY_LENGTH(_nandInfoBuffer));
	SetNandData(_nandInfoBuffer, (void *)-1);
	s32 ret = SendNandCommand(SelectedNandChip.Info.Commands.ReadStatusPrefix,
	                          0, ReadFlag, 0x40);
	if (ret != 0)
		return ret;

	OSAhbFlushFrom(AHB_NAND);
	OSAhbFlushTo(AHB_STARLET);

	return (s32)(_nandInfoBuffer[0] << 0x1F) < 0 ? IPC_BADBLOCK : IPC_SUCCESS;
}

s32 DeleteCluster(u16 cluster)
{
	s32 ret;

	// Validate cluster is within NAND bounds
	const u32 maxClusters =
	    1 << ((SelectedNandChip.Info.SizeInfo.NandSizeBitShift - CLUSTER_SIZE_SHIFT) & 0xFF);

	if (cluster >= maxClusters)
	{
		ret = IPC_EINVAL;
		goto logAndReturn;
	}

	if (!IsNandInitialized())
	{
		ret = IPC_NOTREADY;
		goto logAndReturn;
	}

	// Clear bit 31 of NAND_CONF before erase
	clear32(NAND_CONF, 0x80000000);

	// Convert cluster to page number: cluster << (CLUSTER_SIZE_SHIFT - PageSizeBitShift)
	u32 pageNumber =
	    (u32)cluster
	    << ((CLUSTER_SIZE_SHIFT - SelectedNandChip.Info.SizeInfo.PageSizeBitShift) & 0xFF);
	SetNandAddress(0xFFFFFFFF, pageNumber);

	// Send delete prefix command (block erase setup)
	ret = SendNandCommand(SelectedNandChip.Info.Commands.DeletePrefix,
	                      SelectedNandChip.Info.Commands.InputAddress & 0x1C, 0, 0);
	if (ret != IPC_SUCCESS)
		goto logAndReturn;

	// Send delete command (block erase execute) with IRQ and wait
	ret = SendNandCommand(SelectedNandChip.Info.Commands.Delete, 0, IrqFlag | WaitFlag, 0);
	if (ret != IPC_SUCCESS)
		goto logAndReturn;

	// Read status to check if erase succeeded
	ret = ReadNandStatus();

logAndReturn:
	// Set bit 31 of NAND_CONF after erase
	set32(NAND_CONF, 0x80000000);
	LogCommand(cluster, DeleteCommand, ret);
	return ret;
}

// Copy a cluster page-by-page from source to destination
s32 CopyCluster(u16 srcCluster, u16 dstCluster)
{
	if (!IsNandInitialized())
		return IPC_ENOENT;

	s32 ret = IPC_SUCCESS;
	const u32 pagesPerCluster =
	    1 << ((CLUSTER_SIZE_SHIFT - SelectedNandChip.Info.SizeInfo.PageSizeBitShift) & 0xFF);

	// Copy each page within the cluster
	for (u32 i = 0; i < pagesPerCluster; i++)
	{
		u32 srcPage = (u32)srcCluster * pagesPerCluster + i;
		u32 dstPage = (u32)dstCluster * pagesPerCluster + i;

		ret = CopyPage(srcPage, dstPage);
		if (ret != IPC_SUCCESS)
			return ret;
	}

	return ret;
}

// Copy a single page from source to destination
s32 CopyPage(u32 srcPage, u32 dstPage)
{
	s32 ret = IPC_SUCCESS;
	bool usedReadBuffer = false;

	// Calculate max page count
	const u32 maxPages = 1 << ((SelectedNandChip.Info.SizeInfo.NandSizeBitShift -
	                            SelectedNandChip.Info.SizeInfo.PageSizeBitShift) &
	                           0xFF);

	// Validate parameters
	if (srcPage >= maxPages || dstPage >= maxPages || srcPage == dstPage)
		return IPC_EINVAL;

	if (!IsNandInitialized())
		return IPC_ENOENT;

	// Clear bit 31 of NAND_CONF
	clear32(NAND_CONF, 0x80000000);

	// Calculate shift amount for block comparison
	const u32 blockShift = (SelectedNandChip.Info.SizeInfo.BlockSizeBitShift -
	                        SelectedNandChip.Info.SizeInfo.PageSizeBitShift) &
	                       0xFF;

	// Check if chip supports copy-back and pages are in same block
	if (SelectedNandChip.Info.SizeInfo.SupportPageCopy == 0 ||
	    ((srcPage >> blockShift) & (SelectedNandChip.Info.SizeInfo.PageCopyMask - 1)) !=
	        ((dstPage >> blockShift) & (SelectedNandChip.Info.SizeInfo.PageCopyMask - 1)))
	{
		// Pages not in same block or no copy-back support - use read buffer
		usedReadBuffer = true;

		// Read source page into buffer
		ret = ReadNandPage(srcPage, _readPageBuffer, _writePageBuffer, true);
		if (ret != IPC_SUCCESS && ret != IPC_ECC && ret != IPC_ECC_CRIT)
			goto copyPageEnd;

		// Write buffer to destination page
		ret = WriteNandPage(dstPage, _readPageBuffer, _writePageBuffer, 0, true);
	}
	else
	{
		// Use NAND copy-back feature for same-block copies
		u8 readAddress = 0;

		// Determine read command based on chip type
		if (SelectedNandChip.Info.Commands.ReadPrefix == UNUSED_CMD)
			readAddress = (u8)SelectedNandChip.Info.Commands.InputAddress;
		else
		{
			// Setup for copy-back read
			SetNandAddress(0, srcPage);
			ret = SendNandCommand(SelectedNandChip.Info.Commands.ReadPrefix,
			                      SelectedNandChip.Info.Commands.InputAddress, 0, 0);
			if (ret != IPC_SUCCESS)
				goto copyPageEnd;
		}

		// Determine which read command to use
		u8 readCmd = SelectedNandChip.Info.Commands.ReadCopyBack == UNUSED_CMD ?
		                 SelectedNandChip.Info.Commands.Read :
		                 SelectedNandChip.Info.Commands.ReadCopyBack;

		// Read from source
		SetNandAddress(0, srcPage);
		ret = SendNandCommand(readCmd, readAddress, IrqFlag | WaitFlag, 0);
		if (ret != IPC_SUCCESS)
			goto copyPageEnd;

		// Write copy-back to destination
		SetNandAddress(0, dstPage);
		ret = SendNandCommand(SelectedNandChip.Info.Commands.WriteCopyBackPrefix,
		                      SelectedNandChip.Info.Commands.InputAddress, 0, 0);
		if (ret != IPC_SUCCESS)
			goto copyPageEnd;

		// Check if we need delete prefix
		if (SelectedNandChip.Info.Commands.WriteCopyBack != UNUSED_CMD)
		{
			ret = SendNandCommand(SelectedNandChip.Info.Commands.WriteCopyBack,
			                      0, IrqFlag | WaitFlag, 0);
			if (ret != IPC_SUCCESS)
				goto copyPageEnd;
		}

		// Read status to verify
		ret = ReadNandStatus();
	}

copyPageEnd:
	// Restore bit 31 of NAND_CONF
	set32(NAND_CONF, 0x80000000);

	// Only log if we used the read buffer path
	if (!usedReadBuffer)
	{
		LogCommand(dstPage >>
		               ((CLUSTER_SIZE_SHIFT - SelectedNandChip.Info.SizeInfo.PageSizeBitShift) & 0xFF),
		           CopyCommand, ret);
	}

	return ret;
}

static s32 GetNandSizeInfo(NandSizeInformation *dest)
{
	if (dest == NULL)
		return IPC_EINVAL;

	if (!IsNandInitialized())
		return IPC_NOTREADY;

	memcpy(dest, &SelectedNandChip.Info.SizeInfo, sizeof(NandSizeInformation));
	return IPC_SUCCESS;
}

s32 SelectNandSize(bool selectNandSize)
{
	s32 errno = IPC_SUCCESS;
	if (selectNandSize)
		errno = GetNandSizeInfo(&SelectedNandSizeInfo);

	return TranslateErrno(errno);
}

s32 CorrectNandData(void *dataBuffer, void *eccBuffer)
{
	s32 ret = IPC_SUCCESS;
	const u32 spareSize = 4 << (SelectedNandChip.Info.SizeInfo.PageSizeBitShift - 9);
	const u32 eccSize = 1 << (SelectedNandChip.Info.SizeInfo.EccSizeBitShift & 0xFF);
	const u32 readOffset = eccSize - spareSize;
	const u8 *ecc = (u8 *)eccBuffer;
	const u8 *data = (u8 *)dataBuffer;
	ret = memcmp(ecc + 0x40, ecc + readOffset, spareSize);

	// Data is correct, nothing to do
	if (ret == 0)
		return ret;

	for (u32 index = 0; index < spareSize; index++)
	{
		u32 eccCalc = *(u32 *)(ecc + 0x40 + (index * 4));
		u32 eccRead = *(u32 *)(ecc + readOffset + (index * 4));

		if (eccCalc == eccRead)
			continue;

		const u32 xoredEcc = (eccRead >> 0x18 | (eccRead & 0xFF0000) >> 0x08 |
		                      (eccRead & 0xFF00) << 0x08 | (eccRead < 0x18)) ^
		                     (eccCalc >> 0x18 | (eccCalc & 0xFF0000) >> 0x08 |
		                      (eccCalc & 0xFF00) << 0x08 | (eccCalc < 0x18));
		u32 syndrome = xoredEcc & 0x0FFF0FFF;
		// Single-bit error in ECC
		if (!((syndrome - 1) & syndrome))
		{
			ret = IPC_ECC;
			continue;
		}

		u32 unknown = syndrome >> 0x10;
		// Is it still recoverable?
		if ((((syndrome | 0xFFFFF000) ^ unknown) & 0xFFFF) != 0xFFFF)
			return IPC_ECC_CRIT;

		// Select bit 3-12
		u32 location = (unknown >> 3) & 0x1FF;
		u8 *dataPointer = (u8 *)(data + location + index * 0x200);
		u8 correctedByte = (1 << (unknown & 0x07)) ^ *dataPointer;
		// lol memcpy for 1 byte? silly ios, must be for the mem1 bug i suppose
		memcpy(dataPointer, &correctedByte, 1);
		ret = IPC_ECC;
	}

	//this can't be right. it cant be returning an error on ecc error, right? :/
	return ret;
}

s32 ReadNandPage(u32 pageNumber, void *data, void *ecc, bool readEcc)
{
	s32 ret = 0;
	u8 read_address = 0;
	u32 flags;

	// See if page is within the pages of the nand
	const u32 maxPage = 1 << ((SelectedNandChip.Info.SizeInfo.NandSizeBitShift -
	                           SelectedNandChip.Info.SizeInfo.PageSizeBitShift) &
	                          0xFF);
	if (pageNumber >= maxPage || data == NULL)
	{
		ret = IPC_EINVAL;
		goto return_read;
	}

	if (!IsNandInitialized())
	{
		ret = IPC_NOTREADY;
		goto return_read;
	}

	const u32 pageSize = 1 << (SelectedNandChip.Info.SizeInfo.PageSizeBitShift & 0xFF);
	const u32 eccSize = 1 << (SelectedNandChip.Info.SizeInfo.EccSizeBitShift & 0xFF);
	const u32 spareSize =
	    4 << ((SelectedNandChip.Info.SizeInfo.PageSizeBitShift - 9) & 0xFF);
	SetNandAddress(0, pageNumber);
	if (SelectedNandChip.Info.Commands.ReadPrefix != UNUSED_CMD)
	{
		ret = SendNandCommand(SelectedNandChip.Info.Commands.ReadPrefix,
		                      SelectedNandChip.Info.Commands.InputAddress, 0, 0);
		if (ret != 0)
			goto return_read;
	}
	else
		read_address = SelectedNandChip.Info.Commands.InputAddress;

	if (!readEcc)
		SetNandData(_readPageBuffer, (void *)-1);
	else
		SetNandData(data, EccBuffer);

	if (!readEcc)
		OSDCInvalidateRange(_readPageBuffer, pageSize + eccSize);
	else
	{
		OSDCInvalidateRange(data, pageSize);
		OSDCInvalidateRange(EccBuffer, eccSize);
		//unknown why OSDCInvalidateRange is called again for ecc + 0x40. some kind of seperate buffer we didnt notice?
		OSDCInvalidateRange(EccBuffer + 0x40, spareSize);
	}

	flags = IrqFlag | WaitFlag | ReadFlag | (!readEcc ? 0 : EccFlag);
	ret = SendNandCommand(SelectedNandChip.Info.Commands.Read, read_address, flags, pageSize);
	if (ret != 0)
		goto return_read;

	OSAhbFlushFrom(AHB_NAND);
	OSAhbFlushTo(AHB_STARLET);

	if (ecc != NULL)
	{
		if (readEcc == 0)
			memcpy(EccBuffer, &_readPageBuffer[pageSize], eccSize);
		else
			memcpy(ecc, EccBuffer, eccSize);
	}

	if (readEcc == 0)
		memcpy(data, _readPageBuffer, pageSize);
	else
		ret = CorrectNandData(data, EccBuffer);

return_read:
	LogCommand(pageNumber >> (0xe - SelectedNandChip.Info.SizeInfo.PageSizeBitShift),
	           ReadCommand, ret);
	return ret;
}

s32 WriteNandPage(u32 pageNumber, void *data, void *ecc, u8 unknownWriteflag, bool writeEcc)
{
	s32 ret;
	u8 cmd;
	u32 spareSize;
	u32 flags;
	bool readPage = false;

	// See if page is within the pages of the nand
	const u32 maxPage = 1 << ((SelectedNandChip.Info.SizeInfo.NandSizeBitShift -
	                           SelectedNandChip.Info.SizeInfo.PageSizeBitShift) &
	                          0xFF);
	if (pageNumber >= maxPage || data == NULL)
	{
		ret = IPC_EINVAL;
		goto return_write;
	}

	if (!IsNandInitialized())
	{
		ret = IPC_NOTREADY;
		goto return_write;
	}

	write32(NAND_CONF, read32(NAND_CONF) & 0x7FFFFFFF);
	if (unknownWriteflag == 0 || SelectedNandChip.Info.Commands.WriteUnknown == 0xFE)
	{
		unknownWriteflag = 0;
		cmd = SelectedNandChip.Info.Commands.Write;
	}
	else
		cmd = SelectedNandChip.Info.Commands.WriteUnknown;

	if (ecc == NULL)
		memset(EccBuffer, 0, 1 << (SelectedNandChip.Info.SizeInfo.EccSizeBitShift & 0xFF));
	else
		memcpy(EccBuffer, ecc, 1 << (SelectedNandChip.Info.SizeInfo.EccSizeBitShift & 0xFF));

	EccBuffer[SelectedNandChip.Info.SizeInfo.EccDataCheckByteOffset] = 0xFF;
	spareSize = 4 << ((SelectedNandChip.Info.SizeInfo.PageSizeBitShift - 9) & 0xFF);
	OSDCFlushRange(data, 1 << (SelectedNandChip.Info.SizeInfo.PageSizeBitShift & 0xFF));
	OSAhbFlushTo(AHB_NAND);
	if (writeEcc)
		OSDCInvalidateRange(&EccBuffer[0x40], spareSize);

	SetNandAddress(0, pageNumber);
	SetNandData(data, EccBuffer);
	flags = IrqFlag | WriteFlag | (writeEcc ? EccFlag : 0);
	ret = SendNandCommand(SelectedNandChip.Info.Commands.WritePrefix,
	                      SelectedNandChip.Info.Commands.InputAddress, flags,
	                      1 << (SelectedNandChip.Info.SizeInfo.PageSizeBitShift & 0xFF));
	if (ret != 0)
		goto return_write;

	if (writeEcc)
	{
		OSAhbFlushFrom(AHB_NAND);
		OSAhbFlushTo(AHB_NAND);
		const u32 offset =
		    ((1 << (SelectedNandChip.Info.SizeInfo.EccSizeBitShift & 0xff)) - spareSize);
		memcpy(&EccBuffer[offset], &EccBuffer[0x40], spareSize);
	}

	// Do we write out the ecc or skip it?
	if (ecc != NULL || writeEcc)
	{
		if (SelectedNandChip.Info.Commands.RandomDataInput == 0xFE)
		{
			spareSize = 0;
			ret = SendNandCommand(cmd, 0, IrqFlag | WaitFlag, 0);
			if (ret != 0)
				goto return_write;
			else if (unknownWriteflag == 0)
			{
				ret = ReadNandStatus();
				if (ret != 0)
					goto return_write;
			}

			if (SelectedNandChip.Info.Commands.ReadPost == 0xFE)
				spareSize = 1 << (SelectedNandChip.Info.SizeInfo.PageSizeBitShift & 0xFF);
			else
			{
				ret = SendNandCommand(SelectedNandChip.Info.Commands.ReadPost, 0, 0, 0);
				if (ret != 0)
					goto return_write;
				readPage = true;
			}

			OSDCFlushRange(EccBuffer,
			               1 << (SelectedNandChip.Info.SizeInfo.EccSizeBitShift & 0xFF));
			OSAhbFlushTo(AHB_NAND);
			SetNandAddress(spareSize, pageNumber);
			SetNandData(EccBuffer, (void *)0xFFFFFFFF);
			ret = SendNandCommand(
			    SelectedNandChip.Info.Commands.WritePrefix,
			    SelectedNandChip.Info.Commands.InputAddress, IrqFlag | WriteFlag,
			    1 << (SelectedNandChip.Info.SizeInfo.EccSizeBitShift & 0xFF));
		}
		else
		{
			OSDCFlushRange(EccBuffer,
			               1 << (SelectedNandChip.Info.SizeInfo.EccSizeBitShift & 0xFF));
			OSAhbFlushTo(AHB_NAND);
			SetNandAddress(1 << (SelectedNandChip.Info.SizeInfo.PageSizeBitShift & 0xFF),
			               0xFFFFFFFF);
			SetNandData(EccBuffer, (void *)0xFFFFFFFF);

			ret = SendNandCommand(
			    SelectedNandChip.Info.Commands.RandomDataInput,
			    SelectedNandChip.Info.Commands.InputAddress & 3, IrqFlag | WriteFlag,
			    1 << (SelectedNandChip.Info.SizeInfo.EccSizeBitShift & 0xFF));
		}

		if (ret != 0)
			goto return_write;
	}

	ret = SendNandCommand(cmd, 0, IrqFlag | WaitFlag, 0);
	if (ret == 0 && unknownWriteflag == 0)
		ret = ReadNandStatus();

return_write:
	if (readPage)
		ret = SendNandCommand(SelectedNandChip.Info.Commands.Read, 0, 0, 0);

	set32(NAND_CONF, 0x08000000);
	LogCommand(pageNumber >> (0xe - SelectedNandChip.Info.SizeInfo.PageSizeBitShift),
	           WriteCommand, ret);
	return ret;
}

s32 CheckNandBlock(u8 block)
{
	// Is block higher than max block on detected nand
	if (block > (1 << ((SelectedNandChip.Info.SizeInfo.NandSizeBitShift - 0x0E) & 0xFF)))
		return IPC_EINVAL;

	if (NandStates[0].Initialized == 0)
		return IPC_NOTREADY;

	s32 ret;
	u32 unkn =
	    (u32)(block >> (((u8)SelectedNandChip.Info.SizeInfo.BlockSizeBitShift - 0x0E) & 0xFF))
	    << ((SelectedNandChip.Info.SizeInfo.BlockSizeBitShift -
	         SelectedNandChip.Info.SizeInfo.PageSizeBitShift) &
	        0xFF);
	for (u32 i = 0; i < 2; i++)
	{
		if (SelectedNandChip.Info.Commands.ReadPost != UNUSED_CMD)
		{
			OSDCInvalidateRange(EccBuffer, 1 << (SelectedNandChip.Info.SizeInfo.EccSizeBitShift &
			                                     0xFF));
			SetNandAddress(0, unkn + i);
			SetNandData(EccBuffer, (void *)-1);
			ret = SendNandCommand(
			    SelectedNandChip.Info.Commands.ReadPost,
			    SelectedNandChip.Info.Commands.InputAddress, IrqFlag | ReadFlag | WaitFlag,
			    1 << (SelectedNandChip.Info.SizeInfo.EccSizeBitShift & 0xFF));
			if (ret != 0)
				return ret;
			OSAhbFlushFrom(AHB_NAND);
			OSAhbFlushTo(AHB_STARLET);
			ret = SendNandCommand(SelectedNandChip.Info.Commands.Read, 0, 0, 0);
			if (ret != 0)
				return ret;
		}
		else
		{
			SetNandAddress(1 << (SelectedNandChip.Info.SizeInfo.PageSizeBitShift & 0xFF),
			               unkn + i);
			ret = SendNandCommand(SelectedNandChip.Info.Commands.ReadPrefix,
			                      SelectedNandChip.Info.Commands.InputAddress, 0, 0);
			if (ret != 0)
				return ret;

			OSDCInvalidateRange(EccBuffer, 1 << (SelectedNandChip.Info.SizeInfo.EccSizeBitShift &
			                                     0xFF));
			SetNandData(EccBuffer, (void *)-1);
			ret = SendNandCommand(
			    SelectedNandChip.Info.Commands.Read, 0, IrqFlag | ReadFlag | WaitFlag,
			    1 << (SelectedNandChip.Info.SizeInfo.EccSizeBitShift & 0xFF));
			if (ret != 0)
				return 0;

			OSAhbFlushFrom(AHB_NAND);
			OSAhbFlushTo(AHB_STARLET);
			ret = 0;
		}

		if (EccBuffer[SelectedNandChip.Info.SizeInfo.EccDataCheckByteOffset] != 0xff)
			return IPC_BADBLOCK;
	}

	return ret;
}
