/*
	starstruck - a Free Software reimplementation for the Nintendo/BroadOn IOS.
	sha - the sha engine in starlet

	Copyright (C) 2021	DacoTaco

# This code is licensed to you under the terms of the GNU GPL, version 2;
# see file COPYING or http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt
*/

#include <string.h>
#include <ios/processor.h>
#include <ios/errno.h>

#include "sha.h"
#include "panic.h"
#include "memory/memory.h"
#include "memory/heaps.h"
#include "core/hollywood.h"
#include "interrupt/irq.h"
#include "messaging/message_queue.h"
#include "messaging/resourceManager.h"
#include "messaging/ipc.h"

typedef enum 
{ 
	SHACommandZero = 0x00,
	SHACommandOne = 0x01,
	SHACommandTwo = 0x02,
	SHACommandThree = 0x03,
	ShaCommandSixTeen = 0x0F
} SHACommandTypes;

s32 ShaEventMessageQueueId = 0;

void ShaEngineHandler(void)
{
	u32 eventMessageQueue[1];
	u32 resourceManagerMessageQueue[0x10];
	IpcMessage* ipcMessage;
	IoctlvMessage* ioctlvMessage;
	IpcRequest* ipcReply;

	s32 messageQueueId = CreateMessageQueue((void**)&eventMessageQueue, 1);
	ShaEventMessageQueueId = messageQueueId;
	if(messageQueueId < 0)
		panic("Unable to create SHA event queue: %d\n", messageQueueId);
	
	s32 ret = RegisterEventHandler(IRQ_SHA1, messageQueueId, NULL);
	if(ret < 0)
		panic("Unable to register SHA event handler: %d\n", ret);

	ret = CreateMessageQueue((void**)&resourceManagerMessageQueue, 0x10);
	if(ret < 0)
		panic("Unable to create SHA rm queue: %d\n", ret);
	
	ret = RegisterResourceManager(SHA_DEVICE_NAME, ret);
	if(ret < 0)
		panic("Unable to register resource manager: %d\n", ret);

	while(1)
	{
		//main loop should start here
		ret = ReceiveMessage(messageQueueId, (void**)&ipcMessage, None);
		if(ret != 0)
			goto receiveMessageError;

		ret = IPC_EINVAL;
		switch (ipcMessage->Request.Command)
		{
			default:
				goto sendReply;
			case IOS_CLOSE:
				ret = IPC_SUCCESS;
				goto sendReply;
			case IOS_OPEN:
				ret = memcmp(&ipcMessage->Request.Open.Filepath, SHA_DEVICE_NAME, SHA_DEVICE_NAME_SIZE);
				if(ret != 0)
					ret = IPC_ENOENT;
				//not needed, since 0 == IPC_SUCCESS anyway
				/*else
					ret = IPC_SUCCESS;*/
				
				goto sendReply;
			case IOS_IOCTLV:
				ioctlvMessage = &ipcMessage->Request.Ioctlv;
				s32 ioctl = ioctlvMessage->Ioctl;
				switch (ioctl)
				{
					case SHACommandZero:
					case SHACommandOne:
					case SHACommandTwo:
					case SHACommandThree:
					case ShaCommandSixTeen:
						/* code */
						break;
				
					default:
						goto sendReply;
				}
				break;
		}

sendReply:
		ResourceReply(ipcReply, ret);
		continue;
receiveMessageError:
		panic("iosReceiveMessage: %d\n", ret);
		//not sure why ios does this haha
		//the panic makes it loop forever...
		if(ipcMessage->Request.Command == IOS_CLOSE)
			break;
	}

}