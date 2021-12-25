/*
	starstruck - a Free Software reimplementation for the Nintendo/BroadOn IOS.
	syscalls - internal communications over software interrupts

	Copyright (C) 2021	DacoTaco

# This code is licensed to you under the terms of the GNU GPL, version 2;
# see file COPYING or http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt
*/

#include <ios/gecko.h>

#include "interrupt/irq.h"
#include "scheduler/timer.h"
#include "scheduler/threads.h"
#include "memory/memory.h"
#include "memory/heaps.h"
#include "memory/ahb.h"
#include "messaging/message_queue.h"

//#define _DEBUG_SYSCALL

typedef u32 (*SyscallHandler)(u32 r0, u32 r1, u32 r2, u32 r3, u32 r4, u32 r5, u32 r6);
static u32 syscall_handlers[] = {
	(u32)CreateThread,					//0x0000
	(u32)JoinThread,					//0x0001
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
	(u32)SendMessage,					//0x000C
	(u32)0x00000000,					//0x000D
	(u32)ReceiveMessage,				//0x000E
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
	(u32)0x00000000,					//0x0020
	(u32)0x00000000,					//0x0021
	(u32)0x00000000,					//0x0022
	(u32)0x00000000,					//0x0023
	(u32)0x00000000,					//0x0024
	(u32)0x00000000,					//0x0025
	(u32)0x00000000,					//0x0026
	(u32)0x00000000,					//0x0027
	(u32)0x00000000,					//0x0028
	(u32)0x00000000,					//0x0029
	(u32)0x00000000,					//0x002A
	(u32)SetUID,						//0x002B
	(u32)GetUID,						//0x002C
	(u32)SetGID,						//0x002D
	(u32)GetGID,						//0x002E
	(u32)AhbFlushFrom,					//0x002F
	(u32)AhbFlushTo,					//0x0030
	(u32)0x00000000,					//0x0031
	(u32)0x00000000,					//0x0032
	(u32)0x00000000,					//0x0033
	(u32)0x00000000,					//0x0034
	(u32)0x00000000,					//0x0035
	(u32)0x00000000,					//0x0036
	(u32)0x00000000,					//0x0037
	(u32)0x00000000,					//0x0038
	(u32)0x00000000,					//0x0039
	(u32)0x00000000,					//0x003A
	(u32)0x00000000,					//0x003B
	(u32)0x00000000,					//0x003C
	(u32)0x00000000,					//0x003D
	(u32)0x00000000,					//0x003E
	(u32)DCInvalidateRange,				//0x003F
	(u32)DCFlushRange,					//0x0040
	(u32)0x00000000,					//0x0041
	(u32)0x00000000,					//0x0042
	(u32)0x00000000,					//0x0043
	(u32)0x00000000,					//0x0044
	(u32)0x00000000,					//0x0045
	(u32)0x00000000,					//0x0046
	(u32)0x00000000,					//0x0047
	(u32)0x00000000,					//0x0048
	(u32)0x00000000,					//0x0049
	(u32)0x00000000,					//0x004A
	(u32)0x00000000,					//0x004B
	(u32)0x00000000,					//0x004C
	(u32)0x00000000,					//0x004D
	(u32)0x00000000,					//0x004E
	(u32)VirtualToPhysical,				//0x004F
};

//We implement syscalls using the SVC/SWI instruction. 
//Nintendo/IOS however was using undefined instructions and just caught those in their exception handler lol
//Both our SWI and (if applicable) undefined instruction handlers call this function (see exception_asm.S & exception.c)
s32 HandleSyscall(u16 syscall, ThreadContext* threadContext)
{
#ifdef _DEBUG_SYSCALL
	gecko_printf("syscall : 0x%04X\n", syscall);
	if(threadContext != NULL)
	{
		gecko_printf("Context (%p):\n", threadContext);
		gecko_printf("  R0-R3: %08x %08x %08x %08x\n", threadContext->registers[0], threadContext->registers[1], threadContext->registers[2], threadContext->registers[3]);
	}
	else
		gecko_printf("threadContext == NULL");
#endif	

	//is the syscall within our range ?
	//sizeof(syscall) = the amout of bytes, so /4 since its 4 bytes per address
	//and -1 since we start at 0000
	if(syscall > ((sizeof(syscall_handlers) / 4) -1 ))
	{
		gecko_printf("unknown syscall 0x%04X\n", syscall);
		return -666;
	}
	
	SyscallHandler handler = (SyscallHandler)syscall_handlers[syscall];
	u32* reg = threadContext->registers;
	
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
	Hence we use our own asm to store the state, get the parameters and call the HandleSyscall function
*/

/*__attribute__ ((interrupt ("SWI"))) int syscall_handler(u32 r0, u32 r1, u32 r2, u32 r3)
{
	//always retrieve the number first, before any registers get touched !
	u16 syscall = 0;
	__asm__ volatile ("ldr\t%0, [lr,#-4]" : "=r" (syscall));
	unsigned* parameters;
	__asm__ volatile ("mov\t%0, sp " : "=r" (parameters));
	return HandleSyscall(syscall & 0xFFFF, parameters);
}*/

