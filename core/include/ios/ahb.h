/*
	mini - a Free Software replacement for the Nintendo/BroadOn IOS.
	memory management, MMU, caches, and flushing

Copyright (C) 2008, 2009	Hector Martin "marcan" <marcan@marcansoft.com>

# This code is licensed to you under the terms of the GNU GPL, version 2;
# see file COPYING or http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt
*/

#ifndef __IOS_AHB_H__
#define __IOS_AHB_H__

typedef enum
{
	AHB_STARLET = 0, //or MEM2 or some controller or bus or ??
	AHB_1 = 1, //ppc or something else???
	AHB_UNKN2 = 2, //Unknown
	AHB_NAND = 3,
	AHB_AES = 4,
	AHB_SHA1 = 5,
	AHB_UNKN6 = 6, //Unknown
	AHB_UNKN7 = 7, //Unknown
	AHB_UNKN8 = 8, //Unknown
	AHB_SDHC = 9,
	AHB_UNKN10 = 0x0A, //Unknown
	AHB_UNKN11 = 0x0B, //Unknown
	AHB_UNKN12 = 0x0C, //Unknown
} AHBDEV;

#endif