/*
	starstruck - a Free Software reimplementation for the Nintendo/BroadOn IOS.
	threads - manage threads on the starlet

	Copyright (C) 2021	DacoTaco

# This code is licensed to you under the terms of the GNU GPL, version 2;
# see file COPYING or http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt
*/

#ifndef __KERNEL_USB_H__
#define __KERNEL_USB_H__

#include <types.h>

//unknown usb host controller registers. once known -> hollywood.h
#define		USB_REG_A4		(USB_REG_BASE+0x00a4)
#define		USB_REG_B0		(USB_REG_BASE+0x00b0)
#define		USB_REG_B4		(USB_REG_BASE+0x00b4)
#define		USB_REG_CC		(USB_REG_BASE+0x00cc)

void ConfigureUsbController(u32 hardwareRevision);

#endif

