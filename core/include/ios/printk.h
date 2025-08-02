/*
	StarStruck - a Free Software reimplementation for the Nintendo/BroadOn IOS.
	printk - printk implementation in ios

	Copyright (C) 2022	DacoTaco

# This code is licensed to you under the terms of the GNU GPL, version 2;
# see file COPYING or http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt
*/

#ifndef __PRINTK_H__
#define __PRINTK_H__

int printk(const char *fmt, ...) __attribute__((format(printf, 1, 2)));

#endif