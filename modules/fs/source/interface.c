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

#include "interface.h"

//for more info : 
//https://wiibrew.org/wiki/Hardware/NAND_Interface

typedef union {
	struct {
		u32 Execute : 1;
		u32 GenerateIrq : 1;
		u32 HasError : 1;
		u32 Address : 5;
		u32 Command : 8;
		u32 Wait : 1;
		u32 WriteData : 1;
		u32 ReadData : 1;
		u32 CalculateEEC : 1;
		u32 DataLength : 12;
	} Fields;
	u32 Value;
} NandCommand;
CHECK_SIZE(NandCommand, 4);

typedef struct {
	u32 Initialized;
	u32 Opened;
} InterfaceState;

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
				.Unknown = { 0x00, 0x00, 0x00, 0x0e },
				.PageSizeBitShift = 0x00000009,
				.EccSizeBitShift = 0x00000004,
				.Unknown2 = { 0x00, 0x00, 0x00, 0x02, 0x00, 0x01, 0x00, 0x00, 0x00, 0x05, 0x00, 0x00 },
			},
			.ChipType = 0x04,
			.ChipAttributes1 = 0x03,
			.ChipAttributes2 = 0x3e,
			.ChipAttributes3 = 0x0e,
			.ChipAttributes4 = 0x7f,
			.Padding = { 0x00, 0x00, 0x00 }
		},
		.Extension = {
			.Unknown = 0,
			.UnknownRegister = 0x01,
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
				.Unknown = { 0x00, 0x00, 0x00, 0x11 },
				.PageSizeBitShift = 0x0000000B,
				.EccSizeBitShift = 0x00000006,
				.Unknown2 = { 0x00, 0x00, 0x00, 0x05, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
			},
			.ChipType = 0x03,
			.ChipAttributes1 = 0x03,
			.ChipAttributes2 = 0x3e,
			.ChipAttributes3 = 0x0e,
			.ChipAttributes4 = 0x7f,
			.Padding = { 0x00, 0x00, 0x00 }
		},
		.Extension = {
			.Unknown = 0,
			.UnknownRegister = 0x01,
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
				.Unknown = { 0x00, 0x00, 0x00, 0x11 },
				.PageSizeBitShift = 0x0000000B,
				.EccSizeBitShift = 0x00000006,
				.Unknown2 = { 0x00, 0x00, 0x00, 0x05, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
			},
			.ChipType = 0x07,
			.ChipAttributes1 = 0x04,
			.ChipAttributes2 = 0x3f,
			.ChipAttributes3 = 0x3f,
			.ChipAttributes4 = 0xff,
			.Padding = { 0x00, 0x00, 0x00 }
		},
		.Extension = {
			.Unknown = 0,
			.UnknownRegister = 0x00,
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
				.Unknown = { 0x00, 0x00, 0x00, 0x0e },
				.PageSizeBitShift = 0x00000009,
				.EccSizeBitShift = 0x00000004,
				.Unknown2 = { 0x00, 0x00, 0x00, 0x02, 0x00, 0x04, 0x00, 0x00, 0x00, 0x05, 0x00, 0x00 },
			},
			.ChipType = 0x04,
			.ChipAttributes1 = 0x03,
			.ChipAttributes2 = 0x3e,
			.ChipAttributes3 = 0x0e,
			.ChipAttributes4 = 0x7f,
			.Padding = { 0x00, 0x00, 0x00 }
		},
		.Extension = {
			.Unknown = 0,
			.UnknownRegister = 0x01,
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
				.Unknown = { 0x00, 0x00, 0x00, 0x11 },
				.PageSizeBitShift = 0x0000000B,
				.EccSizeBitShift = 0x00000006,
				.Unknown2 = { 0x00, 0x00, 0x00, 0x05, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
			},
			.ChipType = 0x03,
			.ChipAttributes1 = 0x03,
			.ChipAttributes2 = 0x3e,
			.ChipAttributes3 = 0x3e,
			.ChipAttributes4 = 0x7f,
			.Padding = { 0x00, 0x00, 0x00 }
		},
		.Extension = {
			.Unknown = 0,
			.UnknownRegister = 0x01,
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
				.Unknown = { 0x00, 0x00, 0x00, 0x11 },
				.PageSizeBitShift = 0x0000000B,
				.EccSizeBitShift = 0x00000006,
				.Unknown2 = { 0x00, 0x00, 0x00, 0x05, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
			},			
			.ChipType = 0x04,
			.ChipAttributes1 = 0x03,
			.ChipAttributes2 = 0x3e,
			.ChipAttributes3 = 0x0e,
			.ChipAttributes4 = 0x7f,
			.Padding = { 0x00, 0x00, 0x00 }
		},
		.Extension = {
			.Unknown = 0,
			.UnknownRegister = 0x01,
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
				.Unknown = { 0x00, 0x00, 0x00, 0x11 },
				.PageSizeBitShift = 0x0000000B,
				.EccSizeBitShift = 0x00000006,
				.Unknown2 = { 0x00, 0x00, 0x00, 0x05, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
			},
			.ChipType = 0x07,
			.ChipAttributes1 = 0x04,
			.ChipAttributes2 = 0x3f,
			.ChipAttributes3 = 0x3f,
			.ChipAttributes4 = 0xff,
			.Padding = { 0x00, 0x00, 0x00 }
		},
		.Extension = {
			.Unknown = 0,
			.UnknownRegister = 0x00,
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
				.Unknown = { 0x00, 0x00, 0x00, 0x0e },
				.PageSizeBitShift = 0x00000009,
				.EccSizeBitShift = 0x00000004,
				.Unknown2 = { 0x00, 0x00, 0x00, 0x02, 0x00, 0x04, 0x00, 0x00, 0x00, 0x05, 0x00, 0x00 },
			},
			.ChipType = 0x04,
			.ChipAttributes1 = 0x03,
			.ChipAttributes2 = 0x3e,
			.ChipAttributes3 = 0x0e,
			.ChipAttributes4 = 0x7f,
			.Padding = { 0x00, 0x00, 0x00 }
		},
		.Extension = {
			.Unknown = 0,
			.UnknownRegister = 0x01,
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
				.Unknown = { 0x00, 0x00, 0x00, 0x11 },
				.PageSizeBitShift = 0x0000000B,
				.EccSizeBitShift = 0x00000006,
				.Unknown2 = { 0x00, 0x00, 0x00, 0x05, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
			},
			.ChipType = 0x03,
			.ChipAttributes1 = 0x03,
			.ChipAttributes2 = 0x3e,
			.ChipAttributes3 = 0x0e,
			.ChipAttributes4 = 0x7f,
			.Padding = { 0x00, 0x00, 0x00 }
		},
		.Extension = {
			.Unknown = 0,
			.UnknownRegister = 0x01,
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
				.Unknown = { 0x00, 0x00, 0x00, 0x11 },
				.PageSizeBitShift = 0x0000000B,
				.EccSizeBitShift = 0x00000006,
				.Unknown2 = { 0x00, 0x00, 0x00, 0x05, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
			},
			.ChipType = 0x04,
			.ChipAttributes1 = 0x03,
			.ChipAttributes2 = 0x3e,
			.ChipAttributes3 = 0x0e,
			.ChipAttributes4 = 0x7f,
			.Padding = { 0x00, 0x00, 0x00 }
		},
		.Extension = {
			.Unknown = 0,
			.UnknownRegister = 0x01,
			.Padding = { 0x00, 0x00 }
		}
	},
};

