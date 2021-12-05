/*
	mini - a Free Software replacement for the Nintendo/BroadOn IOS.
	memory management, MMU, caches, and flushing

Copyright (C) 2008, 2009	Hector Martin "marcan" <marcan@marcansoft.com>
Copyright (C) 2021			DacoTaco

# This code is licensed to you under the terms of the GNU GPL, version 2;
# see file COPYING or http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt
*/

#include <ios/processor.h>
#include <ios/gecko.h>

#include "core/hollywood.h"
#include "memory/memory.h"
#include "interrupt/irq.h"
#include "utils.h"

//#define NO_CACHES

#define LINESIZE 			0x20
#define CACHESIZE 			0x4000

#define PAGE_ENTRY(x)				((x)>>0x14)
#define COURSEPAGE_ENTRY_VALUE(x)	((x << 0x0C) >> 0x18)
#define PAGE_DOMAIN(x)				((x)<<5)
#define PAGE_MASK					0x13
#define SECTION_SECTION				0x01
#define COURSE_SECTION				0x02
#define PAGE_TYPE_MASK				( COURSE_SECTION | SECTION_SECTION )
#define SECTION_PAGE				0x12
#define COURSE_PAGE					0x11

#define	NONBUFFERABLE				0x000
#define	BUFFERABLE					0x004
#define	WRITETHROUGH_CACHE			0x008
#define	WRITEBACK_CACHE				0x00C

//Domain bits
#define DOMAIN_VALUE(domain, value)	((value & 0x03) << (domain * 2))
#define DOMAIN_NOACCESS				0x00
#define DOMAIN_CLIENT				0x01
#define DOMAIN_RESERVED				0x02
#define DOMAIN_MANAGER				0x03

//Access permissions
//we can have multiple APs per second level page, hence the formula to calculate the value for us
#define APX_VALUE(number, access)	((access & 0x03) << ((2+number)*2))
#define AP_VALUE(access)			APX_VALUE(3, access)
#define AP_ROM						0x00
#define AP_NOUSER					0x01
#define AP_ROUSER					0x02
#define AP_RWUSER					0x03

//CR bits
#define CR_MMU						(1 << 0)
#define CR_DCACHE					(1 << 2)
#define CR_ICACHE					(1 << 12)

void _dc_inval_entries(void *start, int count);
void _dc_flush_entries(const void *start, int count);
void _dc_flush(void);
void _ic_inval(void);
void _dc_inval(void);

//the variables defined in the linker script are variables containing the value of the address, not a pointer to the address
//but we need pointers, so casting the variables to an array (so its considered an array starting at the address) and then to pointer seems to work
extern const u32 __kernel_heap_end[];
extern const u32 __kernel_heap_ptr[];
extern const u32 __kernel_heap_size;
u8* heapStart = (u8*)__kernel_heap_ptr;
const u8* heapEnd = (u8*)__kernel_heap_end;

//the pagetable for the mmu's translation table base register MUST be 0x4000 (16KB aligned) !
//this is (kinda) ensured by having the heap 16KB aligned and this being the first malloc
u32* MemoryTranslationTable = NULL;
u32 DomainAccessControlTable[MAX_PROCESSES];
u32* HardwareRegistersAccessTable[MAX_PROCESSES];

