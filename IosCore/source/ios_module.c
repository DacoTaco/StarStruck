/*
	starstruck - a Free Software reimplementation for the Nintendo/BroadOn IOS.
	ios module template

Copyright (C) 2021	DacoTaco

# This code is licensed to you under the terms of the GNU GPL, version 2;
# see file COPYING or http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt
*/

#include "ios_module.h"

void* ExposeInnerAddress(unsigned int base_address, void* addr)
{
	return (addr == NULL) ? NULL : (void*)(base_address + addr);
}