/*
	starstruck - a Free Software reimplementation for the Nintendo/BroadOn IOS.
	keyring - key management

	Copyright (C) 2021	DacoTaco
	Copyright (C) 2023	Jako

# This code is licensed to you under the terms of the GNU GPL, version 2;
# see file COPYING or http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt
*/

#include <ios/errno.h>
#include <string.h>
#include "crypto/keyring.h"
#include "crypto/otp.h"
#include "crypto/seeprom.h"
#include "crypto/iosc.h"

KeyringEntry KeyringEntries[KEYRING_TOTAL_ENTRIES];
KeyringMetadataType KeyringMetadata[KEYRING_METADATA_TOTAL_ENTRIES];

static inline void Keyring_Init_WithKey(s32 index, KeyType type, KeySubtype subType, const void* key, const u32 keySize)
{
	KeyringMetadata[index].IsUsed = 1;
	KeyringMetadata[index].Kind.Type = type;
	KeyringMetadata[index].Kind.Subtype = subType;
	KeyringMetadata[index].KeyringIndex = Keyring_GetKeyIndexFitSize(keySize);
	Keyring_SetKey(index, key, keySize);
}
static inline void Keyring_Init_WithMetadata(s32 index, KeyType type, KeySubtype subType, const u32 metadata)
{
	KeyringMetadata[index].IsUsed = 1;
	KeyringMetadata[index].Kind.Type = type;
	KeyringMetadata[index].Kind.Subtype = subType;
	KeyringMetadata[index].KeyringIndex = 0;
	Keyring_SetKeyMetadata(index, &metadata);
}

void Keyring_Init(void)
{
	u8 otpCommonKey[OTP_COMMONKEY_SIZE];
	u8 eepromCommonKey[OTP_COMMONKEY_SIZE];
	u8 ngPrivKey[OTP_NGPRIVKEY_SIZE];
	u8 nandHmac[OTP_NANDHMAC_SIZE];
	u8 nandKey[OTP_NANDKEY_SIZE];
	u8 rngSeed[OTP_RNGSEED_SIZE];
	u32 ngId;

	// mark all as unused, no links, invalid types
	for(s32 i = 0; i < KEYRING_METADATA_TOTAL_ENTRIES; ++i)
	{
		KeyringMetadata[i].IsUsed = 0;
		KeyringMetadata[i].Kind.Type = 0xf;
		KeyringMetadata[i].Kind.Subtype = 0xf;
	}
	for(s32 i = 0; i < KEYRING_TOTAL_ENTRIES; ++i)
	{
		KeyringEntries[i].IsUsed = 0;
		KeyringEntries[i].KeyNextPartIndex = 0;
	}

	OTP_GetNgId(&ngId);
	Keyring_Init_WithMetadata(1, Other, UNKNOWN1, ngId);

	OTP_GetRngSeed(rngSeed);
	SEEPROM_GetCommonKey(eepromCommonKey);
	OTP_GetKeys(ngPrivKey, otpCommonKey, nandHmac, nandKey);

	// stored in plaintext in IOS, kept in the source here with a xorpad (to not have the key directly)
	u8 privkeyAesSd[OTP_NANDKEY_SIZE] = {0x40,0xe5,0x93,0xfa,0xbf,0xe7,0xb8,0xec,0xe7,0x63,0x1d,0x08,0xcc,0x43,0x0f,0xaa};
	for(u32 i = 0; i < OTP_NANDKEY_SIZE; ++i)
		privkeyAesSd[i] ^= otpCommonKey[i];

	Keyring_Init_WithKey(0, PrivateKey, ECC_233, ngPrivKey, OTP_NGPRIVKEY_SIZE);
	Keyring_Init_WithKey(2, PrivateKey, AES_128, nandKey, OTP_RNGSEED_SIZE);
	Keyring_Init_WithKey(3, PrivateKey, HMAC, nandHmac, OTP_NANDHMAC_SIZE);
	Keyring_Init_WithKey(4, PrivateKey, AES_128, otpCommonKey, OTP_COMMONKEY_SIZE);

	Keyring_Init_WithMetadata(7, Other, UNKNOWN2, IOSC_BOOT2_GetVersion());
	Keyring_Init_WithMetadata(8, Other, UNKNOWN2, IOSC_BOOT2_GetUnk1());
	Keyring_Init_WithMetadata(9, Other, UNKNOWN2, IOSC_BOOT2_GetUnk2());
	Keyring_Init_WithMetadata(10, Other, UNKNOWN2, IOSC_NAND_GetGen());

	Keyring_Init_WithKey(5, PrivateKey, AES_128, rngSeed, OTP_RNGSEED_SIZE);
	Keyring_Init_WithKey(6, PrivateKey, AES_128, privkeyAesSd, OTP_NANDKEY_SIZE);
	Keyring_Init_WithKey(11, PrivateKey, AES_128, eepromCommonKey, OTP_COMMONKEY_SIZE);
}