static MemorySection KernelMemoryMaps[] = 
{
	// physical		virtual		size		domain		access		unknown
	{ 0xFFF00000, 0xFFF00000, 0x00100000, 0x0000000F, AP_NOUSER, 0x00000001 }, //Starlet sram 
	{ 0x13A70000, 0x13A70000, 0x00020000, 0x0000000F, AP_NOUSER, 0x00000001 }, //    ???
	{ 0x13AC0000, 0x13AC0000, 0x00020000, 0x0000000F, AP_NOUSER, 0x00000001 }, //Thread stacks
	{ 0x0D800000, 0x0D800000, 0x000D0000, 0x0000000F, AP_ROUSER, 0x00000000 }, //Hardware registers(AHB mirror)
	{ 0x00000000, 0x00000000, 0x04000000, 0x00000008, AP_RWUSER, 0x00000001 }, //    ???
	{ 0x10000000, 0x10000000, 0x03600000, 0x00000008, AP_RWUSER, 0x00000001 }, //MEM2
	{ 0x13870000, 0x13870000, 0x00030000, 0x0000000F, AP_RWUSER, 0x00000000 }, //    ???
	{ 0x13600000, 0x13600000, 0x00020000, 0x0000000F, AP_RWUSER, 0x00000001 }, //    ???
	{ 0x13C40000, 0x13C40000, 0x00080000, 0x0000000F, AP_RWUSER, 0x00000001 }, //IOBuf ?
	{ 0x13850000, 0x13850000, 0x00020000, 0x0000000F, AP_ROUSER, 0x00000000 }, //Kernel heap
	{ 0x138F0000, 0x138F0000, 0x000C0000, 0x0000000F, AP_RWUSER, 0x00000001 }, //Module elf??
	{ 0x13F00000, 0x13F00000, 0x00100000, 0x0000000F, AP_RWUSER, 0x00000001 }, //Todo : delete this, this is temp while developing to store data like boot2
};

static ProcessMemorySection HWRegistersMemoryMaps[] = 
{
	  // 		physical	virtual		size		domain		access		unknown
	{0x00,	{ 0x0D000000, 0x0D000000, 0x000D0000, 0x0000000F, AP_NOUSER, 0x00000000 }},
	{0x02,	{ 0x0D010000, 0x0D010000, 0x00010000, 0x0000000F, AP_RWUSER, 0x00000000 }},
	{0x03,	{ 0x0D806000, 0x0D006000, 0x00001000, 0x0000000F, AP_RWUSER, 0x00000000 }},
	{0x06,	{ 0x0D040000, 0x0D040000, 0x00030000, 0x0000000F, AP_RWUSER, 0x00000000 }},
	{0x07,	{ 0x0D070000, 0x0D070000, 0x00010000, 0x0000000F, AP_RWUSER, 0x00000000 }},
	{0x0B,	{ 0x0D080000, 0x0D080000, 0x00010000, 0x0000000F, AP_RWUSER, 0x00000000 }},
};

void dc_flushrange(const void *start, u32 size)
{
	u32 cookie = irq_kill();
	if(size > 0x4000) {
		_dc_flush();
	} else {
		void *end = ALIGN_FORWARD(((u8*)start) + size, LINESIZE);
		start = ALIGN_BACKWARD(start, LINESIZE);
		_dc_flush_entries(start, (end - start) / LINESIZE);
	}
	flush_memory();
	_ahb_flush_from(AHB_1);
	irq_restore(cookie);
}

void dc_invalidaterange(void *start, u32 size)
{
	u32 cookie = irq_kill();
	void *end = ALIGN_FORWARD(((u8*)start) + size, LINESIZE);
	start = ALIGN_BACKWARD(start, LINESIZE);
	_dc_inval_entries(start, (end - start) / LINESIZE);
	AhbFlushTo(AHB_STARLET);
	irq_restore(cookie);
}

void dc_flushall(void)
{
	u32 cookie = irq_kill();
	_dc_flush();
	flush_memory();
	_ahb_flush_from(AHB_1);
	irq_restore(cookie);
}

void ic_invalidateall(void)
{
	u32 cookie = irq_kill();
	_ic_inval();
	AhbFlushTo(AHB_STARLET);
	irq_restore(cookie);
}

void mem_setswap(int enable)
{
	u32 d = read32(HW_MEMMIRR);

	if((d & 0x20) && !enable)
		write32(HW_MEMMIRR, d & ~0x20);
	if((!(d & 0x20)) && enable)
		write32(HW_MEMMIRR, d | 0x20);
}

