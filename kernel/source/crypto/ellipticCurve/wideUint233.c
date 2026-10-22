/*
	starstruck - a Free Software reimplementation for the Nintendo/BroadOn IOS.
	wideUint233 - the 480 bit intermediates of iosc's GF(2^233) multiply and inversion

	Copyright (C) 2021	DacoTaco

# This code is licensed to you under the terms of the GNU GPL, version 2;
# see file COPYING or http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt
*/

#include "wideUint233.h"

//the square of every byte: its bits spread to the even positions (bit n -> bit 2n)
#define SQUARE_BYTE(b)                                                                                   \
	(u16)(((b) & 0x01) | ((b) & 0x02) << 1 | ((b) & 0x04) << 2 | ((b) & 0x08) << 3 | ((b) & 0x10) << 4 | \
	      ((b) & 0x20) << 5 | ((b) & 0x40) << 6 | ((b) & 0x80) << 7)
#define SQUARE_BYTES_4(b) SQUARE_BYTE(b), SQUARE_BYTE((b) + 1), SQUARE_BYTE((b) + 2), SQUARE_BYTE((b) + 3)
#define SQUARE_BYTES_16(b) \
	SQUARE_BYTES_4(b), SQUARE_BYTES_4((b) + 4), SQUARE_BYTES_4((b) + 8), SQUARE_BYTES_4((b) + 12)
#define SQUARE_BYTES_64(b) \
	SQUARE_BYTES_16(b), SQUARE_BYTES_16((b) + 16), SQUARE_BYTES_16((b) + 32), SQUARE_BYTES_16((b) + 48)

//he multiples 0 * b .. 15 * b of MultiplyToWide.
static UInt233Wide MultiplyToWideTable[16];

//square lookup table of every byte from 0 - 255
static const u16 SquareLookupTable[256] = { SQUARE_BYTES_64(0), SQUARE_BYTES_64(64), SQUARE_BYTES_64(128),
	                                        SQUARE_BYTES_64(192) };

void WideClear(UInt233Wide value)
{
	for (u32 index = 0; index < UINT233_WIDE_WORDS; index++) value[index] = 0;
}

void WideCopy(const UInt233Wide src, UInt233Wide dst)
{
	for (u32 index = 0; index < UINT233_WIDE_WORDS; index++) dst[index] = src[index];
}

// zero-extend a number into a wide value
void WideLoad(const UInt233 src, UInt233Wide dst)
{
	WideClear(dst);
	for (u32 index = 0; index < UINT233_WORDS; index++) dst[index + (UINT233_WORDS - 1)] = src[index];
}

// store the uint233 from the wide version
void WideStore(const UInt233Wide src, UInt233 dst)
{
	const u32 offset = UINT233_WIDE_WORDS - UINT233_WORDS;
	for (u32 index = 0; index < UINT233_WORDS; index++) dst[index] = src[offset + index];
}

void WideXor(const UInt233Wide a, const UInt233Wide b, UInt233Wide out)
{
	for (u32 index = 0; index < UINT233_WIDE_WORDS; index++) out[index] = a[index] ^ b[index];
}

void WideAnd(const UInt233Wide a, const UInt233Wide b, UInt233Wide out)
{
	for (u32 index = 0; index < UINT233_WIDE_WORDS; index++) out[index] = a[index] & b[index];
}

// clear the bits of mask in value
void WideClearMask(UInt233Wide value, const UInt233Wide mask)
{
	for (u32 index = 0; index < UINT233_WIDE_WORDS; index++) value[index] &= ~mask[index];
}

//double value by shifting it left by 1
void WideDouble(UInt233Wide value)
{
	u32 carry = 0;
	for (s16 index = UINT233_WIDE_WORDS - 1; index >= 0; index--)
	{
		u32 v = carry | (value[index] << 1);
		carry = value[index] >> 0x1F;
		value[index] = v;
	}
}

// value = value * 2^bits
// this is done by shifting it by x bits.
void WideMultiply(UInt233Wide value, u32 bits)
{
	const u32 wordShift = bits / 32;
	const u32 bitShift = bits % 32;

	const s32 keptWords = UINT233_WIDE_WORDS - (s32)wordShift;
	s32 index = 0;
	if (bitShift == 0)
	{
		for (; index < keptWords; index++) value[index] = value[index + (s32)wordShift];
	}
	else
	{
		u32 word = value[wordShift] << bitShift;
		for (; index < keptWords - 1; index++)
		{
			const u32 next = value[index + (s32)wordShift + 1];
			value[index] = word | next >> (32 - bitShift);
			word = next << bitShift;
		}

		// the last kept word has nothing below it to carry in
		value[index++] = word;
	}

	for (u32 count = 0; count < wordShift; count++) value[index++] = 0;
}

