/*
	starstruck - a Free Software reimplementation for the Nintendo/BroadOn IOS.
	iosc - ios crypto syscalls

	Copyright (C) 2021	DacoTaco
	Copyright (C) 2023	Jako

# This code is licensed to you under the terms of the GNU GPL, version 2;
# see file COPYING or http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt
*/

#include "crypto/iosc.h"
#include "crypto/otp.h"
#include "crypto/keyring.h"
#include "crypto/boot2.h"
#include "crypto/nand.h"
#include "crypto/seeprom.h"
#include "interrupt/irq.h"
#include "filedesc/calls_inner.h"
#include "memory/memory.h"
#include "memory/heaps.h"
#include "messaging/messageQueue.h"
#include <ios/errno.h>
#include <ios/ipc.h>

#ifndef MIOS

static u32 IOSC_BOOT2_DummyVersion = 0;
static u32 IOSC_BOOT2_DummyUnk1 = 0;
static u32 IOSC_BOOT2_DummyUnk2 = 0;
static u32 IOSC_NAND_DummyGen = 0;
static u32 IOSC_SEEPROM_DummyPRNGSeed = 0;

typedef struct {
	u32 val0;
	void* message;
	u32 messageQueueId;
	u32 val3;
	u32 rngSeed;
} IOSC_InformationHolder;

static IOSC_InformationHolder IOSC_Information;
// provisional size, is located at the end of the kernel section
extern u32 __ioscstack_addr[];
#define IOSC_SafeStackEnd (u32)__ioscstack_addr

extern void IOSC_SwapStack(u32 currentStackBase, u32 newStackBase);

static s32 IOSC_BOOT2_UpdateVersion(void);
static s32 IOSC_BOOT2_UpdateUnk1(void);
static s32 IOSC_BOOT2_UpdateUnk2(void);
static s32 IOSC_NAND_UpdateGen(void);
static s32 IOSC_SEEPROM_UpdatePRNGSeed(void);
static s32 IOSC_SEEPROM_GetPRNGSeed(void);

static s32 IOSC_SendEmptyMessageToQueue(const u32 queueId)
{
	s32 ret = SendMessageUnsafe(queueId, NULL, RegisteredEventHandler);
	return ret;
}
static s32 IOSC_DiscardMessageFromQueue(const u32 queueId)
{
	IpcMessage* receivedMessage = NULL;
	s32 ret = ReceiveMessageUnsafe(queueId, (void**)&receivedMessage, None);
	return ret;
}
static inline s32 IOSC_CheckCurrentProcessOwnsKey(u32 keyHandle)
{
	if(keyHandle == RSA4096_ROOTKEY)
		return IPC_SUCCESS;

	u32 ownerProcess = 0;
	s32 ret = Keyring_GetKeyOwnerProcess(keyHandle, &ownerProcess);
	if(ret != IPC_SUCCESS)
		return ret;

	if(((1 << (CurrentThread->ProcessId & 0xff)) & ownerProcess) == 0)
		return IOSC_EACCES;

	return IPC_SUCCESS;
}
static inline s32 IOSC_CheckCurrentProcessCanRead(const void* ptr, u32 size)
{
	return CheckMemoryPointer(ptr, size, 3, CurrentThread->ProcessId, 0);
}
static inline s32 IOSC_CheckCurrentProcessCanReadWrite(const void* ptr, u32 size)
{
	return CheckMemoryPointer(ptr, size, 4, CurrentThread->ProcessId, 0);
}

static s32 DispatchIoctlv(s32 fd, u32 requestId, u32 vectorInputCount, u32 vectorIOCount, IoctlvMessageData* vectors)
{
	u32 flags = DisableInterrupts();
	s32 ret = IoctlvFD_InnerWithFlag(fd, requestId, vectorInputCount, vectorIOCount, vectors, NULL, NULL, 0);

	RestoreInterrupts(flags);
	return ret;
}