/* NAND Registers */

#define		NAND_REG_BASE		0xd010000

#define		NAND_CMD			(NAND_REG_BASE + 0x000)
#define		NAND_STATUS			NAND_CMD
#define		NAND_CONF			(NAND_REG_BASE + 0x004)
#define		NAND_ADDR0			(NAND_REG_BASE + 0x008)
#define		NAND_ADDR1			(NAND_REG_BASE + 0x00c)
#define		NAND_DATA			(NAND_REG_BASE + 0x010)
#define		NAND_ECC			(NAND_REG_BASE + 0x014)
#define		NAND_UNK1			(NAND_REG_BASE + 0x018)
#define		NAND_UNK2			(NAND_REG_BASE + 0x01c)

#define READ_CMD()				((NandCommand)read32(NAND_CMD))
#define UNUSED_CMD				0xFE
#define DEFAULT_RESET_CMD		0xFF
#define DEFAULT_READID_CMD		0x90

InterfaceState InterfaceStates[7] = {0};
#define IsInitialized() InterfaceStates[0].Initialized
u32 IrqMessageQueueId = 0;
u32 IoscMessageQueueId = 0;
NandInformation SelectedNandChip;
NandCommandLog InterfaceCommandLog;
static u32 _ioscMessage;
static u32 _irqMessageQueue[4] = { 0 };
static u8 _nandInfoBuffer[0x40] = { 0 };
static u8 _readPageBuffer[0x800] = { 0 };
static u8 _writePageBuffer[0x900] = { 0 };
static u8 _eccBuffer[0x40] = { 0 };
static u8 _unknown_19Bytes[0x13] = { 0 };

