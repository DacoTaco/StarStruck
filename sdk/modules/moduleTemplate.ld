/*
	StarStruck - a Free Software reimplementation for the Nintendo/BroadOn IOS.
	Copyright (C) 2022	DacoTaco

# This code is licensed to you under the terms of the GNU GPL, version 2;
# see file COPYING or http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt
*/

OUTPUT_FORMAT("elf32-bigarm");
OUTPUT_ARCH(arm);
EXTERN(_startup);
ENTRY(_startup);

__physicalAddress = __PHYSADDR__;
__priority = __PRIORITY__;
__processId = __PROCESSID__;
__stackSize = __STACKSIZE__;

/* 
    Flags:
    0x00000001 : PF_X
    0x00000002 : PF_W
    0x00000004 : PF_R
    0x0XX00000 : domain
*/

PHDRS
{
	phdrs		PT_PHDR PHDRS FLAGS(0x00F00000);
	note		PT_NOTE FLAGS(0);
	headers		PT_LOAD PHDRS FLAGS(0x00F00000);
	ro			PT_LOAD FLAGS((__processId << 0x14) + 0x05) /* Read | Execute */;
	data		PT_LOAD FLAGS((__processId << 0x14) + 0x06) /* Read | Write */;
	bss			PT_LOAD FLAGS((__processId << 0x14) + 0x06) /* Read | Write */;
}

SECTIONS
{
	.noteHdr 0x00000000 + SIZEOF_HEADERS - 0x34: ALIGN(4)
	{
		/*this is the Elf32_Nhdr header*/
		LONG(0);
		LONG(SIZEOF(.note));
		LONG(6);	
	} :headers :note

	.note :
	{
		KEEP (*.note)
		LONG(0x0B); /*ProcessID*/
		LONG(__processId);
		LONG(0x09); /* EntryPoint */
		LONG(ADDR(.module));
		LONG(0x7D);
		LONG(__priority);
		LONG(0x7E);
		LONG(__stackSize);
		LONG(0x7F);
		LONG(__stack);
	} :headers :note

	/* Program */
	.module : AT(__physicalAddress) ALIGN(4) 
	{
		KEEP (*_startup.o)
		*(.module.init)
		*(.module)
		*(.text .text.* .gnu.linkonce.t*)
		*(.rodata)
		*(.roda)
		*(.rodata.*)
		. = ALIGN(4);
		__module_end = .;
	} :ro

	.module.data : ALIGN(0x1000)
	{
		*(.module.data*)
		*(.data)
		*(.data.*)
		*(.gnu.linkonce.d*)
		. = ALIGN(4);
	} :data

	.module.bss(NOLOAD) : ALIGN(0x1000)
	{
		__bss_start = . ;
		*(.module.bss*)
		*(.dynbss)
		*(.gnu.linkonce.b*)
		*(.bss*)
		*(COMMON)
		. = ALIGN(8);
		__stack = .;
		. += __stackSize;
		__stackEnd = .;
	} :bss
}