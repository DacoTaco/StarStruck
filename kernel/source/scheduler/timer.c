/*
	starstruck - a Free Software reimplementation for the Nintendo/BroadOn IOS.
	timer - manage timer on the starlet

	Copyright (C) 2021	DacoTaco

# This code is licensed to you under the terms of the GNU GPL, version 2;
# see file COPYING or http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt
*/

#include <ios/processor.h>
#include <ios/errno.h>

#include "core/defines.h"
#include "core/hollywood.h"
#include "interrupt/irq.h"
#include "messaging/message_queue.h"
#include "scheduler/timer.h"
#include "scheduler/threads.h"
#include "panic.h"

u32 timerFrequency = 0;
TimerInfo timers[MAX_TIMERS] SRAM_DATA ALIGNED(0x10);
TimerInfo initialTimer = { 0, 0, NULL, NULL, 0, &initialTimer, &initialTimer };
TimerInfo* CurrentTimer ALIGNED(0x10) = &initialTimer;
u32 PreviousTimerValue = 0;

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

void QueueTimer(TimerInfo* timerInfo)
{
	if(timerInfo == NULL)
		return;

	TimerInfo* nextTimer = CurrentTimer->NextTimer;
	u32 intervalInTicks = timerInfo->IntervalInTicks;
	u32 timePassed = read32(HW_TIMER) - PreviousTimerValue;
	PreviousTimerValue = read32(HW_TIMER);

	//if the next timer isn't the current timer & there is still time left -> correct the interval
	//otherwise make it 0
	if(nextTimer != CurrentTimer)
	{
		nextTimer->IntervalInTicks = (timePassed < nextTimer->IntervalInTicks)
			? nextTimer->IntervalInTicks - timePassed
			: 0;
	}

	while(1)
	{
		if(nextTimer == CurrentTimer || nextTimer->IntervalInTicks >= intervalInTicks)
			break;
		
		intervalInTicks -= nextTimer->IntervalInTicks;
		nextTimer = nextTimer->NextTimer;
	}
	
	//set timer interval that is left & fix next thread interval
	timerInfo->IntervalInTicks = intervalInTicks;
	if(nextTimer != CurrentTimer)
		nextTimer->IntervalInTicks = nextTimer->IntervalInTicks - intervalInTicks;

	//shove timer between our previous and next timer
	TimerInfo* previousTimer = nextTimer->PreviousTimer;
	timerInfo->PreviousTimer = previousTimer;
	timerInfo->NextTimer = nextTimer;
	nextTimer->PreviousTimer = timerInfo;
	previousTimer->NextTimer = timerInfo;

	//if the current running timer is our previous timer, we need to re-set the timer alarm with the new interval between the 2 timers
	if(timerInfo->PreviousTimer == CurrentTimer)
		SetTimerAlarm(timerInfo->IntervalInTicks);
}

void TimerHandler(void)
{

	u32 timer_messages[1];
	s32 ret;
	u32 interupts = 0;
	u32 timerTicks = 0;
	TimerInfo* previousTimer = NULL;
	TimerInfo* nextTimer = NULL;

	//IOS sets up the timer here, but we've set it up in the intial timer setup
	/*CurrentTimer->IntervalInTicks = 0;
	CurrentTimer->IntervalInµs = 0;
	CurrentTimer->Message = NULL;
	CurrentTimer->PreviousTimer = CurrentTimer;
	CurrentTimer->NextTimer = CurrentTimer;
	CurrentTimer->MessageQueue = NULL;*/

	ret = CreateMessageQueue((void**)&timer_messages, 1);
	if(ret < 0)
		panic("Unable to create timer message queue: %d\n", ret);

	const u32 timerQueueId = (u32)ret;
	ret = RegisterEventHandler(IRQ_TIMER, timerQueueId, 0);
	if(ret < 0)
		panic("Unable to register timer event handler: %d\n", ret);
	
	while(1)
	{
		//wait for an irq message, which signals us to do our work
		do
		{
			ret = ReceiveMessage(timerQueueId, (void **)0x0, None);
		} while (ret != 0);

		//lets not get interrupted while processing the timer message
		interupts = DisableInterrupts();

		TimerInfo* timerInfo = CurrentTimer->NextTimer;
		while(timerInfo != CurrentTimer)
		{
			while(1)
			{
				timerTicks = PreviousTimerValue;
				PreviousTimerValue = read32(HW_TIMER);
				timerTicks = read32(HW_TIMER) - timerTicks;

				//will the next timer trigger before next loop?
				//if so, fix its interval and set the hardware timer
				if(timerTicks < timerInfo->IntervalInTicks)
				{
					timerTicks = timerInfo->IntervalInTicks - timerTicks;
					timerInfo->IntervalInTicks = timerTicks;
					SetTimerAlarm(timerTicks);
					goto _continue_loop;
				}

				//timer passed, remove timer from queue and recalculate its position
				u32 interval = timerInfo->IntervalInµs;
				previousTimer = timerInfo->PreviousTimer;

				//remove timer from queue
				nextTimer = timerInfo->NextTimer;
				previousTimer->NextTimer = nextTimer;
				timerInfo->NextTimer = NULL;
				nextTimer->PreviousTimer = previousTimer;
				timerInfo->PreviousTimer = NULL;

				if(interval != 0)
				{
					interval = ConvertDelayToTicks(interval);
					if(timerInfo->IntervalInTicks + interval < timerTicks)
						timerTicks = 1;
					else
						timerTicks = ((timerInfo->IntervalInTicks - timerTicks) + interval) - 1;
					
					timerInfo->IntervalInTicks = timerTicks;
					QueueTimer(timerInfo);
				}

				if(timerInfo->MessageQueue == NULL)
					break;

				SendMessageToQueue(timerInfo->MessageQueue, timerInfo->Message, RegisteredEventHandler);
				timerInfo = CurrentTimer->NextTimer;
				if(timerInfo == CurrentTimer)
					goto _reset_previous_and_continue;
			}
		}

_reset_previous_and_continue:
		PreviousTimerValue = 0;
_continue_loop:
		RestoreInterrupts(interupts);
	}
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

	write32(HW_ALARM, read32(HW_TIMER) + ticks);
	PreviousTimerValue = read32(HW_TIMER);
	return;
}

