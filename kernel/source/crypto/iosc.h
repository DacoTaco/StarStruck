/*
	starstruck - a Free Software reimplementation for the Nintendo/BroadOn IOS.
	iosc - ios crypto syscalls

	Copyright (C) 2021	DacoTaco

# This code is licensed to you under the terms of the GNU GPL, version 2;
# see file COPYING or http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt
*/

#ifndef __IOSC_H__
#define __IOSC_H__

#include <types.h>
#include "crypto/keyring.h"

void IOSC_InitInformation(void);
s32 IOSC_Init(void);

s32 IOSC_BOOT2_GetVersion(void);
s32 IOSC_BOOT2_GetUnk1(void);
s32 IOSC_BOOT2_GetUnk2(void);
s32 IOSC_NAND_GetGen(void);

// Syscalls start here
s32 IOSC_CreateObject(u32* key_handle, KeyType type, KeySubtype subtype);
s32 IOSC_DeleteObject(u32 key_handle);
s32 IOSC_GetData(u32 keyHandle, u32* value);
s32 IOSC_GetKeySize(u32* keysize, u32 keyHandle);
s32 IOSC_GetSignatureSize(u32* signatureSize, u32 keyHandle);

#endif
