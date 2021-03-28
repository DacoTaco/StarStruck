OUTPUT_FORMAT(elf32-bigarm)
OUTPUT_ARCH(arm)
EXTERN(_start)
ENTRY(_start)

__stack_size = 0x600;
__base_addr	= 0x13400000;

MEMORY {
	binary : ORIGIN = __base_addr, LENGTH = 11M
}

SECTIONS
{
	. = __base_addr;

	.init :
	{		
		KEEP (*(.init))
		. = ALIGN(4);
		KEEP (*(.crt0))
		. = ALIGN(4);
	} >binary
	
	.crt0	:
	{
		
	} >binary

	.text :
	{
		*(.text*)
		*(.text.*)
		*(.gnu.warning)
		*(.gnu.linkonce.t*)
		*(.glue_7)
		*(.glue_7t)
		. = ALIGN(4);
	} >binary

	.rodata :
	{
		*(.rodata)
		*all.rodata*(*)
		*(.roda)
		*(.rodata.*)
		*(.gnu.linkonce.r*)
		. = ALIGN(4);
	} >binary

	.data :
	{
		*(.data)
		*(.data.*)
		*(.gnu.linkonce.d*)
		. = ALIGN(4);
	} >binary

	.bss :
	{
		__bss_start = . ;
		*(.dynbss)
		*(.gnu.linkonce.b*)
		*(.bss*)
		*(COMMON)
		. = ALIGN(4);
		__bss_end = . ;
	} >binary
	
	.stack :
	{
		. = ALIGN(16);
		__stack_end = .;
		. = . +__stack_size;
		. = ALIGN(16);
		__stack_addr = .;
	} >binary

	/DISCARD/ :
	{
		*(.ARM.exidx*)
		*(.ARM.extab*)
	}
}