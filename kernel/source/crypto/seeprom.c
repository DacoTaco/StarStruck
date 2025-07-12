/*
	starstruck - a Free Software reimplementation for the Nintendo/BroadOn IOS.
	seeprom - the seeprom chip

	Copyright (C) 2021	DacoTaco
	Copyright (C) 2023	Jako

# This code is licensed to you under the terms of the GNU GPL, version 2;
# see file COPYING or http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt
*/

#include <ios/errno.h>
#include <ios/processor.h>
#include <string.h>

#include "crypto/seeprom.h"
#include "crypto/otp.h"
#include "core/hollywood.h"
#include "core/gpio.h"
#include "utils.h"
#include "interrupt/irq.h"

#define SEEPROM_Wait() BusyDelay(125)

static u8 SEEPROM_Dummy_CommonKey[OTP_COMMONKEY_SIZE] = { 0 };
static u32 SEEPROM_Dummy_MsId = 3;
static u32 SEEPROM_Dummy_CaId = 2;
static u32 SEEPROM_Dummy_NgKeyId = 0x69D2A2EF;
static u8 SEEPROM_Dummy_NgSignature[60] = {
	0x00, 0x50, 0xcc, 0x52, 0xdf, 0x22, 0x17, 0xf1, 0xe9, 0x3e, 0xe1, 0x9d,
	0x5b, 0xb0, 0xe7, 0x51, 0x0e, 0x5b, 0xf4, 0xcb, 0xd2, 0x0b, 0x9f, 0xa4,
	0xf4, 0x39, 0x45, 0x2b, 0x14, 0x9b, 0x00, 0xbd, 0xd5, 0x81, 0x3b, 0x42,
	0xdd, 0x86, 0x03, 0xaa, 0x0b, 0xd5, 0x90, 0x9e, 0x8d, 0x5b, 0x3e, 0xd8,
	0x2c, 0x57, 0x5b, 0x54, 0xe7, 0x5a, 0x01, 0x1f, 0x27, 0xb8, 0xa5, 0xf2
};

typedef struct
{
	u8 Boot2Version;
	u8 Unknown1;
	u8 Unknown2;
	u8 Pad;
	u32 UpdateTag;
	u16 Checksum;
} __attribute__((packed)) EepCounter;
CHECK_SIZE(EepCounter, 0x0A);
CHECK_OFFSET(EepCounter, 0x00, Boot2Version);
CHECK_OFFSET(EepCounter, 0x01, Unknown1);
CHECK_OFFSET(EepCounter, 0x02, Unknown2);
CHECK_OFFSET(EepCounter, 0x03, Pad);
CHECK_OFFSET(EepCounter, 0x04, UpdateTag);
CHECK_OFFSET(EepCounter, 0x08, Checksum);

typedef struct
{
	union
	{
		struct
		{
			u32 MsId;
			u32 CaId;
			u32 NgKeyId;
			u8 NgSignature[60];
			EepCounter Counters[2];
			u8 Padding[0x18];
			u8 KoreanKey[16];
		};
		u8 Data[256];
	};
} __attribute__((packed)) SeepRom;

CHECK_SIZE(SeepRom, 256);
CHECK_OFFSET(SeepRom, 0x00, Data);
CHECK_OFFSET(SeepRom, 0x00, MsId);
CHECK_OFFSET(SeepRom, 0x04, CaId);
CHECK_OFFSET(SeepRom, 0x08, NgKeyId);
CHECK_OFFSET(SeepRom, 0x0C, NgSignature);
CHECK_OFFSET(SeepRom, 0x48, Counters);
CHECK_OFFSET(SeepRom, 0x5C, Padding);
CHECK_OFFSET(SeepRom, 0x74, KoreanKey);

#define SEEPROM_ChipSelectHigh() set32(HW_GPIO1OUT, GP_EEP_CS)
#define SEEPROM_ChipSelectLow()  clear32(HW_GPIO1OUT, GP_EEP_CS)

#define SEEPROM_ClockEnable()    set32(HW_GPIO1OUT, GP_EEP_CLK)
#define SEEPROM_ClockDisable()   clear32(HW_GPIO1OUT, GP_EEP_CLK)

static void SEEPROM_SetMOSI(const u32 bit)
{
	if (bit)
		set32(HW_GPIO1OUT, GP_EEP_MOSI);
	else
		clear32(HW_GPIO1OUT, GP_EEP_MOSI);
}

