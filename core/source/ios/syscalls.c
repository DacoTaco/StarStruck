/*
	starstruck - a Free Software reimplementation for the Nintendo/BroadOn IOS.
	syscallcore - internal communications over software interrupts

	Copyright (C) 2021	DacoTaco

# This code is licensed to you under the terms of the GNU GPL, version 2;
# see file COPYING or http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt
*/

#include "ios/processor.h"
#include "ios/syscalls.h"

//We implement syscalls using the SVC/SWI instruction. 
//Nintendo/IOS however was using undefined instructions and just caught those in their exception handler lol
#define _syscall(syscall, returnValue, parameter1, parameter2, parameter3, parameter4, parameter5, parameter6){ \
	register s32 retValue 	__asm__("r0"); \
	register u32 par1	 	__asm__("r1") = parameter1; \
	register u32 par2		__asm__("r2") = parameter2; \
	register u32 par3		__asm__("r3") = parameter3; \
	register u32 par4		__asm__("r4") = parameter4; \
	register u32 par5		__asm__("r5") = parameter5; \
	register u32 par6		__asm__("r6") = parameter6; \
	\
	__asm__ volatile \
	(\
		"swi "syscall \
		: "=r"  (retValue)\
		: "r"  (par1), "r"  (par2), "r"  (par3), "r"  (par4), "r"  (par5), "r"  (par6)\
		: "memory"\
	); \
	if(returnValue != NULL)\
		*(s32*)returnValue = retValue;}

s32 os_createThread(s32 main, void *arg, u32 *stack_top, u32 stacksize, s32 priority, u32 detached)
{
	s32 ret = -1;
	_syscall(STR(SYSCALL_CREATETHREAD), &ret, main, (u32)arg, (u32)stack_top, stacksize, priority, detached);	
	return ret;
}

s32 os_stopThread( s32 threadid, u32 return_value )
{
	s32 ret = -1;
	_syscall(STR(SYSCALL_STOPTHREAD), &ret, threadid, return_value, 0, 0, 0, 0);	
	return ret;
}

s32 os_startThread( s32 threadid )
{
	s32 ret = -1;
	_syscall(STR(SYSCALL_STARTTHREAD), &ret, threadid, 0, 0, 0, 0, 0);	
	return ret;
}

s32 os_yieldThread( void )
{
	s32 ret = -1;
	_syscall(STR(SYSCALL_YIELDTHREAD), &ret, 0, 0, 0, 0, 0, 0);	
	return ret;
}

s32 os_getThreadPriority( s32 threadid )
{
	s32 ret = -1;
	_syscall(STR(SYSCALL_GETTHREADPRIORITY), &ret, threadid, 0, 0, 0, 0, 0);	
	return ret;
}

s32 os_setThreadPriority( s32 threadid, s32 priority )
{
	s32 ret = -1;
	_syscall(STR(SYSCALL_SETTHREADPRIORITY), &ret, threadid, priority, 0, 0, 0, 0);	
	return ret;
}

s32 os_createMessageQueue(void *ptr, u32 size)
{
	s32 ret = -1;
	_syscall(STR(SYSCALL_CREATEMESSAGEQUEUE), &ret, (u32)ptr, size, 0, 0, 0, 0);	
	return ret;
}

s32 os_destroyMessageQueue(s32 queueid)
{
	s32 ret = -1;
	_syscall(STR(SYSCALL_DESTROYMESSAGEQUEUE), &ret, queueid, 0, 0, 0, 0, 0);
	return ret;
}

s32 os_sendMessage(s32 queueid, void *message, u32 flags)
{
	s32 ret = -1;
	_syscall(STR(SYSCALL_SENDMESSAGE), &ret, queueid, (u32)message, flags, 0, 0, 0);
	return ret;
}
s32 os_receiveMessage(s32 queueid, void *message, u32 flags)
{
	s32 ret = -1;
	_syscall(STR(SYSCALL_RECEIVEMESSAGE), &ret, queueid, (u32)message, flags, 0, 0, 0);
	return ret;
}

s32 os_registerEventHandler(u8 device, s32 queueid, s32 message)
{
	s32 ret = -1;
	_syscall(STR(SYSCALL_REGISTEREVENTHANDLER), &ret, device, queueid, message, 0, 0, 0);
	return ret;
}

s32 os_unregisterEventHandler(u8 device)
{
	s32 ret = -1;
	_syscall(STR(SYSCALL_UNREGISTEREVENTHANDLER), &ret, device, 0, 0, 0, 0, 0);
	return ret;
}

s32 os_createHeap(void *ptr, u32 size)
{
	s32 ret = -1;
	_syscall(STR(SYSCALL_CREATEHEAP), &ret, (u32)ptr, size, 0, 0, 0, 0);	
	return ret;
}

s32 os_destroyHeap(s32 heapid)
{
	s32 ret = -1;
	_syscall(STR(SYSCALL_DESTROYHEAP), &ret, heapid, 0, 0, 0, 0, 0);
	return ret;
}
	
void* os_allocateMemory(s32 heapid, u32 size)
{
	u32 ptr = 0;
	_syscall(STR(SYSCALL_MALLOC), &ptr, heapid, size, 0, 0, 0, 0);
	return (void*)ptr;
}

void* os_alignedAllocateMemory(s32 heapid, u32 size, u32 align)
{
	u32 ptr = 0;
	_syscall(STR(SYSCALL_MEMALIGN), &ptr, heapid, size, align, 0, 0, 0);
	return (void*)ptr;
}

s32 os_freeMemory(s32 heapid, void *ptr)
{
	s32 ret = -1;
	_syscall(STR(SYSCALL_MEMFREE), &ret, heapid, (s32)ptr, 0, 0, 0, 0);
	return ret;
}