/*
	starstruck - a Free Software reimplementation for the Nintendo/BroadOn IOS.
	uint233 - the GF(2^233) numbers and arithmetic of iosc's elliptic curve code

	Copyright (C) 2021	DacoTaco

# This code is licensed to you under the terms of the GNU GPL, version 2;
# see file COPYING or http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt
*/

#pragma once

#include <types.h>

// a 233 bit number is 8 words. few bits are ignored but there for ease of access
#define UINT233_BITS      233
#define UINT233_WORDS     ((UINT233_BITS + 31) / 32) // 8
#define UINT233_NUM_BYTES ((UINT233_BITS + 7) / 8)   // 30

typedef u32 UInt233[UINT233_WORDS];
CHECK_SIZE(UInt233, 0x20);

void ClearUInt233(UInt233 value);
void CopyUInt233(const UInt233 src, UInt233 dst);
void HalveUInt233(UInt233 value);
void MultiplyField(const UInt233 a, const UInt233 b, UInt233 out);
void InvertField(const UInt233 a, UInt233 out);

// conversion between numbers and their big-endian bytes
void ReverseWordOrder(const UInt233 srcWords, UInt233 destWords, u16 wordCount);
void LoadUInt233(const u8* src, UInt233 out);
void LoadUInt233Pair(const u8* src, s32 size, UInt233* out);
void StoreUInt233(u8* dst, const UInt233 value);

static inline bool IsZeroUInt233(const UInt233 value)
{
	u32 bits = 0;
	for (u32 index = 0; index < UINT233_WORDS; index++) bits |= value[index];
	return bits == 0;
}

static inline bool EqualUInt233(const UInt233 a, const UInt233 b)
{
	bool differ = false;
	for (u32 index = 0; index < UINT233_WORDS; index++)
	{
		if (a[index] != b[index])
			differ = true;
	}

	return !differ;
}

static inline void XorUInt233(const UInt233 a, const UInt233 b, UInt233 out)
{
	for (u32 index = 0; index < UINT233_WORDS; index++) out[index] = a[index] ^ b[index];
}
