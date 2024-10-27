/*
	starstruck - a Free Software reimplementation for the Nintendo/BroadOn IOS.
	otp - the otp registers in the soc

	Copyright (C) 2021	DacoTaco

# This code is licensed to you under the terms of the GNU GPL, version 2;
# see file COPYING or http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt
*/

#ifndef __OTP_H__
#define __OTP_H__

#include <types.h>

#define OTP_COMMONKEY_SIZE 16
#define OTP_NGPRIVKEY_SIZE 30
#define OTP_NANDKEY_SIZE 16
#define OTP_NANDHMAC_SIZE 20
#define OTP_RNGSEED_SIZE 16

void OTP_FetchData(u32 addr, void *out, u32 size);
u32 OTP_IsSet(void);
void OTP_GetNgId(u32 *ngId);
void OTP_GetRngSeed(u8 seed[OTP_RNGSEED_SIZE]);
void OTP_GetKeys(u8 ng_privkey_out[OTP_NGPRIVKEY_SIZE], u8 common_key_out[OTP_COMMONKEY_SIZE], u8 nand_hmac_out[OTP_NANDHMAC_SIZE], u8 nand_key_out[OTP_NANDKEY_SIZE]);

#endif
