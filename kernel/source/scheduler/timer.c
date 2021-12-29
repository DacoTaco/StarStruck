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
#include "scheduler/threads.h"

static u32 timerFrequency = 0;
static TimerInfo timers[MAX_TIMERS];
static TimerInfo* currentTimer = timers;
static u32 previousTimerValue = 0;

u32 ConvertDelayToTicks(u32 delay)
{
	u32 clk = GetCoreClock();

	//from what i gather, it uses the clk values (which are mode & hardware rev depended) to decide the formula
	if(clk == 0x51)
		return (delay >> 1) + (delay >> 3) + (delay >> 7);
	else if(clk == 0x36)
		return (delay >> 2) + (delay >> 3) + (delay >> 5) + (delay >> 6);
	else if(clk == 0x6C)
		return (delay >> 1) + (delay >> 2) + (delay >> 4) + (delay >> 5);
	else if(clk == 0xA2) //Gamecube mode clk
		return (delay >> 2) + (delay >> 6) + delay;

	//fallback
	return delay + (delay >> 1) + (delay >> 2) + (delay >> 3) + (delay >> 6) + (delay >> 7);
}

void HandleTimerInterrupt(void)
{
	//clear Timer
	write32(HW_ALARM, 0);

	//change thread queue? 
	//TODO : check with IOS
	PopNextThreadFromQueue(mainQueuePtr);

	//Reset Timer
	if (timerFrequency)
		write32(HW_ALARM, read32(HW_TIMER) + timerFrequency);
}

void TimerHandler(void)
{
	gecko_printf("hello from TimerHandler\n");

	//when thread is done init'ing : execute function
	//HandleTimerInterrupt
	return;
}

u32 GetTimerValue(void)
{
	return read32(HW_TIMER);
}

void SetTimerAlarm(u32 ticks)
{
	if (ticks < 2)
		ticks = 2;

	set32(HW_ALARM, read32(HW_ALARM) + ticks);
	previousTimerValue = read32(HW_ALARM);
	return;
}