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
	InitializeHashContext = 0x00,
	AddData = 0x01,
	FinalizeHash = 0x02,
	SHACommandThree = 0x03,
	ShaCommandFifthTeen = 0x0F
} ShaCommandTypes;

typedef union {
	struct {
		u32 Execute : 1;
		u32 GenerateIrq : 1;
		u32 HasError : 1;
		u32 Unknown : 19;
		u32 NumberOfBlocks : 10;
	} Fields;
 	u32 Value;
} ShaControl;

const u32 Sha1IntialState[SHA_NUM_WORDS] = { 0x67452301, 0xEFCDAB89, 0x98BADCFE, 0x10325476, 0xC3D2E1F0 };
u8 LastBlockBuffer[0x80] ALIGNED(0x40) = { 0x00 };
u8 HmacKey[0x40] ALIGNED(0x40) = { 0x00 };
s32 ShaEventMessageQueueId = 0;

s32 GenerateSha(ShaContext* hashContext, const void* input, u32 inputSize, s32 chainingMode, u32* hashData)
{
	u32 numberOfBlocks = 0;
	s32 ret = IPC_EINVAL;
	write32(SHA_CMD, 0);

	//chainingMode 0 == reset. so we set the internal hash states to the initial state
	if(chainingMode == 0)
	{
		memcpy(hashContext->ShaStates, Sha1IntialState, sizeof(Sha1IntialState));
		hashContext->Length = 0;
		ret = 0;
	}

	//floors data size to blocks of 512 bits
	const u32 flooredDataSize = inputSize & (~(SHA_BLOCK_SIZE-1));
	DCFlushRange(input, flooredDataSize);
	AhbFlushTo(AHB_SHA1);

	//happens in all chaining modes
	if(flooredDataSize != 0)
	{
		//check the requested blocks to be processed
		numberOfBlocks = (flooredDataSize / 64) -1;
		if(numberOfBlocks >= 1024)
			return IOSC_INVALID_SIZE;

		//if this isn't the last block contributed, make sure the input data is a whole multiple of blocks large
		if ((chainingMode != 2) && ((inputSize & (SHA_BLOCK_SIZE-1)) != 0x0))
        	return IOSC_INVALID_SIZE;

		//copy over the states from the context to the registers
		for(s8 i = 0; i < SHA_NUM_WORDS; i++)
			write32(SHA_H0 + (i*4), hashContext->ShaStates[i]);
		
		write32(SHA_SRC, VirtualToPhysical((u32)input));
		ShaControl control = {
			.Fields = {
				.Execute = 1,
				.GenerateIrq = 1,
				.NumberOfBlocks = numberOfBlocks
			}
		};

		write32(SHA_CMD, control.Value);
		void* message;
		ret = ReceiveMessage(ShaEventMessageQueueId, &message, None);
		if(ret != IPC_SUCCESS)
			panic("iosReceiveMessage: %d\n", ret);

		control.Value = read32(SHA_CMD);
		if(control.Fields.HasError != 0)
			return IPC_EACCES;
	}

	// ChangingMode 2 : Last block contributed to hash
	if(chainingMode == 2)
	{
		hashContext->Length += flooredDataSize * 8;

		//This pads the final block (or rather 2 blocks) of data
		memset(LastBlockBuffer, 0, (SHA_BLOCK_SIZE * 2));
		u32 lastBlockLength = inputSize - flooredDataSize;
		if(lastBlockLength != 0)
			memcpy(LastBlockBuffer, input + flooredDataSize, lastBlockLength);
		
		LastBlockBuffer[lastBlockLength] = 0x80;	//Demarcates end of last block's data and beginning of padding
		hashContext->Length += lastBlockLength * 8;	
		numberOfBlocks = ((lastBlockLength + 1) < (SHA_BLOCK_SIZE - 7)) ? 1 : 2;
		
		//places the 64-bit length value at the end of the block the data ends in
		//I think this is what's happening, but the decompiled pseudocode is next to unreadable 
		//winging it for now, should be tested to ensure it behaves as intended
		u32 index = numberOfBlocks * SHA_BLOCK_SIZE;
		write32((u32)&LastBlockBuffer[index-4], (u32)hashContext->Length);
		write32((u32)&LastBlockBuffer[index-8], (u32)(hashContext->Length >> 32));
		DCFlushRange(LastBlockBuffer, (numberOfBlocks * SHA_BLOCK_SIZE));
		AhbFlushTo(AHB_SHA1);

		//copy over the states from the context to the registers
		if (flooredDataSize == 0) {
			for(s8 i = 0; i < SHA_NUM_WORDS; i++) {
				write32(SHA_H0 + (i*4), hashContext->ShaStates[i]);
			}
		}
		
		//set up hash engine
		write32(SHA_SRC, VirtualToPhysical((u32)LastBlockBuffer));
		ShaControl control = {
			.Fields = {
				.Execute = 1,
				.GenerateIrq = 0,	//no irq for this one, instead the function idles until it detects the execution has halted
				.NumberOfBlocks = numberOfBlocks -1
			}
		};

		//execute hash engine, and while waiting spin idly
		write32(SHA_CMD, control.Value);
		while (((ShaControl)read32(SHA_CMD)).Fields.Execute == 1) {}

		//copy over the states from the registers to the context
		for(s8 i = 0; i < SHA_NUM_WORDS; i++)
			hashData[i] = read32(SHA_H0 + (i * 4));
		
		ret = IPC_SUCCESS;
	}

	//happens in all chaining modes
	if(flooredDataSize != 0)
	{
		hashContext->Length += flooredDataSize * 8;
		//copy over the states from the registers to the context
		for(s8 i = 0; i < SHA_NUM_WORDS; i++)
			hashContext->ShaStates[i] = read32(SHA_H0 + (i * 4));

		ret = IPC_SUCCESS;
	}

	return ret;
}

