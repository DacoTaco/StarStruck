/*
	starstruck - a Free Software reimplementation for the Nintendo/BroadOn IOS.
	threads - manage threads on the starlet

	Copyright (C) 2021	DacoTaco

# This code is licensed to you under the terms of the GNU GPL, version 2;
# see file COPYING or http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt
*/

#include <types.h>
#include <ios/processor.h>

#include "core/pll.h"
#include "core/hollywood.h"

#include "utils.h"

//i also have no idea what this is all about and what device this PLL is for
//its ripped straight out of IOS
void ConfigureAiPLL(u8 gcMode, u8 forceInit)
{
	u32 hardwareVersion = 0;
	u32 hardwareRevision = 0;
	GetHollywoodVersion(&hardwareVersion, &hardwareRevision);
	
	u32 value = read32(HW_PLLAIEXT);
	
	//IOS checks if the value is > -1, which i can only assume means it compared to 0xFFFFFFFF (which is -1 as u32) ?
	//we compare to the s32 value to keep the logic the same, because wtf?
	if( 0 >= (s32)value && forceInit == 0)
		return;
	
	value = read32(HW_DIFLAGS) & 0xFFFFFEFF;
	if(!gcMode)
		value |= 0x100;
	
	set32(HW_DIFLAGS, value & 0xFFFFFF7F);
	value = read32(HW_PLLAIEXT);
	set32(HW_PLLAIEXT, value & 0x3FFFFFFF);
	
	if(hardwareVersion < 2)
	{
		if(gcMode == 1)
		{
			set32(HW_PLLAI, (read32(HW_PLLAI) & 0xF8000000) | 0x04640FC0);
		}
		else
		{
			set32(HW_PLLAIEXT, read32(HW_PLLAIEXT) & 0xEFFFFFFF);
			set32(HW_PLLAI, (read32(HW_PLLAI) & 0xF8000000) | 0x04B0FFCE);
		}
	}
	else if(gcMode == 1)
	{
		set32(HW_PLLAIEXT1, (read32(HW_PLLAIEXT1) & 0xB8000000) | 0x04640FC0);
	}
	
	udelay(10);
	set32(HW_PLLAIEXT1, (read32(HW_PLLAIEXT1) & 0xBFFFFFFF) | 0x40000000);
	udelay(500);
	set32(HW_PLLAIEXT1, (read32(HW_PLLAIEXT1) & 0x7FFFFFFF) | 0x80000000);
	udelay(2);
}

//I have no idea what this is doing, and i can only assume it sets up the PLL of the VI/EXT
void ConfigureVideoInterfacePLL(u8 forceInit)
{
	u32 value = read32(HW_PLLVIEXT);

	//IOS checks if the value is > -1, which i can only assume means it compared to 0xFFFFFFFF (which is -1 as u32) ?
	//we compare to the s32 value to keep the logic the same, because wtf?
	if( 0 >= (s32)value && forceInit == 0)
		return;
	
	set32(HW_PLLVIEXT, value & 0x7FFFFFFF);
	udelay(2);
	set32(HW_PLLVIEXT, value & 0x3FFFFFFF);
	udelay(10);
	
	value = read32(HW_PLLVIEXT);
	set32(HW_PLLVIEXT, (value & 0xBFFFFFFF) | 0x40000000);
	udelay(50);
	
	value = read32(HW_PLLVIEXT);
	set32(HW_PLLVIEXT, (value & 0x7FFFFFFF) | 0x80000000);
	udelay(2);
}

//Same here, no idea what every step does, but i assume it configures the USB PLL
void ConfigureUsbHostPLL()
{
	u32 value = read32(HW_PLLUSBEXT);
	write32(HW_PLLUSBEXT, value & 0x7FFFFFFF);
	udelay(2);
	write32(HW_PLLUSBEXT, value & 0x3FFFFFFF);
	udelay(10);
	
	write32(HW_PLLUSBEXT, (read32(HW_PLLUSBEXT) & 0xBFFFFFFF) | 0x40000000);
	udelay(50);
	
	write32(HW_PLLUSBEXT, (read32(HW_PLLUSBEXT) & 0x7FFFFFFF) | 0x80000000);
	udelay(2);
}