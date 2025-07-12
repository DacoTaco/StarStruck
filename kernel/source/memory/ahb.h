/*
	mini - a Free Software replacement for the Nintendo/BroadOn IOS.
	memory management, MMU, caches, and flushing

Copyright (C) 2008, 2009	Hector Martin "marcan" <marcan@marcansoft.com>

# This code is licensed to you under the terms of the GNU GPL, version 2;
# see file COPYING or http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt
*/

#ifndef __AHB_H__
#define __AHB_H__

#include <types.h>
#include <ios/ahb.h>

//this is just pure guessing. once confirmed -> hollywood.h
#define HW_AHB_BASE (HW_REG_BASE + 0xb0000)
#define HW_AHB_08   (HW_AHB_BASE + 0x08)
#define HW_AHB_10   (HW_AHB_BASE + 0x10)

//Syscalls
void AhbFlushFrom(AHBDEV type);
void AhbFlushTo(AHBDEV dev);

//Internal functions
void _ahb_flush_from(AHBDEV dev);
void _ahb_flush_to(AHBDEV dev);

#endif