u32 dma_addr(void *p)
{
	u32 addr = (u32)p;

	switch(addr>>20) {
		case 0xfff:
		case 0x0d4:
		case 0x0dc:
			if(read32(HW_MEMMIRR) & 0x20) {
				addr ^= 0x10000;
			}
			addr &= 0x0001FFFF;
			addr |= 0x0d400000;
			break;
	}
	//gecko_printf("DMA to %p: address %08x\n", p, addr);
	return addr;
}

void mem_shutdown(void)
{
	u32 cookie = irq_kill();
	_dc_flush();
	flush_memory();
	u32 cr = get_cr();
	cr &= ~(CR_MMU | CR_DCACHE | CR_ICACHE); //disable ICACHE, DCACHE, MMU
	set_cr(cr);
	_ic_inval();
	_dc_inval();
	tlb_invalidate();
	irq_restore(cookie);
}

void* kmalloc(KernelMemoryType type)
{
	u8* ptr = heapStart;
	u32 size = 0;
	
	switch(type)
	{
		case PageTable:
			size = 0x4000;
			break;
		case Unknown:
			size = 0x1000;
			break;
		case CoursePage:
			size = 0x400;
			break;
		default:
			return NULL;
	}

	u8* ptrEnd = ptr + size;
	
	if(ptrEnd > heapEnd)
	{
		heapStart = ptrEnd;
		return NULL;
	}
	
	size = (ptrEnd - ptr);
	heapStart = ptrEnd;
	memset8(ptr, 0, size);
	return ptr;
}

s32 MapMemoryAsSection(MemorySection* memorySection)
{
	/*Example of a mapping : 
	 virtual address : 0xFFF00000
	 physical address : 0xFFF00000
	 size : 0x00100000
	 domain : 0x0F
	 accessRights : 0x01;
	 unknown : 0x01;
	 page table[FFF ( 0xFFF00000 >> 20 )] = ( 0x1E | 0xFFF00000 | 0x400 | 0x1E0 )
	 aka page[0xFFF] (0x13853FFC) = 0xFFF005FE ( b1111.1111.1111.0000.0000.0101.1111.1110 )
	 aka regular section entry, writeback/cache, domain 0x0F, read, and redirects to physical address 0xFFFxxxxx
	*/
	
	if(memorySection == NULL)
		return -1;
	
	//either map section as regular section, or section with writeback cache & buffer enabled
	u32 translationBase = SECTION_PAGE;
	if(memorySection->unknown != 0)
		translationBase |= WRITEBACK_CACHE;
	
	u32* page = &MemoryTranslationTable[PAGE_ENTRY(memorySection->virtualAddress)];
	*page = translationBase | (memorySection->physicalAddress & 0xFFF00000) | AP_VALUE(memorySection->accessRights) | PAGE_DOMAIN(memorySection->domain);
	dc_flushrange(page, 4);
	
	memorySection->size = memorySection->size - 0x100000;
	memorySection->physicalAddress = memorySection->physicalAddress + 0x100000;
	memorySection->virtualAddress = memorySection->virtualAddress + 0x100000;
	return 0;
}

