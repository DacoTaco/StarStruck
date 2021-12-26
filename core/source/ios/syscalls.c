/*
	starstruck - a Free Software reimplementation for the Nintendo/BroadOn IOS.
	syscallcore - internal communications over software interrupts

	Copyright (C) 2021	DacoTaco

# This code is licensed to you under the terms of the GNU GPL, version 2;
# see file COPYING or http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt
*/

#include "ios/processor.h"
#include "ios/syscalls.h"

#define SYSCALL_CREATETHREAD			0x0000
#define SYSCALL_STOPTHREAD				0x0002
#define SYSCALL_STARTTHREAD				0x0005
#define SYSCALL_YIELDTHREAD				0x0007
#define SYSCALL_GETTHREADPRIORITY		0x0008
#define SYSCALL_SETTHREADPRIORITY		0x0009
#define SYSCALL_CREATEMESSAGEQUEUE		0x000A
#define SYSCALL_DESTROYMESSAGEQUEUE		0x000B
#define SYSCALL_SENDMESSAGE				0x000C
#define SYSCALL_RECEIVEMESSAGE			0x000E
#define SYSCALL_REGISTEREVENTHANDLER	0x000F
#define SYSCALL_UNREGISTEREVENTHANDLER	0x0010
#define SYSCALL_CREATEHEAP				0x0016
#define SYSCALL_DESTROYHEAP				0x0017
#define SYSCALL_MALLOC					0x0018
#define SYSCALL_MEMALIGN				0x0019
#define SYSCALL_MEMFREE					0x001A

//We implement syscalls using the SVC/SWI instruction. 
//Nintendo/IOS however was using undefined instructions and just caught those in their exception handler lol
#define _syscall_base(syscall, param_asm) \
	__asm__ volatile \
	(\
		"swi "syscall \
		: "=r"  (ret)\
		: param_asm \
		: "memory"\
	)
	
#define _syscall_base_asm6 "r"  (par1), "r"  (par2), "r"  (par3), "r"  (par4), "r"  (par5), "r"  (par6)
#define _syscall6(syscall, returnType, pType1, parameter1, pType2, parameter2, pType3, parameter3, pType4, parameter4, pType5, parameter5, pType6, parameter6) \
	register returnType ret	 	__asm__("r0"); \
	register pType1 par1	 	__asm__("r0") = parameter1; \
	register pType2 par2		__asm__("r1") = parameter2; \
	register pType3 par3		__asm__("r2") = parameter3; \
	register pType4 par4		__asm__("r3") = parameter4; \
	register pType5 par5		__asm__("r4") = parameter5; \
	register pType6 par6		__asm__("r5") = parameter6; \
	\
	_syscall_base(syscall, _syscall_base_asm6);
	
#define _syscall_base_asm5 "r"  (par1), "r"  (par2), "r"  (par3), "r"  (par4), "r"  (par5)
#define _syscall5(syscall, returnType, pType1, parameter1, pType2, parameter2, pType3, parameter3, pType4, parameter4, pType5, parameter5) \
	register returnType ret	 	__asm__("r0"); \
	register pType1 par1	 	__asm__("r0") = parameter1; \
	register pType2 par2		__asm__("r1") = parameter2; \
	register pType3 par3		__asm__("r2") = parameter3; \
	register pType4 par4		__asm__("r3") = parameter4; \
	register pType5 par5		__asm__("r4") = parameter5; \
	\
	_syscall_base(syscall, _syscall_base_asm5);
	
#define _syscall_base_asm4 "r"  (par1), "r"  (par2), "r"  (par3), "r"  (par4)
#define _syscall4(syscall, returnType, pType1, parameter1, pType2, parameter2, pType3, parameter3, pType4, parameter4) \
	register returnType ret	 	__asm__("r0"); \
	register pType1 par1	 	__asm__("r0") = parameter1; \
	register pType2 par2		__asm__("r1") = parameter2; \
	register pType3 par3		__asm__("r2") = parameter3; \
	register pType4 par4		__asm__("r3") = parameter4; \
	\
	_syscall_base(syscall, _syscall_base_asm4);

#define _syscall_base_asm3 "r"  (par1), "r"  (par2), "r"  (par3)
#define _syscall3(syscall, returnType, pType1, parameter1, pType2, parameter2, pType3, parameter3) \
	register returnType ret	 	__asm__("r0"); \
	register pType1 par1	 	__asm__("r0") = parameter1; \
	register pType2 par2		__asm__("r1") = parameter2; \
	register pType3 par3		__asm__("r2") = parameter3; \
	\
	_syscall_base(syscall, _syscall_base_asm3);

