/*
	starstruck - a Free Software reimplementation for the Nintendo/BroadOn IOS.
	boot2 - the boot2 interface

	Copyright (C) 2021	DacoTaco

# This code is licensed to you under the terms of the GNU GPL, version 2;
# see file COPYING or http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt
*/

#ifndef __BOOT2_H__
#define __BOOT2_H__

#include <types.h>

typedef struct
{
	union
	{
		struct
		{
			u8 Version;
			u8 Unk1;
			u8 Unk2;
			u8 Padding;
			u32 UpdateTag;
		} __attribute__((packed));
		u16 Data[4];
	};
	u16 Checksum;
} BOOT2_Counter;
CHECK_SIZE(BOOT2_Counter, 10);
CHECK_OFFSET(BOOT2_Counter, 0x00, Data);
CHECK_OFFSET(BOOT2_Counter, 0x08, Checksum);

u16 BOOT2_ComputeCounterChecksum(const BOOT2_Counter *counter);

s32 BOOT2_GetVersion(void);
s32 BOOT2_GetUnk1(void);
s32 BOOT2_GetUnk2(void);

s32 BOOT2_UpdateVersion(void);
s32 BOOT2_UpdateUnk1(void);
s32 BOOT2_UpdateUnk2(void);

#endif
