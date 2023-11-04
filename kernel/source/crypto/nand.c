/*
	starstruck - a Free Software reimplementation for the Nintendo/BroadOn IOS.
	nand - the nand interface

	Copyright (C) 2021	DacoTaco
	Copyright (C) 2023	Jako

# This code is licensed to you under the terms of the GNU GPL, version 2;
# see file COPYING or http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt
*/

#include "crypto/nand.h"
#include "crypto/seeprom.h"
#include <ios/errno.h>

u16 NAND_ComputeCounterChecksum(const NAND_Counter* data)
{
	u16 sum = 0;
	for(s32 i = 0; i < ARRAY_LENGTH(data->Data); ++i)
	{
		sum += data->Data[i];
	}
	return sum;
}

u32 NAND_GetGen(void)
{
	NAND_Counter data;
	s32 counter_write_index = -1;

	s32 ret = SEEPROM_NAND_GetCounter(&data, &counter_write_index);
	if (ret != IPC_SUCCESS)
		return ret;

	return data.NandGen;
}
s32 NAND_UpdateGen(void)
{
	NAND_Counter data;
	s32 counter_write_index = -1;

	s32 ret = SEEPROM_NAND_GetCounter(&data, &counter_write_index);
	if (ret != IPC_SUCCESS)
		return ret;

	data.NandGen += 1;
	data.Checksum = NAND_ComputeCounterChecksum(&data);
	ret = SEEPROM_NAND_UpdateCounter(&data, counter_write_index);

	return ret;
}