static s32 SEEPROM_SendBits(u32 numBits, const u32 value)
{
	if (numBits > 32)
		return IPC_EINVAL;

	while (numBits)
	{
		--numBits;

		SEEPROM_ClockDisable();
		SEEPROM_ChipSelectHigh();
		SEEPROM_SetMOSI((value >> numBits) & 1);
		SEEPROM_Wait();
		SEEPROM_ClockEnable();
		SEEPROM_Wait();
	}

	return IPC_SUCCESS;
}

static s32 SEEPROM_SendZeros(u32 numBits)
{
	while (numBits)
	{
		--numBits;

		SEEPROM_ClockDisable();
		SEEPROM_ChipSelectLow();
		SEEPROM_SetMOSI(0);
		SEEPROM_Wait();
		SEEPROM_ClockEnable();
		SEEPROM_Wait();
	}

	return IPC_SUCCESS;
}

static s32 SEEPROM_ReceiveBits(u32 numBits, u32 *data)
{
	if (numBits > 32)
		return IPC_EINVAL;

	u32 out = 0;
	while (numBits)
	{
		--numBits;

		SEEPROM_ClockDisable();
		SEEPROM_ChipSelectHigh();
		SEEPROM_Wait();
		SEEPROM_ClockEnable();
		SEEPROM_Wait();
		out |= (read32(HW_GPIO1IN) & GP_EEP_MISO) ? (1u << numBits) : 0u;
	}
	*data = out;

	return IPC_SUCCESS;
}

static s32 SEEPROM_WaitNotBusy(void)
{
	u32 timeout = 0;
	s32 ret = IPC_SUCCESS;
	for (; timeout < 100; ++timeout)
	{
		u32 readVal = 0;
		ret = SEEPROM_ReceiveBits(10, &readVal);
		if ((readVal & 1) == 1)
			break;
	}

	SEEPROM_SendZeros(2);
	if (timeout == 100)
	{
		ret = IPC_UNKNOWN;
	}

	return ret;
}

static s32 SEEPROM_SendWriteEnable(void)
{
	s32 ret = SEEPROM_SendBits(11, 0x4c0);
	if (ret != IPC_SUCCESS)
		return ret;

	SEEPROM_SendZeros(2);
	return ret;
}
static s32 SEEPROM_SendWriteDisable(void)
{
	s32 ret = SEEPROM_SendBits(11, 0x400);
	if (ret != IPC_SUCCESS)
		return ret;

	SEEPROM_SendZeros(2);
	return ret;
}
static s32 SEEPROM_SendWord(const u32 word_offset, const u16 value)
{
	s32 ret = SEEPROM_SendBits(27, ((u32)(0x500 | (word_offset & 0xff)) << 16) | value);
	if (ret != IPC_SUCCESS)
		return ret;

	SEEPROM_SendZeros(2);
	ret = SEEPROM_WaitNotBusy();

	return ret;
}

static s32 SEEPROM_ReadWord(const u32 word_offset, u32 *data)
{
	s32 ret = SEEPROM_SendBits(11, 0x600 | (word_offset & 0xff));
	if (ret != IPC_SUCCESS)
		return ret;

	ret = SEEPROM_ReceiveBits(16, data);
	if (ret == IPC_SUCCESS)
		SEEPROM_SendZeros(2);

	return ret;
}
static s32 SEEPROM_ReadBuffer(u32 word_offset, void *data, u32 word_count)
{
	u32 receivedData;

	char *destination = (char *)data;
	for (u32 i = 0; i < word_count; ++i)
	{
		s32 ret = SEEPROM_ReadWord(word_offset + i, &receivedData);
		if (ret != IPC_SUCCESS)
			return ret;

		destination[0] = (receivedData >> 8) & 0xff;
		destination[1] = receivedData & 0xff;
		destination += sizeof(u16);
	}

	return IPC_SUCCESS;
}

