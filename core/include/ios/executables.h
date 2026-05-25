/*
	starstruck - a Free Software reimplementation for the Nintendo/BroadOn IOS.
	binary types used on the wii

Copyright (C) 2025	DacoTaco

# This code is licensed to you under the terms of the GNU GPL, version 2;
# see file COPYING or http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt
*/

#pragma once

#include <types.h>

// ELF Program header to indicate PPC segment
#define ELF_PF_PPC        0x07f00000

//DOL executable binary format
#define DOL_TEXT_SEGMENTS 7
#define DOL_DATA_SEGMENTS 11

typedef struct
{
	struct
	{
		u32 Text[DOL_TEXT_SEGMENTS];
		u32 Data[DOL_DATA_SEGMENTS];
	} Offsets;
	struct
	{
		u32 Text[DOL_TEXT_SEGMENTS];
		u32 Data[DOL_DATA_SEGMENTS];
	} Addresses;
	struct
	{
		u32 Text[DOL_TEXT_SEGMENTS];
		u32 Data[DOL_DATA_SEGMENTS];
	} Sizes;
	struct
	{
		u32 Address;
		u32 Size;
	} BSS;
	u32* EntryPoint;
	u8 Padding[0x1C];
} DolHeader;

CHECK_SIZE(DolHeader, 0x100);
CHECK_OFFSET(DolHeader, 0x00, Offsets.Text);
CHECK_OFFSET(DolHeader, 0x1C, Offsets.Data);
CHECK_OFFSET(DolHeader, 0x48, Addresses.Text);
CHECK_OFFSET(DolHeader, 0x64, Addresses.Data);
CHECK_OFFSET(DolHeader, 0x90, Sizes.Text);
CHECK_OFFSET(DolHeader, 0xAC, Sizes.Data);
CHECK_OFFSET(DolHeader, 0xD8, BSS.Address);
CHECK_OFFSET(DolHeader, 0xDC, BSS.Size);
CHECK_OFFSET(DolHeader, 0xE0, EntryPoint);
CHECK_OFFSET(DolHeader, 0xE4, Padding);

typedef struct
{
	u32 HeaderSize;
	u32 LoaderSize;
	u32 ElfSize;
	u32 Arguments;
} IosKernelHeader;
CHECK_SIZE(IosKernelHeader, 0x10);
CHECK_OFFSET(IosKernelHeader, 0x00, HeaderSize);
CHECK_OFFSET(IosKernelHeader, 0x04, LoaderSize);
CHECK_OFFSET(IosKernelHeader, 0x08, ElfSize);
CHECK_OFFSET(IosKernelHeader, 0x0C, Arguments);