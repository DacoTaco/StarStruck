/*

priiloader(preloader mod) - A tool which allows to change the default boot up sequence on the Wii console

Copyright (C) 2013-2019  DacoTaco

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation version 2.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.

*/

#ifndef ___IOS_H_
#define ___IOS_H_

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <unistd.h>
#include <string.h>
#include <vector>
#include <algorithm>

#include <gccore.h>
#include <vector>

//IOS Patching structs
typedef void (*Iospatcher)(u8 *address);
typedef struct _IosPatch
{
	std::vector<u8> Pattern;
	Iospatcher Patch;
} IosPatch;

extern const IosPatch OpenFSAsFlash;
extern const IosPatch OpenFSAsFS;
extern const IosPatch AhbProtPatcher;
extern const IosPatch NandAccessPatcher;
extern const IosPatch DebugRedirectionPatch;

bool DisableAHBProt(void);
s32 ReloadIOS(s32 iosToLoad, s8 keepAhbprot);
s8 PatchIOS(std::vector<IosPatch> patches);
s8 PatchIOSKernel(std::vector<IosPatch> patches);

#endif