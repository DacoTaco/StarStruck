/*
	starstruck - a Free Software reimplementation for the Nintendo/BroadOn IOS.
	powerpc - manage the Hollywood CPU

	Copyright (C) 2021	DacoTaco

# This code is licensed to you under the terms of the GNU GPL, version 2;
# see file COPYING or http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt
*/

#pragma once
#include <types.h>

extern const u32 PPC_LaunchBS1[0x10];
extern const u32 PPC_GameCubeResetVector[0x0C];
extern const u32 PPC_WiiResetVector[0x0D];

static inline u32 PPCVirtToPhys(u32 addr)
{
	if (addr > 0x80000000)
		return addr & ~0x80000000u;
	return addr;
}

void PPCHardReset(void);
void PPCSoftReset(void);
void PPCLoadCode(bool wiiMode, const void *code, u32 codeSize);
void PPCSetMEM1(u32 hollywoodVersion, u32 gddrVendorCode);
void PPCStart(void);
void PPCSetSDKSemaphore(bool hasSemaphore);

#ifndef MIOS
int LoadPPC(const char *path);
#endif

//old mini stuff
void powerpc_upload_stub(u32 entry);
void powerpc_hang(void);