void Keyring_ClearEntryData(u32 keyHandle)
{
	memset(&KeyringEntries[keyHandle], 0, sizeof(KeyringEntry));
}

s32 Keyring_GetKeyIndexFitSize(const u32 keySize)
{
	u32 runningKeySize = 0;
	u32 previousLink = -1;
	u32 ret = -1;

	for (u32 currentIndex = 0; currentIndex < KEYRING_TOTAL_ENTRIES && runningKeySize < keySize; ++currentIndex)
	{
		if (!KeyringEntries[currentIndex].IsUsed)
		{
			Keyring_ClearEntryData(currentIndex);
			KeyringEntries[currentIndex].IsUsed = 1;
			if (runningKeySize != 0) { // head found, previousLink valid, link it
				KeyringEntries[previousLink].KeyNextPartIndex = currentIndex;
			}
			else { // first/head part: make return value its index
				ret = currentIndex;
			}
			previousLink = currentIndex;
			runningKeySize += KEYRING_SINGLE_ENTRY_KEY_MAX_SIZE;
		}
	} 

	// could not fit the requested size, clear the temporarily linked entries
	if (runningKeySize < keySize)
	{
		u32 keyringIndex = ret;
		do {
			const u32 currentIndex = keyringIndex;
			KeyringEntries[currentIndex].IsUsed = 0;
			keyringIndex = KeyringEntries[currentIndex].KeyNextPartIndex;
			Keyring_ClearEntryData(currentIndex);
			if (keyringIndex == 0)
				return IOSC_FAIL_ALLOC;

		} while (keyringIndex < KEYRING_TOTAL_ENTRIES);

		ret = IOSC_FAIL_ALLOC;
	}

	return ret;
}

s32 Keyring_SetKeyMetadata(u32 keyHandle, const void *data)
{
	if (keyHandle >= KEYRING_METADATA_TOTAL_ENTRIES)
		return IOSC_EINVAL;
		
	if (!KeyringMetadata[keyHandle].IsUsed)
		return IOSC_EINVAL;

	memcpy(&KeyringMetadata[keyHandle].Metadata, data, sizeof(KeyringMetadata[keyHandle].Metadata));
	return IPC_SUCCESS;
}
s32 Keyring_GetKeyMetadata(u32 keyHandle, void *data)
{
	if (keyHandle >= KEYRING_METADATA_TOTAL_ENTRIES)
		return IOSC_EINVAL;
		
	if (!KeyringMetadata[keyHandle].IsUsed)
		return IOSC_EINVAL;

	memcpy(data, &KeyringMetadata[keyHandle].Metadata, sizeof(KeyringMetadata[keyHandle].Metadata));
	return IPC_SUCCESS;
}

s32 Keyring_GetKeySizeFromType(KeyType keyType, KeySubtype keySubtype, u32 *keySize)
{
	switch(keyType)
	{
		//Public Key Lengths
		case PublicKey:
			if(keySubtype == RSA_4096) //RSA 4096 (512 bytes/4096 bits)
				*keySize = 0x200;
			else if(keySubtype == RSA_2048) //RSA 2048 (256 bytes/2048 bits)
				*keySize = 0x100;
			else if(keySubtype == ECC_233) //ECC 233 (60 bytes/480 bits)
				*keySize = 0x3C;
			else
				return IOSC_EINVAL;
			break;
		//Private Key Lengths
		case PrivateKey:
			if(keySubtype == HMAC) //SHA-1 HMAC (20 bytes/160 bits) 
				*keySize = 0x14;
			else if(keySubtype == AES_128) //AES 128 (16 bytes/128 bits)
				*keySize = 0x10;
			else if(keySubtype == ECC_233) //ECC 233 (30 bytes/240 bits)
				*keySize = 0x1e;
			else
				return IOSC_EINVAL;
			
			break;
		//Public + Private Key Combined Lengths
		case PublicAndPrivateKey:
			if(keySubtype != ECC_233)
				return IOSC_EINVAL;
			
			//ECC 233 (90 bytes/720 bits)
			*keySize = 0x5a;
			break;
		case Other:
			if(keySubtype == UNKNOWN1 || keySubtype == UNKNOWN2)
				*keySize = 0;
			else
				return IOSC_EINVAL;
			
			break;
		default:
			return IOSC_EINVAL;
	}

	return IPC_SUCCESS;
}

s32 Keyring_FindKeyTypeRaw(u32 keyHandle, KeyTypeAndSubtype *keyKind)
{
	if (keyHandle >= KEYRING_METADATA_TOTAL_ENTRIES)
		return IOSC_EINVAL;
		
	if (!KeyringMetadata[keyHandle].IsUsed)
		return IOSC_EINVAL;

	*keyKind = KeyringMetadata[keyHandle].Kind;
	return IPC_SUCCESS;
}

