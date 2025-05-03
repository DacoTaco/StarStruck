/*
	mini - a Free Software replacement for the Nintendo/BroadOn IOS.

	ELF loader

Copyright (C) 2008, 2009	Hector Martin "marcan" <marcan@marcansoft.com>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, version 2.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

*/

#include <types.h>
#include <string.h>
#include <ios/processor.h>
#include "utils.h"
#include "elf.h"

typedef struct {
	u32 hdrsize;
	u32 loadersize;
	u32 elfsize;
	u32 argument;
} ioshdr;

#define	HW_MEMMIRR	0xd800060
#define	HW_BOOT0	0xd80018C

extern void debug_output(u8 byte);

void *loadelf(const u8 *elf) {
	if(memcmp("\x7F" "ELF\x01\x02\x01",elf,7)) {
		panic(0xE3);
	}
	
	Elf32_Ehdr *ehdr = (Elf32_Ehdr*)elf;
	if(ehdr->e_phoff == 0) {
		panic(0xE4);
	}
	int count = ehdr->e_phnum;
	Elf32_Phdr *phdr = (Elf32_Phdr*)(elf + ehdr->e_phoff);
	while(count--)
	{
		if(phdr->p_type == PT_LOAD) {
			const void *src = elf + phdr->p_offset;
			memcpy(phdr->p_paddr, src, phdr->p_filesz);
		}
		phdr++;
	}
	return ehdr->e_entry;
}

static inline void disable_boot0()
{
	set32(HW_BOOT0, 0x1000);
}

static inline void mem_setswap()
{
	set32(HW_MEMMIRR, 0x20);
}

void *_main(void *base)
{
	ioshdr *hdr = (ioshdr*)base;
	u8 *elf;
	void *entry;
	
	elf = (u8*) base;
	elf += hdr->hdrsize + hdr->loadersize;
	
	debug_output(0xF1);
	mem_setswap();
	disable_boot0();
	
	entry = loadelf(elf);
	debug_output(0xC1);
	return entry;
}