//In all honesty, i don't full understand what it is doing in here...
s32 MapMemoryAsCoursePage(MemorySection* memorySection, u8 mode)
{
	/*
		Example of a mapping : 
		virtual address : 0x13A70000
		physical address : 0x13A70000
		size : 0x00020000
		domain : 0x0F
		accessRights : 0x01;
		unknown : 0x01;
		
		first the page is allocated using kmalloc (mem range > 0x13854000)
		after that its saved as a course page in our translation table :
			page[0x13A] = ( 0x13854000 & 0xFFFFFC00 ) | domain << 5 (0x01E0) | COURSE_PAGE(0x11)
			page[0x13A] = 0x138541F1
			0x138504E8 = 0x138541F1
			
		that takes care of the level 1 mapping. 
		
	*/
	
	if(memorySection == NULL)
		return -1;
	
	u32** entry = (u32**)&MemoryTranslationTable[PAGE_ENTRY(memorySection->virtualAddress)];
	u32* pageValue = *entry;
	if(pageValue == NULL)
	{
		pageValue = (u32*)kmalloc(CoursePage);
		if(pageValue == NULL)
			return -22;
		
		*entry = (u32*)((0xFFFFFC00 & (u32)pageValue) | PAGE_DOMAIN(memorySection->domain) | COURSE_PAGE);
	}
	else
	{
		if(mode == 0 && (PAGE_MASK & (u32)pageValue) != COURSE_PAGE)
			return -4;
		else if (mode != 0)
		{
			pageValue = (u32*)((0xFFFFFC00 & (u32)pageValue) | PAGE_DOMAIN(memorySection->domain) | COURSE_PAGE);
			*entry = pageValue;
		}
		
		pageValue = (u32*)(0xFFFFFC00 & (u32)pageValue);
	}
	
	if(mode == 0 && pageValue[COURSEPAGE_ENTRY_VALUE(memorySection->virtualAddress)] != 0)
		return -4;
	
	u32 accessRights = memorySection->accessRights;
	u32 type = COURSE_SECTION;
	if(memorySection->unknown != 0)
		type |= WRITEBACK_CACHE;

	pageValue[COURSEPAGE_ENTRY_VALUE(memorySection->virtualAddress)] = (memorySection->physicalAddress & 0xFFFFF000) | type | APX_VALUE(3, accessRights) | APX_VALUE(2, accessRights) | APX_VALUE(1, accessRights) | APX_VALUE(0, accessRights);
	dc_flushrange(pageValue, 0x1000);
	memorySection->size -= 0x1000;
	memorySection->physicalAddress += 0x1000;
	memorySection->virtualAddress += 0x1000;
	return 0;
}

//basically mmap
u32 MapMemory(MemorySection* entry)
{
	if(entry == NULL)
		return -1;
	
	MemorySection memorySection;
	memcpy32(&memorySection, entry, sizeof(MemorySection));
	
	u32 ret = 0;
	while(ret == 0 && memorySection.size > 0)
	{
		if(memorySection.size == 0)
			break;
		
		//page table entries on arm are either 1MB (section) or at least 4KB (level 2 section)
		if((memorySection.virtualAddress & 0xFFFFF) == 0 && (memorySection.physicalAddress & 0xFFFFF) == 0 && memorySection.size > 0xFFFFF)
			ret = MapMemoryAsSection(&memorySection);
		else if( ((memorySection.virtualAddress & 0xFFF) != 0 || (memorySection.physicalAddress & 0xFFF) != 0) || memorySection.size < 0x1000)
		{
			ret = -4;
			break;
		}
		else
		{
			ret = MapMemoryAsCoursePage(&memorySection, 0);
		}
	}
		
	flush_memory();
	tlb_invalidate();
	return ret;
}

s32 MapHardwareRegisters()
{
	u32** page = (u32**) &MemoryTranslationTable[0xD0];
	u32 index = 0;
	u32 ret = 0;
	
	while(ret == 0)
	{
		if(index >= sizeof(HWRegistersMemoryMaps)/sizeof(HWRegistersMemoryMaps[0]))
			break;

		ret = MapMemory(&HWRegistersMemoryMaps[index].memorySection);
		if(ret != 0)
			break;
		
		HardwareRegistersAccessTable[HWRegistersMemoryMaps[index].processId] = *page;
		u32* pageValue = (u32*)(((u32)*page) & 0xFFFFFC00);
		for(int i = 0; i < 0x100; i++)
		{
			if(*pageValue == 0)
				*pageValue = (i * 0x1000) + (0x0D000000 | SECTION_PAGE | PAGE_DOMAIN(0x0A) | AP_VALUE(AP_NOUSER));
			
			pageValue += 1;
		}
		
		index += 1;
		*page = 0;
	}
	
	//fill in some gaps?
	HardwareRegistersAccessTable[4] = HardwareRegistersAccessTable[6];
	HardwareRegistersAccessTable[5] = HardwareRegistersAccessTable[6];
	
	//set defaults to PID 0's access rights
	for(int i = 0; i < MAX_PROCESSES; i++)
	{
		if(HardwareRegistersAccessTable[i] == NULL)
			HardwareRegistersAccessTable[i] = HardwareRegistersAccessTable[0];
	}
	
	//set the access rights and return
	*page = HardwareRegistersAccessTable[0];	
	return ret;
}

