/*
	starstruck - a Free Software reimplementation for the Nintendo/BroadOn IOS.
	syscalls - internal communications over software interrupts

	Copyright (C) 2021	DacoTaco

# This code is licensed to you under the terms of the GNU GPL, version 2;
# see file COPYING or http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt
*/

#include <types.h>
#include <syscallcore.h>
#include "gecko.h"

//We implement syscalls using the SVC/SWI instruction. 
//Nintendo/IOS however was using undefined instructions and just caught those in their exception handler lol
//Both our SWI and (if applicable) undefined instruction handlers call this function
int handle_syscall(u16 syscall, unsigned *parameters)
{	
	gecko_printf("hello from handle_syscall\n");
	gecko_printf("syscall : 0x%04X\n", syscall);
	if(parameters != NULL)
	{
		gecko_printf("Registers (%p):\n", parameters);
		gecko_printf("  R0-R3: %08x %08x %08x %08x\n", parameters[0], parameters[1], parameters[2], parameters[3]);
	}
	else
		gecko_printf("parameters == NULL");
	
	switch(syscall)
	{
		case SYSCALL_MEMALIGN:
			break;
			
		case SYSCALL_MEMFREE:
			break;
		
		default:
			gecko_printf("unknown syscall 0x%04X\n", syscall);
			break;
	}
	
	return -666;
}

/*	
	This is how to implement the SWI handler in C++/GCC.
	However, it overwrites our return register(r0) when restoring the registers to their original state...
	Hence we use our own asm to store the state, get the parameters and call the handle_syscall function
*/

/*__attribute__ ((interrupt ("SWI"))) int syscall_handler(u32 r0, u32 r1, u32 r2, u32 r3)
{
	//always retrieve the number first, before any registers get touched !
	u16 syscall = 0;
	__asm__ volatile ("ldr\t%0, [lr,#-4]" : "=r" (syscall));
	unsigned* parameters;
	__asm__ volatile ("mov\t%0, sp " : "=r" (parameters));
	return handle_syscall(syscall & 0xFFFF, parameters);
}*/

