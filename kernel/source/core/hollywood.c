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
	//eh? isnt this the same as '(version & 0x000000F0) >> 4' ?
	*hardwareVersion = (version << 24) >> 28;
	*hardwareRevision = version & 0x0F;
}

//unknown WTF this is about...
u32 NumberMagic(u32 param_1, u32 param_2)
{
	if(param_2 == 0 || param_1 > param_2)
		return 0;

	u32 ret = 0;
	u32 unknwn = 0;

	while(param_2 < 0x10000000 && (param_2 < param_1)) 
	{
      param_2 = param_2 << 4;
      unknwn = unknwn << 4;
    }

	while (param_2 < 0x80000000 && (param_2 < param_1))
	{
      param_2 = param_2 << 1;
      unknwn = unknwn << 1;
    }

	while(1) 
	{
      if (param_2 <= param_1) {
        param_1 -= param_2;
        ret |= unknwn;
      }
      if (param_2 >> 1 <= param_1) {
        param_1 -= param_2 >> 1;
        ret |= unknwn >> 1;
      }
      if (param_2 >> 2 <= param_1) {
        param_1 -= param_2 >> 2;
        ret |= unknwn >> 2;
      }
      if (param_2 >> 3 <= param_1) {
        param_1 -= param_2 >> 3;
        ret |= unknwn >> 3;
      }

	  unknwn = unknwn >> 4;
      if (param_1 == 0 || unknwn == 0)
	  	break;
      param_2 = param_2 >> 4;
    }

	return ret;
}

u32 GetCoreClock(void)
{
	//gamecube mode?
	if((s32)(read32(HW_CLOCKS) << 0x1E) < 0)
	{
		return 0xA2;
	}

	u32 clk = 0;
	u32 hwRev, hwVer;
	GetHollywoodVersion(&hwRev, &hwVer);

	if(hwVer < 2)
	{
		if(read32(HW_CLOCKS) & 1 != 0)
			return NumberMagic(0xF30, read32(HW_PLLSYSEXT) & 0x1FF);

		clk = 0xF30;
	}
	else
	{
		if(read32(HW_CLOCKS) & 1 != 0)
			return NumberMagic(0x798, read32(HW_PLLSYSEXT) & 0x1FF);
		
		clk = 0x798;
	}

	return NumberMagic(clk, (read32(HW_PLLSYS) << 5) >> 0x17);
}