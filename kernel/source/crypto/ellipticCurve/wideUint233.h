/*
	starstruck - a Free Software reimplementation for the Nintendo/BroadOn IOS.
	wideUint233 - the 480 bit intermediates of iosc's GF(2^233) multiply and inversion

	Copyright (C) 2021	DacoTaco

# This code is licensed to you under the terms of the GNU GPL, version 2;
# see file COPYING or http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt
*/

#pragma once

#include <types.h>

#include "uint233.h"

// the product of two 233 bit numbers has up to 2 * 233 - 1 bits before it is reduced, held in 15 words.
//usually used when doing math on a uint233 so there is room to store the result
#define UINT233_WIDE_BITS  (2 * UINT233_BITS - 1)
#define UINT233_WIDE_WORDS ((UINT233_WIDE_BITS + 31) / 32) // 15

typedef u32 UInt233Wide[UINT233_WIDE_WORDS];
CHECK_SIZE(UInt233Wide, 0x3C);

void WideClear(UInt233Wide value);
void WideCopy(const UInt233Wide src, UInt233Wide dst);
void WideLoad(const UInt233 src, UInt233Wide dst);
void WideStore(const UInt233Wide src, UInt233 dst);
void WideXor(const UInt233Wide a, const UInt233Wide b, UInt233Wide out);
void WideAnd(const UInt233Wide a, const UInt233Wide b, UInt233Wide out);
void WideClearMask(UInt233Wide value, const UInt233Wide mask);
void WideDouble(UInt233Wide value);
void WideMultiply(UInt233Wide value, u32 bits);
void WideDivide(UInt233Wide value, u32 bits);

// the product of two numbers, not reduced
void MultiplyToWide(const UInt233 a, const UInt233 b, UInt233Wide out);
void SquareToWide(const UInt233 a, UInt233Wide out);