void Keyring_FindKeyTypes(u32 keyHandle, KeyType *keytype, KeySubtype *keySubtype)
{
	if (keyHandle == RSA4096_ROOTKEY) {
		*keytype = PublicKey;
		*keySubtype = RSA_4096;
		return;
	}
	
	KeyTypeAndSubtype typeAndSubtype;
	Keyring_FindKeyTypeRaw(keyHandle, &typeAndSubtype);
	*keytype = typeAndSubtype.Type;
	*keySubtype = typeAndSubtype.Subtype;
}

s32 Keyring_FindKeySize(u32 *keySize, u32 keyHandle)
{
	KeySubtype keySubtype;
	KeyType keyType;
	
	if (keyHandle == RSA4096_ROOTKEY) 
	{
		*keySize = 0x200;
		return IPC_SUCCESS;
	}

	Keyring_FindKeyTypes(keyHandle, &keyType, &keySubtype);
	if (Keyring_GetKeySizeFromType(keyType, keySubtype, keySize) == IPC_SUCCESS)
		return IPC_SUCCESS;

	return IOSC_FAIL_INTERNAL;
}

s32 Keyring_SetKey(u32 keyHandle, const void *data, u32 keySize)
{
	if (keyHandle >= KEYRING_METADATA_TOTAL_ENTRIES)
		return IOSC_EINVAL;

	if(!KeyringMetadata[keyHandle].IsUsed)
		return IOSC_EINVAL;

	u32 entryIndex = KeyringMetadata[keyHandle].KeyringIndex;
	u32 bytesCopied = 0;
	do {
		if(entryIndex >= KEYRING_TOTAL_ENTRIES || !KeyringEntries[entryIndex].IsUsed)
			return IOSC_EINVAL;

		u32 bytesToCopy = KEYRING_SINGLE_ENTRY_KEY_MAX_SIZE;
		if (keySize - bytesCopied <= KEYRING_SINGLE_ENTRY_KEY_MAX_SIZE)
			bytesToCopy = keySize - bytesCopied;

		memcpy(KeyringEntries[entryIndex].Key, (const char*)data + bytesCopied, bytesToCopy);
		bytesCopied += bytesToCopy;

		entryIndex = KeyringEntries[entryIndex].KeyNextPartIndex;
	} while (entryIndex != 0);

	return IPC_SUCCESS;
}
s32 Keyring_GetKey(u32 keyHandle, void *keyPtr, u32 keySize)
{
	if (keyHandle >= KEYRING_METADATA_TOTAL_ENTRIES)
		return IOSC_EINVAL;

	if(!KeyringMetadata[keyHandle].IsUsed)
		return IOSC_EINVAL;

	u32 entryIndex = KeyringMetadata[keyHandle].KeyringIndex;
	u32 bytesCopied = 0;
	do {
		if(entryIndex >= KEYRING_TOTAL_ENTRIES || !KeyringEntries[entryIndex].IsUsed)
			return IOSC_EINVAL;

		u32 bytesToCopy = KEYRING_SINGLE_ENTRY_KEY_MAX_SIZE;
		if (keySize - bytesCopied <= KEYRING_SINGLE_ENTRY_KEY_MAX_SIZE)
			bytesToCopy = keySize - bytesCopied;

		memcpy((char*)keyPtr + bytesCopied, KeyringEntries[entryIndex].Key, bytesToCopy);
		bytesCopied += bytesToCopy;

		entryIndex = KeyringEntries[entryIndex].KeyNextPartIndex;
	} while (entryIndex != 0);

	return IPC_SUCCESS;
}

s32 Keyring_SetKeyOwnerIfUsed(u32 keyHandle, u32 owner)
{
	if (keyHandle >= KEYRING_METADATA_TOTAL_ENTRIES)
		return IOSC_EINVAL;
	
	if(!KeyringMetadata[keyHandle].IsUsed)
		return IOSC_EINVAL;

	KeyringMetadata[keyHandle].KeyHandleOwner = owner;
	return IPC_SUCCESS;
}
s32 Keyring_SetKeyZeroesIfUsed(u32 keyHandle, u32 zeroes)
{
	if (keyHandle >= KEYRING_METADATA_TOTAL_ENTRIES)
		return IOSC_EINVAL;
	
	if(!KeyringMetadata[keyHandle].IsUsed)
		return IOSC_EINVAL;

	KeyringMetadata[keyHandle].Zeroes = zeroes;
	return IPC_SUCCESS;
}
s32 Keyring_SetKeyZeroesIfAnyPrivate(u32 keyHandle, u32 zeroes)
{
	KeySubtype keySubtype = AES_128;
	KeyType keyType = PrivateKey;
	s32 ret = IOSC_INVALID_OBJTYPE;

	Keyring_FindKeyTypes(keyHandle, &keyType, &keySubtype);
	if(keyType == PrivateKey || keyType == PublicAndPrivateKey)
		ret = Keyring_SetKeyZeroesIfUsed(keyHandle, zeroes);

	return ret;
}
