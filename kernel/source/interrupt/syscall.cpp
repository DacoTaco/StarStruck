/*
	starstruck - a Free Software reimplementation for the Nintendo/BroadOn IOS.
	syscalls - internal communications over software interrupts

	Copyright (C) 2021	DacoTaco

# This code is licensed to you under the terms of the GNU GPL, version 2;
# see file COPYING or http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt
*/

extern "C" {
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
}

//#define _DEBUG_SYSCALL

//this is different from IOS's syscall table.
//we use a single Syscall entry info, while IOS had 2 tables.
//1 for the syscall handler, and 1 for the stack arg count.
struct SyscallEntry
{
	const void *Handler;
	u8 StackArgumentCount;
};

// Template to deduce argument count from function pointer type
template <typename ReturnType, typename... Args> struct ArgCounter
{
	static constexpr u8 count = sizeof...(Args);
	static constexpr u8 stackArgs = sizeof...(Args) > 4 ? sizeof...(Args) - 4 : 0;
};

template <typename ReturnType, typename... Args>
constexpr ArgCounter<ReturnType, Args...> GetArgumentCount(ReturnType (*)(Args...));

// Helper macro to create a SyscallEntry with automatic stack argument count deduction
#define SYSCALL(func)                                                     \
	{                                                                     \
		.Handler = reinterpret_cast<const void *>(func),                  \
		.StackArgumentCount = decltype(GetArgumentCount(func))::stackArgs \
	}
#define SYSCALL_NULL                                 \
	{                                                \
		.Handler = nullptr, .StackArgumentCount = 0, \
	}

typedef s32 (*SyscallHandler)(u32 r0, u32 r1, u32 r2, u32 r3, u32 r4, u32 r5,
                              u32 r6, u32 r7, u32 r8, u32 r9);

