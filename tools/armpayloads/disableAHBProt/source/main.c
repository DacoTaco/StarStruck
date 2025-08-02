void DisableAHBProt()
{
 //disable AHBPROT of PPC
	unsigned int *ptr = (unsigned int *)0x0D800064;
	*ptr = *ptr | 0x80000DFE;

 //disable HW_MEMMIRR
	ptr = (unsigned int *)0x0D800060;
	*ptr = *ptr | 0x00000008;

 //set MEM_PROT_REG
	unsigned short *ptr2 = (unsigned short *)0x0D804202;
	*ptr2 = 0x0000;
}