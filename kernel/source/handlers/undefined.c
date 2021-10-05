/*
	StarStruck - a Free Software reimplementation for the Nintendo/BroadOn IOS.
	undefined - undefined exception handler

	Copyright (C) 2021	DacoTaco

# This code is licensed to you under the terms of the GNU GPL, version 2;
# see file COPYING or http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt
*/

#include <ios/gecko.h>

#include "handlers/exception.h"
#include "handlers/syscall.h"

void undf_handler(unsigned instruction, unsigned *regs)
{
	//Nintendo's implementation of a syscall is actually an invalid instruction. 
	//the instruction is 0xE6000010 | (syscall_num << 5).
	//so if we do the reverse, we have a syscall
	u16 syscall = (instruction & 0xE6007FE0) >> 5;
	if( syscall > 0 )
	{
		gecko_printf("Nintendo syscall detected\n");
		return handle_syscall(syscall & 0xFF, regs);
	}
			
	//actual invalid instruction lol.
	exc_handler(1, 0, regs);
}