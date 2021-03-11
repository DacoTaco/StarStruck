OUTPUT_FORMAT(elf32-bigarm)
OUTPUT_ARCH(arm)
EXTERN(_start)
ENTRY(_start)

__base_addr	= 0x13400000;

MEMORY {
	binary : ORIGIN = __base_addr, LENGTH = 11M
}

SECTIONS
{
	. = __base_addr;
	
/*	.header :
	{
		LONG(0xB16B00B5);
		LONG(0x53746172);
	} >header*/	

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
	
	.rodata.mem2 :
	{
		*(.rodata.mem2)
		. = ALIGN(4);
	} >binary

	.data.mem2 :
	{
		*(.data.mem2)
		. = ALIGN(4);
	} >binary

	.bss.mem2 :
	{
		__bss2_start = . ;
		*(.bss.mem2)
		. = ALIGN(4);
		__bss2_end = . ;
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

	/DISCARD/ :
	{
		*(.ARM.exidx*)
		*(.ARM.extab*)
	}

}