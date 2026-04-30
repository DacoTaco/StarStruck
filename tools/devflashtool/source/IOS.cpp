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

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <unistd.h>
#include <malloc.h>
#include <string>
#include <vector>
#include <algorithm>

#include <gccore.h>
#include <ogc/machine/processor.h>

#include "IOS.hpp"
#include "gecko.h"
#include "disableAHBProt_bin.h"

#define SRAMADDR(x) (0x0d400000 | ((x) & 0x000FFFFF))

//note : these are less "safe" then libogc's but libogc forces uncached MEM1 addresses, even for writes.
//this can cause some issues sometimes when patching ios in mem2
static inline u8 ReadRegister8(u32 address)
{
	DCFlushRange(reinterpret_cast<void *>(0xC0000000 | address), 2);
	return read8(address);
}

static inline u16 ReadRegister16(u32 address)
{
	DCFlushRange(reinterpret_cast<void *>(0xC0000000 | address), 4);
	return read16(address);
}

static inline u32 ReadRegister32(u32 address)
{
	DCFlushRange(reinterpret_cast<void *>(0xC0000000 | address), 8);
	return read32(address);
}

static inline void WriteRegister8(u32 address, u8 value)
{
	if (address < 0x80000000)
		address |= 0xC0000000;

	asm("stb %0,0(%1) ; eieio" : : "r"(value), "b"(address));
}

static inline void WriteRegister16(u32 address, u16 value)
{
	if (address < 0x80000000)
		address |= 0xC0000000;

	asm("sth %0,0(%1) ; eieio" : : "r"(value), "b"(address));
}

static inline void WriteRegister32(u32 address, u32 value)
{
	if (address < 0x80000000)
		address |= 0xC0000000;

	asm("stw %0,0(%1) ; eieio" : : "r"(value), "b"(address));
}

//IOS Patches
const IosPatch OpenFSAsFlash = {
	//Pattern
	{
	    0x23, 0x00, //mov        r3,#0x0
	    0x2b, 0x01, //cmp        r3,#0x1
	    0xd1, 0x02, //bne        LAB_200057f6
	    0xf7, 0xff, 0xfe, 0x00, //bl         FS_OpenInterface
	    0xe0, 0x1a, //b          LAB_2000582c

	},

	//Apply Patch
	[](u8 *address) {
	    gprintf("Found /dev/flash open check @ 0x%X, patching...\n", address);
	    WriteRegister8((u32)address + 1, 0x01);
	}
};

const IosPatch OpenFSAsFS = {
	//Pattern
	{
	    0x23, 0x01, //mov        r3,#0x0
	    0x2b, 0x01, //cmp        r3,#0x1
	    0xd1, 0x02, //bne        LAB_200057f6
	    0xf7, 0xff, 0xfe, 0x00, //bl         FS_OpenInterface
	    0xe0, 0x1a, //b          LAB_2000582c

	},

	//Apply Patch
	[](u8 *address) {
	    gprintf("Found /dev/flash open check @ 0x%X, patching...\n", address);
	    WriteRegister8((u32)address + 1, 0x00);
	}
};

//Ahbprot : 68 1B -> 0x23 0xFF
const IosPatch AhbProtPatcher = {
	//Pattern - patch by tuedj
	{
	    0x68, 0x5B, // ldr r3,[r3,#4]  ; get TMD pointer
	    0x22, 0xEC, 0x00, 0x52, // movls r2, 0x1D8
	    0x18, 0x9B, // adds r3, r3, r2 ; add offset of access rights field in TMD
	    0x68, 0x1B, // ldr r3, [r3]    ; load access rights (haxxme!)
	    0x46, 0x98, // mov r8, r3      ; store it for the DVD video bitcheck later
	    0x07, 0xDB // lsls r3, r3, #31; check AHBPROT bit
	},

	//Apply Patch
	[](u8 *address) {
	    gprintf("Found ES_AHBPROT check @ 0x%X, patching...\n", address);
	    WriteRegister8((u32)address + 8, 0x23); // li r3, 0xFF.aka, make it look like the TMD had max settings
	    WriteRegister8((u32)address + 9, 0xFF);
	}
};

//nand permissions : 42 8b d0 01 25 66 -> 42 8b e0 01 25 66
const IosPatch NandAccessPatcher = {
	//Pattern
	{ 0x42, 0x8B, 0xD0, 0x01, 0x25, 0x66 },

	//Apply Patch
	[](u8 *address) {
	    gprintf("Found NAND Permission check @ 0x%X, patching...\n", address);
	    WriteRegister8((u32)address + 2, 0xE0);
	    WriteRegister8((u32)address + 3, 0x01);
	}
};