static s32 DispatchIoctlvAsync(s32 fd, u32 requestId, u32 vectorInputCount, u32 vectorIOCount, IoctlvMessageData *vectors, u32 messageQueueId, IpcMessage* message)
{
	u32 flags = DisableInterrupts();
	s32 ret = 0;
	if(messageQueueId >= MAX_MESSAGEQUEUES)
	{
		ret = -1;
		goto _return_cleanup_dispatchIoctlvAsync;
	}

	if(MessageQueues[messageQueueId].ProcessId != CurrentThread->ProcessId)
	{
		return -4;
		goto _return_cleanup_dispatchIoctlvAsync;
	}

	ret = CheckMemoryPointer(message, 0x20, 4, MessageQueues[messageQueueId].ProcessId, 0);
	if(ret != 0)
		goto _return_cleanup_dispatchIoctlvAsync;

	ret = IoctlvFD_InnerWithFlag(fd, requestId, vectorInputCount, vectorIOCount, vectors, &MessageQueues[messageQueueId], message, 0);

_return_cleanup_dispatchIoctlvAsync:
	RestoreInterrupts(flags);
	return ret;
}

static s32 _IOSC_Decrypt(const u32 keyHandle, void* ivData, const void* inputData, const u32 dataSize, void* outputData, const u32 MessageQueueId, IpcMessage* message )
{
	if(((u32)inputData & 0x1F) != 0 || ((u32)outputData & 0x1F) != 0)
		return -2016;

	void* keyBlob = AllocateOnHeap(KernelHeapId, 0x10);
	if(keyBlob == NULL)
		return -22;
	
	s32 ret = 0;
	IoctlvMessageData* messageData = (IoctlvMessageData*)AllocateOnHeap(KernelHeapId, 0x20);
	if(messageData == NULL)
	{
		ret = -22;
		goto _aes_decrypt_cleanup_return;
	}

	u32 keyRingSize = 0;
	ret = Keyring_FindKeySize(&keyRingSize, keyHandle);
	if(ret != 0)
		goto _aes_decrypt_cleanup_return;

	ret = Keyring_GetKey(keyHandle, keyBlob, keyRingSize);
	if(ret != 0)
	{
		ret = -21;
		goto _aes_decrypt_cleanup_return;
	}

	messageData->Data = (void*)inputData;
	messageData->Length = dataSize;
	messageData[1].Data = keyBlob;
	messageData[1].Length = 0x10;
	messageData[2].Data = outputData;
	messageData[2].Length = dataSize;
	messageData[3].Data = ivData;
	messageData[3].Length = 0x10;

	ret = (s32)MessageQueueId == -1
		? DispatchIoctlv(AES_STATIC_FILEDESC, 3, 2, 2, messageData)
		: DispatchIoctlvAsync(AES_STATIC_FILEDESC, 3, 2, 2, messageData, MessageQueueId, (IpcMessage*)message);

_aes_decrypt_cleanup_return:
	if(keyBlob)
		FreeOnHeap(KernelHeapId, keyBlob);
	
	if(messageData)
		FreeOnHeap(KernelHeapId, messageData);

	return ret;
}
static s32 _IOSC_Encrypt(const u32 keyHandle, void* ivData, const void* inputData, const u32 dataSize, void* outputData, const u32 MessageQueueId, IpcMessage* message )
{
	if(((u32)inputData & 0x1F) != 0 || ((u32)outputData & 0x1F) != 0)
		return -2016;

	void* keyBlob = AllocateOnHeap(KernelHeapId, 0x10);
	if(keyBlob == NULL)
		return -22;

	s32 ret = 0;
	IoctlvMessageData* messageData = (IoctlvMessageData*)AllocateOnHeap(KernelHeapId, 0x20);
	if(messageData == NULL)
	{
		ret = -22;
		goto _aes_encrypt_cleanup_return;
	}

	u32 keyRingSize = 0;
	ret = Keyring_FindKeySize(&keyRingSize, keyHandle);
	if(ret != 0)
		goto _aes_encrypt_cleanup_return;

	ret = Keyring_GetKey(keyHandle, keyBlob, keyRingSize);
	if(ret != 0)
	{
		ret = -21;
		goto _aes_encrypt_cleanup_return;
	}

	messageData->Data = (void*)inputData;
	messageData->Length = dataSize;
	messageData[1].Data = keyBlob;
	messageData[1].Length = 0x10;
	messageData[2].Data = outputData;
	messageData[2].Length = dataSize;
	messageData[3].Data = ivData;
	messageData[3].Length = 0x10;

	ret = (s32)MessageQueueId == -1
		? DispatchIoctlv(AES_STATIC_FILEDESC, 3, 2, 2, messageData)
		: DispatchIoctlvAsync(AES_STATIC_FILEDESC, 3, 2, 2, messageData, MessageQueueId, (IpcMessage*)message);

_aes_encrypt_cleanup_return:
	if(keyBlob)
		FreeOnHeap(KernelHeapId, keyBlob);
	
	if(messageData)
		FreeOnHeap(KernelHeapId, messageData);

	return ret;
}
static s32 _IOSC_GenerateBlockMAC(const ShaContext* context, 
	const void *inputData, const u32 inputSize, const void *customData, const u32 customDataSize, const u32 keyHandle, const u32 hmacCommand,
	const void *signData, const s32 messageQueueId, IpcMessage* message)
{
	if(((u32)inputData & 0x3F) != 0)
		return -2016;

	//hmac command should be the 0 based command, not the merged command
	if(hmacCommand >= FinalizeShaState)
		return -4;

	s32 ret = 0;
	IoctlvMessageData* messageData = (IoctlvMessageData*)AllocateOnHeap(KernelHeapId, 0x28);
	if(messageData == NULL)
	{
		ret = -22;
		goto _hmac_generate_cleanup_return;
	}

	messageData->Length = inputSize;
	messageData->Data = (void*) inputData;
	messageData[1].Data = (void*) context;
	messageData[1].Length = sizeof(ShaContext);
	messageData[2].Data = (void*) signData;
	messageData[2].Length = signData == NULL ? 0 : 0x14;
	messageData[3].Data = (void*)&keyHandle;
	messageData[3].Length = sizeof(u32);
	messageData[4].Data = (void*) customData;
	messageData[4].Length = customDataSize;

	ret = messageQueueId == -1
		? DispatchIoctlv(SHA_STATIC_FILEDESC, (u32) FinalizeShaState + hmacCommand, 3, 2, messageData)
		: DispatchIoctlvAsync(SHA_STATIC_FILEDESC, FinalizeShaState + hmacCommand, 3, 2, messageData, (u32)messageQueueId, message);

_hmac_generate_cleanup_return:
	if(messageData)
		FreeOnHeap(KernelHeapId, messageData);

	return ret;	
}


