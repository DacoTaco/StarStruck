/*
	mini - a Free Software replacement for the Nintendo/BroadOn IOS.
	panic flash codes

Copyright (C) 2008, 2009	Hector Martin "marcan" <marcan@marcansoft.com>

# This code is licensed to you under the terms of the GNU GPL, version 2;
# see file COPYING or http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt
*/

#include <stdarg.h>
#include <vsprintf.h>
#include <ios/processor.h>
#include <ios/gecko.h>

#include "core/hollywood.h"
#include "core/gpio.h"
#include "interrupt/irq.h"
#include "utils.h"
#include "panic.h"

#define PANIC_ON	200000
#define PANIC_OFF	300000
#define PANIC_INTER	1000000

void panic(const char *fmt, ...)
{
	va_list args;
	char buffer[256];

	va_start(args, fmt);
	vsprintf(buffer, fmt, args);
	va_end(args);
	gecko_printf(buffer);

	DisableInterrupts();
	panic2(0, PANIC_EXCEPTION);
}

// figure out a use for mode...
void panic2(int mode, ...)
{
	int arg;
	va_list ap;

	clear32(HW_GPIO1OUT, GP_SLOTLED);
	clear32(HW_GPIO1DIR, GP_SLOTLED);
	clear32(HW_GPIO1OWNER, GP_SLOTLED);

	while(1) {
		va_start(ap, mode);
		
		while(1) {
			arg = va_arg(ap, int);
			if(arg < 0)
				break;
			set32(HW_GPIO1OUT, GP_SLOTLED);
			udelay(arg * PANIC_ON);
			clear32(HW_GPIO1OUT, GP_SLOTLED);
			udelay(PANIC_OFF);

			gecko_printf("PANIIIIIIIIC!!!");
			*(u32*)HW_RESETS &= ~RSTBINB;
		}
		
		va_end(ap);
		
		udelay(PANIC_INTER);
	}
}

