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

void handle_syscall(u16 syscall, u32 parameter1, void** parameter2, u32 parameter3, void** parameter4)
{	
	gecko_printf("hello from handle_syscall\n");
	gecko_printf("syscall : 0x%04X\n", syscall);
	gecko_printf("parameters : 0x%08X - 0x%p - 0x%08X - 0x%p \n", parameter1, parameter2, parameter3, parameter4);
	
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
	
	return;
}

//We implement syscalls using the SVC/SWI instruction. 
//Nintendo/IOS however was using undefined instructions and just caught those in their exception handler lol
__attribute__ ((interrupt ("SWI"))) void syscall_handler(u32 parameter1, void** parameter2, u32 parameter3, void** parameter4)
{
	//always retrieve the number first, before any registers get touched !
	u16 syscall = 0;
	__asm__ volatile ("ldr\t%0, [lr,#-4]" : "=r" (syscall));
	handle_syscall(syscall, parameter1, parameter2, parameter3, parameter4);
}