static s32 IOSC_SetNewKeyKind(u32* keyHandle, KeyType type, KeySubtype subtype)
{
	u32 size = 0;
	s32 ret = Keyring_GetKeySizeFromType(type, subtype, &size);
	if(ret != IPC_SUCCESS)
		return IOSC_FAIL_ALLOC;

	ret = Keyring_GetHandleFitSize(keyHandle, size);
	if(ret != IPC_SUCCESS)
		return IOSC_FAIL_ALLOC;

	ret = Keyring_SetKeyKind(*keyHandle, (KeyKind){.Type = type, .Subtype = subtype});
	if(ret != IPC_SUCCESS)
		return IOSC_FAIL_ALLOC;
	
	return IPC_SUCCESS;
}
static s32 IOSC_DeleteKeyringEntry(u32 keyHandle)
{
	if(keyHandle >= KEYRING_METADATA_TOTAL_ENTRIES)
		return IOSC_EINVAL;

	if(!KeyringMetadata[keyHandle].IsUsed)
		return IOSC_EINVAL;

	KeyringMetadata[keyHandle].IsUsed = 0;
	s16 keyringIndex = KeyringMetadata[keyHandle].KeyringIndex;
	if (keyringIndex == -1)
		return IPC_SUCCESS;

	if(keyringIndex >= KEYRING_TOTAL_ENTRIES)
		return IOSC_EINVAL;

	while(keyringIndex < KEYRING_TOTAL_ENTRIES)
	{
		if (!KeyringEntries[keyringIndex].IsUsed)
			return IOSC_EINVAL;

		KeyringEntries[keyringIndex].IsUsed = 0;
		const s16 nextKeyringIndex = (s16)KeyringEntries[keyringIndex].KeyNextPartIndex;
		Keyring_ClearEntryData((u16)keyringIndex);

		keyringIndex = nextKeyringIndex;
		if(keyringIndex == 0)
			break;
	}

	return IPC_SUCCESS;
}
static s32 IOSC_DeleteCustomKey(u32 keyHandle)
{
	if(keyHandle < KEYRING_CUSTOM_START_INDEX)
		return IOSC_EACCES;

	if(keyHandle == RSA4096_ROOTKEY)
		return IOSC_EACCES;
	
	if(IOSC_DeleteKeyringEntry(keyHandle) != IPC_SUCCESS)
		return IOSC_FAIL_ALLOC;

	return IPC_SUCCESS;
}
void IOSC_InitInformation(void)
{
	s32 messageQueueId = CreateMessageQueue(&IOSC_Information.message, 1);
	if(messageQueueId >= 0)
	{
		s32 result = SendMessage((u32)messageQueueId, NULL, RegisteredEventHandler);
		if(result != 0)
		{
			DestroyMessageQueue((u32)messageQueueId);
			messageQueueId = result;
		}
	}
	IOSC_Information.messageQueueId = (u32)messageQueueId;
	IOSC_SEEPROM_UpdatePRNGSeed();
	IOSC_Information.rngSeed = (u32)IOSC_SEEPROM_GetPRNGSeed();
	IOSC_Information.val3 = 0;
	IOSC_Information.val0 = 0;
}
s32 IOSC_Init(void)
{
	// bitset of ProcessIDs allowed to access this key
	const u32 HandlesAndOwners[][2] = {
		{KEYRING_CONST_NG_PRIVATE_KEY, 3},
		{KEYRING_CONST_NG_ID, RSA4096_ROOTKEY},
		{KEYRING_CONST_NAND_KEY, 5},
		{KEYRING_CONST_NAND_HMAC, 5},
		{KEYRING_CONST_OTP_COMMON_KEY, 3},
		{KEYRING_CONST_BOOT2_VERSION, 3},
		{KEYRING_CONST_BOOT2_UNK1, 3},
		{KEYRING_CONST_BOOT2_UNK2, 3},
		{KEYRING_CONST_NAND_GEN, 5},
		{KEYRING_CONST_OTP_RNG_SEED, 3},
		{KEYRING_CONST_SD_PRIVATE_KEY, 3},
		{KEYRING_CONST_EEPROM_COMMON_KEY, 3},
	};

	const u32 HandlesAndZeroes[][2] = {
		{KEYRING_CONST_NG_PRIVATE_KEY, 0},
		{KEYRING_CONST_NAND_KEY, 0},
		{KEYRING_CONST_NAND_HMAC, 0},
		{KEYRING_CONST_OTP_COMMON_KEY, 0},
		{KEYRING_CONST_EEPROM_COMMON_KEY, 0},
		{KEYRING_CONST_OTP_RNG_SEED, 0},
		{KEYRING_CONST_SD_PRIVATE_KEY, 0},
	};

	Keyring_Init();

	s32 ret = IPC_SUCCESS;
	for(u32 i = 0; ret == IPC_SUCCESS && i < ARRAY_LENGTH(HandlesAndZeroes); ++i)
	{
		const u32 handle = HandlesAndZeroes[i][0];
		const u32 zeroes = HandlesAndZeroes[i][1];
		ret = Keyring_SetKeyZeroesIfAnyPrivate(handle, zeroes);
	}

	ret = IPC_SUCCESS;
	for(u32 i = 0; ret == IPC_SUCCESS && i < ARRAY_LENGTH(HandlesAndOwners); ++i)
	{
		const u32 handle = HandlesAndOwners[i][0];
		const u32 owner = HandlesAndOwners[i][1];
		ret = Keyring_SetKeyOwnerProcess(handle, owner);
	}

	return ret;
}