s32 CreateTimer(u32 delayUs, u32 periodUs, const u32 queueid, void *message)
{
	s32 ret = 0;
	u32 interupts = DisableInterrupts();
	u32 ticks = 0;

	if(queueid > MAX_MESSAGEQUEUES)
	{
		ret = IPC_EINVAL;
		goto return_create_timer;
	}

	if(MessageQueues[queueid].ProcessId != CurrentThread->ProcessId)
	{
		ret = IPC_EACCES;
		goto return_create_timer;
	}

	ret = 0;
	while(ret < MAX_TIMERS)
	{
		if(timers[ret].MessageQueue == NULL)
			break;
		
		ret++;
	}

	if (ret >= MAX_TIMERS) 
	{
		ret = IPC_EMAX;
		goto return_create_timer;
	}

	timers[ret].Message = message;
	timers[ret].MessageQueue = &MessageQueues[queueid];
	timers[ret].IntervalInµs = periodUs;
	if(delayUs != 0)
		periodUs = delayUs;

	ticks = ConvertDelayToTicks(periodUs);
	timers[ret].IntervalInTicks = ticks;
	timers[ret].ProcessId = CurrentThread->ProcessId;
	if(ticks != 0)
		QueueTimer(&timers[ret]);

return_create_timer:
	RestoreInterrupts(interupts);
	return ret;
}
s32 RestartTimer(s32 timerId, u32 timeUs, u32 repeatTimeUs)
{
	u32 interupts = DisableInterrupts();
	s32 ret = 0;

	if(timerId >= MAX_TIMERS)
	{
		ret = IPC_EINVAL;
		goto return_restart_timer;
	}

	if(timers[timerId].ProcessId != CurrentThread->ProcessId)
	{
		ret = IPC_EACCES;
		goto return_restart_timer;
	}

	if( timers[timerId].IntervalInµs != 0 || timers[timerId].IntervalInTicks != 0 || 
		timers[timerId].NextTimer != NULL || timers[timerId].PreviousTimer != NULL)
		goto return_restart_timer;

	timers[timerId].IntervalInµs = repeatTimeUs;
	if(timeUs != 0)
		repeatTimeUs = timeUs;
		
	u32 ticks = ConvertDelayToTicks(repeatTimeUs);
	timers[timerId].IntervalInTicks = ticks;
	if(ticks != 0)
		QueueTimer(&timers[timerId]);

return_restart_timer:
	RestoreInterrupts(interupts);
	return ret;
}
s32 StopOrDestroyTimer(s32 timerId, s32 destroyTimer)
{
	s32 ret = 0;
	u32 interupts = DisableInterrupts();
	TimerInfo* timerInfo = NULL;
	TimerInfo* nextTimer = NULL;
	TimerInfo* previousTimer = NULL;
	u32 prevTimerValue = PreviousTimerValue;

	if(timerId > MAX_TIMERS)
	{
		ret = IPC_EINVAL;
		goto return_stop_timer;
	}

	timerInfo = &timers[timerId];
	if(timerInfo->ProcessId != CurrentThread->ProcessId)
	{
		ret = IPC_EACCES;
		goto return_stop_timer;
	}

	nextTimer = timerInfo->NextTimer;
	if(nextTimer == NULL)
		goto clear_timer;

	if(nextTimer != CurrentTimer)
		goto clear_previousTimer;

	//if we are destroying the next timer, we need to recalculate all the timer timings
	if(CurrentTimer->NextTimer == timerInfo)
	{
		PreviousTimerValue = read32(HW_TIMER);
		if(nextTimer->IntervalInTicks + timerInfo->IntervalInTicks <= read32(HW_TIMER - prevTimerValue))
		{
			nextTimer->IntervalInTicks = 0;
			goto clear_previousTimer;
		}
		prevTimerValue = nextTimer->IntervalInTicks + (timerInfo->IntervalInTicks - read32(HW_TIMER) - prevTimerValue);
	}
	else
		PreviousTimerValue = nextTimer->IntervalInTicks + timerInfo->IntervalInTicks;

	nextTimer->IntervalInTicks = PreviousTimerValue;

//clear the previous timer and connect it to the next timer
clear_previousTimer:
	previousTimer = timers[timerId].PreviousTimer;
	previousTimer->NextTimer = nextTimer;
	(timers[timerId].NextTimer)->PreviousTimer = previousTimer;

//clear the timer struct
clear_timer:
	if(destroyTimer)
		memset8(timerInfo, 0, sizeof(TimerInfo));
	else
	{
		timerInfo->PreviousTimer = NULL;
		timerInfo->NextTimer = NULL;
		timerInfo->IntervalInµs = 0;
		timerInfo->IntervalInTicks = 0;
	}

return_stop_timer:
	RestoreInterrupts(interupts);
	return ret;
}
s32 StopTimer(s32 timerId)
{
	return StopOrDestroyTimer(timerId, 0);
}
s32 DestroyTimer(s32 timerId)
{
	return StopOrDestroyTimer(timerId, 1);
}