const IosPatch DebugRedirectionPatch = {
	//Pattern
	{ 0x46, 0x72, 0x1C, 0x01, 0x20, 0x05 },

	//Apply Patch
	[](u8 *address) {
	    gprintf("patching DebugRedirectionPatch 0x%08X\n", (0xFFFF0000) | (u32)address);
	    if ((((u32)address) & 0x90000000) != 0)
		    return;

	    u8 patch[] = {
		    0xE9, 0xAD, 0x40, 0x1F, 0xE1, 0x5E, 0x30, 0xB2, 0xE2, 0x03, 0x30,
		    0xFF, 0xE3, 0x53, 0x00, 0xAB, 0x1A, 0x00, 0x00, 0x07, 0xE3, 0x50,
		    0x00, 0x04, 0x1A, 0x00, 0x00, 0x05, 0xE5, 0x9F, 0x30, 0x58, 0xE5,
		    0xD1, 0x20, 0x00, 0xEB, 0x00, 0x00, 0x04, 0xE2, 0x81, 0x10, 0x01,
		    0xE3, 0x52, 0x00, 0x00, 0x1A, 0xFF, 0xFF, 0xFA, 0xE8, 0x3D, 0x40,
		    0x1F, 0xE1, 0xB0, 0xF0, 0x0E, 0xE3, 0xA0, 0x00, 0xD0, 0xE5, 0x83,
		    0x00, 0x00, 0xE3, 0xA0, 0x02, 0x0B, 0xE1, 0x80, 0x0A, 0x02, 0xE5,
		    0x83, 0x00, 0x10, 0xE3, 0xA0, 0x00, 0x19, 0xE5, 0x83, 0x00, 0x0C,
		    0xE5, 0x93, 0x00, 0x0C, 0xE3, 0x10, 0x00, 0x01, 0x1A, 0xFF, 0xFF,
		    0xFC, 0xE5, 0x93, 0x00, 0x10, 0xE3, 0x10, 0x03, 0x01, 0xE3, 0xA0,
		    0x00, 0x00, 0xE5, 0x83, 0x00, 0x00, 0x0A, 0xFF, 0xFF, 0xF0, 0xE1,
		    0xA0, 0xF0, 0x0E, 0x0D, 0x80, 0x68, 0x14
	    };

	    for (u32 i = 0; i < sizeof(patch); i += 4)
		    WriteRegister32(((u32)address) + i, *(u32 *)&patch[i]);

	    //redirect svc handler
	    WriteRegister32(SRAMADDR(0xFFFF0028), (0xFFFF0000) | (u32)address);
	    DCFlushRange((void *)SRAMADDR(0xFFFF0028), 16);
	    ICInvalidateRange((void *)SRAMADDR(0xFFFF0028), 16);
	}
};

bool DisableAHBProt()
{
	bool ret = true;
	s32 fd = -1;
	ioctlv *params = NULL;
	try
	{
		//time to drop the exploit bomb on /dev/sha
		gprintf("Preparing IOS Bomb...\n");
		fd = IOS_Open("/dev/sha", 0);
		if (fd < 0)
			throw "Failed to open /dev/sha : " + std::to_string(fd);

		params = static_cast<ioctlv *>(memalign(sizeof(ioctlv) * 4, 32));
		if (params == NULL)
			throw "failed to alloc IOS call data";

		//overwrite the thread 0 state with address 0 (0x80000000)
		memset(params, 0, sizeof(ioctlv) * 4);
		params[1].data = reinterpret_cast<void *>(0xFFFE0028);
		params[1].len = 0;
		DCFlushRange(params, sizeof(ioctlv) * 4);

		//set code to disable ahbprot and stay in loop
		memcpy(reinterpret_cast<void *>(0x80000000), disableAHBProt_bin, disableAHBProt_bin_size);
		DCFlushRange(reinterpret_cast<void *>(0x80000000), disableAHBProt_bin_size);
		ICInvalidateRange(reinterpret_cast<void *>(0x80000000), disableAHBProt_bin_size);

		//send sha init command
		gprintf("Dropping IOS bomb...\n");
		s32 callRet = IOS_Ioctlv(fd, 0x00, 1, 2, params);
		if (callRet < 0)
			throw "failed to send SHA init : " + std::to_string(callRet);

		//wait for it to have processed the sha init and given a timeslice to the mainthread :)
		usleep(50000);
	} catch (const std::string &ex)
	{
		gprintf("Disable AHBPROT: %s\n", ex.c_str());
		ret = false;
	} catch (char const *ex)
	{
		gprintf("Disable AHBPROT: %s\n", ex);
		ret = false;
	} catch (...)
	{
		ret = false;
		gprintf("Disable AHBPROT: Unknown Error Occurred\n");
	}

	if (fd >= 0)
		IOS_Close(fd);

	if (params)
		free(params);

	return ret;
}

