/*
	starstruck - a Free Software reimplementation for the Nintendo/BroadOn IOS.
	keyring - key management

	Copyright (C) 2021	DacoTaco
	Copyright (C) 2023	Jako

# This code is licensed to you under the terms of the GNU GPL, version 2;
# see file COPYING or http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt
*/

#ifndef __KEYRING_H__
#define __KEYRING_H__

#include <types.h>
#include "crypto/sha.h"

typedef enum
{
	PrivateKey = 0,
	PublicKey = 1,
	PublicAndPrivateKey = 2,
	Other = 3
} KeyType;

typedef enum
{
	AES_128 = 0,
	HMAC = 1,
	RSA_2048 = 2,
	RSA_4096 = 3,
	ECC_233 = 4,
	UNKNOWN1 = 5,
	UNKNOWN2 = 6
} KeySubtype;

extern u8 HmacKey[SHA_BLOCK_SIZE];

s32 FindKeySize(u32 *keySize, u32 keyHandle);
void FindKeyTypes(u32 keyHandle, KeyType *keytype, KeySubtype *keySubtype);
s32 FindKeyTypeUnmasked(u32 keyHandle, KeyType *keyType);
s32 GetKeySizeFromType(KeyType keyType, KeySubtype keySubtype, u32 *keySize);

#endif