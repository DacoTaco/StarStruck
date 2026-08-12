/*
	starstruck - a Free Software reimplementation for the Nintendo/BroadOn IOS.
	iosc_hash - iosc's hash functions

	Copyright (C) 2021	DacoTaco

# This code is licensed to you under the terms of the GNU GPL, version 2;
# see file COPYING or http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt
*/
#pragma once

#include <types.h>
#include <ios/sha.h>
#include <ios/ipc.h>

s32 GenerateBlockMAC(const ShaContext* context, const void* inputData, const u32 inputSize, const void* customData,
                     const u32 customDataSize, const u32 keyHandle, const HMacCommandType hmacCommand,
                     const void* signData, const s32 messageQueueId, IpcMessage* message);

s32 GenerateHash(const ShaContext* context, const void* inputData, const u32 inputSize, const u32 shaCommand,
                 void* digest, const s32 messageQueueId, IpcMessage* message);