u32 VirtualToPhysical(u32 virtualAddress)
{
	u32 pageEntry = MemoryTranslationTable[PAGE_ENTRY(virtualAddress)];
	u32 physicalAddress = 0;
	
	if((pageEntry & PAGE_MASK) == SECTION_PAGE)
		physicalAddress = (virtualAddress & 0xFFFFF) | ( pageEntry & 0xFFF00000);
	else if((pageEntry & PAGE_MASK) == COURSE_PAGE)
	{
		u32 page = (COURSEPAGE_ENTRY_VALUE(virtualAddress) * 4) + (pageEntry & 0xFFFFFC00);
		if((page & PAGE_TYPE_MASK) == COURSE_SECTION)
			physicalAddress = (virtualAddress & 0xFFF) | (page & 0xFFFFF000);
	}
	
	if(physicalAddress < 0xFFFE0000)
		return physicalAddress;
	
	u32 offset = (physicalAddress < 0xFFFF0000) ? 0x0D430000 : 0x0D410000;
	return physicalAddress + offset;
}

s32 CheckMemoryBlock(u8* ptr, u32 type, s32 pid, s32 domainPid, u32* blockSize)
{
	u32 pageEntry = MemoryTranslationTable[PAGE_ENTRY((u32)ptr)];
	u32 pageType = pageEntry & PAGE_MASK;
	u32 AccessPermissionsValue = 0;
	
	if(pageType == COURSE_PAGE)
	{
		*blockSize = 0x1000;
		u32 page = (COURSEPAGE_ENTRY_VALUE((u32)ptr) * 4) + (pageEntry & 0xFFFFFC00);
		if((page & PAGE_TYPE_MASK) != COURSE_SECTION)
			goto return_error;
		AccessPermissionsValue = page << 0x1A;		
	}
	else if(pageType == SECTION_PAGE)
	{
		*blockSize = 0x100000;
		AccessPermissionsValue = pageEntry << 0x14;
	}
	else
		goto return_error;
	
	//get the domain bits (b0000.0000.0000.0000.0000.000x.xxx0.0000) of the pageEntry and shift them left with 1 bit
	//result:                                           ^.^^^0
	u32 pageDomain = ((pageEntry << 0x17) >> 0x1C) << 1;
	u32 domainAccess = (DomainAccessControlTable[pid] >> pageDomain) & 3;
	u32 domainAccess2 = (DomainAccessControlTable[domainPid] >> pageDomain) & 3;
	if(domainAccess != 1 || domainAccess2 != 1)
		goto return_error;
	
	if(type == 4 && (AccessPermissionsValue >> 0x1E) == 3)
		return 0;
	else if(((AccessPermissionsValue >> 0x1E) -2) < 2)
		return 0;
	
return_error:
	gecko_printf("failed pointer check: 0x%08X\n", (u32)ptr);
	return -1;
}

s32 CheckMemoryPointer(void* ptr, s32 size, u32 type, s32 pid, s32 domainPid)
{
	if(pid == 0)
		return 0;
	
	s32 ret = 0;
	u32 blockSize;
	u8* startAddress = (u8*)ptr;
	u8* endAddress = ptr + size;
	while(startAddress < endAddress)
	{
		ret = CheckMemoryBlock(startAddress, type, pid, domainPid, &blockSize);
		if(ret != 0)
			return ret;
		
		//align to the next block
		startAddress = (u8*)(((u32)startAddress + blockSize) & -blockSize);
	}
	
	return ret;
}