s32 IOSC_BOOT2_GetVersion(void)
{
	const u32 cookie = DisableInterrupts();
	s32 ret = (s32)IOSC_BOOT2_DummyVersion;
	if(OTP_IsSet())
	{
		ret = BOOT2_GetVersion();
	}
	RestoreInterrupts(cookie);
	return ret;
}
s32 IOSC_BOOT2_GetUnk1(void)
{
	const u32 cookie = DisableInterrupts();
	s32 ret = (s32)IOSC_BOOT2_DummyUnk1;
	if(OTP_IsSet())
	{
		ret = BOOT2_GetUnk1();
	}
	RestoreInterrupts(cookie);
	return ret;
}
s32 IOSC_BOOT2_GetUnk2(void)
{
	const u32 cookie = DisableInterrupts();
	s32 ret = (s32)IOSC_BOOT2_DummyUnk1; // not Unk2. yes, this is a bug in IOS 58
	if(OTP_IsSet())
	{
		ret = BOOT2_GetUnk2();
	}
	RestoreInterrupts(cookie);
	return ret;
}
s32 IOSC_NAND_GetGen(void)
{
	const u32 cookie = DisableInterrupts();
	s32 ret = (s32)IOSC_NAND_DummyGen;
	if(OTP_IsSet())
	{
		ret = NAND_GetGen();
	}
	RestoreInterrupts(cookie);
	return ret;
}
s32 IOSC_SEEPROM_GetPRNGSeed(void)
{
	const u32 cookie = DisableInterrupts();
	s32 ret = (s32)IOSC_SEEPROM_DummyPRNGSeed;
	if(OTP_IsSet())
	{
		ret = SEEPROM_GetPRNGSeed();
	}
	RestoreInterrupts(cookie);
	return ret;
}

