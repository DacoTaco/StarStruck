/*
	starstruck - a Free Software reimplementation for the Nintendo/BroadOn IOS.
	sha - the sha engine in starlet

	Copyright (C) 2021	DacoTaco
	Copyright (C) 2023	Jako

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
#include "core/defines.h"
#include "interrupt/irq.h"
#include "messaging/message_queue.h"
#include "messaging/resourceManager.h"
#include "messaging/ipc.h"
#include "filedesc/filedesc_types.h"

FileDescriptor ShaFileDescriptor SRAM_BSS;

typedef enum 
{
	SHA_InitState = 0x00,
	SHA_ContributeState = 0x01,
	SHA_FinalizeState = 0x02,
	HMAC_InitState = 0x03,
	HMAC_ContributeState = 0x04,
	HMAC_FinalizeState = 0x05,
	ShaCommandUnknown = 0x0F
} ShaIoctlvCommandTypes;

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
u8 LastBlockBuffer[(SHA_BLOCK_SIZE * 2)] ALIGNED(SHA_BLOCK_SIZE) = { 0x00 };
u32 ShaEventMessageQueueId = 0;

s32 GenerateSha(ShaContext* hashContext, const void* input, const u32 inputSize, const ShaCommandType command, FinalShaHash finalHashBuffer)
{
	u32 numberOfBlocks = 0;
	s32 ret = IPC_EINVAL;
	write32(SHA_CMD, 0);

	//chainingMode 0 == reset. so we set the internal hash states to the initial state
	if(command == InitShaState)
	{
		memcpy(hashContext->ShaStates, Sha1IntialState, sizeof(Sha1IntialState));
		hashContext->Length = 0;
		ret = IPC_SUCCESS;
	}

	//floors data size to blocks of 512 bits
	const u32 flooredDataSize = inputSize & (u32)(~(SHA_BLOCK_SIZE-1));
	DCFlushRange(input, flooredDataSize);
	AhbFlushTo(AHB_SHA1);

	//happens with all commands
	if(flooredDataSize != 0)
	{
		//check the requested blocks to be processed
		numberOfBlocks = (flooredDataSize / 64) -1;
		if(numberOfBlocks >= 1024)
			return IOSC_INVALID_SIZE;

		//if this isn't the last block contributed, make sure the input data is a whole multiple of blocks large
		if ((command != FinalizeShaState) && ((inputSize & (SHA_BLOCK_SIZE-1)) != 0x0))
        	return IOSC_INVALID_SIZE;

		//copy over the states from the context to the registers
		for(s8 i = 0; i < SHA_NUM_WORDS; i++)
			write32((u32)(SHA_H0 + (i*4)), hashContext->ShaStates[i]);
		
		write32(SHA_SRC, VirtualToPhysical((u32)input));
		ShaControl control = {
			.Fields = {
				.Execute = 1,
				.GenerateIrq = 1,
				.NumberOfBlocks = numberOfBlocks & 0x3FF
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

	//FinalizeShaState : Last block contributed to hash
	if(command == FinalizeShaState)
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
				write32((u32)(SHA_H0 + (i*4)), hashContext->ShaStates[i]);
			}
		}
		
		//set up hash engine
		write32(SHA_SRC, VirtualToPhysical((u32)LastBlockBuffer));
		ShaControl control = {
			.Fields = {
				.Execute = 1,
				.GenerateIrq = 0,	//no irq for this one, instead the function idles until it detects the execution has halted
				.NumberOfBlocks = (numberOfBlocks -1) & 0x3FF
			}
		};

		//execute hash engine, and while waiting spin idly
		write32(SHA_CMD, control.Value);
		while (((ShaControl)read32(SHA_CMD)).Fields.Execute == 1) {}

		//copy over the states from the registers to the context
		for(s8 i = 0; i < SHA_NUM_WORDS; i++)
			finalHashBuffer[i] = read32((u32)(SHA_H0 + (i * 4)));
		
		ret = IPC_SUCCESS;
	}

	//happens in all chaining modes
	else if(flooredDataSize != 0)
	{
		hashContext->Length += flooredDataSize * 8;
		//copy over the states from the registers to the context
		for(s8 i = 0; i < SHA_NUM_WORDS; i++)
			hashContext->ShaStates[i] = read32((u32)(SHA_H0 + (i * 4)));

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

	s32 ret = CreateMessageQueue((void**)&eventMessageQueue, 1);
	ShaEventMessageQueueId = (u32)ret;
	if(ret < 0)
		panic("Unable to create SHA event queue: %d\n", ret);
	
	ret = RegisterEventHandler(IRQ_SHA1, ShaEventMessageQueueId, NULL);
	if(ret < IPC_SUCCESS)
		panic("Unable to register SHA event handler: %d\n", ret);

	ret = CreateMessageQueue((void**)&resourceManagerMessageQueue, 0x10);
	if(ret < IPC_SUCCESS)
		panic("Unable to create SHA rm queue: %d\n", ret);
	
	const u32 messageQueueId = (u32)ret;
	ret = RegisterResourceManager(SHA_DEVICE_NAME, messageQueueId);
	if(ret < IPC_SUCCESS)
		panic("Unable to register resource manager: %d\n", ret);

	while(1)
	{
		//main loop should start here
		ret = ReceiveMessage(messageQueueId, (void**)&ipcMessage, None);
		if(ret != IPC_SUCCESS)
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
				if(ret != IPC_SUCCESS)
					ret = IPC_ENOENT;
				//not needed, since 0 == IPC_SUCCESS anyway
				/*else
					ret = IPC_SUCCESS;*/
				
				goto sendReply;
			case IOS_IOCTLV:
				ioctlvMessage = &ipcMessage->Request.Data.Ioctlv;
				u32 ioctl = ioctlvMessage->Ioctl;
				IoctlvMessageData* messageData = ioctlvMessage->Data;
				switch (ioctl)
				{
					/*it seems each of these are split based on whether they handle SHA hashing or HMAC verification.
					cases 0, 1, 2 handle SHA-1 hashing, with ioctl deciding chainingMode for GenerateSha() call
					cases 3, 4, 5 handle HMAC verification, with (ioctl - 3) deciding chainingMode for GenerateSha() calls
					no clue what case 0xF does though*/
					case SHA_InitState:
					case SHA_ContributeState:
					case SHA_FinalizeState:
						if(ioctlvMessage->InputArgc != 1 || ioctlvMessage->IoArgc != 2)
							break;
						
						ret = GenerateSha((ShaContext*)ioctlvMessage->Data[1].Data, ioctlvMessage->Data[0].Data, 
										  ioctlvMessage->Data[0].Length, (ShaCommandType)ioctl, (u32*)ioctlvMessage->Data[2].Data);
						
						if(ret != IPC_SUCCESS)
							goto sendReply;

						break;

					case HMAC_InitState:
					case HMAC_ContributeState:
					case HMAC_FinalizeState:
						ShaCommandType shaCommand = (ShaCommandType)ioctl - 3;

						if (shaCommand != ContributeShaState) {
							if (messageData[3].Length == 4) {
								u32 signerHandle = *messageData[6].Data;
							}
						}

						break;
						
					case ShaCommandUnknown:
						/* code */
						break;
				
					default:
						goto sendReply;
				}
				
				if (ret == IPC_SUCCESS) {
					FreeOnHeap(KernelHeapId, ioctlvMessage->Data);
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