s32 ReloadIOS(s32 iosToLoad, s8 keepAhbprot)
{
	s32 ret = keepAhbprot ? PatchIOS({ AhbProtPatcher }) : 1;

	if (ret <= 0)
		return ret;

	IOS_ReloadIOS(iosToLoad);

	if (ReadRegister32(0x0d800064) > 0 && IsUsbGeckoDetected())
		PatchIOSKernel({ DebugRedirectionPatch });

	return (iosToLoad != IOS_GetVersion()) ? -100 : iosToLoad;
}
s8 PatchIOS(std::vector<IosPatch> patches)
{
	if (patches.size() == 0)
		return 0;

	if (ReadRegister32(0x0d800064) != 0xFFFFFFFF)
		return -1;

	if (ReadRegister16(0x0d8b420a))
		WriteRegister16(0x0d8b420a, 0); //there is more you can do to make more available but meh, not needed

	if (ReadRegister16(0x0d8b420a))
		return -2;

	//look in MEM2
	gprintf("Patching IOS in MEM2...\n");
	u32 patchesFound = 0;
	u8 *mem_block = reinterpret_cast<u8 *>(ReadRegister32(0x80003130));
	u32 mem_length = 0x93FFFFFF - (u32)mem_block;
	if (mem_length > 0x03FFFFFF)
		mem_length = 0x100;
	ICInvalidateRange(mem_block, mem_length);
	DCFlushRange(mem_block, mem_length);

	while ((u32)mem_block < 0x93FFFFFF)
	{
		auto iterator =
		    std::find_if(patches.begin(), patches.end(),
		                 [&patchesFound, mem_block](const IosPatch &iosPatch) {
			                 s32 patchSize = iosPatch.Pattern.size();
			                 if (memcmp(mem_block, &iosPatch.Pattern[0], patchSize) != 0)
				                 return false;

			                 //Apply patch
			                 iosPatch.Patch(mem_block);

			                 //flush cache
			                 u8 *address = (u8 *)(((u32)mem_block) >> 5 << 5);
			                 DCFlushRange(address, (patchSize >> 5 << 5) + 64);
			                 ICInvalidateRange(address, (patchSize >> 5 << 5) + 64);
			                 patchesFound++;
			                 return true;
		                 });

		if (iterator != patches.end() && patchesFound == patches.size())
			break;

		mem_block++;
	}

	WriteRegister16(0x0d8b420a, 1);
	return patchesFound;
}
s8 PatchIOSKernel(std::vector<IosPatch> patches)
{
	if (patches.size() == 0)
		return 0;

	if (ReadRegister32(0x0d800064) != 0xFFFFFFFF)
		return -1;

	if (ReadRegister16(0x0d8b420a))
		WriteRegister16(0x0d8b420a, 0); //there is more you can do to make more available but meh, not needed

	if (ReadRegister16(0x0d8b420a))
		return -2;

	gprintf("Patching IOS kernel...\n");
	u32 patchesFound = 0;
	u8 *mem_block = reinterpret_cast<u8 *>(SRAMADDR(0xFFFF0000));
	while ((u32)mem_block < SRAMADDR(0xFFFFFFFF))
	{
		auto iterator =
		    std::find_if(patches.begin(), patches.end(),
		                 [&patchesFound, mem_block](const IosPatch &iosPatch) {
			                 u32 patchSize = iosPatch.Pattern.size();
			                 u32 matches = 0;
			                 for (matches = 0; matches < patchSize; matches++)
			                 {
				                 if (ReadRegister8(((u32)mem_block) + matches) !=
				                     iosPatch.Pattern[matches])
					                 break;
			                 }

			                 if (matches != patchSize)
				                 return false;

			                 //Apply patch
			                 iosPatch.Patch(mem_block);

			                 //flush cache
			                 u8 *address = (u8 *)(((u32)mem_block) >> 5 << 5);
			                 DCFlushRange(address, (matches >> 5 << 5) + 64);
			                 ICInvalidateRange(address, (matches >> 5 << 5) + 64);
			                 return true;
		                 });

		if (iterator != patches.end() && patchesFound == patches.size())
			break;

		mem_block++;
	}

	WriteRegister16(0x0d8b420a, 1);
	return patchesFound;
}