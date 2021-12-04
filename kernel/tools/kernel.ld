/*
	mini - a Free Software replacement for the Nintendo/BroadOn IOS.
	linker script
	
Copyright (C) 2008, 2009	Hector Martin "marcan" <marcan@marcansoft.com>

# This code is licensed to you under the terms of the GNU GPL, version 2;
# see file COPYING or http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt
*/

OUTPUT_FORMAT("elf32-bigarm")
OUTPUT_ARCH(arm)
EXTERN(_start)
EXTERN(__ipc_info)
ENTRY(_start)

__stack_size = 0x100;
__irqstack_size = 0x100;
__excstack_size = 0x100;
__swistack_size = 0x100;

MEMORY {
	sram_data			: ORIGIN = 0xFFFE0000, LENGTH = 62K
	sram				: ORIGIN = 0xFFFF0000, LENGTH = 62K
	modules				: ORIGIN = 0x13400000, LENGTH = 4416K
	kernel_heap			: ORIGIN = 0x13850000, LENGTH = 128K
	modules_stack		: ORIGIN = 0x13AC0000, LENGTH = 256K
	mem2				: ORIGIN = 0x13F00000, LENGTH = 1M
}

__modules_area_start = ORIGIN(modules);
__modules_area_size = LENGTH(modules);
__modules_area_end = ORIGIN(modules) + LENGTH(modules);

__kernel_heap_ptr = ORIGIN(kernel_heap);
__kernel_heap_start = ORIGIN(kernel_heap);
__kernel_heap_size = LENGTH(kernel_heap);
__kernel_heap_end = ORIGIN(kernel_heap) + LENGTH(kernel_heap);

__modules_stack_area_start = ORIGIN(modules_stack);
__modules_stack_area_size = LENGTH(modules_stack);
__modules_stack_area_end = ORIGIN(modules_stack) + LENGTH(modules_stack);

__mem2_area_start = ORIGIN(mem2);
__mem2_area_size = LENGTH(mem2);
__mem2_area_end = ORIGIN(mem2) + LENGTH(mem2);

SECTIONS
{
	.bss.mem2 :
	{
		__bss2_start = . ;
		*(.bss.mem2)
		. = ALIGN(4);
		__bss2_end = . ;
	} >mem2

	.init :
	{
		*(.init)
		. = ALIGN(4);
	} >sram

	.text :
	{
		*(.text*)
		*(.text.*)
		*(.gnu.warning)
		*(.gnu.linkonce.t*)
		*(.glue_7)
		*(.glue_7t)
		. = ALIGN(4);
	} >sram

	.rodata :
	{
		*(.rodata)
		*all.rodata*(*)
		*(.roda)
		*(.rodata.*)
		*(.gnu.linkonce.r*)
		. = ALIGN(4);
	} >sram_data

	.data :
	{
		*(.data)
		*(.data.*)
		*(.gnu.linkonce.d*)
		. = ALIGN(4);
	} >sram_data

	.bss :
	{
		__bss_start = . ;
		*(.dynbss)
		*(.gnu.linkonce.b*)
		*(.bss*)
		*(COMMON)
		. = ALIGN(4);
		__bss_end = . ;
	} >sram_data

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
		__swistack_end = .;
		. = . +__swistack_size;
		. = ALIGN(16);
		__swistack_addr = .;
	} >sram_data

	/DISCARD/ :
	{
		*(.ARM.exidx*)
		*(.ARM.extab*)
	}
}