void SEEPROM_GetKoreanCommonKey(u8 data[OTP_COMMONKEY_SIZE])
{
	const u32 cookie = DisableInterrupts();

	if (OTP_IsSet())
	{
		SEEPROM_ReadBuffer(0x3a, data, OTP_COMMONKEY_SIZE / sizeof(u16));
	}
	else
	{
		memcpy(data, SEEPROM_Dummy_CommonKey, OTP_COMMONKEY_SIZE);
	}

	RestoreInterrupts(cookie);
}
void SEEPROM_GetIdsAndNg(char ms_id_str[0x40], char ca_id_str[0x40],
                         u32 *ng_key_id, char ng_id_str[0x40], u8 ng_signature[60])
{
	const u32 cookie = DisableInterrupts();
	u32 ng_id;
	u32 ms_id, ca_id;

	if (OTP_IsSet())
	{
		SEEPROM_ReadBuffer(0, &ms_id, sizeof(SEEPROM_Dummy_MsId) / sizeof(u16));
		SEEPROM_ReadBuffer(2, &ca_id, sizeof(SEEPROM_Dummy_CaId) / sizeof(u16));
		SEEPROM_ReadBuffer(4, ng_key_id, sizeof(SEEPROM_Dummy_NgKeyId) / sizeof(u16));
		SEEPROM_ReadBuffer(6, ng_signature,
		                   sizeof(SEEPROM_Dummy_NgSignature) / sizeof(u16));
	}
	else
	{
		memcpy(&ms_id, &SEEPROM_Dummy_MsId, sizeof(SEEPROM_Dummy_MsId));
		memcpy(&ca_id, &SEEPROM_Dummy_CaId, sizeof(SEEPROM_Dummy_CaId));
		memcpy(ng_key_id, &SEEPROM_Dummy_NgKeyId, sizeof(SEEPROM_Dummy_NgKeyId));
		memcpy(ng_signature, SEEPROM_Dummy_NgSignature, sizeof(SEEPROM_Dummy_NgSignature));
	}

	RestoreInterrupts(cookie);

	OTP_GetNgId(&ng_id);
	snprintf(ms_id_str, 0x40, "MS%08x", ms_id);
	snprintf(ca_id_str, 0x40, "CA%08x", ca_id);
	snprintf(ng_id_str, 0x40, "NG%08x", ng_id);
}

s32 SEEPROM_GetPRNGSeed(void)
{
	u32 loword = 0;
	s32 ret = SEEPROM_ReadWord(0x7c, &loword);
	if (ret != IPC_SUCCESS)
		return ret;

	u32 hiword = 0;
	ret = SEEPROM_ReadWord(0x7d, &hiword);
	if (ret != IPC_SUCCESS)
		return ret;

	return (s32)((hiword << 16) | loword);
}
s32 SEEPROM_UpdatePRNGSeed(void)
{
	u32 hiword = 0;
	u32 loword = 0;

	s32 ret = SEEPROM_ReadWord(0x7c, &loword);
	if (ret != IPC_SUCCESS)
		return ret;

	ret = SEEPROM_ReadWord(0x7d, &hiword);
	if (ret != IPC_SUCCESS)
		return ret;

	if (loword == 0xFFFF)
	{
		loword = 0;

		if (hiword == 0xFFFF)
		{
			hiword = 0;
		}
		else
		{
			hiword += 1;
		}
	}
	else
	{
		loword += 1;
	}

	ret = SEEPROM_SendWriteEnable();
	if (ret != IPC_SUCCESS)
		return ret;

	ret = SEEPROM_SendWord(0x7c, loword & 0xffff);
	if (ret == IPC_SUCCESS)
		ret = SEEPROM_SendWord(0x7d, hiword & 0xffff);

	SEEPROM_SendWriteDisable();

	return ret;
}

