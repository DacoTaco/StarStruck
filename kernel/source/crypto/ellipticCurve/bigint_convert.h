/*
	starstruck - a Free Software reimplementation for the Nintendo/BroadOn IOS.
	bigint_convert - conversion of big numbers between bytes and words, used by iosc's ECC and RSA code

	Copyright (C) 2021	DacoTaco

# This code is licensed to you under the terms of the GNU GPL, version 2;
# see file COPYING or http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt
*/

#pragma once

#include <types.h>

void LoadBigEndianNumber(u32* destWords, s32 wordCount, const u8* srcBytes, s32 byteCount);
