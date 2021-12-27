/*
	mini - a Free Software replacement for the Nintendo/BroadOn IOS.
	random utilities

Copyright (C) 2008, 2009	Hector Martin "marcan" <marcan@marcansoft.com>

# This code is licensed to you under the terms of the GNU GPL, version 2;
# see file COPYING or http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt
*/

#include <stdarg.h>
#include <types.h>
#include <vsprintf.h>
#include <ios/gecko.h>
#include <ios/processor.h>

#include "core/gpio.h"
#include "core/hollywood.h"
#include "scheduler/timer.h"
#include "utils.h"

//no idea why this has less issues when forced as arm...
__attribute__((target("arm")))
void udelay(u32 delay)
{
	u32 ticks = ConvertDelayToTicks(delay);
	if(ticks < 2)
		ticks = 2;
	
	u32 then = read32(HW_TIMER) + ticks;

	//There are 2 possibilities in waiting here
	//either the HW_Timer value is smaller than our future time (regular wait)
	//or our future overflowed and we have to wait on the timer to overflow back to 0
	if(read32(HW_TIMER) < then)
	{
		while(read32(HW_TIMER) < then);
		return;
	}
	else
	{
		while(read32(HW_TIMER) > 0);
		while(read32(HW_TIMER) < then);
	}
}

void panic(u8 v)
{
	while(1) {
		debug_output(v);
		set32(HW_GPIO1BOUT, GP_SLOTLED);
		udelay(500000);
		debug_output(0);
		clear32(HW_GPIO1BOUT, GP_SLOTLED);
		udelay(500000);
	}
}

