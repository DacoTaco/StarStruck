/*
	starstruck - a Free Software reimplementation for the Nintendo/BroadOn IOS.
	uint233 - the GF(2^233) numbers and arithmetic of iosc's elliptic curve code

	Copyright (C) 2021	DacoTaco

# This code is licensed to you under the terms of the GNU GPL, version 2;
# see file COPYING or http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt
*/

#include <string.h>

#include "uint233.h"
#include "wideUint233.h"
#include "bigint_convert.h"

//the field polynomial f = x^233 + x^74 + 1
static const UInt233 FieldPolynomial = { 0x200, 0, 0, 0, 0, 0x400, 0, 1 };

//the number of trailing zero bits of every byte from 0 - 255, used by InvertField.
//0 gives 8, the most a byte can shift
//somehow, breadon made their own version instead of using gcc's internal stuff??
//this is basically a software version of the clz instruction but with 0 = 8
static const u8 TrailingZeroTable[256] = {
	8, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0, 4, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0,
	5, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0, 4, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0,
	6, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0, 4, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0,
	5, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0, 4, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0,
	7, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0, 4, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0,
	5, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0, 4, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0,
	6, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0, 4, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0,
	5, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0, 4, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0,
};

void ClearUInt233(UInt233 value)
{
	for (u32 index = 0; index < UINT233_WORDS; index++) value[index] = 0;
}

void CopyUInt233(const UInt233 src, UInt233 dst)
{
	for (u32 index = 0; index < UINT233_WORDS; index++) dst[index] = src[index];
}

// out = a * b mod f. When a and b are the same number the square is used, which is faster.
// Like IOS the product is reduced one word up (as a * b * x^32) and the x^32 is divided out at the end
void MultiplyField(const UInt233 a, const UInt233 b, UInt233 out)
{
	UInt233Wide product;
	// the product with the least significant word first, reduced in place. word 0 is the extra x^32
	u32 reversedProduct[UINT233_WIDE_WORDS + 1];
	UInt233Wide reduced;
	UInt233Wide low;
	UInt233Wide folded;
	UInt233Wide lowTimesX74;
	UInt233Wide lowTimesX233;

	if (EqualUInt233(a, b))
		SquareToWide(a, product);
	else
		MultiplyToWide(a, b, product);

	for (s32 index = 0; index < UINT233_WIDE_WORDS; index++)
		reversedProduct[UINT233_WIDE_WORDS - index] = product[index];
	reversedProduct[0] = 0;

	// x^256 = x^97 + x^23 (mod f): fold words 15..8 into the words 8 lower, from the top so the folded bits get folded again
	for (s32 index = UINT233_WIDE_WORDS; index >= UINT233_WORDS; index--)
	{
		const u32 word = reversedProduct[index];
		reversedProduct[index - 8] ^= word << 23;
		reversedProduct[index - 7] ^= word >> 9;
		reversedProduct[index - 5] ^= word << 1;
		reversedProduct[index - 4] ^= word >> 31;
	}

	// x^233 = x^74 + 1 (mod f): fold the bits of word 7 above bit 232
	const u32 top = reversedProduct[UINT233_WORDS - 1] >> 9;
	reversedProduct[0] ^= top;
	reversedProduct[2] ^= top << 10;
	reversedProduct[3] ^= reversedProduct[UINT233_WORDS - 1] >> 31;
	reversedProduct[UINT233_WORDS - 1] &= 0x1FF;

	for (s32 index = 0; index < UINT233_WORDS; index++)
		out[index] = reversedProduct[UINT233_WORDS - 1 - index];

	// divide by x^32: f = 1 (mod x^74), so adding low * f clears the low 32 bits, which are then shifted out
	WideLoad(out, reduced);
	for (u32 index = 0; index < UINT233_WIDE_WORDS - 1; index++) low[index] = 0;
	low[UINT233_WIDE_WORDS - 1] = reduced[UINT233_WIDE_WORDS - 1];
	for (u32 index = 0; index < UINT233_WIDE_WORDS; index++)
	{
		lowTimesX233[index] = low[index];
		lowTimesX74[index] = low[index];
	}

	WideMultiply(lowTimesX233, 233);
	WideMultiply(lowTimesX74, 74);
	WideXor(reduced, lowTimesX233, folded);
	WideXor(folded, lowTimesX74, lowTimesX233);
	WideDivide(lowTimesX233, 32);
	WideStore(lowTimesX233, out);
}

// out = b / x^bits mod f, the last part of InvertField. Like in MultiplyField, adding low * f clears the low bits so they can be
// shifted out. Done 32 bits at a time, then once for the bits that are left
static void FinishInvertField(const UInt233 b, u32 bits, UInt233 out)
{
	UInt233Wide value;
	UInt233Wide low;
	UInt233Wide folded;
	UInt233Wide lowTimesX74;
	UInt233Wide lowTimesX233;
	UInt233Wide mask;
	const u32 words = bits / 32;
	const u32 remainingBits = bits % 32;

	WideLoad(b, value);
	WideClear(mask);
	mask[UINT233_WIDE_WORDS - 1] = 0xFFFFFFFF;
	for (u32 count = 0; count < words; count++)
	{
		WideAnd(value, mask, low);
		for (u32 index = 0; index < UINT233_WIDE_WORDS; index++)
		{
			lowTimesX233[index] = low[index];
			lowTimesX74[index] = low[index];
		}

		WideMultiply(lowTimesX233, 233);
		WideMultiply(lowTimesX74, 74);
		WideXor(value, lowTimesX233, folded);
		WideXor(folded, lowTimesX74, lowTimesX233);
		WideClearMask(lowTimesX233, mask);
		WideDivide(lowTimesX233, 32);
		WideCopy(lowTimesX233, value);
	}

	WideClear(mask);
	u32 bit = 1;
	for (u32 count = 0; count < remainingBits; count++)
	{
		mask[UINT233_WIDE_WORDS - 1] |= bit;
		bit <<= 1;
	}

	WideAnd(value, mask, low);
	WideCopy(low, lowTimesX233);
	WideCopy(low, lowTimesX74);
	WideMultiply(lowTimesX233, 233);
	WideMultiply(lowTimesX74, 74);
	WideXor(value, lowTimesX233, folded);
	WideXor(folded, lowTimesX74, lowTimesX233);
	WideClearMask(lowTimesX233, mask);
	WideDivide(lowTimesX233, remainingBits);
	WideCopy(lowTimesX233, value);
	WideStore(value, out);
}

