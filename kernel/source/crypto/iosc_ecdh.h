/*
	starstruck - a Free Software reimplementation for the Nintendo/BroadOn IOS.
	iosc_ecdh - iosc's eliptic curve Diffie-Hellman implementation

	Copyright (C) 2021	DacoTaco

# This code is licensed to you under the terms of the GNU GPL, version 2;
# see file COPYING or http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt
*/

#pragma once

#include <types.h>

#include "ellipticCurve/uint233.h"

// Every number of a key is UINT233_NUM_BYTES (30) big-endian bytes or 233 bits (aligned to 8bits, so 240)
typedef u8 ECCPrivateKey[UINT233_NUM_BYTES];
CHECK_SIZE(ECCPrivateKey, 0x1E);

// the curve point: x then y
typedef struct
{
	u8 X[UINT233_NUM_BYTES];
	u8 Y[UINT233_NUM_BYTES];
} ECCPublicKey;
CHECK_SIZE(ECCPublicKey, 0x3C);

// what the keyring holds for a key that is both: the private key, then the public key
typedef struct
{
	ECCPrivateKey Private;
	ECCPublicKey Public;
} ECCPublicAndPrivateKey;
CHECK_SIZE(ECCPublicAndPrivateKey, 0x5A);
CHECK_OFFSET(ECCPublicAndPrivateKey, 0x00, Private);
CHECK_OFFSET(ECCPublicAndPrivateKey, 0x1E, Public);

typedef struct
{
	u8 R[UINT233_NUM_BYTES];
	u8 S[UINT233_NUM_BYTES];
} ECCSignature;
CHECK_SIZE(ECCSignature, 0x3C);

bool ECDH_ComputeSharedSecretHash(const ECCPrivateKey privateKey, const ECCPublicKey* publicKey, u8* out, u32 outSize);