void LogCommand(u32 page, CommandType commandType, s32 returnValue)
{
	if(returnValue == 0)
	{
		if(commandType == DeleteCommand)
			InterfaceCommandLog.SuccessfulDeletes++;
		else if(commandType == UnknownCommandType)
			InterfaceCommandLog.Unknown2++;
		else if(commandType == ReadCommand)
			InterfaceCommandLog.SuccessfulReads++;
		else
			InterfaceCommandLog.Unknown4++;
	}
	else
	{
		//oh no, an error to log
		u32 index = 0;
		if(InterfaceCommandLog.ErrorIndex == (ERROR_LOG_SIZE -1))
		{
			index = InterfaceCommandLog.ErrorOverflowIndex;
			InterfaceCommandLog.ErrorOverflowIndex = (InterfaceCommandLog.ErrorOverflowIndex + 1) & (ERROR_LOG_SIZE -2);
		}
		else
		{
			index = InterfaceCommandLog.ErrorOverflowIndex + InterfaceCommandLog.ErrorIndex;
			InterfaceCommandLog.ErrorIndex++;
		}

		InterfaceCommandLog.Errors[index].Page = page;
		InterfaceCommandLog.Errors[index].CommandType = commandType;
		InterfaceCommandLog.Errors[index].Return = returnValue;
	}
}
void SetNandData(void* data, void* ecc)
{
	if ((s32)data != -1) {
		write32(NAND_DATA, OSVirtualToPhysical((u32)data));
	}

	if ((s32)ecc != -1) {
		u32 addr = OSVirtualToPhysical((u32)ecc);
		if(addr & 0x7f)
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
s32 SendNandCommand(u8 cmd, u32 address, u32 flags, u32 dataLength)
{
	if(cmd == UNUSED_CMD)
		return -4;

	s32 ret = 0;
	const NandCommand command = {
		.Fields = {
			.Execute = 1,
			.Wait = (flags & WaitFlag) > 0,
			.GenerateIrq = (flags & IrqFlag) > 0,
			.CalculateEEC = (flags & EccFlag) > 0,
			.ReadData = (flags & ReadFlag) > 0,
			.WriteData = (flags & WriteFlag) > 0,
			.Command = cmd,
			.Address = address & 0x1F,
			.DataLength = dataLength & 0x0FFF
		}
	};
	write32(NAND_CMD, command.Value);
	if((flags & IrqFlag) > 0)
	{
		u32 message;
		ret = OSReceiveMessage(IrqMessageQueueId, &message, 0);
		if(ret != 0 || message != 1)
		{
			ret = -9;
			goto return_error;
		}
	}
	else
	{
		//wait for command to end
		while(READ_CMD().Fields.Execute){}
	}

	if(!READ_CMD().Fields.HasError)
		return 0;

	ret = -1;
return_error:
	//wait for command to end
	while(READ_CMD().Fields.Execute){}
	NandCommand waitCommand = {
		.Fields = {
			.Execute = 1,
			.Wait = 1,
			.Command = DEFAULT_RESET_CMD
		}
	};
	write32(NAND_CMD, waitCommand.Value);
	return ret;
}
s32 InitializeNand()
{
	if(IsInitialized())
		return 0;

	// enable NAND controller
	write32(NAND_CONF, read32(NAND_CONF) | 0x08000000);

	s32 ret = OSCreateMessageQueue(&_irqMessageQueue, 4);
	if(ret < 0)
	{
		ret = -9;
		goto return_init;
	}

	IrqMessageQueueId = (u32)ret;
	ret = OSCreateMessageQueue(&_ioscMessage, 1);
	if(ret < 0)
	{
		ret = -9;
		goto destroy_irq_return;
	}

	IoscMessageQueueId = (u32)ret;
	ret = OSRegisterEventHandler(IRQ_NAND, IrqMessageQueueId, (void*)1);
	if(ret != 0)
		goto destroy_iosc_return;

	//reset/init the interface
	ret = SendNandCommand(DEFAULT_RESET_CMD, 0, IrqFlag | WaitFlag, 0);
	if(ret != 0)
		goto destroy_and_return;
	
	OSDCInvalidateRange(_nandInfoBuffer, ARRAY_LENGTH(_nandInfoBuffer));
	SetNandAddress(0, (u32)-1);
	SetNandData(_nandInfoBuffer, (void*)-1);
	ret = SendNandCommand(DEFAULT_READID_CMD, 1, ReadFlag, 0x40);
	if(ret != 0)
		goto destroy_and_return;
	
	OSAhbFlushFrom(AHB_NAND);
	OSAhbFlushTo(AHB_STARLET);

	int index = 0;
	for(; index < 10; index++)
	{
		if(SupportedNandChips[index].Info.ChipId != *(u16*)_nandInfoBuffer)
			continue;

		memcpy(&SelectedNandChip, &SupportedNandChips[index], sizeof(NandInformation));
		
		//set config according to the nand information + force enable & 512MB chip
		u32 config = 0x88000000 | (SelectedNandChip.Info.ChipType << 0x1C) |
			(SelectedNandChip.Info.ChipAttributes1 << 0x18) | (SelectedNandChip.Info.ChipAttributes2 << 0x10) |
			(SelectedNandChip.Info.ChipAttributes3 << 0x8) | SelectedNandChip.Info.ChipAttributes4;
		
		write32(NAND_CONF, config);
		write32(NAND_UNK1, (read32(NAND_UNK1) & 0xFFFFFFFE) | SelectedNandChip.Extension.UnknownRegister);
		break;
	}

	if(index >= 10)
	{
		ret = -9;
		goto destroy_and_return;
	}

	InterfaceCommandLog.SuccessfulDeletes = 0;
	InterfaceCommandLog.Unknown2 = 0;
	InterfaceCommandLog.SuccessfulReads = 0;
	InterfaceCommandLog.ErrorIndex = 0;
	InterfaceCommandLog.ErrorOverflowIndex = 0;
	InterfaceStates[0].Initialized = 1;
	return 0;

destroy_and_return:
	OSUnregisterEventHandler(IRQ_NAND);
destroy_iosc_return:
	OSDestroyMessageQueue(IoscMessageQueueId);
destroy_irq_return:
	OSDestroyMessageQueue(IrqMessageQueueId);
return_init:
	//read config & disable its enable pin
	write32(NAND_CONF, read32(NAND_CONF) & 0xF7FFFFFF);
	return ret;
}
s32 ReadNandStatus(void)
{
	OSDCInvalidateRange(_nandInfoBuffer, ARRAY_LENGTH(_nandInfoBuffer));
	SetNandData(_nandInfoBuffer, (void*)-1);
	s32 ret = SendNandCommand(SelectedNandChip.Info.Commands.ReadStatusPrefix, 0, ReadFlag, 0x40);
	if(ret != 0)
		return ret;

	OSAhbFlushFrom(AHB_NAND);
	OSAhbFlushTo(AHB_STARLET);
	if((s32)(_nandInfoBuffer[0] << 0x1F) < 0 )
		return IPC_EUNKN;
	return 0;
}
s32 CorrectNandData(void* data, void* ecc)
{
	s32 ret = 0;
	const u32 spareSize = 4 << (SelectedNandChip.Info.SizeInfo.PageSizeBitShift - 9);
	const u32 eccSize = 1 << (SelectedNandChip.Info.SizeInfo.EccSizeBitShift & 0xFF);
	const u32 readOffset = eccSize - spareSize;
	ret = memcmp(ecc + 0x40, ecc + readOffset, spareSize);
	
	//data is correct, nothing to do
	if(ret == 0)
		return ret;
	
	for(u32 index = 0; index < spareSize; index++)
	{
		u32 eccCalc = *(u32*)(ecc + 0x40 + (index*4));
		u32 eccRead = *(u32*)(ecc + readOffset + (index*4));
		
		if(eccCalc == eccRead)
			continue;

		const u32 xoredEcc = (eccRead >> 0x18 | (eccRead & 0xFF0000) >> 0x08 | (eccRead & 0xFF00) << 0x08 | (eccRead < 0x18))
							 ^
							 (eccCalc >> 0x18 | (eccCalc & 0xFF0000) >> 0x08 | (eccCalc & 0xFF00) << 0x08 | (eccCalc < 0x18));
		u32 syndrome = xoredEcc & 0x0FFF0FFF;
		//single-bit error in ECC
		if(!((syndrome - 1) & syndrome))
		{
			ret = IPC_EAGAIN;
			continue;
		}

		u32 unknown = syndrome >> 0x10;
		//is it still recoverable?
		if((((syndrome | 0xFFFFF000) ^ unknown) & 0xFFFF) != 0xFFFF)
			return -12;
		
		//select bit 3-12
		u32 location = (unknown >> 3) & 0x1FF;
		u8* dataPointer = (u8*)(data + location + index * 0x200);
		u8 correctedByte = (1 << (unknown & 0x07)) ^ *dataPointer;
		//lol memcpy for 1 byte? silly ios, must be for the mem1 bug i suppose
		memcpy(dataPointer, &correctedByte, 1);
		ret = IPC_EAGAIN;
	}
	
	//this can't be right. it cant be returning an error on ecc error, right? :/
	return ret;
}
s32 ReadNandPage(u32 pageNumber, void* data, void* ecc, u8 readEcc)
{
	s32 ret = 0;
	u32 read_address = 0;
	u32 flags;

	//see if page is within the pages of the nand
	const u32 maxPage = 1 << ((SelectedNandChip.Info.SizeInfo.NandSizeBitShift - SelectedNandChip.Info.SizeInfo.PageSizeBitShift) & 0xFF);
	if(pageNumber >= maxPage || data == NULL)
	{
		ret = -4;
		goto return_read;
	}

	if(!IsInitialized())
	{
		ret = -10;
		goto return_read;
	}

	const u32 pageSize = 1 << SelectedNandChip.Info.SizeInfo.PageSizeBitShift;
	const u32 eccSize = 1 << SelectedNandChip.Info.SizeInfo.EccSizeBitShift;
	const u32 spareSize = 4 << (SelectedNandChip.Info.SizeInfo.PageSizeBitShift - 9);
	SetNandAddress(0, pageNumber);
	if(SelectedNandChip.Info.Commands.ReadPrefix != UNUSED_CMD)
	{
		ret = SendNandCommand(SelectedNandChip.Info.Commands.ReadPrefix, SelectedNandChip.Info.Commands.InputAddress, 0, 0);
		if(ret != 0)
			goto return_read;
	}
	else
		read_address = SelectedNandChip.Info.Commands.InputAddress;

	if(!readEcc)
		SetNandData(_readPageBuffer, (void*)-1);
	else
		SetNandData(data, _eccBuffer);

	if(!readEcc)
		OSDCInvalidateRange(_readPageBuffer, pageSize + eccSize);
	else
	{
		OSDCInvalidateRange(data, pageSize);
		OSDCInvalidateRange(_eccBuffer, eccSize);
		OSDCInvalidateRange(_unknown_19Bytes, spareSize);
	}

	flags = IrqFlag | WaitFlag | ReadFlag | (!readEcc ? 0 : EccFlag);
	ret = SendNandCommand(SelectedNandChip.Info.Commands.Read, read_address, flags, pageSize);
	if(ret != 0)
		goto return_read;
	
	OSAhbFlushFrom(AHB_NAND);
	OSAhbFlushTo(AHB_STARLET);

	if(ecc != NULL)
	{
		if(readEcc == 0)
			memcpy(_eccBuffer, &_readPageBuffer[pageSize], eccSize);
		else
			memcpy(ecc, _eccBuffer, eccSize);
	}

	if(readEcc == 0)
		memcpy(data, _readPageBuffer, pageSize);
	else
		ret = CorrectNandData(data, _eccBuffer);

return_read:
	LogCommand(pageNumber >> (0xe - SelectedNandChip.Info.SizeInfo.PageSizeBitShift), ReadCommand, ret);
	return ret;
}