s32 IOSC_BOOT2_UpdateVersion(void)
{
	const u32 cookie = DisableInterrupts();
	s32 ret = 0;
	if(OTP_IsSet())
	{
		if(BOOT2_GetVersion() + 1 < 0x100)
		{
			ret = BOOT2_UpdateVersion();
		}
		else
		{
			ret = -1;
		}
	}
	else
	{
		IOSC_BOOT2_DummyVersion += 1;
	}
	RestoreInterrupts(cookie);
	return ret;
}
s32 IOSC_BOOT2_UpdateUnk1(void)
{
	const u32 cookie = DisableInterrupts();
	s32 ret = 0;
	if(OTP_IsSet())
	{
		if(BOOT2_GetUnk1() + 1 < 0x100)
		{
			ret = BOOT2_UpdateUnk1();
		}
		else
		{
			ret = -1;
		}
	}
	else
	{
		IOSC_BOOT2_DummyUnk1 += 1;
	}
	RestoreInterrupts(cookie);
	return ret;
}
s32 IOSC_BOOT2_UpdateUnk2(void)
{
	const u32 cookie = DisableInterrupts();
	s32 ret = 0;
	if(OTP_IsSet())
	{
		if(BOOT2_GetUnk2() + 1 < 0x100)
		{
			ret = BOOT2_UpdateUnk2();
		}
		else
		{
			ret = -1;
		}
	}
	else
	{
		IOSC_BOOT2_DummyUnk2 += 1;
	}
	RestoreInterrupts(cookie);
	return ret;
}
s32 IOSC_NAND_UpdateGen(void)
{
	const u32 cookie = DisableInterrupts();
	s32 ret = 0;
	if(OTP_IsSet())
	{
		if(NAND_GetGen() != -2)
		{
			ret = NAND_UpdateGen();
		}
		else
		{
			ret = -1;
		}
	}
	else
	{
		IOSC_NAND_DummyGen += 1;
	}
	RestoreInterrupts(cookie);
	return ret;
}
s32 IOSC_SEEPROM_UpdatePRNGSeed(void)
{
	const u32 cookie = DisableInterrupts();
	s32 ret = 0;
	if(OTP_IsSet())
	{
		ret = SEEPROM_UpdatePRNGSeed();
	}
	else
	{
		IOSC_SEEPROM_DummyPRNGSeed += 1;
	}
	RestoreInterrupts(cookie);
	return ret;
}

