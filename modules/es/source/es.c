/*
	StarStruck - a Free Software reimplementation for the Nintendo/BroadOn IOS.
	printk - printk implementation in ios

	Copyright (C) 2022	DacoTaco

# This code is licensed to you under the terms of the GNU GPL, version 2;
# see file COPYING or http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt
*/

#include <ios/module.h>
#include <ios/syscalls.h>

#include "es.h"

int main(void)
{
	OSPrintk("Hello from ES!");
	while(1)
	{
		OSYieldThread();
	}
	return 0;
}