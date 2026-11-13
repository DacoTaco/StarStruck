/*
	StarStruck - a Free Software reimplementation for the Nintendo/BroadOn IOS.
	Copyright (C) 2021	DacoTaco

# This code is licensed to you under the terms of the GNU GPL, version 2;
# see file COPYING or http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt
*/

#pragma once

#include "types.h"
#include "ios/sha.h"

typedef struct
{
	ShaContext ShaContext;
	u8 Unknown[0x44];
} IOSCHashContext;
CHECK_SIZE(IOSCHashContext, 0x60);
CHECK_OFFSET(IOSCHashContext, 0x00, ShaContext);