void ProtectMemory(int enable, void *start, void *end)
{
	write16(MEM_PROT, enable?1:0);
	write16(MEM_PROT_START, (((u32)start) & 0xFFFFFFF) >> 12);
	write16(MEM_PROT_END, (((u32)end) & 0xFFFFFFF) >> 12);
	udelay(10);
}

s32 InitiliseMemory(void)
{
	u32 cr;
	s32 ret = 0;
	u32 cookie = irq_kill();

	gecko_printf("MEM: cleaning up\n");

	//Disable MMU+Cache & invalidate all caches & tlb
	set_cr(get_cr() & ~(CR_DCACHE | CR_MMU | CR_ICACHE));
	_ic_inval();
	_dc_inval();
	tlb_invalidate();

	memset32(heapStart, 0, heapEnd - heapStart);
	gecko_printf("MEM: mapping sections\n");
	MemoryTranslationTable = (u32*)kmalloc(PageTable);	
	if(MemoryTranslationTable == NULL)
	{
		ret = -0x16;
		goto ret_init;
	}
	
	for(u32 i = 0; i < (sizeof(KernelMemoryMaps)/sizeof(KernelMemoryMaps[0])); i++)
	{
		MemorySection* section = &KernelMemoryMaps[i];
		ret = MapMemory(section);
		if(ret < 0)
			goto ret_init;
	}
	
	//Ios also maps the registers/mirror with certain access for each process
	ret = MapHardwareRegisters();
	if(ret != 0)
		goto ret_init;
	
	//init all dacr values for all processes
	//default is domain no access besides domain 8 & 15 (client) -> 0x40010000;
	for(s32 i = 0; i <= 0x13; i++)
		DomainAccessControlTable[i] = DOMAIN_VALUE(8, DOMAIN_CLIENT) | DOMAIN_VALUE(15, DOMAIN_CLIENT);
	
	DomainAccessControlTable[0] = 0x55555555; //PID 0 = client access in all domains
	
	//give a few processes client access to their own domain. PID 1 to domain 1, PID 2 to domain 2, etc etc
	DomainAccessControlTable[1] |= DOMAIN_VALUE(1, DOMAIN_CLIENT);
	DomainAccessControlTable[2] |= DOMAIN_VALUE(2, DOMAIN_CLIENT);
	DomainAccessControlTable[3] |= DOMAIN_VALUE(3, DOMAIN_CLIENT);
	DomainAccessControlTable[7] |= DOMAIN_VALUE(7, DOMAIN_CLIENT);
	DomainAccessControlTable[14] |= DOMAIN_VALUE(14, DOMAIN_CLIENT);
	
	//PID 19 is a bit different, it has access to domain 9?
	DomainAccessControlTable[19] |= DOMAIN_VALUE(9, DOMAIN_CLIENT);
	
	//PID 15 is also special, it only gets access to domain 8
	DomainAccessControlTable[15] = DOMAIN_VALUE(8, DOMAIN_CLIENT);
	
	//setup memory registers
	set_dfsr(0);
	set_ifsr(0);
	set_far(0);
	set_ttbr((u32)MemoryTranslationTable); //configure translation table
	set_dacr(DomainAccessControlTable[0]);

	//drain buffer & invalidate tlb
	flush_memory();
	tlb_invalidate();
	
	cr = get_cr();

#ifndef NO_CACHES
	gecko_printf("MEM: enabling caches & MMU\n");
	cr |= CR_DCACHE | CR_ICACHE | CR_MMU;
#else
	gecko_printf("MEM: enabling MMU\n");
	cr |= CR_MMU;
#endif

	set_cr(cr);
	gecko_printf("MEM: init done\n");
ret_init:
	if(ret < 0)
		gecko_printf("failed to init memory : %d\n", ret);

	irq_restore(cookie);
	return ret;
}
