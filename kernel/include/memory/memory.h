/*
	mini - a Free Software replacement for the Nintendo/BroadOn IOS.
	memory management, MMU, caches, and flushing

Copyright (C) 2008, 2009	Hector Martin "marcan" <marcan@marcansoft.com>

# This code is licensed to you under the terms of the GNU GPL, version 2;
# see file COPYING or http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt
*/

#ifndef __MEMORY_H__
#define __MEMORY_H__

#define MEM2_PHY2VIRT(addr)		( addr | 0x80000000 )
#define MEM1_BASE				0x00000000

#define MEM1_MEM2PHYSICALSIZE	( MEM1_BASE | 0x3118 )
#define MEM1_MEM2SIMULATESIZE	( MEM1_BASE | 0x311C )
#define MEM1_MEM2BAT			( MEM1_BASE | 0x3120 )
#define MEM1_MEM2INITLOW		( MEM1_BASE | 0x3124 )
#define MEM1_MEM2INITHIGH		( MEM1_BASE | 0x3128 )
#define MEM1_IOSHEAPLOW			( MEM1_BASE | 0x3130 )
#define MEM1_IOSHEAPHIGH		( MEM1_BASE | 0x3134 )
#define MEM1_3148				( MEM1_BASE | 0x3148 )
#define MEM1_314C				( MEM1_BASE | 0x314C )

#include <types.h>

#include "memory/ahb.h"
#include "scheduler/threads.h"

typedef struct
{
	u32 physicalAddress;
	u32 virtualAddress;
	u32 size;
	u32 domain;
	u32 accessRights;
	u32 unknown;
} MemorySection;

CHECK_OFFSET(MemorySection, 0x00, physicalAddress);
CHECK_OFFSET(MemorySection, 0x04, virtualAddress);
CHECK_OFFSET(MemorySection, 0x08, size);
CHECK_OFFSET(MemorySection, 0x0C, domain);
CHECK_OFFSET(MemorySection, 0x10, accessRights);
CHECK_OFFSET(MemorySection, 0x14, unknown);
CHECK_SIZE(MemorySection, 0x18);

typedef struct
{
	u32 processId;
	MemorySection memorySection;
} ProcessMemorySection;

CHECK_OFFSET(ProcessMemorySection, 0x00, processId);
CHECK_OFFSET(ProcessMemorySection, 0x04, memorySection);
CHECK_SIZE(ProcessMemorySection, 0x1C);

typedef enum
{
	PageTable = 0,
	Unknown = 1,
	CoursePage = 2,
} KernelMemoryType;

extern u32* MemoryTranslationTable;
extern u32 DomainAccessControlTable[MAX_PROCESSES];
extern u32* HardwareRegistersAccessTable[MAX_PROCESSES];
	
s32 InitiliseMemory(void);
void ProtectMemory(int enable, void *start, void *end);
u32 MapMemory(MemorySection* entry);
u32 VirtualToPhysical(u32 virtualAddress);
s32 CheckMemoryPointer(void* ptr, s32 size, u32 type, s32 pid, s32 domainPid);
void DCInvalidateRange(void* start, u32 size);
void DCFlushRange(void *start, u32 size);
void DCFlushAll(void);
void ICInvalidateAll(void);

void mem_shutdown(void);

u32 dma_addr(void *);
u32 tlb_invalidate(void);
void flush_memory(void);

u32 get_cr(void);
u32 get_ttbr(void);
u32 get_dacr(void);
u32 get_dfsr(void);
u32 get_ifsr(void);
u32 get_far(void);

void set_cr(u32 data);
void set_ttbr(u32 data);
void set_dacr(u32 data);
void set_dfsr(u32 data);
void set_ifsr(u32 data);
void set_far(u32 data);

#endif

