/*
	StarStruck - a Free Software reimplementation for the Nintendo/BroadOn IOS.
	Copyright (C) 2021	DacoTaco

# This code is licensed to you under the terms of the GNU GPL, version 2;
# see file COPYING or http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt
*/

#pragma once

#include "types.h"

// AES-128 works on blocks of 16 bytes. The key and the IV of an AES command are this size too, so is the part of the data that is
// chained into the next IV. Like the other array types (UInt233, FinalShaHash) it is passed around as a pointer
typedef u8 AESBlock[0x10];
CHECK_SIZE(AESBlock, 0x10);
