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
#include "heaps.h"
#include "threads.h"
#include "message_queue.h"
#include "irq.h"

//#define _DEBUG_SYSCALL

typedef u32 (*syscall_handler)(u32 r0, u32 r1, u32 r2, u32 r3, u32 r4, u32 r5, u32 r6);
static u32 syscall_handlers[] = {
	(u32)CreateThread,					//0x0000
	(u32)0x00000000,					//0x0001
	(u32)CancelThread,					//0x0002
	(u32)GetThreadID,					//0x0003
	(u32)GetProcessID,					//0x0004
	(u32)StartThread,					//0x0005
	(u32)0x00000000,					//0x0006
	(u32)YieldThread,					//0x0007
	(u32)GetThreadPriority,				//0x0008
	(u32)SetThreadPriority,				//0x0009
	(u32)CreateMessageQueue,			//0x000A
	(u32)DestroyMessageQueue,			//0x000B
	(u32)0x00000000,					//0x000C
	(u32)0x00000000,					//0x000D
	(u32)0x00000000,					//0x000E
	(u32)RegisterEventHandler,			//0x000F
	(u32)UnregisterEventHandler,		//0x0010
	(u32)0x00000000,					//0x0011
	(u32)0x00000000,					//0x0012
	(u32)0x00000000,					//0x0013
	(u32)0x00000000,					//0x0014
	(u32)GetTimerValue,					//0x0015
	(u32)CreateHeap,					//0x0016
	(u32)DestroyHeap,					//0x0017
	(u32)AllocateOnHeap,				//0x0018
	(u32)MallocateOnHeap,				//0x0019
	(u32)FreeOnHeap,					//0x001A
	(u32)0x00000000,					//0x001B
	(u32)0x00000000,					//0x001C
	(u32)0x00000000,					//0x001D
	(u32)0x00000000,					//0x001E
	(u32)0x00000000,					//0x001F
};

//We implement syscalls using the SVC/SWI instruction. 
//Nintendo/IOS however was using undefined instructions and just caught those in their exception handler lol
//Both our SWI and (if applicable) undefined instruction handlers call this function (see exception_asm.S & exception.c)
int handle_syscall(u16 syscall, Registers* registers)
{
#ifdef _DEBUG_SYSCALL
	gecko_printf("syscall : 0x%04X\n", syscall);
	if(registers != NULL)
	{
		gecko_printf("Registers (%p):\n", registers);
		gecko_printf("  R0-R3: %08x %08x %08x %08x\n", registers->registers[0], registers->registers[1], registers->registers[2], registers->registers[3]);
	}
	else
		gecko_printf("registers == NULL");
#endif	

	//is the syscall within our range ?
	//sizeof(syscall) = the amout of bytes, so /4 since its 4 bytes per address
	//and -1 since we start at 0000
	if(syscall > ((sizeof(syscall_handlers) / 4) -1 ))
	{
		gecko_printf("unknown syscall 0x%04X\n", syscall);
		return -666;
	}
	
	syscall_handler handler = (syscall_handler)syscall_handlers[syscall];
	u32* reg = registers->registers;
	
	if(handler == NULL)
	{
		gecko_printf("unimplemented syscall 0x%04X\n", syscall);
		return -666;
	}
	
	//dive into the handler
	return handler(reg[1], reg[2], reg[3], reg[4], reg[5], reg[6], reg[7]);
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

