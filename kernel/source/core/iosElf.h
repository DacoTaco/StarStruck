/*
	mini - a Free Software replacement for the Nintendo/BroadOn IOS.
	ELF structures

Copyright (C) 2008, 2009	Hector Martin "marcan" <marcan@marcansoft.com>

# This code is licensed to you under the terms of the GNU GPL, version 2;
# see file COPYING or http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt
*/

#ifndef __IOS_ELF_H__
#define __IOS_ELF_H__

#include <elf.h>
#include "types.h"

//domain are in the elf flags in bit 21-24 (0x0xx00000)
#define FLAGSTODOMAIN(flags) ((flags) >> 0x14)

#define ELFMAGIC ((ELFMAG0 << 24) | (ELFMAG1 << 16) | (ELFMAG2 << 8) | (ELFMAG3))
#define IOSELFINFO ((ELFCLASS32 << 24) | (ELFDATA2MSB << 16) | (01 << 8) | (ELFOSABI_ARM))

typedef struct {
	u32 hdrsize;
	u32 loadersize;
	u32 elfsize;
	u32 argument;
} ioshdr;

CHECK_SIZE(ioshdr, 0x10);

#endif

