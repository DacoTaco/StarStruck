/*
	mini - a Free Software replacement for the Nintendo/BroadOn IOS.
	memory management, MMU, caches, and flushing

Copyright (C) 2008, 2009	Hector Martin "marcan" <marcan@marcansoft.com>

# This code is licensed to you under the terms of the GNU GPL, version 2;
# see file COPYING or http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt
*/

#ifndef __MEMORY_H__
#define __MEMORY_H__

#define MEM2_BASE				0x10000000
#define MEM2_PHY2VIRT(addr)		( (u32)(addr) | 0x80000000 )

#define MEM1_BASE				0x00000000
#define MEM1_END				(MEM1_BASE | 0x01800000 )

#define MEM1_MEM2PHYSICALSIZE	( MEM1_BASE | 0x3118 )
#define MEM1_MEM2SIMULATESIZE	( MEM1_BASE | 0x311C )
#define MEM1_MEM2BAT			( MEM1_BASE | 0x3120 )
#define MEM1_MEM2INITLOW		( MEM1_BASE | 0x3124 )
#define MEM1_MEM2INITHIGH		( MEM1_BASE | 0x3128 )
#define MEM1_IOSIPCLOW			( MEM1_BASE | 0x3130 )
#define MEM1_IOSIPCHIGH			( MEM1_BASE | 0x3134 )
#define MEM1_IOSHEAPLOW			( MEM1_BASE | 0x3148 )
#define MEM1_IOSHEAPHIGH		( MEM1_BASE | 0x314C )

//Access permissions
//we can have multiple APs per second level page, hence the formula to calculate the value for us
#define APX_VALUE(number, access)	((access & 0x03) << ((2+number)*2))
#define AP_VALUE(access)			APX_VALUE(3, access)
#define AP_ROM						0x00
#define AP_NOUSER					0x01
#define AP_ROUSER					0x02
#define AP_RWUSER					0x03

#include <types.h>

#include "memory/ahb.h"
#include "scheduler/threads.h"

typedef struct
{
	u32 PhysicalAddress;
	u32 VirtualAddress;
	u32 Size;
	u32 Domain;
	u32 AccessRights;
	u32 IsCached;
} MemorySection;

CHECK_OFFSET(MemorySection, 0x00, PhysicalAddress);
CHECK_OFFSET(MemorySection, 0x04, VirtualAddress);
CHECK_OFFSET(MemorySection, 0x08, Size);
CHECK_OFFSET(MemorySection, 0x0C, Domain);
CHECK_OFFSET(MemorySection, 0x10, AccessRights);
CHECK_OFFSET(MemorySection, 0x14, IsCached);
CHECK_SIZE(MemorySection, 0x18);

typedef struct
{
	u32 ProcessId;
	MemorySection MemorySection;
} ProcessMemorySection;

CHECK_OFFSET(ProcessMemorySection, 0x00, ProcessId);
CHECK_OFFSET(ProcessMemorySection, 0x04, MemorySection);
CHECK_SIZE(ProcessMemorySection, 0x1C);

typedef enum
{
	PageTable = 0,
	Unknown = 1,
	CoursePage = 2,
} KernelMemoryType;
	
s32 InitializeMemory(void);
void ProtectMemory(int enable, void *start, void *end);
void* KMalloc(u32 size);
u32 MapMemory(MemorySection* entry);
u32 VirtualToPhysical(u32 virtualAddress);
s32 CheckMemoryPointer(const void* ptr, s32 size, u32 type, s32 pid, s32 domainPid);
void DCInvalidateRange(const void* start, u32 size);
void DCFlushRange(const void *start, u32 size);
void DCFlushAll(void);
void ICInvalidateAll(void);
u32 TlbInvalidate(void);
void FlushMemory(void);

u32 GetControlRegister(void);
void SetControlRegister(u32 data);
u32 GetTranslationTableBaseRegister(void);
void SetTranslationTableBaseRegister(u32 data);
u32 GetDomainAccessControlRegister(void);
void SetDomainAccessControlRegister(u32 data);
u32 GetDataFaultStatusRegister(void);
void SetDataFaultStatusRegister(u32 data);
u32 GetInstructionFaultStatusRegister(void);
void SetInstructionFaultStatusRegister(u32 data);
u32 GetFaultAddressRegister(void);
void SetFaultAddressRegister(u32 data);

void mem_shutdown(void);
u32 dma_addr(void *);

#endif