s32 SEEPROM_BOOT2_GetCounter(BOOT2_Counter *data, s32 *counter_write_index)
{
	s32 ret = IPC_SUCCESS;
	BOOT2_Counter read_data[2] = { 0 };

	for (u32 current_counter = 0; current_counter < 2; ++current_counter)
	{
		char *const data_ptr = (char *)&read_data[current_counter];
		u32 value = 0;
		for (u32 i = 0; i < sizeof(BOOT2_Counter); i += sizeof(u16))
		{
			ret = SEEPROM_ReadWord(
			    (0x48 + current_counter * sizeof(BOOT2_Counter) + i) / sizeof(u16), &value);
			const u16 word_value = value & 0xffff;
			memcpy(&data_ptr[i], &word_value, sizeof(u16));
			if (ret != IPC_SUCCESS)
				break;
		}
	}

	s32 index = -1;
	void *ptr_out = NULL;
	u32 highest_value = 0;
	u32 lowest_value = (u32)-1;
	for (u32 current_counter = 0; current_counter < 2; ++current_counter)
	{
		if (BOOT2_ComputeCounterChecksum(&read_data[current_counter]) ==
		    read_data[current_counter].Checksum)
		{
			char *const data_ptr = (char *)(&read_data[current_counter].UpdateTag);
			const u32 current_value = (u32)(data_ptr[3]) | ((u32)(data_ptr[2]) << 8) |
			                          ((u32)(data_ptr[1]) << 16) |
			                          ((u32)(data_ptr[0]) << 24);
			if (highest_value <= current_value)
			{
				ptr_out = data_ptr;
				highest_value = current_value;
			}
			if (current_value < lowest_value)
			{
				index = (s32)current_counter;
				lowest_value = current_value;
			}
		}
		else
		{
			index = (s32)current_counter;
			lowest_value = 0;
		}
	}

	if (ptr_out == NULL || index < 0)
	{
		ret = IPC_EMAX;
	}
	else
	{
		memcpy(data, ptr_out, sizeof(BOOT2_Counter));
		*counter_write_index = index;
	}

	return ret;
}
s32 SEEPROM_BOOT2_UpdateCounter(const BOOT2_Counter *data, s32 counter_write_index)
{
	if (0 > counter_write_index || counter_write_index >= 2)
		return IPC_EINVAL;

	s32 ret = SEEPROM_SendWriteEnable();
	if (ret != IPC_SUCCESS)
		return ret;

	const char *const data_ptr = (const char *)data;
	u16 value = 0;
	for (u32 i = 0; i < sizeof(*data); i += sizeof(u16))
	{
		memcpy(&value, &data_ptr[i], sizeof(u16));
		ret = SEEPROM_SendWord(
		    (0x48 + (u32)counter_write_index * sizeof(*data) + i) / sizeof(u16), value);
		if (ret != IPC_SUCCESS)
			break;
	}

	SEEPROM_SendWriteDisable();
	return ret;
}

s32 SEEPROM_NAND_GetCounter(NAND_Counter *data, s32 *counter_write_index)
{
	s32 ret = IPC_SUCCESS;
	NAND_Counter read_data[3] = { 0 };

	for (u32 current_counter = 0; current_counter < 3; ++current_counter)
	{
		char *const data_ptr = (char *)&read_data[current_counter];
		u32 value = 0;
		for (u32 i = 0; i < sizeof(NAND_Counter); i += sizeof(u16))
		{
			ret = SEEPROM_ReadWord(
			    (0x5c + current_counter * sizeof(NAND_Counter) + i) / sizeof(u16), &value);
			const u16 word_value = value & 0xffff;
			memcpy(&data_ptr[i], &word_value, sizeof(u16));
			if (ret != IPC_SUCCESS)
				break;
		}
	}

	s32 index = -1;
	void *ptr_out = NULL;
	u32 highest_value = 0;
	u32 lowest_value = (u32)-1;
	for (u32 current_counter = 0; current_counter < 3; ++current_counter)
	{
		if (NAND_ComputeCounterChecksum(&read_data[current_counter]) ==
		    read_data[current_counter].Checksum)
		{
			char *const data_ptr = (char *)(&read_data[current_counter].NandGen);
			const u32 current_value = (u32)(data_ptr[3]) | ((u32)(data_ptr[2]) << 8) |
			                          ((u32)(data_ptr[1]) << 16) |
			                          ((u32)(data_ptr[0]) << 24);
			if (highest_value <= current_value)
			{
				ptr_out = data_ptr;
				highest_value = current_value;
			}
			if (current_value < lowest_value)
			{
				index = (s32)current_counter;
				lowest_value = current_value;
			}
		}
		else
		{
			index = (s32)current_counter;
			lowest_value = 0;
		}
	}

	if (ptr_out == NULL || index < 0)
	{
		ret = IPC_EMAX;
	}
	else
	{
		memcpy(data, ptr_out, sizeof(NAND_Counter));
		*counter_write_index = index;
	}

	return ret;
}
s32 SEEPROM_NAND_UpdateCounter(const NAND_Counter *data, s32 counter_write_index)
{
	if (0 > counter_write_index || counter_write_index >= 3)
		return IPC_EINVAL;

	s32 ret = SEEPROM_SendWriteEnable();
	if (ret != IPC_SUCCESS)
		return ret;

	const char *const data_ptr = (const char *)data;
	u16 value = 0;
	for (u32 i = 0; i < sizeof(*data); i += sizeof(u16))
	{
		memcpy(&value, &data_ptr[i], sizeof(u16));
		ret = SEEPROM_SendWord(
		    (0x5c + (u32)counter_write_index * sizeof(*data) + i) / sizeof(u16), value);
		if (ret != IPC_SUCCESS)
			break;
	}

	SEEPROM_SendWriteDisable();
	return ret;
}
