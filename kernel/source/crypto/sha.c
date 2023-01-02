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

const u8 ShaUnknownBuffer[0x80] = { 0 };
const u32 Sha1IntialState[5] = { 0x67452301, 0xEFCDAB89, 0x98BADCFE, 0x10325476, 0xC3D2E1F0 };
u8 HmacKey[64] = { 0x00 };
s32 ShaEventMessageQueueId = 0;

s32 GenerateSha(ShaContext* hashContext, void* input, u32 inputSize, s32 chainingMode, int hashData)
{
	u32 numberOfBlocks = 0;
	u32 physicalInputAddress = 0;
	u32* ShaControlPointer = NULL;
	s32 ret = IPC_EINVAL;
	write32(SHA_CMD, 0);

	//chainingMode 0 == reset. so we set the internal hash states to the initial state
	if(chainingMode == 0)
	{
		memcpy(hashContext->ShaStates, Sha1IntialState, sizeof(Sha1IntialState));
		hashContext->LengthLower = 0;
		hashContext->LengthHigher = 0;
		ret = 0;
	}

	u32 flooredDataSize = inputSize & 0xFFFFFFC0;	//floors data size to blocks of 512 bits
	DCFlushRange(input, flooredDataSize);
	AhbFlushTo(AHB_SHA1);
	if(flooredDataSize != 0)
	{
		//check the requested blocks to be processed
		numberOfBlocks = (inputSize / 64) -1;
		if(numberOfBlocks > 1023)
			return IOSC_INVALID_SIZE;

		//copy over the states from the context to the registers
		for(s8 i = 0; i < 5; i++)
			write32(SHA_H0+i, hashContext->ShaStates[i]);
		
		physicalInputAddress = VirtualToPhysical((u32)input);
		ShaControlPointer = (u32*)SHA_CMD;
		write32(SHA_SRC, physicalInputAddress);
		ShaControl control = {
			.Fields = {
				.Execute = 1,
				.GenerateIrq = 1,
				.NumberOfBlocks = numberOfBlocks
			}
		};

		write32(SHA_CMD, control.Value);
		u32* message;
		ret = ReceiveMessage(ShaEventMessageQueueId, &message, None);
		if(ret != IPC_SUCCESS)
			panic("iosReceiveMessage: %d\n", ret);

		control.Value = read32(SHA_CMD);
		if(control.Fields.HasError != 0)
			return IPC_EACCES;
	}

	ShaControlPointer = ShaUnknownBuffer;

	// ChangingMode 2 : Last block contributed to hash
	if(chainingMode == 2)
	{
		u32 higherBits = hashContext->LengthHigher;
		u32 lowerBits = hashContext->LengthLower;
		//if we had an overflow in the lower bits, we need to raise the upper bits by 1
		if(((flooredDataSize * 8) + lowerBits) < lowerBits)
			higherBits += 1;
		
		//add size to the higher bits
		higherBits += inputSize >> 29;

		//set length properties
		hashContext->LengthLower = numberOfBlocks;
		hashContext->LengthHigher = higherBits;

		//I think this is trying to calculate some sort of padding. 
		//Notice it subtracting inputSize from flooredDataSize.
		memset(ShaControlPointer, 0, ARRAY_LENGTH(ShaUnknownBuffer));
		u32 lastBlockLength = inputSize - flooredDataSize;
		if(lastBlockLength != 0)
			memcpy(ShaControlPointer, input + flooredDataSize, lastBlockLength);
		ShaControlPointer[lastBlockLength] = 0x80;

		//reset the length properties?
		lowerBits = (lastBlockLength) * 8 + numberOfBlocks;
		if(lowerBits < numberOfBlocks)
			higherBits++;

		higherBits += lastBlockLength >> 29;
		hashContext->LengthLower = lowerBits;
		hashContext->LengthHigher = higherBits;
	}
}

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
					case SHACommandZero:
					case SHACommandOne:
					case SHACommandTwo:
					case SHACommandThree:
					case ShaCommandFifthTeen:
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