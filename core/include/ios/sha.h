/*
	StarStruck - a Free Software reimplementation for the Nintendo/BroadOn IOS.
	Copyright (C) 2021	DacoTaco
	Copyright (C) 2023	Jako

# This code is licensed to you under the terms of the GNU GPL, version 2;
# see file COPYING or http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt
*/

#pragma once

#include "types.h"

#define SHA_BLOCK_SIZE      0x40
#define SHA_NUM_WORDS       5
#define MAX_HMAC_CHUNK_SIZE 0x10000

typedef enum
{
	InitShaState = 0x00,
	ContributeShaState = 0x01,
	FinalizeShaState = 0x02,
	UnknownShaCommand = 0x0F
} ShaCommandType;

typedef enum
{
	InitHMacState = 0x03,
	ContributeHMacState = 0x04,
	FinalizeHmacState = 0x05,
} HMacCommandType;

typedef u32 FinalShaHash[SHA_NUM_WORDS];
CHECK_SIZE(FinalShaHash, 0x14);

#pragma pack(push, 1)
typedef struct
{
	u32 ShaStates[SHA_NUM_WORDS];
	u64 Length;  // length in bits of total data contributed to SHA-1 hash
} ShaContext;
#pragma pack(pop)
CHECK_SIZE(ShaContext, 0x1C);
CHECK_OFFSET(ShaContext, 0x00, ShaStates);
CHECK_OFFSET(ShaContext, 0x14, Length);
