/*
	StarStruck - a Free Software replacement for the Nintendo/BroadOn IOS.
	kernel defines

Copyright (C) 2021		DacoTaco

# This code is licensed to you under the terms of the GNU GPL, version 2;
# see file COPYING or http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt
*/

#define SRAM_BSS    __attribute__((section(".bss")))
#define SRAM_DATA   __attribute__((section(".data.sram")))
#define SRAM_RODATA __attribute__((section(".rodata")))
