/*
	starstruck - a Free Software reimplementation for the Nintendo/BroadOn IOS.
	syscallcore - internal communications over software interrupts

	Copyright (C) 2021	DacoTaco

# This code is licensed to you under the terms of the GNU GPL, version 2;
# see file COPYING or http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt
*/

#include "types.h"

#define SYSCALL_MEMALIGN		0x0001
#define SYSCALL_MEMFREE			0x0002

void* os_allocateMemory(u32 size);
void os_freeMemory(void* ptr);