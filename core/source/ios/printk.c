/*
	StarStruck - a Free Software reimplementation for the Nintendo/BroadOn IOS.
	printk - printk implementation in ios

	Copyright (C) 2022	DacoTaco

# This code is licensed to you under the terms of the GNU GPL, version 2;
# see file COPYING or http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt
*/

#include <stdio.h>
#include <stdarg.h>
#include <string.h>

#include "types.h"
#include "ios/syscalls.h"
#include "ios/printk.h"

int printk(const char *fmt, ...)
{
	va_list args;
	char buffer[256];
	memset(buffer, 0, sizeof(buffer));

	va_start(args, fmt);
	s32 len = vsnprintf(buffer, sizeof(buffer), fmt, args);
	va_end(args);

 //nintendo's debug interface is super fun
 //it expects data to be sent in chunks of 16 bytes
 //it buffers this untill a newline is sent.
 //since we are sending a string, we will send 15 bytes + null byte.
	//in the end, we will end it all with a newline + null byte, if our string did not have any
	s32 index = 0;
	char syscallBuffer[16] = { 0 };
	while (index < len)
	{
		s32 chunkSize = index + 15 <= len ? 15 : len - index;
		memset(syscallBuffer, 0, 16);
		memcpy(syscallBuffer, &buffer[index], (u32)chunkSize);
		OSPrintk(syscallBuffer);

		index += chunkSize;
	}

	if (len > 0 && buffer[len - 1] != '\n')
		OSPrintk("\n\0");

	return len;
}