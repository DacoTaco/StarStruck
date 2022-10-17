/*
	StarStruck - a Free Software reimplementation for the Nintendo/BroadOn IOS.
	undefined - undefined exception handler

	Copyright (C) 2021	DacoTaco

# This code is licensed to you under the terms of the GNU GPL, version 2;
# see file COPYING or http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt
*/

#include <ios/gecko.h>

#include "interrupt/exception.h"
#include "interrupt/syscall.h"

s32 undf_handler(unsigned instruction, ThreadContext* regs)
{
	//Nintendo's implementation of a syscall is actually an invalid instruction. 
	//the instruction is 0xE6000010 | (syscall_num << 5).
	//so if we do the reverse, we have a syscall
	u16 syscall = (instruction & 0xE6007FE0) >> 5;
	if( instruction > 0xE6007FE0 && syscall < 0xFF )
	{
		gecko_printf("Nintendo syscall detected ( 0x%08X - %04X ) @ 0x%08X\n", instruction, syscall, regs->ProgramCounter);
		s32 ret = HandleSyscall(syscall & 0xFF, regs);
		
		if(ret != -666)
			return ret;
		else
			gecko_printf("syscall handle failed, handling exception...\n");
	}
			
	//actual invalid instruction lol.
	ExceptionHandler(1, 0, (u32*)regs);
	return 0;
}