/*
	mini - a Free Software replacement for the Nintendo/BroadOn IOS.
	random utilities

Copyright (C) 2008, 2009	Hector Martin "marcan" <marcan@marcansoft.com>

# This code is licensed to you under the terms of the GNU GPL, version 2;
# see file COPYING or http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt
*/

#include <stdarg.h>

#include "ios/processor.h"
#include "vsprintf.h"

int sprintf(char *buffer, const char *fmt, ...)
{
	va_list args;
	int i;

	va_start(args, fmt);
	i = vsprintf(buffer, fmt, args);
	va_end(args);
	return i;
}