#define _syscall_base_asm2 "r"  (par1), "r"  (par2)
#define _syscall2(syscall, returnType, pType1, parameter1, pType2, parameter2) \
	register returnType ret	 	__asm__("r0"); \
	register pType1 par1	 	__asm__("r0") = parameter1; \
	register pType2 par2		__asm__("r1") = parameter2; \
	\
	_syscall_base(syscall, _syscall_base_asm2);

#define _syscall_base_asm1 "r"  (par1)
#define _syscall1(syscall, returnType, pType1, parameter1) \
	register returnType ret	 	__asm__("r0"); \
	register pType1 par1	 	__asm__("r0") = parameter1; \
	\
	_syscall_base(syscall, _syscall_base_asm1);
		
#define _syscall_base_asm0 
#define _syscall0(syscall, returnType) \
	register returnType ret	 	__asm__("r0"); \
	\
	_syscall_base(syscall, _syscall_base_asm0);

s32 os_createThread(s32 main, void *arg, u32 *stack_top, u32 stacksize, s32 priority, u32 detached)
{
	_syscall6(STR(SYSCALL_CREATETHREAD), s32, s32, main, void*, arg, u32*, stack_top, u32, stacksize, s32, priority, u32, detached);	
	return ret;
}

s32 os_stopThread( s32 threadid, u32 return_value )
{
	_syscall2(STR(SYSCALL_STOPTHREAD), s32, s32, threadid, u32, return_value);	
	return ret;
}

s32 os_startThread( s32 threadid )
{
	_syscall1(STR(SYSCALL_STARTTHREAD), s32, s32, threadid);	
	return ret;
}

void os_yieldThread( void )
{
	_syscall0(STR(SYSCALL_YIELDTHREAD), s32);	
	return;
}

s32 os_getThreadPriority( s32 threadid )
{
	_syscall1(STR(SYSCALL_GETTHREADPRIORITY), s32, s32, threadid);	
	return ret;
}

s32 os_setThreadPriority( s32 threadid, s32 priority )
{
	_syscall2(STR(SYSCALL_SETTHREADPRIORITY), s32, s32, threadid, s32, priority);	
	return ret;
}

s32 os_createMessageQueue(void *ptr, u32 size)
{
	_syscall2(STR(SYSCALL_CREATEMESSAGEQUEUE), s32, void*, ptr, u32, size);	
	return ret;
}

s32 os_destroyMessageQueue(s32 queueid)
{
	_syscall1(STR(SYSCALL_DESTROYMESSAGEQUEUE), s32, s32, queueid);
	return ret;
}

s32 os_sendMessage(s32 queueid, void *message, u32 flags)
{
	_syscall3(STR(SYSCALL_SENDMESSAGE), s32, s32, queueid, void*, message, u32, flags);
	return ret;
}
s32 os_receiveMessage(s32 queueid, void *message, u32 flags)
{
	_syscall3(STR(SYSCALL_RECEIVEMESSAGE), s32, s32, queueid, void *, message, u32, flags);
	return ret;
}

s32 os_registerEventHandler(u8 device, s32 queueid, s32 message)
{
	_syscall3(STR(SYSCALL_REGISTEREVENTHANDLER), s32, u8, device, s32, queueid, s32, message);
	return ret;
}

s32 os_unregisterEventHandler(u8 device)
{
	_syscall1(STR(SYSCALL_UNREGISTEREVENTHANDLER), s32, u8, device);
	return ret;
}

s32 os_createHeap(void *ptr, u32 size)
{
	_syscall2(STR(SYSCALL_CREATEHEAP), s32, void*, ptr, u32, size);	
	return ret;
}

s32 os_destroyHeap(s32 heapid)
{
	_syscall1(STR(SYSCALL_DESTROYHEAP), s32, s32, heapid);
	return ret;
}
	
void* os_allocateMemory(s32 heapid, u32 size)
{
	_syscall2(STR(SYSCALL_MALLOC), void*, s32, heapid, u32, size);
	return ret;
}

void* os_alignedAllocateMemory(s32 heapid, u32 size, u32 align)
{
	_syscall3(STR(SYSCALL_MEMALIGN), void*, s32, heapid, u32, size, u32, align);
	return ret;
}

s32 os_freeMemory(s32 heapid, void *ptr)
{
	_syscall2(STR(SYSCALL_MEMFREE), s32, s32, heapid, void*, ptr);
	return ret;
}