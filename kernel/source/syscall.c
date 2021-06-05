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

	switch(syscall)
	{
		case SYSCALL_CREATETHREAD:
			return CreateThread((s32)registers->registers[1], (void*)registers->registers[2], (u32*)registers->registers[3], (u32)registers->registers[4], (s32)registers->registers[5], (u32)registers->registers[6]);

		case SYSCALL_STOPTHREAD:
			return CancelThread((s32)registers->registers[1], (u32)registers->registers[2]);

		case SYSCALL_STARTTHREAD:
			return StartThread((s32)registers->registers[1]);
			
		case SYSCALL_YIELDTHREAD:
			YieldThread();
			return 0;
			
		case SYSCALL_GETTHREADPRIORITY:
			return GetThreadPriority((u32)registers->registers[1]);
		
		case SYSCALL_SETTHREADPRIORITY:
			return SetThreadPriority((u32)registers->registers[1], (s32)registers->registers[2]);

		case SYSCALL_CREATEMESSAGEQUEUE:
			return CreateMessageQueue((void*)registers->registers[1], (u32)registers->registers[2]);
		
		case SYSCALL_DESTROYMESSAGEQUEUE:
			return DestroyMessageQueue((s32)registers->registers[1]);
			
		case SYSCALL_REGISTEREVENTHANDLER:
			return RegisterEventHandler((u8)registers->registers[1], (s32)registers->registers[2], (s32)registers->registers[3]);
			
		case SYSCALL_UNREGISTEREVENTHANDLER:
			return UnregisterEventHandler((u8)registers->registers[1]);
		
		case SYSCALL_CREATEHEAP:
			return CreateHeap((void*)registers->registers[1], (u32)registers->registers[2]);
			
		case SYSCALL_DESTROYHEAP:
			return DestroyHeap((s32)registers->registers[1]);
			
		case SYSCALL_MALLOC:
		case SYSCALL_MEMALIGN:
			return (s32)AllocateOnHeap((s32)registers->registers[1], (u32)registers->registers[2], (syscall == SYSCALL_MALLOC) ? 0x20 : (u32)registers->registers[3]);
			
		case SYSCALL_MEMFREE:
			return FreeOnHeap((s32)registers->registers[1], (void*)registers->registers[2]);
		
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

