/*
	starstruck - a Free Software reimplementation for the Nintendo/BroadOn IOS.
	bigint_convert - conversion of big numbers between bytes and words, used by iosc's ECC and RSA code

	Copyright (C) 2021	DacoTaco

# This code is licensed to you under the terms of the GNU GPL, version 2;
# see file COPYING or http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt
*/

#include "bigint_convert.h"

void LoadBigEndianNumber(u32* destWords, s32 wordCount, const u8* srcBytes, s32 byteCount)
{
	s32 byteIndex = byteCount - 1;
	for (s32 wordIndex = 0; wordIndex < wordCount; wordIndex++)
	{
		u32 word = 0;
		//only set the word if we have bytes left to move around, otherwise just set 0
		for (u32 shift = 0; byteIndex >= 0 && shift < (sizeof(u32) * 8); shift += 8, byteIndex--)
			word |= (u32)srcBytes[byteIndex] << shift;

		destWords[wordIndex] = word;
	}
}
