/*
	starstruck - a Free Software reimplementation for the Nintendo/BroadOn IOS.
	Copyright (C) 2023	DacoTaco

# This code is licensed to you under the terms of the GNU GPL, version 2;
# see file COPYING or http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt
*/

PHDRS
{
	phdrs			PT_PHDR PHDRS FLAGS(0x00F00000);
	note			PT_NOTE FLAGS(0);
	headers			PT_LOAD PHDRS FLAGS(0x00F00000) AT(0x138F0000);
}

MEMORY 
{
	headers_heap	: ORIGIN = 0x138F0000, LENGTH = 768K
}

__headers_size = LENGTH(headers_heap);
__headers_addr = ORIGIN(headers_heap);

SECTIONS
{
	.noteHdr __headers_addr + SIZEOF_HEADERS - 0x34:
	{
		/*this is the Elf32_Nhdr header*/
		LONG(0)
		__modules_size = .;
		LONG(SIZEOF(.noteHdr) - 0x0C)
		LONG(6)
		__modules = .;
		*(.note);
		KEEP(*_notes.bin.o(.rodata.*notes_bin));
		*(.rodata.*notes_bin);
	} > headers_heap :headers :note

	.heap :
	{
		. += __headers_size - (SIZEOF_HEADERS - 0x34) - SIZEOF(.noteHdr);
	} >headers_heap :headers
}