// value = value / 2^bits rounded down using a shift.
void WideDivide(UInt233Wide value, u32 bits)
{
	const u32 wordShift = bits / 32;
	const u32 bitShift = bits % 32;

	// like IOS, bits is not checked: a shift of 480 bits or more reads and writes past the array. The callers shift less
	const s32 keptWords = UINT233_WIDE_WORDS - (s32)wordShift;
	s32 index = UINT233_WIDE_WORDS - 1;
	if (bitShift == 0)
	{
		for (s32 count = 0; count < keptWords; count++, index--) value[index] = value[index - (s32)wordShift];
	}
	else
	{
		for (s32 count = 0; count < keptWords - 1; count++, index--)
		{
			const s32 source = index - (s32)wordShift;
			value[index] = value[source - 1] << (32 - bitShift) | value[source] >> bitShift;
		}

		// the first kept word has nothing above it to carry in
		value[index] = value[index - (s32)wordShift] >> bitShift;
		index--;
	}

	for (u32 count = 0; count < wordShift; count++) value[index--] = 0;
}

// out = a * b without reducing it. Carry-less: the partial products are XORed instead of added.
// a is handled 4 bits at a time, from the top: out = out * x^4 + (4 bits of a) * b, with the 16 multiples of b in a table
void MultiplyToWide(const UInt233 a, const UInt233 b, UInt233Wide out)
{
	UInt233Wide multiple;

	// b, b * x, b * x^2 and b * x^3
	WideClear(MultiplyToWideTable[0]);
	WideLoad(b, MultiplyToWideTable[1]);
	WideCopy(MultiplyToWideTable[1], multiple);
	WideDouble(multiple);
	WideCopy(multiple, MultiplyToWideTable[2]);
	WideDouble(multiple);
	WideCopy(multiple, MultiplyToWideTable[4]);
	WideDouble(multiple);
	WideCopy(multiple, MultiplyToWideTable[8]);

	// the other multiples are sums of those 4
	for (s32 index = UINT233_WIDE_WORDS - 1; index >= 0; index--)
	{
		const u32 b1 = MultiplyToWideTable[1][index];
		const u32 b2 = MultiplyToWideTable[2][index];
		const u32 b4 = MultiplyToWideTable[4][index];
		const u32 b8 = MultiplyToWideTable[8][index];
		MultiplyToWideTable[3][index] = b2 ^ b1;
		MultiplyToWideTable[5][index] = b4 ^ b1;
		MultiplyToWideTable[6][index] = b4 ^ b2;
		MultiplyToWideTable[7][index] = b4 ^ b2 ^ b1;
		MultiplyToWideTable[9][index] = b8 ^ b1;
		MultiplyToWideTable[10][index] = b8 ^ b2;
		MultiplyToWideTable[11][index] = b8 ^ b2 ^ b1;
		MultiplyToWideTable[12][index] = b8 ^ b4;
		MultiplyToWideTable[13][index] = b8 ^ b4 ^ b1;
		MultiplyToWideTable[14][index] = b8 ^ b4 ^ b2;
		MultiplyToWideTable[15][index] = b8 ^ b4 ^ b2 ^ b1;
	}

	WideClear(out);
	for (s32 nibble = 7; nibble >= 0; nibble--)
	{
		// the same 4 bits of every word of a, word 7 first like IOS
		for (s32 word = UINT233_WORDS - 1; word >= 0; word--)
		{
			const u32 digit = (a[word] >> (nibble * 4)) & 0xF;

			// a multiple has at most 233 + 3 bits, so only its low 8 words (7..14) are added, at the weight of a[word]
			for (s32 index = 0; index < UINT233_WORDS; index++)
				out[word + index] ^= MultiplyToWideTable[digit][(UINT233_WIDE_WORDS - UINT233_WORDS) + index];
		}

		if (nibble == 0)
			break;

		// out = out * x^4, from word 14 up. IOS does it in place here instead of calling WideMultiply
		u32 carry = 0;
		for (s32 index = UINT233_WIDE_WORDS - 1; index >= 0; index--)
		{
			const u32 value = out[index];
			out[index] = carry | value << 4;
			carry = value >> 28;
		}
	}
}

// out = a^2 without reducing it. In GF(2) the cross terms of a square cancel out, so squaring only spreads the bits
// (bit n -> bit 2n), a byte at a time with SquareLookupTable.computer
void SquareToWide(const UInt233 a, UInt233Wide out)
{
	u32 square[UINT233_WIDE_WORDS + 1];
	for (s32 index = UINT233_WORDS - 1; index >= 0; index--)
	{
		const u32 word = a[index];
		square[2 * index] = (u32)SquareLookupTable[word >> 24] << 16 | SquareLookupTable[(word >> 16) & 0xFF];
		square[(2 * index) + 1] = (u32)SquareLookupTable[(word >> 8) & 0xFF] << 16 | SquareLookupTable[word & 0xFF];
	}

	for (s32 index = UINT233_WIDE_WORDS - 1; index >= 0; index--) out[index] = square[index + 1];
}