static const SyscallEntry syscall_handlers[] __attribute__((section(".syscalls"))) = {
	SYSCALL(CreateThread), //0x0000
	SYSCALL(JoinThread), //0x0001
	SYSCALL(CancelThread), //0x0002
	SYSCALL(GetThreadID), //0x0003
	SYSCALL(GetProcessID), //0x0004
	SYSCALL(StartThread), //0x0005
	SYSCALL(SuspendThread), //0x0006
	SYSCALL(YieldThread), //0x0007
	SYSCALL(GetThreadPriority), //0x0008
	SYSCALL(SetThreadPriority), //0x0009
	SYSCALL(CreateMessageQueue), //0x000A
	SYSCALL(DestroyMessageQueue), //0x000B
	SYSCALL(SendMessage), //0x000C
	SYSCALL(JamMessage), //0x000D
	SYSCALL(ReceiveMessage), //0x000E
	SYSCALL(RegisterEventHandler), //0x000F
	SYSCALL(UnregisterEventHandler), //0x0010
	SYSCALL(CreateTimer), //0x0011
	SYSCALL(RestartTimer), //0x0012
	SYSCALL(StopTimer), //0x0013
	SYSCALL(DestroyTimer), //0x0014
	SYSCALL(GetTimerValue), //0x0015
	SYSCALL(CreateHeap), //0x0016
	SYSCALL(DestroyHeap), //0x0017
	SYSCALL(AllocateOnHeap), //0x0018
	SYSCALL(MallocateOnHeap), //0x0019
	SYSCALL(FreeOnHeap), //0x001A
	SYSCALL(RegisterResourceManager), //0x001B
	SYSCALL(OpenFD), //0x001C
	SYSCALL(CloseFD), //0x001D
	SYSCALL(ReadFD), //0x001E
	SYSCALL(WriteFD), //0x001F
	SYSCALL(SeekFD), //0x0020
	SYSCALL(IoctlFD), //0x0021
	SYSCALL(IoctlvFD), //0x0022
	SYSCALL(OpenFDAsync), //0x0023
	SYSCALL(CloseFDAsync), //0x0024
	SYSCALL(ReadFDAsync), //0x0025
	SYSCALL(WriteFDAsync), //0x0026
	SYSCALL(SeekFDAsync), //0x0027
	SYSCALL(IoctlFDAsync), //0x0028
	SYSCALL(IoctlvFDAsync), //0x0029
	SYSCALL(ResourceReply), //0x002A
	SYSCALL(SetUID), //0x002B
	SYSCALL(GetUID), //0x002C
	SYSCALL(SetGID), //0x002D
	SYSCALL(GetGID), //0x002E
	SYSCALL(AhbFlushFrom), //0x002F
	SYSCALL(AhbFlushTo), //0x0030
	SYSCALL(ClearAndEnableIPCInterrupt), //0x0031
#ifndef MIOS
	SYSCALL(ClearAndEnableDIInterrupt), //0x0032
	SYSCALL(ClearAndEnableSDInterrupt), //0x0033
	SYSCALL(ClearAndEnableEvent), //0x0034
	SYSCALL_NULL, //0x0035
	SYSCALL_NULL, //0x0036
	SYSCALL_NULL, //0x0037
	SYSCALL_NULL, //0x0038
	SYSCALL_NULL, //0x0039
	SYSCALL_NULL, //0x003A
	SYSCALL_NULL, //0x003B
	SYSCALL_NULL, //0x003C
	SYSCALL_NULL, //0x003D
	SYSCALL_NULL, //0x003E
	SYSCALL(DCInvalidateRange), //0x003F
	SYSCALL(DCFlushRange), //0x0040
	SYSCALL_NULL, //0x0041
	SYSCALL_NULL, //0x0042
	SYSCALL_NULL, //0x0043
	SYSCALL_NULL, //0x0044
	SYSCALL_NULL, //0x0045
	SYSCALL_NULL, //0x0046
	SYSCALL_NULL, //0x0047
	SYSCALL_NULL, //0x0048
	SYSCALL_NULL, //0x0049
	SYSCALL_NULL, //0x004A
	SYSCALL_NULL, //0x004B
	SYSCALL_NULL, //0x004C
	SYSCALL_NULL, //0x004D
	SYSCALL_NULL, //0x004E
	SYSCALL(VirtualToPhysical), //0x004F
	SYSCALL_NULL, //0x0050
	SYSCALL_NULL, //0x0051
	SYSCALL_NULL, //0x0052
	SYSCALL_NULL, //0x0053
	SYSCALL_NULL, //0x0054
	SYSCALL(GetCoreClock), //0x0055
	SYSCALL_NULL, //0x0056
	SYSCALL_NULL, //0x0057
	SYSCALL_NULL, //0x0058
	SYSCALL_NULL, //0x0059
	SYSCALL(LaunchRM), //0x005A
	SYSCALL(IOSC_CreateObject), //0x005B
	SYSCALL(IOSC_DeleteObject), //0x005C
	SYSCALL_NULL, //0x005D
	SYSCALL_NULL, //0x005E
	SYSCALL_NULL, //0x005F
	SYSCALL_NULL, //0x0060
	SYSCALL_NULL, //0x0061
	SYSCALL_NULL, //0x0062
	SYSCALL(IOSC_GetData), //0x0063
	SYSCALL(IOSC_GetKeySize), //0x0064
	SYSCALL(IOSC_GetSignatureSize), //0x0065
	SYSCALL_NULL, //0x0066
	SYSCALL_NULL, //0x0067
	SYSCALL(IOSC_EncryptAsync), //0x0068
	SYSCALL(IOSC_Encrypt), //0x0069
	SYSCALL(IOSC_DecryptAsync), //0x006A
	SYSCALL(IOSC_Decrypt), //0x006B
	SYSCALL_NULL, //0x006C
	SYSCALL(IOSC_GenerateBlockMAC), //0x006D
	SYSCALL(IOSC_GenerateBlockMACAsync), //0x006E
	SYSCALL_NULL, //0x006F
	SYSCALL_NULL, //0x0070
	SYSCALL_NULL, //0x0071
	SYSCALL_NULL, //0x0072
	SYSCALL_NULL, //0x0073
	SYSCALL_NULL, //0x0074
	SYSCALL_NULL, //0x0075
	SYSCALL_NULL, //0x0076
	SYSCALL_NULL, //0x0077
	SYSCALL_NULL, //0x0078
	SYSCALL_NULL, //0x0079
	SYSCALL_NULL, //0x007A
	SYSCALL_NULL, //0x007B
	SYSCALL_NULL, //0x007C
	SYSCALL_NULL, //0x007D
	SYSCALL_NULL, //0x007E
	SYSCALL_NULL, //0x007F
#endif
};