// value = value / x^shift for words start..7, the words above start are 0. shift is 1 - 8
// IOS58: inlined in 0x13a7a868 (ECC_InvertField), e.g. 0x13a7a8e0
static inline void InvertFieldShiftDown(UInt233 value, u32 start, u8 shift)
{
	u32 carry = 0;
	for (u32 index = start; index < UINT233_WORDS; index++)
	{
		const u32 word = value[index];
		value[index] = carry << (32 - shift) | word >> shift;
		carry = word;
	}
}

// value = value * x^shift for words top..7, the words above top are 0. top moves up when the value grows. shift is 1 - 8
static inline void InvertFieldShiftUp(UInt233 value, s32* top, u32 shift)
{
	u32 carry = 0;
	s32 index = UINT233_WORDS - 1;
	for (; index >= *top; index--)
	{
		const u32 word = value[index];
		value[index] = carry | word << shift;
		carry = word >> (32 - shift);
	}

	if (carry != 0)
	{
		*top = index;
		value[index] = carry;
	}
}

// skip the words that are 0 in both values
static inline u32 InvertFieldSkipZeroWords(const UInt233 first, const UInt233 second, u32 start)
{
	while (first[start] == 0 && second[start] == 0) start++;

	return start;
}

static inline bool InvertFieldIsOne(const UInt233 value, u32 start)
{
	for (u32 index = start; index < UINT233_WORDS - 1; index++)
	{
		if (value[index] != 0)
			return false;
	}

	return value[UINT233_WORDS - 1] == 1;
}

// out = 1 / a mod f, with the binary almost inverse algorithm. It keeps a * b = u * x^bits and a * c = v * x^bits (mod f),
// starting from u = a, v = f, b = 1, c = 0. Every step adds the smaller of u and v to the larger one (and c to b, or b to c),
// which makes it even, then divides it by x until it is odd again while the other side's c or b is multiplied by x.
// When u or v is 1, b or c is a / x^bits and FinishInvertField divides out the x^bits.
// Like IOS, a = 0 never returns (there is no inverse, the trailing zeros of 0 are shifted out forever). The curve code never passes 0
void InvertField(const UInt233 a, UInt233 out)
{
	UInt233 v;
	UInt233 c;
	UInt233 b;
	UInt233 u;
	u32 bits = 0;
	u8 shift;
	// the words above start are 0 in both u and v, the words above top are 0 in both b and c
	u32 start = 0;
	s32 top = UINT233_WORDS - 1;

	ClearUInt233(c);
	ClearUInt233(b);
	CopyUInt233(a, u);
	CopyUInt233(FieldPolynomial, v);
	b[UINT233_WORDS - 1] = 1;

	// make u odd. b stays 1: dividing u by x is counted in bits
	do
	{
		shift = TrailingZeroTable[u[UINT233_WORDS - 1] & 0xFF];
		bits += shift;
		if (shift == 0)
			break;

		InvertFieldShiftDown(u, start, shift);
	}
	while (shift == 8 && (u[UINT233_WORDS - 1] & 1) == 0);

	if (InvertFieldIsOne(u, 0))
		goto finish;

	// like IOS the larger one is found by its top word only, and on equal top words the last step is repeated
	for (;;)
	{
		start = InvertFieldSkipZeroWords(u, v, start);
		while (u[start] >= v[start])
		{
			for (u32 index = start; index < UINT233_WORDS; index++) u[index] ^= v[index];
			for (s32 index = top; index < UINT233_WORDS; index++) b[index] ^= c[index];

			do
			{
				shift = TrailingZeroTable[u[UINT233_WORDS - 1] & 0xFF];
				bits += shift;
				InvertFieldShiftUp(c, &top, shift);
				InvertFieldShiftDown(u, start, shift);
			}
			while (shift == 8 && (u[UINT233_WORDS - 1] & 1) == 0);

			if (InvertFieldIsOne(u, start))
				goto finish;

			start = InvertFieldSkipZeroWords(u, v, start);
		}

		do
		{
			for (u32 index = start; index < UINT233_WORDS; index++) v[index] ^= u[index];
			for (s32 index = top; index < UINT233_WORDS; index++) c[index] ^= b[index];

			do
			{
				shift = TrailingZeroTable[v[UINT233_WORDS - 1] & 0xFF];
				bits += shift;
				InvertFieldShiftUp(b, &top, shift);
				InvertFieldShiftDown(v, start, shift);
			}
			while (shift == 8 && (v[UINT233_WORDS - 1] & 1) == 0);

			if (InvertFieldIsOne(v, start))
			{
				CopyUInt233(c, b);
				goto finish;
			}

			start = InvertFieldSkipZeroWords(v, u, start);
		}
		while (v[start] >= u[start]);
	}

finish:
	FinishInvertField(b, bits, out);
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
