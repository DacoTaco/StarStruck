/*
	starstruck - a Free Software reimplementation for the Nintendo/BroadOn IOS.
	syscallcore - internal communications over software interrupts

	Copyright (C) 2021	DacoTaco

# This code is licensed to you under the terms of the GNU GPL, version 2;
# see file COPYING or http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt
*/

#include "utils.h"
#include "syscallcore.h"
#include "ios_module.h"

//We implement syscalls using the SVC/SWI instruction. 
//Nintendo/IOS however was using undefined instructions and just caught those in their exception handler lol
#define _syscall(syscall, returnValue, parameter1, parameter2, parameter3, parameter4){ \
	register s32 retValue 	__asm__("r0"); \
	register u32 par1	 	__asm__("r1") = parameter1; \
	register u32 par2		__asm__("r2") = parameter2; \
	register u32 par3		__asm__("r3") = parameter3; \
	register u32 par4		__asm__("r4") = parameter4; \
	\
	__asm__ volatile \
	(\
		"swi "syscall \
		: "=r"  (retValue)\
		: "r"  (par1), "r"  (par2), "r"  (par3), "r"  (par4)\
		: "memory"\
	); \
	if(returnValue != NULL)\
		*(s32*)returnValue = retValue;}

s32 os_createHeap(void *ptr, u32 size)
{
	s32 ret = -1;
	_syscall(STR(SYSCALL_CREATEHEAP), &ret, (u32)ptr, size, 0, 0);	
	return ret;
}

s32 os_destroyHeap(s32 heapid)
{
	s32 ret = -1;
	_syscall(STR(SYSCALL_DESTROYHEAP), &ret, heapid, 0, 0, 0);
	return ret;
}
	
void* os_allocateMemory(s32 heapid, u32 size)
{
	u32 ptr = 0;
	_syscall(STR(SYSCALL_MALLOC), &ptr, heapid, size, 0, 0);
	return (void*)ptr;
}

void* os_alignedAllocateMemory(s32 heapid, u32 size, u32 align)
{
	u32 ptr = 0;
	_syscall(STR(SYSCALL_MEMALIGN), &ptr, heapid, size, align, 0);
	return (void*)ptr;
}

void os_freeMemory(s32 heapid, void *ptr)
{
	_syscall(STR(SYSCALL_MEMFREE), NULL, heapid, (s32)ptr, 0, 0);
	return;
}