void ShaEngineHandler(void)
{
	u32 eventMessageQueue[1];
	u32 resourceManagerMessageQueue[0x10];
	IpcMessage* ipcMessage;
	IoctlvMessage* ioctlvMessage;
	IpcMessage* ipcReply;

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

		ipcReply = ipcMessage;
		ret = IPC_EINVAL;
		s32 hmacRet = IPC_EINVAL;
		switch (ipcMessage->Request.Command)
		{
			default:
				goto sendReply;
			case IOS_CLOSE:
				ret = IPC_SUCCESS;
				goto sendReply;
			case IOS_OPEN:
				ret = memcmp(ipcMessage->Request.Data.Open.Filepath, SHA_DEVICE_NAME, SHA_DEVICE_NAME_SIZE);
				if(ret != 0)
					ret = IPC_ENOENT;
				//not needed, since 0 == IPC_SUCCESS anyway
				/*else
					ret = IPC_SUCCESS;*/
				
				goto sendReply;
			case IOS_IOCTLV:
				ioctlvMessage = &ipcMessage->Request.Data.Ioctlv;
				s32 ioctl = ioctlvMessage->Ioctl;
				switch (ioctl)
				{
					case InitializeHashContext:
					case AddData:
					case FinalizeHash:
						if(ioctlvMessage->InputArgc != 1 || ioctlvMessage->IoArgc != 2)
							break;
						ret = GenerateSha((ShaContext*)ioctlvMessage->Data[1].Data, ioctlvMessage->Data[0].Data, ioctlvMessage->Data[0].Length, ioctl, ioctlvMessage->Data[2].Data);
						if(ret != IPC_SUCCESS)
							goto sendReply;

						break;
					case SHACommandThree:
					case ShaCommandFifthTeen: 
						break;
				
					default:
						goto sendReply;
				}
				break;
		}

		//remove message data from heap if it came from there
		FreeOnHeap(KernelHeapId, ioctlvMessage->Data);

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