/*
	StarStruck - a Free Software reimplementation for the Nintendo/BroadOn IOS.
	Copyright (C) 2022	DacoTaco

# This code is licensed to you under the terms of the GNU GPL, version 2;
# see file COPYING or http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt
*/

#include <errno.h>
#include <ios/errno.h>
#include <ios/processor.h>
#include <ios/syscalls.h>
#include <ios/printk.h>

#include "interface.h"
#include "fs.h"
#include "fileSystem.h"

int main(void)
{
	u32 messageQueueMessages[8] ALIGNED(0x10);
	u32* message;
	printk("$IOSVersion:  FFSP: %s %s 64M $", __DATE__, __TIME__);
	s32 ret = OSCreateMessageQueue((void**)&messageQueueMessages, 8);
	u32 messageQueueId = (u32)ret;
	if(ret >= 0) ret = OSRegisterResourceManager("/dev/boot2", messageQueueId);
	if(ret >= 0) ret = OSRegisterResourceManager("/", messageQueueId);
	if(ret < IPC_SUCCESS)
		return ret;
	
	ret = InitializeNand();
	if(ret == IPC_SUCCESS) ret = InitializeSuperBlockInfo(1);

	while(1)
	{
      	s32 ret = OSReceiveMessage(messageQueueId, &message, 0);
		if(ret < 0)
			break;
	}
	return 0;
}