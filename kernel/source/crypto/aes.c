/*
	starstruck - a Free Software reimplementation for the Nintendo/BroadOn IOS.
	aes - the aes engine in starlet

	Copyright (C) 2021	DacoTaco

# This code is licensed to you under the terms of the GNU GPL, version 2;
# see file COPYING or http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt
*/

#include <string.h>
#include <ios/processor.h>
#include <ios/errno.h>

#include "aes.h"
#include "panic.h"
#include "memory/memory.h"
#include "memory/heaps.h"
#include "core/hollywood.h"
#include "core/defines.h"
#include "interrupt/irq.h"
#include "messaging/message_queue.h"
#include "messaging/resourceManager.h"
#include "messaging/ipc.h"
#include "filedesc/filedesc_types.h"

FileDescriptor AesFileDescriptor SRAM_BSS;

typedef enum 
{ 
	COPY = 0,
	ENCRYPT = 2,
	DECRYPT = 3
} AESCommandTypes;

typedef union {
	struct {
		u32 Command : 1;
		u32 GenerateIrq : 1;
		u32 HasError : 1;
		u32 EnableDataHandling : 1;
		u32 IsDecryption : 1;
		u32 Unknown : 14;
		u32 KeepIV : 1;
		u32 NumberOfBlocks : 12;
	} Fields;
	u32 Value;
} AESCommand;

s32 AesEventMessageQueueId = 0;

void AesEngineHandler(void)
{
	u32 eventMessageQueue[1];
	u32 resourceManagerMessageQueue[8];
	u32 ivBuffer[0x10] = { 0 };
	IpcMessage* ipcMessage;
	IoctlvMessage* ioctlvMessage;
	IpcMessage* ipcReply;

	s32 messageQueue = CreateMessageQueue((void**)&eventMessageQueue, 1);
	AesEventMessageQueueId = messageQueue;
	if(messageQueue < 0)
		panic("Unable to create AES event queue: %d\n", messageQueue);

	s32 ret = RegisterEventHandler(IRQ_AES, AesEventMessageQueueId, 0);
	if(ret < 0)
		panic("Unable to register AES event handler: %d\n", ret);

	ret = CreateMessageQueue((void**)&resourceManagerMessageQueue, 8);
	if(ret < 0)
		panic("Unable to create AES rm queue: %d\n", ret);

	ret = RegisterResourceManager(AES_DEVICE_NAME, ret);
	if(ret < 0)
		panic("Unable to register resource manager: %d\n", ret);
	
	while(1)
	{
		//main loop should start here
		ret = ReceiveMessage(messageQueue, (void**)&ipcMessage, None);
		if(ret != 0)
			goto receiveMessageError;

		ipcReply = ipcMessage;
		ret = IPC_EINVAL;
		IoctlvMessageData* IVVector = NULL;
		IoctlvMessageData* sourceVector = NULL;
		switch (ipcMessage->Request.Command)
		{
			default:
				goto sendReply;
			case IOS_CLOSE:
				ret = IPC_SUCCESS;
				goto sendReply;
			case IOS_OPEN:
				ret = memcmp(ipcMessage->Request.Data.Open.Filepath, AES_DEVICE_NAME, AES_DEVICE_NAME_SIZE);
				if(ret != 0)
					ret = IPC_ENOENT;
				//not needed, since 0 == IPC_SUCCESS anyway
				/*else
					ret = IPC_SUCCESS;*/
				
				goto sendReply;
			case IOS_IOCTLV:
				ret = IPC_EINVAL;
				ioctlvMessage = &ipcMessage->Request.Data.Ioctlv;
				write32(AES_CMD, 0 );

				u32 ioctl = ioctlvMessage->Ioctl;
				switch (ioctl)
				{
					case ENCRYPT:
					case DECRYPT:
						if(ioctlvMessage->InputArgc != 2 || ioctlvMessage->IoArgc != 2)
							goto sendReply;
						if(ioctlvMessage->Data[1].Length != 0x10 || ((u32)ioctlvMessage->Data[1].Data & 3) != 0 ||
						   ioctlvMessage->Data[3].Length != 0x10 || ((u32)ioctlvMessage->Data[3].Data & 3) != 0)
							goto sendReply;

						//lets copy over the AES key & IV
						u32* key = ioctlvMessage->Data[1].Data;
						u32* iv = ioctlvMessage->Data[3].Data;
						for(int i = 0; i < 4; i++) 
						{
							write32(AES_KEY, *key);
							write32(AES_IV, *iv);
							key++;
							iv++;
						}
						IVVector = &ioctlvMessage->Data[3];
						sourceVector = &ioctlvMessage->Data[0];
						goto processAesCommand;
					case COPY:
						if(ioctlvMessage->InputArgc != 1 || ioctlvMessage->IoArgc != 1)
							goto sendReply;
processAesCommand:
						IoctlvMessageData* inputData = &ioctlvMessage->Data[0];
						IoctlvMessageData* outputData = &ioctlvMessage->Data[ioctlvMessage->InputArgc];
						if( inputData->Length != outputData->Length || ((inputData->Length - 0x10) & 0xFFFF000F) != 0 ||
							((u32)inputData->Data & 0x0F) != 0 || ((u32)outputData->Data & 0x0F) != 0)
							goto sendReply;
						if(ioctl == DECRYPT)
							memcpy(ivBuffer, inputData->Data + inputData->Length - 0x10, 0x10);
						
						write32(AES_SRC, VirtualToPhysical((u32)inputData->Data));
						write32(AES_DEST, VirtualToPhysical((u32)outputData->Data));
						DCFlushRange(inputData->Data, inputData->Length);
						DCInvalidateRange(outputData->Data, outputData->Length);
						AhbFlushTo(AHB_AES);
						
						AESCommand command =
						{
							.Fields = {
								.Command = 1,
								.GenerateIrq = 1,
								.EnableDataHandling = ioctl != 0,
								.IsDecryption = ioctl == DECRYPT,
								.KeepIV = 0,
								.NumberOfBlocks = (inputData->Length - 0x10U) >> 4
							}
						};
						write32(AES_CMD, command.Value);
						u32* irqMessage;
						ret = ReceiveMessage(AesEventMessageQueueId, (void**)&irqMessage,0);
						if(ret != IPC_SUCCESS)
							goto receiveMessageError;

						AhbFlushFrom(AHB_AES);
						AhbFlushTo(AHB_STARLET);
						command.Value = read32(AES_CMD);
						if(command.Fields.HasError)
						{
							ret = -1;
							goto sendReply;
						}
						
						if(IVVector != NULL)
						{
							if(ioctl == ENCRYPT)
								memcpy(ivBuffer, outputData->Data + outputData->Length - 0x10, 0x10);
							
							if(ioctl <= DECRYPT)
								memcpy(IVVector->Data, ivBuffer, 0x10);

							if(sourceVector != NULL)
								FreeOnHeap(KernelHeapId, sourceVector->Data);
							
							FreeOnHeap(KernelHeapId, ipcReply->Request.Data.Ioctlv.Data);
							ret = 0;
						}
						goto sendReply;
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