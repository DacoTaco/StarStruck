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

void GetHollywoodVersion(u32 *hardwareVersion, u32 *hardwareRevision)
{
	u32 version = read32(HW_VERSION);
 //eh? isnt this the same as '(version & 0x000000F0) >> 4' ?
	*hardwareVersion = (version << 24) >> 28;
	*hardwareRevision = version & 0x0F;
}

u32 GetCoreClock(void)
{
 //gamecube mode?
	if ((s32)(read32(HW_CLOCKS) << 0x1E) < 0)
		return 0xA2;

	u32 clk = 0;
	u32 hwRev = 0;
	u32 hwVer = 0;
	GetHollywoodVersion(&hwVer, &hwRev);

	if (hwVer < 2)
	{
		if ((read32(HW_CLOCKS) & 1) != 0)
			return 0xF30 / read32(HW_PLLSYSEXT) & 0x1FF;

		clk = 0xF30;
	}
	else
	{
		if ((read32(HW_CLOCKS) & 1) != 0)
			return 0x798 / read32(HW_PLLSYSEXT) & 0x1FF;

		clk = 0x798;
	}

	return clk / (read32(HW_PLLSYS) << 5) >> 0x17;
}