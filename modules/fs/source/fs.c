/*
	StarStruck - a Free Software reimplementation for the Nintendo/BroadOn IOS.
	Copyright (C) 2022	DacoTaco

# This code is licensed to you under the terms of the GNU GPL, version 2;
# see file COPYING or http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt
*/

#include <ios/syscalls.h>
#include "ios/printk.h"

#include "fs.h"

int main(void)
{
	u32 messageQueueMessages[8] ALIGNED(0x10);
	u32* message;
	printk("$IOSVersion:  FFSP: %s %s 64M $", __DATE__, __TIME__);
	s32 messageQueueId = OSCreateMessageQueue((void**)&messageQueueMessages, 8);
	if(messageQueueId < 0)
	{
		printk("failed to create messagequeue! %d\n", messageQueueId);
		return -408;
	}	

	while(1)
	{
      	s32 ret = OSReceiveMessage(messageQueueId, &message, 0);
		if(ret < 0)
			break;
	}
	return 0;
}