#define IOSC_BEGIN_SAFETY_WRAPPER(mainRetVarName, secondaryRetVarName) \
	secondaryRetVarName = IOSC_DiscardMessageFromQueue(IOSC_Information.messageQueueId); \
	mainRetVarName = IOSC_EACCES; \
	if(secondaryRetVarName == IPC_SUCCESS) { \
		IOSC_SwapStack(CurrentThread->DefaultThreadStack, IOSC_SafeStackEnd);


#define IOSC_END_SAFETY_WRAPPER(mainRetVarName, secondaryRetVarName) \
	} \
	IOSC_SwapStack(IOSC_SafeStackEnd, CurrentThread->DefaultThreadStack); \
	secondaryRetVarName = IOSC_SendEmptyMessageToQueue(IOSC_Information.messageQueueId); \
	if (secondaryRetVarName != IPC_SUCCESS && mainRetVarName == IPC_SUCCESS) { mainRetVarName = IOSC_EACCES; }

s32 IOSC_CreateObject(u32* keyHandle, KeyType type, KeySubtype subtype)
{
	s32 ret = IPC_SUCCESS, keyRet = IPC_SUCCESS;
	IOSC_BEGIN_SAFETY_WRAPPER(ret, keyRet)

	do {
		ret = IOSC_CheckCurrentProcessCanReadWrite(keyHandle, sizeof(u32));
		if(ret != IPC_SUCCESS)
			break;

		ret = IOSC_SetNewKeyKind(keyHandle, type, subtype);
		if(ret != IPC_SUCCESS)
			break;

		ret = Keyring_SetKeyOwnerProcess(*keyHandle, 1 << (CurrentThread->ProcessId & 0xff));
	} while(0);

	IOSC_END_SAFETY_WRAPPER(ret, keyRet)
	return ret;
}
s32 IOSC_DeleteObject(u32 keyHandle)
{
	s32 ret = IPC_SUCCESS, keyRet = IPC_SUCCESS;
	IOSC_BEGIN_SAFETY_WRAPPER(ret, keyRet)

	keyRet = IOSC_CheckCurrentProcessOwnsKey(keyHandle);
	if (keyRet == IPC_SUCCESS)
		ret = IOSC_DeleteCustomKey(keyHandle);

	IOSC_END_SAFETY_WRAPPER(ret, keyRet)
	return ret;
}

