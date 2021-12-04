/*
	mini - a Free Software replacement for the Nintendo/BroadOn IOS.
	memory management, MMU, caches, and flushing

Copyright (C) 2008, 2009	Hector Martin "marcan" <marcan@marcansoft.com>

# This code is licensed to you under the terms of the GNU GPL, version 2;
# see file COPYING or http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt
*/

#ifndef __MEMORY_H__
#define __MEMORY_H__

#include "types.h"
#include "memory/ahb.h"
#include "interrupt/threads.h"

#define ALIGN_FORWARD(x,align) \
	((typeof(x))((((u32)(x)) + (align) - 1) & (~(align-1))))

#define ALIGN_BACKWARD(x,align) \
	((typeof(x))(((u32)(x)) & (~(align-1))))

typedef struct
{
	u32 physicalAddress;
	u32 virtualAddress;
	u32 size;
	u32 domain;
	u32 accessRights;
	u32 unknown;
} MemorySection;

typedef struct
{
	u32 processId;
	MemorySection memorySection;
} ProcessMemorySection;

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

void dc_flushrange(const void *start, u32 size);
void dc_invalidaterange(void *start, u32 size);
void dc_flushall(void);
void ic_invalidateall(void);

void mem_setswap(int enable);
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

