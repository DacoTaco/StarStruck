/*
	StarStruck - a Free Software reimplementation for the Nintendo/BroadOn IOS.
	Copyright (C) 2022	DacoTaco

# This code is licensed to you under the terms of the GNU GPL, version 2;
# see file COPYING or http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt
*/
#include <errno.h>
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

NandInformation SelectedNandChip;
const NandInformation SupportedNandChips[10] MODULE_DATA = {
	// Hynix HY27US0812(1/2)B
	{
		.Info = {
			.ChipId = 0xAD76,
			.Unknown = { 0x00, 0x00 },
			.Reset = 0xFF,
			.ReadPrefix = 0xFE,
			.Read = 0x00,
			.ReadAlternative = 0x01,
			.ReadPost = 0x50,
			.ReadCopyBack = 0xFE,
			.Unknown2 = 0xFE,
			.WritePrefix = 0x80,
			.Write = 0x10,
			.WriteCopyBack = 0xFE,
			.Unknown3 = { 0xfe, 0xfe },
			.WriteCopyBackPrefix = 0x8A,
			.DeletePrefix = 0x60,
			.Unknown4 = 0xFE,
			.Delete = 0xD0,
			.RandomDataOutputPrefix = 0xFE,
			.RandomDataOutput = 0xFE,
			.RandomDataInput = 0xFE,
			.ReadStatusPrefix = 0x70,
			.Unknown6 = { 0xfe, 0x1d, 0x00, 0x00, 0x00, 0x00, 0x00, 0x1a, 0x00, 0x00, 0x00, 0x0e, 0x00, 0x00, 0x00, 0x09, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 0x02, 0x00, 0x01, 0x00, 0x00, 0x00, 0x05, 0x00, 0x00 },
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
			.Unknown = { 0x00, 0x00 },
			.Reset = 0xFF,
			.ReadPrefix = 0x00,
			.Read = 0x30,
			.ReadAlternative = 0xFE,
			.ReadPost = 0xFE,
			.ReadCopyBack = 0x35,
			.Unknown2 = 0xFE,
			.WritePrefix = 0x80,
			.Write = 0x10,
			.WriteCopyBack = 0x10,
			.Unknown3 = { 0xfe, 0xfe },
			.WriteCopyBackPrefix = 0x85,
			.DeletePrefix = 0x60,
			.Unknown4 = 0xFE,
			.Delete = 0xD0,
			.RandomDataOutputPrefix = 0x05,
			.RandomDataOutput = 0xE0,
			.RandomDataInput = 0x85,
			.ReadStatusPrefix = 0x70,
			.Unknown6 = { 0xfe, 0x0f, 0x00, 0x00, 0x00, 0x00, 0x00, 0x1b, 0x00, 0x00, 0x00, 0x11, 0x00, 0x00, 0x00, 0x0b, 0x00, 0x00, 0x00, 0x06, 0x00, 0x00, 0x00, 0x05, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
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
			.Unknown = { 0x00, 0x00 },
			.Reset = 0xFF,
			.ReadPrefix = 0x00,
			.Read = 0x30,
			.ReadAlternative = 0xFE,
			.ReadPost = 0xFE,
			.ReadCopyBack = 0x35,
			.Unknown2 = 0xFE,
			.WritePrefix = 0x80,
			.Write = 0x10,
			.WriteCopyBack = 0x10,
			.Unknown3 = { 0xfe, 0xfe },
			.WriteCopyBackPrefix = 0x85,
			.DeletePrefix = 0x60,
			.Unknown4 = 0xFE,
			.Delete = 0xD0,
			.RandomDataOutputPrefix = 0x05,
			.RandomDataOutput = 0xE0,
			.RandomDataInput = 0x85,
			.ReadStatusPrefix = 0x70,
			.Unknown6 = { 0xfe, 0x1f, 0x00, 0x00, 0x00, 0x00, 0x00, 0x1d, 0x00, 0x00, 0x00, 0x11, 0x00, 0x00, 0x00, 0x0b, 0x00, 0x00, 0x00, 0x06, 0x00, 0x00, 0x00, 0x05, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
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
			.Unknown = { 0x00, 0x00 },
			.Reset = 0xFF,
			.ReadPrefix = 0xFE,
			.Read = 0x00,
			.ReadAlternative = 0x01,
			.ReadPost = 0x50,
			.ReadCopyBack = 0xFE,
			.Unknown2 = 0x03,
			.WritePrefix = 0x80,
			.Write = 0x10,
			.WriteCopyBack = 0x10,
			.Unknown3 = { 0x11, 0xfe },
			.WriteCopyBackPrefix = 0x8A,
			.DeletePrefix = 0x60,
			.Unknown4 = 0x60,
			.Delete = 0xD0,
			.RandomDataOutputPrefix = 0xFE,
			.RandomDataOutput = 0xFE,
			.RandomDataInput = 0xFE,
			.ReadStatusPrefix = 0x70,
			.Unknown6 = { 0x71, 0x1d, 0x00, 0x00, 0x00, 0x00, 0x00, 0x1a, 0x00, 0x00, 0x00, 0x0e, 0x00, 0x00, 0x00, 0x09, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 0x02, 0x00, 0x04, 0x00, 0x00, 0x00, 0x05, 0x00, 0x00 },
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
			.Unknown = { 0x00, 0x00 },
			.Reset = 0xFF,
			.ReadPrefix = 0x00,
			.Read = 0x30,
			.ReadAlternative = 0xfe,
			.ReadPost = 0xFE,
			.ReadCopyBack = 0x35,
			.Unknown2 = 0xFE,
			.WritePrefix = 0x80,
			.Write = 0x10,
			.WriteCopyBack = 0x10,
			.Unknown3 = { 0xfe, 0xfe },
			.WriteCopyBackPrefix = 0x85,
			.DeletePrefix = 0x60,
			.Unknown4 = 0xfe,
			.Delete = 0xD0,
			.RandomDataOutputPrefix = 0x05,
			.RandomDataOutput = 0xE0,
			.RandomDataInput = 0x85,
			.ReadStatusPrefix = 0x70,
			.Unknown6 = { 0xfe, 0x0f, 0x00, 0x00, 0x00, 0x00, 0x00, 0x1b, 0x00, 0x00, 0x00, 0x11, 0x00, 0x00, 0x00, 0x0b, 0x00, 0x00, 0x00, 0x06, 0x00, 0x00, 0x00, 0x05, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
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
			.Unknown = { 0x00, 0x00 },
			.Reset = 0xFF,
			.ReadPrefix = 0x00,
			.Read = 0x30,
			.ReadAlternative = 0xfe,
			.ReadPost = 0xFE,
			.ReadCopyBack = 0x35,
			.Unknown2 = 0xFE,
			.WritePrefix = 0x80,
			.Write = 0x10,
			.WriteCopyBack = 0x10,
			.Unknown3 = { 0xfe, 0xfe },
			.WriteCopyBackPrefix = 0x85,
			.DeletePrefix = 0x60,
			.Unknown4 = 0xfe,
			.Delete = 0xD0,
			.RandomDataOutputPrefix = 0x05,
			.RandomDataOutput = 0xE0,
			.RandomDataInput = 0x85,
			.ReadStatusPrefix = 0x70,
			.Unknown6 = { 0xfe, 0x1f, 0x00, 0x00, 0x00, 0x00, 0x00, 0x1c, 0x00, 0x00, 0x00, 0x11, 0x00, 0x00, 0x00, 0x0b, 0x00, 0x00, 0x00, 0x06, 0x00, 0x00, 0x00, 0x05, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
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
			.Unknown = { 0x00, 0x00 },
			.Reset = 0xFF,
			.ReadPrefix = 0x00,
			.Read = 0x30,
			.ReadAlternative = 0xfe,
			.ReadPost = 0xFE,
			.ReadCopyBack = 0x35,
			.Unknown2 = 0xFE,
			.WritePrefix = 0x80,
			.Write = 0x10,
			.WriteCopyBack = 0x10,
			.Unknown3 = { 0xfe, 0xfe },
			.WriteCopyBackPrefix = 0x85,
			.DeletePrefix = 0x60,
			.Unknown4 = 0xfe,
			.Delete = 0xD0,
			.RandomDataOutputPrefix = 0x05,
			.RandomDataOutput = 0xE0,
			.RandomDataInput = 0x85,
			.ReadStatusPrefix = 0x70,
			.Unknown6 = { 0xfe, 0x1f, 0x00, 0x00, 0x00, 0x00, 0x00, 0x1d, 0x00, 0x00, 0x00, 0x11, 0x00, 0x00, 0x00, 0x0b, 0x00, 0x00, 0x00, 0x06, 0x00, 0x00, 0x00, 0x05, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
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
			.Unknown = { 0x00, 0x00 },
			.Reset = 0xFF,
			.ReadPrefix = 0xFE,
			.Read = 0x00,
			.ReadAlternative = 0x01,
			.ReadPost = 0x50,
			.ReadCopyBack = 0xFE,
			.Unknown2 = 0xFE,
			.WritePrefix = 0x80,
			.Write = 0x10,
			.WriteCopyBack = 0xFE,
			.Unknown3 = { 0x11, 0xfe },
			.WriteCopyBackPrefix = 0xFE,
			.DeletePrefix = 0x60,
			.Unknown4 = 0x60,
			.Delete = 0xD0,
			.RandomDataOutputPrefix = 0xFE,
			.RandomDataOutput = 0xFE,
			.RandomDataInput = 0xFE,
			.ReadStatusPrefix = 0x70,
			.Unknown6 = { 0x71, 0x1d, 0x00, 0x00, 0x00, 0x00, 0x00, 0x1a, 0x00, 0x00, 0x00, 0x0e, 0x00, 0x00, 0x00, 0x09, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 0x02, 0x00, 0x04, 0x00, 0x00, 0x00, 0x05, 0x00, 0x00 },
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
			.Unknown = { 0x00, 0x00 },
			.Reset = 0xFF,
			.ReadPrefix = 0x00,
			.Read = 0x30,
			.ReadAlternative = 0xfe,
			.ReadPost = 0xFE,
			.ReadCopyBack = 0xFE,
			.Unknown2 = 0xFE,
			.WritePrefix = 0x80,
			.Write = 0x10,
			.WriteCopyBack = 0xFE,
			.Unknown3 = { 0xfe, 0xfe },
			.WriteCopyBackPrefix = 0xFE,
			.DeletePrefix = 0x60,
			.Unknown4 = 0xfe,
			.Delete = 0xD0,
			.RandomDataOutputPrefix = 0x05,
			.RandomDataOutput = 0xE0,
			.RandomDataInput = 0xFE,
			.ReadStatusPrefix = 0x70,
			.Unknown6 = { 0xfe, 0x0f, 0x00, 0x00, 0x00, 0x00, 0x00, 0x1b, 0x00, 0x00, 0x00, 0x11, 0x00, 0x00, 0x00, 0x0b, 0x00, 0x00, 0x00, 0x06, 0x00, 0x00, 0x00, 0x05, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
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
			.Unknown = { 0x00, 0x00 },
			.Reset = 0xFF,
			.ReadPrefix = 0x00,
			.Read = 0x30,
			.ReadAlternative = 0xfe,
			.ReadPost = 0xFE,
			.ReadCopyBack = 0xFE,
			.Unknown2 = 0xFE,
			.WritePrefix = 0x80,
			.Write = 0x10,
			.WriteCopyBack = 0xFE,
			.Unknown3 = { 0xfe, 0xfe },
			.WriteCopyBackPrefix = 0xFE,
			.DeletePrefix = 0x60,
			.Unknown4 = 0xfe,
			.Delete = 0xD0,
			.RandomDataOutputPrefix = 0x05,
			.RandomDataOutput = 0xE0,
			.RandomDataInput = 0xFE,
			.ReadStatusPrefix = 0x70,
			.Unknown6 = { 0xfe, 0x1f, 0x00, 0x00, 0x00, 0x00, 0x00, 0x1c, 0x00, 0x00, 0x00, 0x11, 0x00, 0x00, 0x00, 0x0b, 0x00, 0x00, 0x00, 0x06, 0x00, 0x00, 0x00, 0x05, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
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

u8 _unknown_global1 = 0;
u8 _unknown_global2 = 0;
u8 _unknown_global3 = 0;
u8 _unknown_global4 = 0;
u8 _unknown_global5 = 0;
u8 _nandInitialized = 0;
u32 _irqMessageQueueId = 0;
u32 _ioscMessageQueueId = 0;
static u32 _ioscMessage;
static u32 _irqMessageQueue[4];
static u8 _commandBuffer[40];

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
void SetNandAddress(u32 pageOffset, u32 pageNummer)
{
	if (0 < (s32)pageOffset) 
		write32(NAND_ADDR0, pageOffset);
	
	if (0 < (s32)pageNummer)  
		write32(NAND_ADDR1, pageNummer);
}
s32 SendCommand(u8 cmd, u32 address, u32 flags, u32 dataLength)
{
	if(cmd == UNUSED_CMD)
		return -4;

	s32 ret = 0;
	NandCommand command = {
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
		ret = OSReceiveMessage(_irqMessageQueueId, &message, 0);
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
	if(_nandInitialized)
		return 0;

	// enable NAND controller
	write32(NAND_CONF, 0x08000000);

	s32 ret = OSCreateMessageQueue(&_irqMessageQueue, 4);
	if(ret < 0)
	{
		ret = -9;
		goto return_init;
	}

	_irqMessageQueueId = (u32)ret;
	ret = OSCreateMessageQueue(&_ioscMessage, 1);
	if(ret < 0)
	{
		ret = -9;
		goto destroy_irq_return;
	}

	_ioscMessageQueueId = (u32)ret;
	ret = OSRegisterEventHandler(IRQ_NAND, _irqMessageQueueId, (void*)1);
	if(ret != 0)
		goto destroy_iosc_return;

	//reset/init the interface
	ret = SendCommand(DEFAULT_RESET_CMD, 0, IrqFlag | WaitFlag, 0);
	if(ret != 0)
		goto destroy_and_return;

	//TODO : continue here. flush some data that needs to be figured out what it is/contains/used for
	OSDCInvalidateRange(_commandBuffer, 0x40);
	SetNandAddress(0, (u32)-1);
	SetNandData(_commandBuffer, (void*)-1);
	ret = SendCommand(DEFAULT_READID_CMD, 1, ReadFlag, 0x40);
	if(ret != 0)
		goto destroy_and_return;
	
	OSAhbFlushFrom(AHB_NAND);
	OSAhbFlushTo(AHB_STARLET);

	int index = 0;
	for(; index < 10; index++)
	{
		if(SupportedNandChips[index].Info.ChipId != *(u16*)_commandBuffer)
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

	_unknown_global1 = 0;
	_unknown_global2 = 0;
	_unknown_global3 = 0;
	_unknown_global4 = 0;
	_unknown_global5 = 0;
	_nandInitialized = 1;
	return 0;

destroy_and_return:
	OSUnregisterEventHandler(IRQ_NAND);
destroy_iosc_return:
	OSDestroyMessageQueue(_ioscMessageQueueId);
destroy_irq_return:
	OSDestroyMessageQueue(_irqMessageQueueId);
return_init:
	//read config & disable its enable pin
	write32(NAND_CONF, read32(NAND_CONF) & 0xF7FFFFFF);
	return ret;
}