/*
	starstruck - a Free Software reimplementation for the Nintendo/BroadOn IOS.
	keyring - key management

	Copyright (C) 2021	DacoTaco
	Copyright (C) 2023	Jako

# This code is licensed to you under the terms of the GNU GPL, version 2;
# see file COPYING or http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt
*/

#include <ios/errno.h>
#include "crypto/keyring.h"

#define RSA4096_ROOTKEY 0xFFFFFFF

s32 GetKeySizeFromType(KeyType keyType, KeySubtype keySubtype, u32 *keySize)
{	
	/* Public Key Lengths */
    if (keyType == PublicKey) {
        if (keySubtype == RSA_4096) {
            /* RSA 4096 (512 bytes/4096 bits) */
            *keySize = 0x200;
        }
        else if (keySubtype < ECC_233) {
            if (keySubtype != RSA_2048) {
                return IOSC_EINVAL;
            }
            /* RSA 2048 (256 bytes/2048 bits) */
            *keySize = 0x100;
        }
        else {
            if (keySubtype != ECC_233) {
                return IOSC_EINVAL;
            }
            /* ECC 233 (60 bytes/480 bits) */
            *keySize = 0x3c;
        }
    }
    else {
        /* Private Key Lengths */
        if (keyType < PublicAndPrivateKey) {
            if (keyType != PrivateKey) {
                return IOSC_EINVAL;
            }
            if (keySubtype == HMAC) {
                /* SHA-1 HMAC (20 bytes/160 bits) */
                *keySize = 0x14;
            }
            else if (keySubtype < RSA_2048) {
                if (keySubtype != AES_128) {
                    return IOSC_EINVAL;
                }
                /* AES 128 (16 bytes/128 bits) */
                *keySize = 0x10;
            }
            else {
                if (keySubtype != ECC_233) {
                    return IOSC_EINVAL;
                }
                /* ECC 233 (30 bytes/240 bits) */
                *keySize = 0x1e;
            }
        }
        else {
            /* Public + Private Key Combined Lengths */
            if (keyType == PublicAndPrivateKey) {
                if (keySubtype != ECC_233) {
                    return IOSC_EINVAL;
                }
                /* ECC 233 (90 bytes/720 bits) */
                *keySize = 0x5a;
            }
            else {
                /* Unknown zero length keys? */
                if (keyType != Other) {
                    return IOSC_EINVAL;
                }
                if (keySubtype == UNKNOWN1) {
                    *keySize = 0;
                }
                else {
                    if (keySubtype != UNKNOWN2) {
                        return IOSC_EINVAL;
                    }
                    *keySize = 0;
                }
            }
        }
    }
    return IPC_SUCCESS;
}

s32 FindKeyTypeUnmasked(u32 keyHandle, KeyType *keyType)
{
	/* seems like this buffer is 0x20 elements long, and each element is at least 0x14 bytes long
	my bet is on 0x10 bytes long each */
	u8 keyArray[0x20] = {0}; //placeholder, need to have someone RE this buffer

    if ((keyHandle < 0x20) && (keyArray[keyHandle * 0x14] != 0)) 
	{
        *keyType = keyArray[keyHandle * 0x14 + 1];
        return IPC_SUCCESS;
    }

    return IOSC_EINVAL;
}

void FindKeyTypes(u32 keyHandle, KeyType *keytype, KeySubtype *keySubtype)
{
    if (keyHandle == RSA4096_ROOTKEY) {
        *keytype = PublicKey;
        *keySubtype = RSA_4096;
		return;
    }
    
	KeyType retKeyType;
	u32 keyTypeUnmasked = FindKeyTypeUnmasked(keyHandle, &retKeyType);
	*keytype = keyTypeUnmasked >> 4;
	*keySubtype = keyTypeUnmasked & 0xf;
    return;
}

s32 FindKeySize(u32 *keySize, u32 keyHandle)
{
    u32 keySizeValidity;
    KeySubtype keySubtype;
    KeyType keyType;
    
    if (keyHandle == RSA4096_ROOTKEY) 
	{
        *keySize = 0x200;
        return IPC_SUCCESS;
    }

    FindKeyTypes(keyHandle, &keyType, &keySubtype);
	keySizeValidity = GetKeySizeFromType(keyType, keySubtype, keySize);
	if (keySizeValidity == IPC_SUCCESS)
		return IPC_SUCCESS;

    return IOSC_FAIL_INTERNAL;
}
