/*
	starstruck - a Free Software reimplementation for the Nintendo/BroadOn IOS.
	otp - the otp registers in the soc

	Copyright (C) 2021	DacoTaco
	Copyright (C) 2023	Jako

# This code is licensed to you under the terms of the GNU GPL, version 2;
# see file COPYING or http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt
*/

#include "crypto/otp.h"
#include <ios/errno.h>
#include <ios/processor.h>
#include "core/hollywood.h"
#include "utils.h"
#include <string.h>
#include "interrupt/irq.h"

static const u32 OTP_Dummy_NgId = 0x00004A39;
static const u8 OTP_Dummy_CommonKey[OTP_COMMONKEY_SIZE] = {0xeb, 0xe4, 0x2a, 0x22, 0x5e, 0x85, 0x93, 0xe4, 0x48, 0xd9, 0xc5, 0x45, 0x73, 0x81, 0xaa, 0xf7};
static const u8 OTP_Dummy_NgPrivKey[OTP_NGPRIVKEY_SIZE] = {0x00, 0x00, 0x4a, 0x39, 0x00, 0x4d, 0x4c, 0x1e, 0xa0, 0xb1, 0x76, 0xe3, 0xdd, 0xfb, 0xf8, 0x2e, 0xa3, 0x86, 0x14, 0x66, 0xf1, 0xd4, 0xc1, 0xe6, 0x4e, 0x39, 0x83, 0xc8, 0x57, 0x07};
static const u8 OTP_Dummy_NandKey[OTP_NANDKEY_SIZE] = {0x2b, 0xda, 0x4e, 0x8f, 0x68, 0x6a, 0x69, 0x88, 0x9e, 0xbf, 0x1e, 0x74, 0xe9, 0x17, 0x8e, 0xd7};
static const u8 OTP_Dummy_NandHmac[OTP_NANDHMAC_SIZE] = {0x32, 0x27, 0x0e, 0x2c, 0xcb, 0x0a, 0x9f, 0x55, 0xc1, 0x81, 0x6a, 0xf7, 0xa2, 0x70, 0xd6, 0x1d, 0xd3, 0xbd, 0xa1, 0xe8};
static const u8 OTP_Dummy_RngSeed[OTP_RNGSEED_SIZE] = {0x41, 0xc9, 0x6c, 0x3f, 0xc4, 0x98, 0x73, 0x85, 0xe8, 0x65, 0xac, 0xa4, 0x90, 0xa4, 0xfa, 0x2c, 0x89, 0xa5, 0x53, 0x66};

void OTP_FetchData(u32 addr, void *out, u32 size)
{
	u8* outPtr = out;
	memset(outPtr, 0, size);
	while(size != 0)
	{
		write32(HW_OTPCMD, 0x80000000 | addr);
		const u32 data = read32(HW_OTPDATA);
		if(size < sizeof(u32))
		{
			mempcy(outPtr, &data, size);
			break;
		}
		else
			mempcy(outPtr, &data, sizeof(u32));

		addr += 1;
		size -= sizeof(u32);
		outPtr += sizeof(u32);
	}
}

u32 OTP_IsSet(void)
{
	static u32 OTP_CheckedCommonKeyIsSet = 0;
	static u32 OTP_CommonKeyIsSet = 0;

	if (!OTP_CheckedCommonKeyIsSet)
	{
		u32 commonKey[OTP_COMMONKEY_SIZE/sizeof(u32)];
		OTP_FetchData(5, commonKey, OTP_COMMONKEY_SIZE);
		// if the key isn't all 0x00
		if ((commonKey[0] | commonKey[1] | commonKey[2] | commonKey[3]) != 0)
			OTP_CommonKeyIsSet = 1;

		OTP_CheckedCommonKeyIsSet = 1;
	}

	return OTP_CommonKeyIsSet;
}
void OTP_GetNgId(u32 *ngId)
{
	const u32 cookie = DisableInterrupts();
	if(OTP_IsSet())
	{
		OTP_FetchData(9, ngId, sizeof(u32));
	}
	else
	{
		memcpy(ngId, &OTP_Dummy_NgId, sizeof(u32));
	}
	RestoreInterrupts(cookie);
}

void OTP_GetRngSeed(u8 seed[OTP_RNGSEED_SIZE])
{
	const u32 cookie = DisableInterrupts();
	if(OTP_IsSet())
	{
		OTP_FetchData(26, seed, OTP_RNGSEED_SIZE);
	}
	else
	{
		memcpy(seed, OTP_Dummy_RngSeed, OTP_RNGSEED_SIZE);
	}
	RestoreInterrupts(cookie);
}

void OTP_GetKeys(u8 ng_privkey_out[OTP_NGPRIVKEY_SIZE], u8 common_key_out[OTP_COMMONKEY_SIZE], u8 nand_hmac_out[OTP_NANDHMAC_SIZE], u8 nand_key_out[OTP_NANDKEY_SIZE])
{
	const u32 cookie = DisableInterrupts();
	if (OTP_IsSet())
	{
		OTP_FetchData(5, common_key_out, OTP_COMMONKEY_SIZE);
		// overlap on the last/first word
		OTP_FetchData(10, ng_privkey_out, OTP_NGPRIVKEY_SIZE);
		OTP_FetchData(17, nand_hmac_out, OTP_NANDHMAC_SIZE);
		OTP_FetchData(22, nand_key_out, OTP_NANDKEY_SIZE);
	}
	else
	{
		memcpy(common_key_out, OTP_Dummy_CommonKey, OTP_COMMONKEY_SIZE);
		memcpy(ng_privkey_out, OTP_Dummy_NgPrivKey, OTP_NGPRIVKEY_SIZE);
		memcpy(nand_hmac_out, OTP_Dummy_NandHmac, OTP_NANDHMAC_SIZE);
		memcpy(nand_key_out, OTP_Dummy_NandKey, OTP_NANDKEY_SIZE);
	}
	RestoreInterrupts(cookie);
}
