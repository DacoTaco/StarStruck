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

#define SWI				"swi "
#define SWI_MEMALIGN	STR(SYSCALL_MEMALIGN)
#define SWI_FREE		STR(SYSCALL_MEMFREE)

//We implement syscalls using the SVC/SWI instruction. 
//Nintendo/IOS however was using undefined instructions and just caught those in their exception handler lol
#define _syscall(syscall, parameter1, parameter2, parameter3, parameter4) { \
	register u32 par1	 	__asm__("r0") = parameter1; \
	register void* par2		__asm__("r1") = parameter2; \
	register u32 par3		__asm__("r2") = parameter3; \
	register void* par4		__asm__("r3") = parameter4; \
	\
	__asm__ volatile \
	(\
		"swi "syscall \
		: \
		: "r"  (par1), "r"  (par2), "r"  (par3), "r"  (par4)\
		: "memory"\
	);}
	
void* os_allocateMemory(u32 size)
{
	u8* ptr = NULL;
	_syscall(SWI_MEMALIGN, size, (void*)&ptr, 0, NULL);
	return (void*)ptr;
}

void os_freeMemory(void* ptr)
{
	_syscall(SWI_FREE, 0, &ptr, 0, NULL);
	return;
}