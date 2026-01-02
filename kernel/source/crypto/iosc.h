/*
	starstruck - a Free Software reimplementation for the Nintendo/BroadOn IOS.
	iosc - ios crypto syscalls

	Copyright (C) 2021	DacoTaco

# This code is licensed to you under the terms of the GNU GPL, version 2;
# see file COPYING or http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt
*/

#pragma once

#include <types.h>
#include <ios/ipc.h>

#include "crypto/keyring.h"

void IOSC_InitInformation(void);
s32 IOSC_Init(void);

s32 IOSC_BOOT2_GetVersion(void);
s32 IOSC_BOOT2_GetUnk1(void);
s32 IOSC_BOOT2_GetUnk2(void);
s32 IOSC_NAND_GetGen(void);

#ifndef MIOS
// Syscalls start here
s32 IOSC_CreateObject(u32 *key_handle, KeyType type, KeySubtype subtype);
s32 IOSC_DeleteObject(u32 key_handle);
s32 IOSC_GetData(u32 keyHandle, u32 *value);
s32 IOSC_GetKeySize(u32 *keysize, u32 keyHandle);
s32 IOSC_GetSignatureSize(u32 *signatureSize, u32 keyHandle);
s32 IOSC_Encrypt(const u32 keyHandle, void *ivData, const void *inputData,
                 const u32 dataSize, void *outputData);
s32 IOSC_EncryptAsync(const u32 keyHandle, void *ivData, const void *inputData,
                      const u32 dataSize, void *outputData,
                      const s32 messageQueueId, IpcMessage *message);
s32 IOSC_Decrypt(const u32 keyHandle, void *ivData, const void *inputData,
                 const u32 dataSize, void *outputData);
s32 IOSC_DecryptAsync(const u32 keyHandle, void *ivData, const void *inputData,
                      const u32 dataSize, void *outputData,
                      const s32 messageQueueId, IpcMessage *message);
s32 IOSC_GenerateBlockMACAsync(const ShaContext *context, const void *inputData,
                               const u32 inputSize, const void *customData,
                               const u32 customDataSize, const u32 keyHandle,
                               const HMacCommandType hmacCommand, const void *signData,
                               const s32 messageQueueId, IpcMessage *message);
s32 IOSC_GenerateBlockMAC(const ShaContext *context, const void *inputData,
                          const u32 inputSize, const void *customData,
                          const u32 customDataSize, const u32 keyHandle,
                          const HMacCommandType hmacCommand, const void *signData);
#endif
