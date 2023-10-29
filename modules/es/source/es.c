/*
	StarStruck - a Free Software reimplementation for the Nintendo/BroadOn IOS.
	Copyright (C) 2022	DacoTaco

# This code is licensed to you under the terms of the GNU GPL, version 2;
# see file COPYING or http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt
*/

#include <ios/syscalls.h>
#include "ios/printk.h"

#include "es.h"

int main(void)
{	
	u32* message;
	u32 messageQueueMessages[8] ALIGNED(0x20) = { 0 };

	OSSetThreadPriority(0,0x50);
	OSSetThreadPriority(0,0x79);
	printk("$IOSVersion:  ES: %s %s 64M $", __DATE__, __TIME__);

	s32 ret = OSCreateMessageQueue((void**)&messageQueueMessages, 1);
	const u32 EsMessageQueueId = (u32)ret;
	if(ret < 0)
	{
		printk("failed to create messagequeue! %d\n", ret);
		return -408;
	}
	
	while(1)
	{
		OSReceiveMessage(EsMessageQueueId, &message, 0);
	}
	return 0;
}