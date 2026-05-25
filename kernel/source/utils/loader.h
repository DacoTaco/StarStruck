/*
	starstruck - a Free Software reimplementation for the Nintendo/BroadOn IOS.
	loader - loading binaries into memory and starting them

	Copyright (C) 2021	DacoTaco

# This code is licensed to you under the terms of the GNU GPL, version 2;
# see file COPYING or http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt
*/

#include <types.h>

#ifndef MIOS

#define IOS_STAGING_AREA_START 0x10100000
#define IOS_STAGING_SIZE       0x00B00001

s32 LoadBinary(const char* path);
s32 LoadKernel(const char* path, s32 suspendBroadway, u32 version);
s32 LaunchKernel(const void* image, u32 version);

#endif