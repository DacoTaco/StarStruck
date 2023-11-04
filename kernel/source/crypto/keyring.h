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

#define RSA4096_ROOTKEY 0x0FFFFFFF

typedef struct {
	// most significant 4 bits (u8 value >> 4)
	u8 Type : 4;
	// least significant 4 bits (u8 value & 0xf)
	u8 Subtype : 4;
} KeyTypeAndSubtype;
CHECK_SIZE(KeyTypeAndSubtype, 0x01);

#define KEYRING_SINGLE_ENTRY_KEY_MAX_SIZE 0x20

typedef struct
{
	u8 IsUsed;
	u8 Key[KEYRING_SINGLE_ENTRY_KEY_MAX_SIZE];
	u8 Padding;
	u16 KeyNextPartIndex;
} KeyringEntry;
CHECK_SIZE(KeyringEntry, 0x24);
CHECK_OFFSET(KeyringEntry, 0x00, IsUsed);
CHECK_OFFSET(KeyringEntry, 0x01, Key);
CHECK_OFFSET(KeyringEntry, 0x22, KeyNextPartIndex);

#define KEYRING_TOTAL_ENTRIES 64

typedef struct
{
	u8 IsUsed;
	KeyTypeAndSubtype Kind;
	u8 Pad1[0x2];
	u32 KeyHandleOwner;
	u32 Zeroes;
	u32 Metadata;
	u16 KeyringIndex;
	u8 Pad3[0x2];
} KeyringMetadataType;
CHECK_SIZE(KeyringMetadataType, 0x14);
CHECK_OFFSET(KeyringMetadataType, 0x00, IsUsed);
CHECK_OFFSET(KeyringMetadataType, 0x01, Kind);
CHECK_OFFSET(KeyringMetadataType, 0x04, KeyHandleOwner);
CHECK_OFFSET(KeyringMetadataType, 0x08, Zeroes);
CHECK_OFFSET(KeyringMetadataType, 0x0C, Metadata);
CHECK_OFFSET(KeyringMetadataType, 0x10, KeyringIndex);

#define KEYRING_METADATA_TOTAL_ENTRIES 32

extern KeyringEntry KeyringEntries[KEYRING_TOTAL_ENTRIES];
extern KeyringMetadataType KeyringMetadata[KEYRING_METADATA_TOTAL_ENTRIES];

void Keyring_Init(void);

void Keyring_ClearEntryData(u32 keyHandle);
s32 Keyring_GetKeyIndexFitSize(const u32 keySize);

s32 Keyring_FindKeySize(u32 *keySize, u32 keyHandle);
s32 Keyring_GetKeySizeFromType(KeyType keyType, KeySubtype keySubtype, u32 *keySize);

void Keyring_FindKeyTypes(u32 keyHandle, KeyType *keytype, KeySubtype *keySubtype);
s32 Keyring_FindKeyTypeRaw(u32 keyHandle, KeyTypeAndSubtype *keyKind);

s32 Keyring_SetKeyMetadata(u32 keyHandle, const void *data);
s32 Keyring_GetKeyMetadata(u32 keyHandle, void *data);
s32 Keyring_SetKey(u32 keyHandle, const void *data, u32 keySize);
s32 Keyring_GetKey(u32 keyHandle, void *keyPtr, u32 keySize);

s32 Keyring_SetKeyOwnerIfUsed(u32 keyHandle, u32 owner);
s32 Keyring_SetKeyZeroesIfUsed(u32 keyHandle, u32 zeroes);
s32 Keyring_SetKeyZeroesIfAnyPrivate(u32 keyHandle, u32 zeroes);

#endif
