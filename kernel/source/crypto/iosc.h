/*
	starstruck - a Free Software reimplementation for the Nintendo/BroadOn IOS.
	iosc - ios crypto syscalls

	Copyright (C) 2021	DacoTaco

# This code is licensed to you under the terms of the GNU GPL, version 2;
# see file COPYING or http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt
*/

#ifndef __IOSC_H__
#define __IOSC_H__

#include <types.h>

s32 IOSC_Init(void);

s32 IOSC_BOOT2_GetVersion(void);
s32 IOSC_BOOT2_GetUnk1(void);
s32 IOSC_BOOT2_GetUnk2(void);
s32 IOSC_NAND_GetGen(void);

#endif