s32 IOSC_GetData(u32 keyHandle, u32* value)
{
	s32 ret = IPC_SUCCESS, keyRet = IPC_SUCCESS;
	IOSC_BEGIN_SAFETY_WRAPPER(ret, keyRet)

	do {
		ret = IOSC_CheckCurrentProcessCanReadWrite(value, sizeof(u32));
		if (ret != IPC_SUCCESS)
			break;

		if(keyHandle >= KEYRING_CUSTOM_START_INDEX && keyHandle != RSA4096_ROOTKEY)
		{
			ret = IOSC_EACCES;
			keyRet = IOSC_CheckCurrentProcessOwnsKey(keyHandle);
			if (keyRet != IPC_SUCCESS)
				break;
		}

		ret = Keyring_GetKeyMetadataIfOthers(keyHandle, value);
	} while(0);

	IOSC_END_SAFETY_WRAPPER(ret, keyRet)
	return ret;
}
s32 IOSC_GetKeySize(u32* keySize, u32 keyHandle)
{
	s32 ret = IPC_SUCCESS, keyRet = IPC_SUCCESS;
	IOSC_BEGIN_SAFETY_WRAPPER(ret, keyRet)

	do {
		keyRet = IOSC_CheckCurrentProcessOwnsKey(keyHandle);
		if (keyRet != IPC_SUCCESS)
			break;

		ret = IOSC_CheckCurrentProcessCanReadWrite(keySize, sizeof(u32));
		if (ret != IPC_SUCCESS)
			break;

		ret = Keyring_FindKeySize(keySize, keyHandle);
	} while(0);

	IOSC_END_SAFETY_WRAPPER(ret, keyRet)
	return ret;
}
s32 IOSC_GetSignatureSize(u32* signatureSize, u32 keyHandle)
{
	s32 ret = IPC_SUCCESS, keyRet = IPC_SUCCESS;
	IOSC_BEGIN_SAFETY_WRAPPER(ret, keyRet)

	do {
		keyRet = IOSC_CheckCurrentProcessOwnsKey(keyHandle);
		if (keyRet != IPC_SUCCESS)
			break;

		ret = IOSC_CheckCurrentProcessCanReadWrite(signatureSize, sizeof(u32));
		if (ret != IPC_SUCCESS)
			break;

		ret = Keyring_GetSignatureSize(signatureSize, keyHandle);
	} while(0);

	IOSC_END_SAFETY_WRAPPER(ret, keyRet)
	return ret;
}

static inline s32 IOSC_EncryptInner(const u32 keyHandle, void* ivData, const void* inputData, const u32 dataSize, void* outputData, const u32 messageQueueId, IpcMessage* message)
{
	s32 ret = IPC_SUCCESS, keyRet = IPC_SUCCESS;
	IOSC_BEGIN_SAFETY_WRAPPER(ret, keyRet);

	do {
		keyRet = IOSC_CheckCurrentProcessOwnsKey(keyHandle);
		if (keyRet != IPC_SUCCESS)
			break;

		ret = IOSC_CheckCurrentProcessCanReadWrite(outputData, dataSize);
		if (ret != IPC_SUCCESS)
			break;

		ret = IOSC_CheckCurrentProcessCanRead(inputData, dataSize);
		if (ret != IPC_SUCCESS)
			break;
		
		ret = IOSC_CheckCurrentProcessCanReadWrite(ivData, 0x10);
		if (ret != IPC_SUCCESS)
			break;

		ret = _IOSC_Encrypt(keyHandle, ivData, inputData, dataSize, outputData, messageQueueId, message);
	} while(0);
	
	IOSC_END_SAFETY_WRAPPER(ret, keyRet)
	return ret;
}
static inline s32 IOSC_DecryptInner(const u32 keyHandle, void* ivData, const void* inputData, const u32 dataSize, void* outputData, const u32 messageQueueId, IpcMessage* message)
{
	s32 ret = IPC_SUCCESS, keyRet = IPC_SUCCESS;
	IOSC_BEGIN_SAFETY_WRAPPER(ret, keyRet);

	do {
		keyRet = IOSC_CheckCurrentProcessOwnsKey(keyHandle);
		if (keyRet != IPC_SUCCESS)
			break;

		ret = IOSC_CheckCurrentProcessCanReadWrite(outputData, dataSize);
		if (ret != IPC_SUCCESS)
			break;

		ret = IOSC_CheckCurrentProcessCanRead(inputData, dataSize);
		if (ret != IPC_SUCCESS)
			break;
		
		ret = IOSC_CheckCurrentProcessCanReadWrite(ivData, 0x10);
		if (ret != IPC_SUCCESS)
			break;

		ret = _IOSC_Decrypt(keyHandle, ivData, inputData, dataSize, outputData, messageQueueId, message);
	} while(0);
	
	IOSC_END_SAFETY_WRAPPER(ret, keyRet)
	return ret;
}
s32 IOSC_Encrypt(const u32 keyHandle, void* ivData, const void* inputData, const u32 dataSize, void* outputData)
{
	return IOSC_EncryptInner(keyHandle, ivData, inputData, dataSize, outputData, (u32)-1, NULL);
}
s32 IOSC_EncryptAsync(const u32 keyHandle, void* ivData, const void* inputData, const u32 dataSize, void* outputData, const u32 messageQueueId, IpcMessage* message)
{
	return IOSC_EncryptInner(keyHandle, ivData, inputData, dataSize, outputData, messageQueueId, message);
}
s32 IOSC_Decrypt(const u32 keyHandle, void* ivData, const void* inputData, const u32 dataSize, void* outputData)
{
	return IOSC_DecryptInner(keyHandle, ivData, inputData, dataSize, outputData, (u32)-1, NULL);
}
s32 IOSC_DecryptAsync(const u32 keyHandle, void* ivData, const void* inputData, const u32 dataSize, void* outputData, const u32 messageQueueId, IpcMessage* message)
{
	return IOSC_DecryptInner(keyHandle, ivData, inputData, dataSize, outputData, messageQueueId, message);
}

