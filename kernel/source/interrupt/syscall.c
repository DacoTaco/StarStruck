/*
	starstruck - a Free Software reimplementation for the Nintendo/BroadOn IOS.
	syscalls - internal communications over software interrupts

	Copyright (C) 2021	DacoTaco

# This code is licensed to you under the terms of the GNU GPL, version 2;
# see file COPYING or http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt
*/

#include <ios/gecko.h>

#include "core/hollywood.h"
#include "interrupt/irq.h"
#include "scheduler/timer.h"
#include "scheduler/threads.h"
#include "memory/memory.h"
#include "memory/heaps.h"
#include "memory/ahb.h"
#include "messaging/ipc.h"
#include "messaging/messageQueue.h"
#include "messaging/resourceManager.h"
#include "crypto/iosc.h"
#include "filedesc/calls.h"
#include "filedesc/calls_async.h"

//#define _DEBUG_SYSCALL

typedef s32 (*SyscallHandler)(u32 r0, u32 r1, u32 r2, u32 r3, u32 r4, u32 r5, u32 r6, u32 r7, u32 r8, u32 r9);
static const void* syscall_handlers[]  __attribute__ ((section (".syscalls"))) = {
	CreateThread,				//0x0000
	JoinThread,					//0x0001
	CancelThread,				//0x0002
	GetThreadID,				//0x0003
	GetProcessID,				//0x0004
	StartThread,				//0x0005
	SuspendThread,				//0x0006
	YieldThread,				//0x0007
	GetThreadPriority,			//0x0008
	SetThreadPriority,			//0x0009
	CreateMessageQueue,			//0x000A
	DestroyMessageQueue,		//0x000B
	SendMessage,				//0x000C
	JamMessage,					//0x000D
	ReceiveMessage,				//0x000E
	RegisterEventHandler,		//0x000F
	UnregisterEventHandler,		//0x0010
	CreateTimer,				//0x0011
	RestartTimer,				//0x0012
	StopTimer,					//0x0013
	DestroyTimer,				//0x0014
	GetTimerValue,				//0x0015
	CreateHeap,					//0x0016
	DestroyHeap,				//0x0017
	AllocateOnHeap,				//0x0018
	MallocateOnHeap,			//0x0019
	FreeOnHeap,					//0x001A
	RegisterResourceManager,	//0x001B
	OpenFD,						//0x001C
	CloseFD,					//0x001D
	ReadFD,						//0x001E
	WriteFD,					//0x001F
	SeekFD,						//0x0020
	IoctlFD,					//0x0021
	IoctlvFD,					//0x0022
	OpenFDAsync,				//0x0023
	CloseFDAsync,				//0x0024
	ReadFDAsync,				//0x0025
	WriteFDAsync,				//0x0026
	SeekFDAsync,				//0x0027
	IoctlFDAsync,				//0x0028
	IoctlvFDAsync,				//0x0029
	ResourceReply,				//0x002A
	SetUID,						//0x002B
	GetUID,						//0x002C
	SetGID,						//0x002D
	GetGID,						//0x002E
	AhbFlushFrom,				//0x002F
	AhbFlushTo,					//0x0030
	ClearAndEnableIPCInterrupt,	//0x0031
#ifndef MIOS
	ClearAndEnableDIInterrupt,	//0x0032
	ClearAndEnableSDInterrupt,	//0x0033
	ClearAndEnableEvent,		//0x0034
	0x00000000,					//0x0035
	0x00000000,					//0x0036
	0x00000000,					//0x0037
	0x00000000,					//0x0038
	0x00000000,					//0x0039
	0x00000000,					//0x003A
	0x00000000,					//0x003B
	0x00000000,					//0x003C
	0x00000000,					//0x003D
	0x00000000,					//0x003E
	DCInvalidateRange,			//0x003F
	DCFlushRange,				//0x0040
	0x00000000,					//0x0041
	0x00000000,					//0x0042
	0x00000000,					//0x0043
	0x00000000,					//0x0044
	0x00000000,					//0x0045
	0x00000000,					//0x0046
	0x00000000,					//0x0047
	0x00000000,					//0x0048
	0x00000000,					//0x0049
	0x00000000,					//0x004A
	0x00000000,					//0x004B
	0x00000000,					//0x004C
	0x00000000,					//0x004D
	0x00000000,					//0x004E
	VirtualToPhysical,			//0x004F
	0x00000000,					//0x0050
	0x00000000,					//0x0051
	0x00000000,					//0x0052
	0x00000000,					//0x0053
	0x00000000,					//0x0054
	GetCoreClock,				//0x0055
	0x00000000,					//0x0056
	0x00000000,					//0x0057
	0x00000000,					//0x0058
	0x00000000,					//0x0059
	LaunchRM,					//0x005A
	IOSC_CreateObject,			//0x005B
	IOSC_DeleteObject,			//0x005C
	0x00000000,					//0x005D
	0x00000000,					//0x005E
	0x00000000,					//0x005F
	0x00000000,					//0x0060
	0x00000000,					//0x0061
	0x00000000,					//0x0062
	IOSC_GetData,				//0x0063
	IOSC_GetKeySize,			//0x0064
	IOSC_GetSignatureSize,		//0x0065
	0x00000000,					//0x0066
	0x00000000,					//0x0067
	IOSC_EncryptAsync,			//0x0068
	IOSC_Encrypt,				//0x0069
	IOSC_DecryptAsync,			//0x006A
	IOSC_Decrypt,				//0x006B
	0x00000000,					//0x006C
	IOSC_GenerateBlockMAC,		//0x006D
	IOSC_GenerateBlockMACAsync,	//0x006E
	0x00000000,					//0x006F
	0x00000000,					//0x0070
	0x00000000,					//0x0071
	0x00000000,					//0x0072
	0x00000000,					//0x0073
	0x00000000,					//0x0074
	0x00000000,					//0x0075
	0x00000000,					//0x0076
	0x00000000,					//0x0077
	0x00000000,					//0x0078
	0x00000000,					//0x0079
	0x00000000,					//0x007A
	0x00000000,					//0x007B
	0x00000000,					//0x007C
	0x00000000,					//0x007D
	0x00000000,					//0x007E
	0x00000000,					//0x007F
#endif
};

