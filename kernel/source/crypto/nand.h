/*
	starstruck - a Free Software reimplementation for the Nintendo/BroadOn IOS.
	nand - the nand interface

	Copyright (C) 2021	DacoTaco

# This code is licensed to you under the terms of the GNU GPL, version 2;
# see file COPYING or http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt
*/

#ifndef __NAND_H__
#define __NAND_H__

#include <types.h>

typedef struct {
	union {
		u32 NandGen; // matches offset 0x8 in nand SFFS blocks
		u16 Data[2];
	} __attribute__((packed));
	u16 Checksum;
} NAND_Counter;
CHECK_SIZE(NAND_Counter, 6);
CHECK_OFFSET(NAND_Counter, 0x00, Data);
CHECK_OFFSET(NAND_Counter, 0x04, Checksum);

u16 NAND_ComputeCounterChecksum(const NAND_Counter* data);

u32 NAND_GetGen(void);
s32 NAND_UpdateGen(void);

#endif
