/*
	starstruck - a Free Software reimplementation for the Nintendo/BroadOn IOS.
	powerpc - manage the Hollywood CPU

	Copyright (C) 2021	DacoTaco

# This code is licensed to you under the terms of the GNU GPL, version 2;
# see file COPYING or http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt
*/

#pragma once
#include <types.h>

//code that will let me PPC start execution @ 0x80003400, which is the BS1 vector, where all dol content usually starts
extern const u32 PPC_LaunchBS1[0x10];
extern const u32 PPC_LaunchExceptionVector[0x0C];
extern const u32 PPC_LaunchExceptionVector2[0x0D];

void PPCSoftReset(void);
void PPCLoadCode(s8 mode, const void *code, u32 codeSize);
void PPCStart(void);

//old mini stuff
void powerpc_upload_stub(u32 entry);
void powerpc_hang(void);