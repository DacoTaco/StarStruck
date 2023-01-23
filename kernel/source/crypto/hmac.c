/*
	starstruck - a Free Software reimplementation for the Nintendo/BroadOn IOS.
	hmac - ios hmac/key management

	Copyright (C) 2021	DacoTaco
	Copyright (C) 2023	Jako

# This code is licensed to you under the terms of the GNU GPL, version 2;
# see file COPYING or http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt
*/

#include "crypto/hmac.h"
#include "crypto/keyring.h"

u8 HmacKey[SHA_BLOCK_SIZE] ALIGNED(SHA_BLOCK_SIZE) = { 0x00 };