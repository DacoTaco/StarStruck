/*
	starstruck - a Free Software reimplementation for the Nintendo/BroadOn IOS.
	keyring - key management

	Copyright (C) 2021	DacoTaco
	Copyright (C) 2023	Jako

# This code is licensed to you under the terms of the GNU GPL, version 2;
# see file COPYING or http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt
*/

#pragma once

// Security flags for IOSC_ImportSecretKey / IOSC_ExportSecretKey.
// Bitmask: bit 0 = AES-128 encrypted (use decryptHandle), bit 1 = HMAC-signed (use verifyHandle).
typedef enum
{
	IOSC_SECRET_NO_PROTECTION = 0,    // Plain import, no verification or decryption
	IOSC_SECRET_ENCRYPTED = 1,        // Key data is AES-128 encrypted
	IOSC_SECRET_SIGNED = 2,           // Key data has HMAC-MAC signature
	IOSC_SECRET_SIGNED_ENCRYPTED = 3, // Key data is both signed and encrypted
} IOSCSecretKeySecurity;

enum
{
	KEYRING_CONST_NG_PRIVATE_KEY,
	KEYRING_CONST_NG_ID,
	KEYRING_CONST_NAND_KEY,
	KEYRING_CONST_NAND_HMAC,
	KEYRING_CONST_OTP_COMMON_KEY,
	KEYRING_CONST_OTP_RNG_SEED,
	KEYRING_CONST_SD_PRIVATE_KEY,

	KEYRING_CONST_BOOT2_VERSION,
	KEYRING_CONST_BOOT2_UNK1,
	KEYRING_CONST_BOOT2_UNK2,
	KEYRING_CONST_NAND_GEN,

	KEYRING_CONST_EEPROM_COMMON_KEY,

	KEYRING_CUSTOM_START_INDEX,
};