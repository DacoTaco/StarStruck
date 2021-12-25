/*
	starstruck - a Free Software reimplementation for the Nintendo/BroadOn IOS.
	threads - manage threads on the starlet

	Copyright (C) 2021	DacoTaco

# This code is licensed to you under the terms of the GNU GPL, version 2;
# see file COPYING or http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt
*/

#ifndef __KERNEL_PLL_H__
#define __KERNEL_PLL_H__

#include <types.h>

void ConfigureAiPLL(u8 gcMode, u8 forceInit);
void ConfigureVideoInterfacePLL(u8 forceInit);
void ConfigureUsbHostPLL(void);

#endif