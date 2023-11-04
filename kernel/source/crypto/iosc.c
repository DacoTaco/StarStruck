/*
	starstruck - a Free Software reimplementation for the Nintendo/BroadOn IOS.
	iosc - ios crypto syscalls

	Copyright (C) 2021	DacoTaco
	Copyright (C) 2023	Jako

# This code is licensed to you under the terms of the GNU GPL, version 2;
# see file COPYING or http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt
*/

#include "crypto/iosc.h"
#include "crypto/otp.h"
#include "crypto/keyring.h"
#include "crypto/boot2.h"
#include "crypto/nand.h"
#include "crypto/seeprom.h"
#include <ios/errno.h>

static u32 IOSC_BOOT2_DummyVersion = 0;
static u32 IOSC_BOOT2_DummyUnk1 = 0;
static u32 IOSC_BOOT2_DummyUnk2 = 0;
static u32 IOSC_NAND_DummyGen = 0;
static u32 IOSC_SEEPROM_DummyPRNGSeed = 0;

static s32 IOSC_BOOT2_UpdateVersion(void);
static s32 IOSC_BOOT2_UpdateUnk1(void);
static s32 IOSC_BOOT2_UpdateUnk2(void);
static s32 IOSC_NAND_UpdateGen(void);
static s32 IOSC_SEEPROM_UpdatePRNGSeed(void);
static s32 IOSC_SEEPROM_GetPRNGSeed(void);

s32 IOSC_Init(void)
{
	const u32 HandlesAndOwners[][2] = {
		{0, 3},
		{1, RSA4096_ROOTKEY},
		{2, 5},
		{3, 5},
		{4, 3},
		{7, 3},
		{8, 3},
		{9, 3},
		{10, 5},
		{5, 3},
		{6, 3},
		{11, 3},
	};

	const u32 HandlesAndZeroes[][2] = {
		{0, 0},
		{2, 0},
		{3, 0},
		{4, 0},
		{11, 0},
		{5, 0},
		{6, 0},
	};

	Keyring_Init();

	s32 ret = IPC_SUCCESS;
	for(s8 i = 0; ret == IPC_SUCCESS && i < ARRAY_LENGTH(HandlesAndZeroes); ++i)
	{
		const u32 handle = HandlesAndZeroes[i][0];
		const u32 zeroes = HandlesAndZeroes[i][1];
		ret = Keyring_SetKeyZeroesIfAnyPrivate(handle, zeroes);
	}

	ret = IPC_SUCCESS;
	for(s8 i = 0; ret == IPC_SUCCESS && i < ARRAY_LENGTH(HandlesAndOwners); ++i)
	{
		const u32 handle = HandlesAndOwners[i][0];
		const u32 owner = HandlesAndOwners[i][1];
		ret = Keyring_SetKeyOwnerIfUsed(handle, owner);
	}

	return ret;
}

s32 IOSC_BOOT2_GetVersion(void)
{
	const u32 cookie = DisableInterrupts();
	s32 ret = IOSC_BOOT2_DummyVersion;
	if(OTP_IsSet())
	{
		ret = BOOT2_GetVersion();
	}
	RestoreInterrupts(cookie);
	return ret;
}
s32 IOSC_BOOT2_GetUnk1(void)
{
	const u32 cookie = DisableInterrupts();
	s32 ret = IOSC_BOOT2_DummyUnk1;
	if(OTP_IsSet())
	{
		ret = BOOT2_GetUnk1();
	}
	RestoreInterrupts(cookie);
	return ret;
}
s32 IOSC_BOOT2_GetUnk2(void)
{
	const u32 cookie = DisableInterrupts();
	s32 ret = IOSC_BOOT2_DummyUnk1; // not Unk2. yes, this is a bug in IOS 58
	if(OTP_IsSet())
	{
		ret = BOOT2_GetUnk2();
	}
	RestoreInterrupts(cookie);
	return ret;
}
s32 IOSC_NAND_GetGen(void)
{
	const u32 cookie = DisableInterrupts();
	s32 ret = IOSC_NAND_DummyGen;
	if(OTP_IsSet())
	{
		ret = NAND_GetGen();
	}
	RestoreInterrupts(cookie);
	return ret;
}
s32 IOSC_SEEPROM_GetPRNGSeed(void)
{
	const u32 cookie = DisableInterrupts();
	s32 ret = IOSC_SEEPROM_DummyPRNGSeed;
	if(OTP_IsSet())
	{
		ret = SEEPROM_GetPRNGSeed();
	}
	RestoreInterrupts(cookie);
	return ret;
}

s32 IOSC_BOOT2_UpdateVersion(void)
{
	const u32 cookie = DisableInterrupts();
	s32 ret = 0;
	if(OTP_IsSet())
	{
		if(BOOT2_GetVersion() + 1 < 0x100)
		{
			ret = BOOT2_UpdateVersion();
		}
		else
		{
			ret = -1;
		}
	}
	else
	{
		IOSC_BOOT2_DummyVersion += 1;
	}
	RestoreInterrupts(cookie);
	return ret;
}
s32 IOSC_BOOT2_UpdateUnk1(void)
{
	const u32 cookie = DisableInterrupts();
	s32 ret = 0;
	if(OTP_IsSet())
	{
		if(BOOT2_GetUnk1() + 1 < 0x100)
		{
			ret = BOOT2_UpdateUnk1();
		}
		else
		{
			ret = -1;
		}
	}
	else
	{
		IOSC_BOOT2_DummyUnk1 += 1;
	}
	RestoreInterrupts(cookie);
	return ret;
}
s32 IOSC_BOOT2_UpdateUnk2(void)
{
	const u32 cookie = DisableInterrupts();
	s32 ret = 0;
	if(OTP_IsSet())
	{
		if(BOOT2_GetUnk2() + 1 < 0x100)
		{
			ret = BOOT2_UpdateUnk2();
		}
		else
		{
			ret = -1;
		}
	}
	else
	{
		IOSC_BOOT2_DummyUnk2 += 1;
	}
	RestoreInterrupts(cookie);
	return ret;
}
s32 IOSC_NAND_UpdateGen(void)
{
	const u32 cookie = DisableInterrupts();
	s32 ret = 0;
	if(OTP_IsSet())
	{
		if(NAND_GetGen() != -2)
		{
			ret = NAND_UpdateGen();
		}
		else
		{
			ret = -1;
		}
	}
	else
	{
		IOSC_NAND_DummyGen += 1;
	}
	RestoreInterrupts(cookie);
	return ret;
}
s32 IOSC_SEEPROM_UpdatePRNGSeed(void)
{
	const u32 cookie = DisableInterrupts();
	s32 ret = 0;
	if(OTP_IsSet())
	{
		ret = SEEPROM_UpdatePRNGSeed();
	}
	else
	{
		IOSC_SEEPROM_DummyPRNGSeed += 1;
	}
	RestoreInterrupts(cookie);
	return ret;
}
