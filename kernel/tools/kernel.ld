/*
	starstruck - a Free Software reimplementation for the Nintendo/BroadOn IOS.
	Copyright (C) 2023	DacoTaco

# This code is licensed to you under the terms of the GNU GPL, version 2;
# see file COPYING or http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt
*/

INCLUDE kernelHeaders.ld
INCLUDE kernelModules.ld
INCLUDE iosModules.ld
OUTPUT_FORMAT("elf32-bigarm")
OUTPUT_ARCH(arm)
EXTERN(_start)
ENTRY(_start)

/* 
    Flags:
    0x00000001 : PF_X
    0x00000002 : PF_W
    0x00000004 : PF_R
    0x0XX00000 : domain
*/
PHDRS
{
	sram		PT_LOAD FLAGS(0x00000006) /* Read | Write */;
	kernel		PT_LOAD FLAGS(0x00000005) /* Read | Execute */;
	rodata		PT_LOAD FLAGS(0x00000004) /* Read */;
	svcs		PT_LOAD FLAGS(0x00000004) /* Read */;
	data		PT_LOAD FLAGS(0x00000006) /* Read | Write */;
}

/*Sram is split up in several parts lol. FFFE0000 = 32K available. then there is 32K of unusable crap, followed by 64K of memory*/
MEMORY 
{
	sram				: ORIGIN = 0xFFFE0000, LENGTH = 32K
	kernel				: ORIGIN = 0xFFFF0000, LENGTH = 64K
	ipc_heap			: ORIGIN = 0x13600000, LENGTH = 128K
	kmalloc_heap		: ORIGIN = 0x13850000, LENGTH = 128K
	thread_stacks		: ORIGIN = 0x13AC0000, LENGTH = 128K
	iobuf_heap			: ORIGIN = 0x13C40000, LENGTH = 512K
	mem2				: ORIGIN = 0x13F00000, LENGTH = 1M
}

__stack_size = 0x300;
__irqstack_size = 0x060;
__excstack_size = 0x100;
__ioscstack_size = 0x400;

__kmalloc_heap_start = ORIGIN(kmalloc_heap);
__kmalloc_heap_size = LENGTH(kmalloc_heap);
__kmalloc_heap_end = ORIGIN(kmalloc_heap) + LENGTH(kmalloc_heap);

__ipc_heap_start = ORIGIN(ipc_heap);
__ipc_heap_size = LENGTH(ipc_heap);

__thread_stacks_area_start = ORIGIN(thread_stacks);
__thread_stacks_area_size = LENGTH(thread_stacks);

__iobuf_heap_area_start = ORIGIN(iobuf_heap);
__iobuf_heap_area_size = LENGTH(iobuf_heap);

SECTIONS
{
	.bss.mem2 (NOLOAD) :
	{
		__bss2_start = . ;
		*(.bss.mem2)
		. = ALIGN(4);
		__bss2_end = . ;
	} >mem2

	.init : ALIGN(0x10)
	{
		*(.init)
		. = ALIGN(4);
	} > kernel : kernel

	.text : ALIGN(0x10)
	{
		*(EXCLUDE_FILE(aes* sha* hmac* iosc* keyring*) .text*)
		*(EXCLUDE_FILE(aes* sha* hmac* iosc* keyring*) .text.*)
		*(.gnu.warning)
		*(EXCLUDE_FILE(aes* sha* hmac* iosc* keyring*) .gnu.linkonce.t*)
		*(.glue_7)
		*(.glue_7t)
		. = ALIGN(4);
	} > kernel : kernel

	.rodata : ALIGN(4)
	{
		*(EXCLUDE_FILE(aes* sha* hmac* iosc* keyring*) .rodata)
		*all.rodata*(*)
		*(EXCLUDE_FILE(aes* sha* hmac* iosc* keyring*) .roda)
		*(EXCLUDE_FILE(aes* sha* hmac* iosc* keyring*) .rodata.*)
		*(EXCLUDE_FILE(aes* sha* hmac* iosc* keyring*) .gnu.linkonce.r*)
		. = ALIGN(4);
	} > kernel : rodata

	.data.sram (NOLOAD) : ALIGN(4)
	{
		__data_sram_start = . ;
		*(.data.sram)
		. = ALIGN(4);
		__data_sram_end = . ;
	} > sram : sram

	.data : ALIGN(0x40)
	{
		*(EXCLUDE_FILE(aes* sha* hmac* iosc* keyring*) .data)
		*(EXCLUDE_FILE(aes* sha* hmac* iosc* keyring*) .data.*)
		*(EXCLUDE_FILE(aes* sha* hmac* iosc* keyring*) .gnu.linkonce.d*)
		. = ALIGN(4);
	} > kernel : data

	.bss(NOLOAD) :
	{
		__bss_start = . ;
		*(EXCLUDE_FILE(aes* sha* hmac* iosc* keyring*) .dynbss)
		*(EXCLUDE_FILE(aes* sha* hmac* iosc* keyring*) .gnu.linkonce.b*)
		*(EXCLUDE_FILE(aes* sha* hmac* iosc* keyring*) .bss*)
		*(EXCLUDE_FILE(aes* sha* hmac* iosc* keyring*) COMMON)
		. = ALIGN(4);
		__bss_end = . ;
	}  > kernel : data

	.stack :
	{
		. = ALIGN(16);
		__stack_end = .;
		. = . +__stack_size;
		. = ALIGN(16);
		__stack_addr = .;
		__irqstack_end = .;
		. = . +__irqstack_size;
		. = ALIGN(16);
		__irqstack_addr = .;
		__excstack_end = .;
		. = . +__excstack_size;
		. = ALIGN(16);
		__excstack_addr = .;
		__ioscstack_end = .;
		. = . +__ioscstack_size;
		. = ALIGN(16);
		__ioscstack_addr = .;
	} > kernel : data

	.syscalls : ALIGN(0x10)
	{
		*(.syscalls)
		. = ALIGN(4);
	} > sram :svcs

	/DISCARD/ :
	{
		*(.ARM.attributes*)
		*(.ARM.exidx*)
		*(.ARM.extab*)
	}
}