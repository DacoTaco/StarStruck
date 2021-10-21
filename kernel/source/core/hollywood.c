/*
	starstruck - a Free Software reimplementation for the Nintendo/BroadOn IOS.
	threads - manage threads on the starlet

	Copyright (C) 2021	DacoTaco

# This code is licensed to you under the terms of the GNU GPL, version 2;
# see file COPYING or http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt
*/

#include <types.h>
#include <ios/processor.h>
#include "core/hollywood.h"

void GetHollywoodVersion(u32* hardwareVersion, u32* hardwareRevision)
{
	u32 version = read32(HW_VERSION);
	//eh? isnt this the same as '(version & 0xFF000000) >> 28' ?
	*hardwareVersion = (version << 24) >> 28;
	*hardwareRevision = version & 0x0F;
}
