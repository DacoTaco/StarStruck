/*
	mini - a Free Software replacement for the Nintendo/BroadOn IOS.
	memory management, MMU, caches, and flushing

Copyright (C) 2008, 2009	Hector Martin "marcan" <marcan@marcansoft.com>

# This code is licensed to you under the terms of the GNU GPL, version 2;
# see file COPYING or http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt
*/

#ifndef __MEMORY_H__
#define __MEMORY_H__

#define MEM2_BASE                 0x10000000
#define MEM2_PHY2VIRT(addr)       ((u32)(addr) | 0x80000000)

#define MEM1_BASE                 0x00000000
#define MEM1_END                  (MEM1_BASE | 0x01800000)

#define MEM1_MEMORYSIZE           (MEM1_BASE | 0x0028)
#define MEM1_HEAPLOW              (MEM1_BASE | 0x0030)
#define MEM1_HEAPHIGH             (MEM1_BASE | 0x0034)
#define MEM1_0080                 (MEM1_BASE | 0x0080)
#define MEM1_SIMMEMORYSIZE        (MEM1_BASE | 0x00F0)
#define MEM1_30F8                 (MEM1_BASE | 0x30F8)
#define MEM1_EXCEPTIONVECTOR      (MEM1_BASE | 0x3100)
#define MEM1_3114                 (MEM1_BASE | 0x3114)
#define MEM1_MEM2PHYSICALSIZE     (MEM1_BASE | 0x3118)
#define MEM1_MEM2SIMULATESIZE     (MEM1_BASE | 0x311C)
#define MEM1_MEM2BAT              (MEM1_BASE | 0x3120)
#define MEM1_MEM2INITLOW          (MEM1_BASE | 0x3124)
#define MEM1_MEM2INITHIGH         (MEM1_BASE | 0x3128)
#define MEM1_312C                 (MEM1_BASE | 0x312C)
#define MEM1_IOSIPCLOW            (MEM1_BASE | 0x3130)
#define MEM1_IOSIPCHIGH           (MEM1_BASE | 0x3134)
#define MEM1_CPUVERSION           (MEM1_BASE | 0x3138)
#define MEM1_313C                 (MEM1_BASE | 0x3138)
#define MEM1_IOSVERSION           (MEM1_BASE | 0x3140)
#define MEM1_IOSBUILDDATE         (MEM1_BASE | 0x3144)
#define MEM1_IOSHEAPLOW           (MEM1_BASE | 0x3148)
#define MEM1_IOSHEAPHIGH          (MEM1_BASE | 0x314C)
#define MEM1_3150                 (MEM1_BASE | 0x3150)
#define MEM1_3154                 (MEM1_BASE | 0x3154)
#define MEM1_GDDRVENDORCODE       (MEM1_BASE | 0x3158)
#define MEM1_LOADMETHOD           (MEM1_BASE | 0x315C)
#define MEM1_INITSEMAPHORE        (MEM1_BASE | 0x3160)

//Access permissions
//we can have multiple APs per second level page, hence the formula to calculate the value for us
#define APX_VALUE(number, access) ((access & 0x03) << ((2 + number) * 2))
#define AP_VALUE(access)          APX_VALUE(3, access)
#define AP_ROM                    0x00
#define AP_NOUSER                 0x01
#define AP_ROUSER                 0x02
#define AP_RWUSER                 0x03

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
	MemorySection Section;
} ProcessMemorySection;

CHECK_OFFSET(ProcessMemorySection, 0x00, ProcessId);
CHECK_OFFSET(ProcessMemorySection, 0x04, Section);
CHECK_SIZE(ProcessMemorySection, 0x1C);

typedef enum
{
	PageTable = 0,
	Unknown = 1,
	CoursePage = 2,
} KernelMemoryType;

#ifndef MIOS
s32 InitializeMemory(void);
void *KMalloc(u32 size);
s32 MapMemory(MemorySection *entry);
u32 VirtualToPhysical(u32 virtualAddress);
s32 CheckMemoryPointer(const void *ptr, u32 size, u32 type, u32 pid, u32 domainPid);
#endif
void ProtectMemory(int enable, void *start, void *end);
void DCInvalidateRange(const void *start, u32 size);
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

void ConfigureDDRMemory(void);
void SetMemoryCompatabilityMode();

void mem_shutdown(void);
u32 dma_addr(void *);

#endif