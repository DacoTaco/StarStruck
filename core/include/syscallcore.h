/*
	starstruck - a Free Software reimplementation for the Nintendo/BroadOn IOS.
	syscallcore - internal communications over software interrupts

	Copyright (C) 2021	DacoTaco

# This code is licensed to you under the terms of the GNU GPL, version 2;
# see file COPYING or http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt
*/

#include "types.h"

#define SYSCALL_CREATEHEAP		0x0016
#define SYSCALL_DESTROYHEAP		0x0017
#define SYSCALL_MALLOC			0x0018
#define SYSCALL_MEMALIGN		0x0019
#define SYSCALL_MEMFREE			0x001A

s32 os_createHeap(void *ptr, u32 size);
s32 os_destroyHeap(s32 heapid);
void* os_allocateMemory(s32 heapid, u32 size);
void* os_alignedAllocateMemory(s32 heapid, u32 size, u32 align);
void os_freeMemory(s32 heapid, void *ptr);