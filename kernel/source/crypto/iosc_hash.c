/*
	starstruck - a Free Software reimplementation for the Nintendo/BroadOn IOS.
	iosc_hash - iosc's hash functions

	Copyright (C) 2021	DacoTaco

# This code is licensed to you under the terms of the GNU GPL, version 2;
# see file COPYING or http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt
*/

#include <ios/errno.h>
#include <ios/ipc.h>
#include <ios/sha.h>
#include <ios/keyring.h>
#include <string.h>

#include "memory/heaps.h"
#include "filedesc/calls_inner.h"
#include "crypto/iosc_helpers.h"

s32 GenerateBlockMAC(const ShaContext* context, const void* inputData, const u32 inputSize, const void* customData,
                     const u32 customDataSize, const u32 keyHandle, const HMacCommandType hmacCommand,
                     const void* signData, const s32 messageQueueId, IpcMessage* message)
{
	if (((u32)inputData & 0x3F) != 0)
		return -2016;

	//hmac command should be the 0 based command, not the merged command
	if (hmacCommand > FinalizeHmacState)
		return IPC_EINVAL;

	s32 ret = 0;
	IoctlvMessageData* messageData = (IoctlvMessageData*)AllocateOnHeap(KernelHeapId, 0x28);
	if (messageData == NULL)
	{
		ret = IPC_ENOMEM;
		goto _hmac_generate_cleanup_return;
	}

	messageData->Length = inputSize;
	messageData->Data = (void*)inputData;
	messageData[1].Data = (void*)context;
	messageData[1].Length = sizeof(ShaContext);
	messageData[2].Data = (void*)signData;
	messageData[2].Length = signData == NULL ? 0 : 0x14;
	messageData[3].Data = (void*)&keyHandle;
	messageData[3].Length = sizeof(u32);
	messageData[4].Data = (void*)customData;
	messageData[4].Length = customDataSize;

	ret = messageQueueId == -1 ?
	          DispatchIoctlv(SHA_STATIC_FILEDESC, hmacCommand, 3, 2, messageData) :
	          DispatchIoctlvAsync(SHA_STATIC_FILEDESC, hmacCommand, 3, 2, messageData, messageQueueId, message);

	// On success, the SHA engine owns messageData and will free it
	if (ret == IPC_SUCCESS)
		return IPC_SUCCESS;

_hmac_generate_cleanup_return:
	if (messageData)
		FreeOnHeap(KernelHeapId, messageData);

	return ret;
}

s32 GenerateHash(const ShaContext* context, const void* inputData, const u32 inputSize, const u32 shaCommand,
                 void* digest, const s32 messageQueueId, IpcMessage* message)
{
	if (((u32)inputData & 0x3F) != 0)
		return IOSC_INVALID_ALIGN;

	/* validate sha command: only Init/Contribute/Finalize are allowed */
	if (shaCommand > FinalizeShaState)
		return IPC_EINVAL;

	IoctlvMessageData* messageData = (IoctlvMessageData*)AllocateOnHeap(KernelHeapId, 0x18);
	if (messageData == NULL)
		return IPC_ENOMEM;

	messageData[0].Data = (void*)inputData;
	messageData[0].Length = inputSize;
	messageData[1].Data = (void*)context;
	messageData[1].Length = sizeof(ShaContext);
	messageData[2].Data = (void*)digest;
	messageData[2].Length = digest == NULL ? 0 : 0x14;

	s32 ret = messageQueueId == -1 ?
	              DispatchIoctlv(SHA_STATIC_FILEDESC, shaCommand, 1, 2, messageData) :
	              DispatchIoctlvAsync(SHA_STATIC_FILEDESC, shaCommand, 1, 2, messageData, messageQueueId, message);

	/* On success the SHA engine takes ownership of messageData and will free it */
	if (ret != IPC_SUCCESS)
		FreeOnHeap(KernelHeapId, messageData);

	return ret;
}
