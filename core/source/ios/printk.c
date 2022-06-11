/*
	StarStruck - a Free Software reimplementation for the Nintendo/BroadOn IOS.
	printk - printk implementation in ios

	Copyright (C) 2022	DacoTaco

# This code is licensed to you under the terms of the GNU GPL, version 2;
# see file COPYING or http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt
*/

#include "types.h"
#include "vsprintf.h"
#include "string.h"

#include "ios/syscalls.h"
#include "ios/printk.h"

int printk(const char *fmt, ...)
{
	va_list args;
	char buffer[256];
	memset(buffer, 0, sizeof(buffer));

	va_start(args, fmt);
	int len = vsnprintf(buffer, sizeof(buffer), fmt, args);
	va_end(args);

	os_printk(buffer);
	return len;
}