//We implement syscalls using the SVC/SWI instruction.
//Nintendo/IOS however was using undefined instructions and just caught those in their exception handler lol
//Both our SWI and (if applicable) undefined instruction handlers call this function (see exception_asm.S & exception.c)
extern "C" s32 HandleSyscall(u16 syscall)
{
	ThreadContext *threadContext = &CurrentThread->UserContext;

#ifdef _DEBUG_SYSCALL
	gecko_printf("syscall : 0x%04X\n", syscall);
	if (threadContext != NULL)
	{
		gecko_printf("Context (%p / SPSR %08x):\n", threadContext, threadContext->StatusRegister);
		gecko_printf("  R0-R3: %08x %08x %08x %08x\n",
		             threadContext->Registers[0], threadContext->Registers[1],
		             threadContext->Registers[2], threadContext->Registers[3]);
		gecko_printf("  R4-R7: %08x %08x %08x %08x\n",
		             threadContext->Registers[4], threadContext->Registers[5],
		             threadContext->Registers[6], threadContext->Registers[7]);
		gecko_printf("  R8-R11: %08x %08x %08x %08x\n",
		             threadContext->Registers[8], threadContext->Registers[9],
		             threadContext->Registers[10], threadContext->Registers[11]);
		gecko_printf("  R12-R15: %08x %08x %08x %08x\n",
		             threadContext->Registers[12], threadContext->Registers[13],
		             threadContext->Registers[14], threadContext->Registers[15]);
	}
	else
		gecko_printf("threadContext == NULL");
#endif

	//is this the special IOS syscall?
	if (syscall == 0xAB && threadContext->Registers[0] == 0x04)
	{
		gecko_printf((char *)threadContext->Registers[1]);
		return 0;
	}

	//is the syscall within our range ?
	constexpr u16 syscallCount = sizeof(syscall_handlers) / sizeof(SyscallEntry);
	if (syscall >= syscallCount)
	{
		gecko_printf("unknown syscall 0x%04X\n", syscall);
		return -666;
	}

	u32 *reg = threadContext->Registers;
	const SyscallEntry &entry = syscall_handlers[syscall];
	SyscallHandler handler = reinterpret_cast<SyscallHandler>(entry.Handler);
	if (handler == NULL)
	{
		gecko_printf("unimplemented syscall 0x%04X\n", syscall);
		return -666;
	}

	//First 4 arguments are in R0-R3, the rest are on the user's stack
	u32 args[10] = { reg[0], reg[1], reg[2], reg[3], 0, 0, 0, 0, 0, 0 };

	if (entry.StackArgumentCount > 0)
	{
		//Get user stack pointer and read additional arguments from it
		u32 *userStack = reinterpret_cast<u32 *>(threadContext->StackPointer);
		for (u8 i = 0; i < entry.StackArgumentCount && i < 6; i++)
		{
			args[4 + i] = userStack[i];
		}
	}

	//dive into the handler
	return handler(args[0], args[1], args[2], args[3], args[4], args[5],
	               args[6], args[7], args[8], args[9]);
}
