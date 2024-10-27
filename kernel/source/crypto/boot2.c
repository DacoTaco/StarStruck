/*
	starstruck - a Free Software reimplementation for the Nintendo/BroadOn IOS.
	boot2 - the boot2 interface

	Copyright (C) 2021	DacoTaco

# This code is licensed to you under the terms of the GNU GPL, version 2;
# see file COPYING or http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt
*/

#include "crypto/boot2.h"
#include "crypto/seeprom.h"
#include <ios/errno.h>

u16 BOOT2_ComputeCounterChecksum(const BOOT2_Counter* data)
{
	u16 sum = 0;
	for(u32 i = 0; i < ARRAY_LENGTH(data->Data); ++i)
	{
		sum += data->Data[i];
	}
	return sum;
}

s32 BOOT2_GetVersion(void)
{
	BOOT2_Counter data;
	s32 counter_write_index = -1;

	s32 ret = SEEPROM_BOOT2_GetCounter(&data, &counter_write_index);
	if(ret == IPC_SUCCESS)
	{
		ret = data.Version;
	}

	return ret;
}
s32 BOOT2_GetUnk1(void)
{
	BOOT2_Counter data;
	s32 counter_write_index = -1;

	s32 ret = SEEPROM_BOOT2_GetCounter(&data, &counter_write_index);
	if(ret == IPC_SUCCESS)
	{
		ret = data.Unk1;
	}

	return ret;
}
s32 BOOT2_GetUnk2(void)
{
	BOOT2_Counter data;
	s32 counter_write_index = -1;

	s32 ret = SEEPROM_BOOT2_GetCounter(&data, &counter_write_index);
	if(ret == IPC_SUCCESS)
	{
		ret = data.Unk2;
	}

	return ret;
}

s32 BOOT2_UpdateVersion(void)
{
	BOOT2_Counter data;
	s32 counter_write_index = -1;

	s32 ret = SEEPROM_BOOT2_GetCounter(&data, &counter_write_index);
	if(ret != IPC_SUCCESS)
		return ret;

	data.Version += 1;
	data.UpdateTag += 1;
	data.Checksum =  BOOT2_ComputeCounterChecksum(&data);
	ret = SEEPROM_BOOT2_UpdateCounter(&data, counter_write_index);

	return ret;
}
s32 BOOT2_UpdateUnk1(void)
{
	BOOT2_Counter data;
	s32 counter_write_index = -1;

	s32 ret = SEEPROM_BOOT2_GetCounter(&data, &counter_write_index);
	if(ret != IPC_SUCCESS)
		return ret;

	data.Unk1 += 1;
	data.UpdateTag += 1;
	data.Checksum =  BOOT2_ComputeCounterChecksum(&data);
	ret = SEEPROM_BOOT2_UpdateCounter(&data, counter_write_index);

	return ret;
}
s32 BOOT2_UpdateUnk2(void)
{
	BOOT2_Counter data;
	s32 counter_write_index = -1;

	s32 ret = SEEPROM_BOOT2_GetCounter(&data, &counter_write_index);
	if(ret != IPC_SUCCESS)
		return ret;

	data.Unk2 += 1;
	data.UpdateTag += 1;
	data.Checksum =  BOOT2_ComputeCounterChecksum(&data);
	ret = SEEPROM_BOOT2_UpdateCounter(&data, counter_write_index);

	return ret;
}