static inline s32 IOSC_GenerateBlockMACInner(const ShaContext* context, 
	const void *inputData, const u32 inputSize, const void *customData, const u32 customDataSize, const u32 keyHandle, const u32 hmacCommand,
	const void *signData, const s32 messageQueueId, IpcMessage* message)
{
	s32 ret = IPC_SUCCESS, keyRet = IPC_SUCCESS;
	IOSC_BEGIN_SAFETY_WRAPPER(ret, keyRet);

	do {
		keyRet = IOSC_CheckCurrentProcessOwnsKey(keyHandle);
		if (keyRet != IPC_SUCCESS)
			break;
		
		ret = IOSC_CheckCurrentProcessCanReadWrite(context, 0x60);
		if(ret != IPC_SUCCESS)
			break;

		ret = IOSC_CheckCurrentProcessCanRead(inputData, inputSize);
		if(ret != IPC_SUCCESS)
			break;

		ret = IOSC_CheckCurrentProcessCanReadWrite(signData, 0x14);
		if(ret != IPC_SUCCESS)
			break;
		
		ret = IOSC_CheckCurrentProcessCanRead(customData, 4);
		if(ret != IPC_SUCCESS)
			break;

		ret = _IOSC_GenerateBlockMAC(context, inputData, inputSize, customData, customDataSize, keyHandle,
									 hmacCommand, signData, messageQueueId, message);
	} while(0);

	IOSC_END_SAFETY_WRAPPER(ret, keyRet)
	return ret;
}
s32 IOSC_GenerateBlockMACAsync(const ShaContext* context, 
	const void *inputData, const u32 inputSize, const void *customData, const u32 customDataSize, const u32 keyHandle, const u32 hmacCommand,
	const void *signData, const s32 messageQueueId, IpcMessage* message)
{
	return IOSC_GenerateBlockMACInner(context, inputData, inputSize, customData, customDataSize, keyHandle, hmacCommand, signData, messageQueueId, message);
}
s32 IOSC_GenerateBlockMAC(const ShaContext* context, 
	const void *inputData, const u32 inputSize, const void *customData, const u32 customDataSize, const u32 keyHandle, const u32 hmacCommand, const void *signData)
{
	return IOSC_GenerateBlockMACInner(context, inputData, inputSize, customData, customDataSize, keyHandle, hmacCommand, signData, -1, NULL);
}

#endif