//We implement syscalls using the SVC/SWI instruction. 
//Nintendo/IOS however was using undefined instructions and just caught those in their exception handler lol
//Both our SWI and (if applicable) undefined instruction handlers call this function (see exception_asm.S & exception.c)
s32 HandleSyscall(u16 syscall)
{
	ThreadContext* threadContext = &CurrentThread->UserContext;

#ifdef _DEBUG_SYSCALL
	gecko_printf("syscall : 0x%04X\n", syscall);
	if(threadContext != NULL)
	{
		gecko_printf("Context (%p / SPSR %08x):\n", threadContext, threadContext->StatusRegister);
		gecko_printf("  R0-R3: %08x %08x %08x %08x\n", threadContext->Registers[0], threadContext->Registers[1], threadContext->Registers[2], threadContext->Registers[3]);
		gecko_printf("  R4-R7: %08x %08x %08x %08x\n", threadContext->Registers[4], threadContext->Registers[5], threadContext->Registers[6], threadContext->Registers[7]);
		gecko_printf("  R8-R11: %08x %08x %08x %08x\n", threadContext->Registers[8], threadContext->Registers[9], threadContext->Registers[10], threadContext->Registers[11]);
		gecko_printf("  R12-R15: %08x %08x %08x %08x\n", threadContext->Registers[12], threadContext->Registers[13], threadContext->Registers[14], threadContext->Registers[15]);
	}
	else
		gecko_printf("threadContext == NULL");
#endif	

	//is this the special IOS syscall?
	if(syscall == 0xAB && threadContext->Registers[0] == 0x04)
	{
		gecko_printf((char*)threadContext->Registers[1]);
		return 0;
	}

	//is the syscall within our range ?
	//sizeof(syscall) = the amout of bytes, so /4 since its 4 bytes per address
	//and -1 since we start at 0000
	if(syscall > ((sizeof(syscall_handlers) / 4) -1 ))
	{
		gecko_printf("unknown syscall 0x%04X\n", syscall);
		return -666;
	}

	u32* reg = threadContext->Registers;
	SyscallHandler handler = (SyscallHandler)syscall_handlers[syscall];		
	if(handler == NULL)
	{
		gecko_printf("unimplemented syscall 0x%04X\n", syscall);
		return -666;
	}
	
	//dive into the handler
	return handler(reg[0], reg[1], reg[2], reg[3], reg[4], reg[5], reg[6], reg[7], reg[8], reg[9]);
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

