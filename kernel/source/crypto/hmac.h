/*
	starstruck - a Free Software reimplementation for the Nintendo/BroadOn IOS.
	hmac - ios hmac/key management

	Copyright (C) 2021	DacoTaco
	Copyright (C) 2023	Jako

# This code is licensed to you under the terms of the GNU GPL, version 2;
# see file COPYING or http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt
*/

#ifndef __HMAC_H__
#define __HMAC_H__

#include <types.h>
#include "crypto/sha.h"

extern u8 HmacKey[SHA_BLOCK_SIZE];

#endif