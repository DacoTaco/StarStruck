
PHDRS
{
	__ModuleName___ro			PT_LOAD FLAGS((__PROCESSID__ << 0x14) + 0x00000005) /* Read | Execute */;
	__ModuleName___data			PT_LOAD FLAGS((__PROCESSID__ << 0x14) + 0x00000006) /* Read | Write */;
	__ModuleName___bss			PT_LOAD FLAGS((__PROCESSID__ << 0x14) + 0x00000006) /* Read */;
}

SECTIONS
{
	.__ModuleName__ __VIRTADDR__ : AT(__PHYSADDR__) ALIGN(4) 
    {
		KEEP (*__ModuleName___module.bin.o(.rodata.*module_bin));
		*(.rodata.__ModuleName___module_bin);
        . = ALIGN(0x1000);
    } :__ModuleName___ro

    .__ModuleName__.data : AT(LOADADDR(.__ModuleName__) + SIZEOF(.__ModuleName__)) ALIGN(0x1000)
    {
		KEEP (*__ModuleName___moduleData.bin.o(.rodata.*moduleData_bin));
		*(.rodata.__ModuleName___moduleData_bin);
        . = ALIGN(0x1000);
    } :__ModuleName___data

    .__ModuleName__.bss (NOLOAD) : AT(LOADADDR(.__ModuleName__.data) + SIZEOF(.__ModuleName__.data)) ALIGN(0x1000)
    {
		KEEP (*(.__ModuleName__.bss));
		. = __BSS_SIZE__;
        . = ALIGN(0x1000);
    } :__ModuleName___bss
}