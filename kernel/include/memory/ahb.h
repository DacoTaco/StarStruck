/*
	mini - a Free Software replacement for the Nintendo/BroadOn IOS.
	memory management, MMU, caches, and flushing

Copyright (C) 2008, 2009	Hector Martin "marcan" <marcan@marcansoft.com>

# This code is licensed to you under the terms of the GNU GPL, version 2;
# see file COPYING or http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt
*/

#ifndef __AHB_H__
#define __AHB_H__

#include "types.h"

// TODO: move to hollywood.h once we figure out WTF
#define		HW_100	(HW_REG_BASE + 0x100)
#define		HW_104	(HW_REG_BASE + 0x104)
#define		HW_108	(HW_REG_BASE + 0x108)
#define		HW_10c	(HW_REG_BASE + 0x10c)
#define		HW_110	(HW_REG_BASE + 0x110)
#define		HW_114	(HW_REG_BASE + 0x114)
#define		HW_118	(HW_REG_BASE + 0x118)
#define		HW_11c	(HW_REG_BASE + 0x11c)
#define		HW_120	(HW_REG_BASE + 0x120)
#define		HW_124	(HW_REG_BASE + 0x124)
#define		HW_130	(HW_REG_BASE + 0x130)
#define		HW_134	(HW_REG_BASE + 0x134)
#define		HW_138	(HW_REG_BASE + 0x138)
#define		HW_188	(HW_REG_BASE + 0x188)
#define		HW_18C	(HW_REG_BASE + 0x18c)

enum AHBDEV {
	AHB_STARLET = 0, //or MEM2 or some controller or bus or ??
	AHB_1 = 1, //ppc or something else???
	AHB_NAND = 3,
	AHB_AES = 4,
	AHB_SHA1 = 5,
	AHB_SDHC = 9,
};

void ahb_flush_from(enum AHBDEV dev);
void ahb_flush_to(enum AHBDEV dev);
void _ahb_flush_to(enum AHBDEV dev);

#endif