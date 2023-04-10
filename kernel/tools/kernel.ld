/*
	starstruck - a Free Software reimplementation for the Nintendo/BroadOn IOS.
	syscalls - internal communications over software interrupts

	Copyright (C) 2023	DacoTaco

# This code is licensed to you under the terms of the GNU GPL, version 2;
# see file COPYING or http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt
*/

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
	phdrs		PT_PHDR PHDRS FLAGS(0x00F00000);
	headers		PT_LOAD PHDRS FLAGS(0x00F00000) AT(0x138F0000);
	crypto		PT_LOAD FLAGS(0x000000007) /* Read | Write | Execute */;
	sram		PT_LOAD FLAGS(0x000000006) /* Read | Write */;
	kernel		PT_LOAD FLAGS(0x000000005) /* Read | Execute */;
	rodata		PT_LOAD FLAGS(0x000000004) /* Read */;
	svcs		PT_LOAD FLAGS(0x000000004) /* Read */;
	data		PT_LOAD FLAGS(0x000000006) /* Read | Write */;
}

/*Sram is split up in several parts lol. FFFE0000 = 32K available. then there is 32K of unusable crap, followed by 64K of memory*/
MEMORY {
	sram				: ORIGIN = 0xFFFE0000, LENGTH = 32K
	kernel				: ORIGIN = 0xFFFF0000, LENGTH = 64K
	ipc_heap			: ORIGIN = 0x13600000, LENGTH = 128K
	kmalloc_heap		: ORIGIN = 0x13850000, LENGTH = 128K
	crypto				: ORIGIN = 0x13A70000, LENGTH = 128K
	thread_stacks		: ORIGIN = 0x13AC0000, LENGTH = 128K
	iobuf_heap			: ORIGIN = 0x13C40000, LENGTH = 512K
	headers_heap		: ORIGIN = 0x138F0000, LENGTH = 768K
	mem2				: ORIGIN = 0x13F00000, LENGTH = 1M
}

__stack_size = 0x300;
__irqstack_size = 0x060;
__excstack_size = 0x100;

__kmalloc_heap_start = ORIGIN(kmalloc_heap);
__kmalloc_heap_size = LENGTH(kmalloc_heap);
__kmalloc_heap_end = ORIGIN(kmalloc_heap) + LENGTH(kmalloc_heap);

__ipc_heap_start = ORIGIN(ipc_heap);
__ipc_heap_size = LENGTH(ipc_heap);

__thread_stacks_area_start = ORIGIN(thread_stacks);
__thread_stacks_area_size = LENGTH(thread_stacks);

__iobuf_heap_area_start = ORIGIN(iobuf_heap);
__iobuf_heap_area_size = LENGTH(iobuf_heap);

__headers_size = LENGTH(headers_heap);
__headers_addr = ORIGIN(headers_heap);

__crypto_addr = ORIGIN(crypto);
__crypto_size = LENGTH(crypto);

SECTIONS
{
	.bss.mem2 (NOLOAD) :
	{
		__bss2_start = . ;
		*(.bss.mem2)
		. = ALIGN(4);
		__bss2_end = . ;
	} >mem2

	.data.sram :
	{
		__data_sram_start = . ;
		*(.data.sram)
		. = ALIGN(4);
		__data_sram_end = . ;
	} > sram : sram

	.init :
	{
		*(.init)
		. = ALIGN(4);
	} > kernel : kernel

	.text :
	{
		*(EXCLUDE_FILE(aes* sha* hmac* keyring*) .text*)
		*(EXCLUDE_FILE(aes* sha* hmac* keyring*) .text.*)
		*(.gnu.warning)
		*(EXCLUDE_FILE(aes* sha* hmac* keyring*) .gnu.linkonce.t*)
		*(.glue_7)
		*(.glue_7t)
		. = ALIGN(4);
	} > kernel : kernel

	.rodata :
	{
		*(EXCLUDE_FILE(aes* sha* hmac* keyring*) .rodata)
		*all.rodata*(*)
		*(.roda)
		*(EXCLUDE_FILE(aes* sha* hmac* keyring*) .rodata.*)
		*(EXCLUDE_FILE(aes* sha* hmac* keyring*) .gnu.linkonce.r*)
		. = ALIGN(4);
	} > kernel : rodata

	.data :
	{
		*(EXCLUDE_FILE(aes* sha* hmac* keyring*) .data)
		*(EXCLUDE_FILE(aes* sha* hmac* keyring*) .data.*)
		*(EXCLUDE_FILE(aes* sha* hmac* keyring*) .gnu.linkonce.d*)
		. = ALIGN(4);
	} > kernel : data

	.bss(NOLOAD) :
	{
		__bss_start = . ;
		*(EXCLUDE_FILE(aes* sha* hmac* keyring*) .dynbss)
		*(EXCLUDE_FILE(aes* sha* hmac* keyring*) .gnu.linkonce.b*)
		*(EXCLUDE_FILE(aes* sha* hmac* keyring*) .bss*)
		*(EXCLUDE_FILE(aes* sha* hmac* keyring*) COMMON)
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
	} > kernel : data

	.syscalls :
	{
		*(.syscalls)
		. = ALIGN(4);
	} > sram :svcs

	.headers (NOLOAD) :
	{
		*(.headers);
		KEEP( *(.headers) );
		. = ALIGN(16);
	} >headers_heap :headers

	.modules __headers_addr + SIZEOF_HEADERS - 0x34:
	{
		*(.modules);
		KEEP( *(.modules) );
		. = __headers_size - SIZEOF_HEADERS + 0x34;
		. = ALIGN(16);
	} :headers

	.crypto.bss (NOLOAD):
	{
		aes* sha* hmac* keyring* (.dynbss)
		aes* sha* hmac* keyring* (.gnu.linkonce.b*)
		aes* sha* hmac* keyring* (.bss*)
		aes* sha* hmac* keyring* (COMMON)
		. = ALIGN(4);
	} > crypto :crypto

	.crypto :
	{
		*(.crypto.text*)
		aes* sha* hmac* keyring* (.text .text.* .gnu.linkonce.t*)
		. = ALIGN(4);
		*(.crypto.data*)
		aes* sha* hmac* keyring* (.rodata)
		aes* sha* hmac* keyring* (.roda)
		aes* sha* hmac* keyring* (.rodata.*)
		aes* sha* hmac* keyring* (.data)
		aes* sha* hmac* keyring* (.data.*)
		aes* sha* hmac* keyring* (.gnu.linkonce.d*)
		. = ALIGN(4);
	} > crypto :crypto

	/DISCARD/ :
	{
		*(.ARM.exidx*)
		*(.ARM.extab*)
	}
}