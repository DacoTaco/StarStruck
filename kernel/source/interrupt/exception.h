/*
	mini - a Free Software replacement for the Nintendo/BroadOn IOS.
	exception handling

Copyright (C) 2008, 2009	Hector Martin "marcan" <marcan@marcansoft.com>
Copyright (C) 2008, 2009	Haxx Enterprises <bushing@gmail.com>

# This code is licensed to you under the terms of the GNU GPL, version 2;
# see file COPYING or http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt
*/

#ifndef __EXCEPTION_H__
#define __EXCEPTION_H__

#include <types.h>

void initializeExceptions(void);
void ExceptionHandler(u32 type, u32 spsr, u32 *regs);

#endif
