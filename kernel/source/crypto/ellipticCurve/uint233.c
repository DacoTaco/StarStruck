/*
	starstruck - a Free Software reimplementation for the Nintendo/BroadOn IOS.
	uint233 - the GF(2^233) numbers and arithmetic of iosc's elliptic curve code

	Copyright (C) 2021	DacoTaco

# This code is licensed to you under the terms of the GNU GPL, version 2;
# see file COPYING or http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt
*/

#include <string.h>

#include "uint233.h"
#include "bigint_convert.h"

void ClearUInt233(UInt233 value)
{
	for (u32 index = 0; index < UINT233_WORDS; index++) value[index] = 0;
}

void CopyUInt233(const UInt233 src, UInt233 dst)
{
	for (u32 index = 0; index < UINT233_WORDS; index++) dst[index] = src[index];
}

// halve the number (k = k / 2). used for scalar k in the multiplication math
void HalveUInt233(UInt233 value)
{
	u32 carry = (value[UINT233_WORDS - 1] & 1) == 0 ? 0 : 0x100;
	for (u32 index = 0; index < UINT233_WORDS; index++)
	{
		u32 v = carry | value[index] >> 1;
		carry = value[index] << 0x1F;
		value[index] = v;
	}

	value[0] &= 0x1FF;
}

void ReverseWordOrder(const UInt233 srcWords, UInt233 destWords, u16 wordCount)
{
	ClearUInt233(destWords);
	if (wordCount == 0)
		return;

	//do -1 so we get array index ( 0 -> wordcount)
	wordCount--;
	for (u16 index = 0; index <= wordCount; index += 1) destWords[index] = srcWords[wordCount - index];
}

// load 30 big-endian bytes into a number. Only the low 233 bits are kept, it is not reduced mod n.
// Used for scalars: private keys (k of k*Q) and the ECDSA nonce
void LoadUInt233(const u8* src, UInt233 out)
{
	out[0] = (((u32)src[0] << 8) | src[1]) & 0x1ff;
	for (u32 index = 1; index < UINT233_WORDS; index++)
	{
		const u8* bytes = src + 2 + (index - 1) * 4;
		out[index] = ((u32)bytes[0] << 24) | ((u32)bytes[1] << 16) | ((u32)bytes[2] << 8) | bytes[3];
	}
}

// split size big-endian bytes (60 for sect233r1) into 2 numbers: out[0] = first half, out[1] = second half.
// Used for public keys (x || y, the curve point Q) and ECDSA signatures (r || s). Nothing is validated
void LoadUInt233Pair(const u8* src, s32 size, UInt233* out)
{
	UInt233 pair[2];
	s32 unitSize = size / 2;

	LoadBigEndianNumber(pair[0], UINT233_WORDS, src, unitSize);
	LoadBigEndianNumber(pair[1], UINT233_WORDS, src + unitSize, unitSize);
	ReverseWordOrder(pair[0], out[0], UINT233_WORDS);
	ReverseWordOrder(pair[1], out[1], UINT233_WORDS);
}

// store a number as 30 big-endian bytes, the inverse of LoadUInt233. The low 240 bits are written, it is not masked to 233 bits.
// Used for x of the shared point k*Q (hashed into the ECDH secret) and for generated private keys
void StoreUInt233(u8* dst, const UInt233 value)
{
	UInt233 buffer;
	for (u32 index = 0; index < (UINT233_WORDS - 1); index++)
	{
		buffer[index] = value[index] << 0x10 | value[index + 1] >> 0x10;
	}

	buffer[7] = value[7] << 0x10;
	memcpy(dst, buffer, UINT233_NUM_BYTES);
}
