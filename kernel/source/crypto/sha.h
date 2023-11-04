/*
	starstruck - a Free Software reimplementation for the Nintendo/BroadOn IOS.
	sha - the sha engine in starlet

	Copyright (C) 2021	DacoTaco
	Copyright (C) 2023	Jako

# This code is licensed to you under the terms of the GNU GPL, version 2;
# see file COPYING or http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt
*/

#ifndef __SHA_H__
#define __SHA_H__

#include <types.h>

#define SHA_DEVICE_NAME 		"/dev/sha"
#define SHA_DEVICE_NAME_SIZE 	sizeof(SHA_DEVICE_NAME)
#define SHA_BLOCK_SIZE 			0x40
#define SHA_NUM_WORDS 			5

typedef enum 
{
	InitShaState = 0x00,
	ContributeShaState = 0x01,
	FinalizeShaState = 0x02,
} ShaCommandType;

typedef u32 FinalShaHash[SHA_NUM_WORDS];
CHECK_SIZE(FinalShaHash, 0x14);

#pragma pack(push, 1)
typedef struct
{
	u32 ShaStates[SHA_NUM_WORDS];
	u64 Length;	//length in bits of total data contributed to SHA-1 hash
} ShaContext;
#pragma pack(pop)
CHECK_SIZE(ShaContext, 0x1C);
CHECK_OFFSET(ShaContext, 0x00, ShaStates);
CHECK_OFFSET(ShaContext, 0x14, Length);

void ShaEngineHandler(void);

#endif