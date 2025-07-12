/*
	starstruck - a Free Software reimplementation for the Nintendo/BroadOn IOS.
	timer - manage timer on the starlet

	Copyright (C) 2021	DacoTaco

# This code is licensed to you under the terms of the GNU GPL, version 2;
# see file COPYING or http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt
*/

#include <types.h>
#include "messaging/messageQueue.h"

#ifdef MIOS

#define MAX_TIMERS     0x08
#define TIMERSTACKSIZE 0x100

#else

#define MAX_TIMERS     0x100
#define TIMERSTACKSIZE 0x00

#endif

typedef struct TimerInfo
{
	u32 IntervalInTicks;
	u32 IntervalInµs;
	MessageQueue *MessageQueue;
	void *Message;
	u32 ProcessId;
	struct TimerInfo *PreviousTimer;
	struct TimerInfo *NextTimer;
} TimerInfo;
CHECK_OFFSET(TimerInfo, 0x00, IntervalInTicks);
CHECK_OFFSET(TimerInfo, 0x04, IntervalInµs);
CHECK_OFFSET(TimerInfo, 0x08, MessageQueue);
CHECK_OFFSET(TimerInfo, 0x0C, Message);
CHECK_OFFSET(TimerInfo, 0x10, ProcessId);
CHECK_OFFSET(TimerInfo, 0x14, PreviousTimer);
CHECK_OFFSET(TimerInfo, 0x18, NextTimer);
CHECK_SIZE(TimerInfo, 0x1C);

extern TimerInfo *CurrentTimer;
extern u32 PreviousTimerValue;
extern const u8 *TimerMainStack;

void TimerHandler(void);
void QueueTimer(TimerInfo *timerInfo);
u32 ConvertDelayToTicks(u32 delay);
s32 CreateTimer(u32 delayUs, u32 periodUs, const s32 queueid, void *message);
s32 RestartTimer(s32 timerId, u32 timeUs, u32 repeatTimeUs);
s32 StopTimer(s32 timerId);
s32 DestroyTimer(s32 timerId);
u32 GetTimerValue(void);
void SetTimerAlarm(u32 ticks);
