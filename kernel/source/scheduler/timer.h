/*
	starstruck - a Free Software reimplementation for the Nintendo/BroadOn IOS.
	timer - manage timer on the starlet

	Copyright (C) 2021	DacoTaco

# This code is licensed to you under the terms of the GNU GPL, version 2;
# see file COPYING or http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt
*/

#include <types.h>
#include "messaging/message_queue.h"

#define MAX_TIMERS		0x100

typedef struct TimerInfo
{
	u32 intervalInTicks;
	u32 intervalInµs;
	MessageQueue* messageQueue;
	void* message;
	u32 processId;
	struct TimerInfo* previousTimer;
	struct TimerInfo* nextTimer;
} TimerInfo;
CHECK_OFFSET(TimerInfo, 0x00, intervalInTicks);
CHECK_OFFSET(TimerInfo, 0x04, intervalInµs);
CHECK_OFFSET(TimerInfo, 0x08, messageQueue);
CHECK_OFFSET(TimerInfo, 0x0C, message);
CHECK_OFFSET(TimerInfo, 0x10, processId);
CHECK_OFFSET(TimerInfo, 0x14, previousTimer);
CHECK_OFFSET(TimerInfo, 0x18, nextTimer);
CHECK_SIZE(TimerInfo, 0x1C);

extern TimerInfo* currentTimer;
extern u32 PreviousTimerValue;

void TimerHandler(void);
void QueueTimer(TimerInfo* timerInfo);
u32 ConvertDelayToTicks(u32 delay);
s32 CreateTimer(u32 delayUs, u32 periodUs, u32 queueid, void *message);
s32 StopTimer(s32 timerId);
s32 DestroyTimer(s32 timerId);
u32 GetTimerValue(void);
void SetTimerAlarm(u32 ticks);
