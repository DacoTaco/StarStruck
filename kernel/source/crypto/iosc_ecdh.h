/*
	starstruck - a Free Software reimplementation for the Nintendo/BroadOn IOS.
	iosc_ecdh - iosc's eliptic curve Diffie-Hellman implementation

	Copyright (C) 2021	DacoTaco

# This code is licensed to you under the terms of the GNU GPL, version 2;
# see file COPYING or http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt
*/

#pragma once

#include <types.h>

// sect233r1 private keys, public keys and shared secrets are all 30 bytes per coordinate
#define ECDH_PRIVATE_KEY_SIZE 0x1e
#define ECDH_PUBLIC_KEY_SIZE  0x3c

bool ECDH_ComputeSharedSecretHash(const u8* privateKey, const u8* publicKey, u8* out, u32 outSize);
