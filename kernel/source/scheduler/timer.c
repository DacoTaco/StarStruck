/*
	starstruck - a Free Software reimplementation for the Nintendo/BroadOn IOS.
	timer - manage timer on the starlet

	Copyright (C) 2021	DacoTaco

# This code is licensed to you under the terms of the GNU GPL, version 2;
# see file COPYING or http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt
*/

#include <ios/processor.h>

#include "core/hollywood.h"
#include "interrupt/irq.h"
#include "scheduler/timer.h"

static u32 _alarm_frequency = 0;

void HandleTimerInterrupt(void)
{
    //clear Timer
    write32(HW_ALARM, 0);

    //change thread queue? 
    //TODO : check with IOS
    
    //Reset Timer
    if (_alarm_frequency)
        write32(HW_ALARM, read32(HW_TIMER) + _alarm_frequency);
}

u32 GetTimerValue(void)
{
	return read32(HW_TIMER);
}

void SetTimerAlarm(u32 ms, u8 enable)
{
    _alarm_frequency = IRQ_ALARM_MS2REG(ms);

	if (enable)
		write32(HW_ALARM, read32(HW_TIMER) + _alarm_frequency);
}