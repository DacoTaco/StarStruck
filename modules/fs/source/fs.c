/*
	StarStruck - a Free Software reimplementation for the Nintendo/BroadOn IOS.
	Copyright (C) 2022	DacoTaco

# This code is licensed to you under the terms of the GNU GPL, version 2;
# see file COPYING or http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt
*/

#include <ios/syscalls.h>

#include "fs.h"

int main(void)
{
	OSPrintk("Hello from FS!");
	while(1)
	{
		OSYieldThread();
	}
	return 0;
}