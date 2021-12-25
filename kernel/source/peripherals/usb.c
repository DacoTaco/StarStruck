/*
	starstruck - a Free Software reimplementation for the Nintendo/BroadOn IOS.
	threads - manage threads on the starlet

	Copyright (C) 2021	DacoTaco

# This code is licensed to you under the terms of the GNU GPL, version 2;
# see file COPYING or http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt
*/

#include <types.h>
#include <ios/processor.h>

#include "core/hollywood.h"
#include "core/pll.h"
#include "peripherals/usb.h"
#include "utils.h"

static u8 _usbConfigurations[] = { 0x02, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 0x05, 0x0F, 0x00, 0x00 };

void _configureUsbController(u32 hardwareRevision)
{
	if (hardwareRevision > 2)
		hardwareRevision = 2;
	
	//IOS does something weird here. it has an array somewhere with the following data
	//data : 02 00 00 00 04 00 00 00 05 0F 00 00
	//yet it only loads 1 or 2 bytes? o.O
	//code in ghidra : 
	/* 	_USB_REG_B0 = (((byte)ARRAY_ffff9ea8[hardwareRevision * 4] & 0xf) * 5 - 4 & 0xff) << 8 | 0x20000;
		_USB_REG_B4 = ((byte)ARRAY_ffff9ea8[hardwareRevision * 4] & 0xf) << 8 | ((byte)ARRAY_ffff9ea8[hardwareRevision * 4 + 1] & 0xf) << 0x17 | 0x2014;*/
		
	//result on wii : rev 1 -> B0 = 0x21018, B4 : 0x2414	
	u32 index = hardwareRevision << 2;
	u8 value = _usbConfigurations[index] & 0x0F;
	u8 subValue = _usbConfigurations[index+1] & 0x0F;

	//yay, usb controller magic values!
	set32(USB_REG_B0, ((((value * 5) -4) & 0xFF) << 8) | 0x20000);	
	set32(USB_REG_B4, value << 8 | subValue << 0x17 | 0x2014);
}

void ConfigureUsbController(u32 hardwareRevision)
{
	set8(HW_USBFRCRST, 0xFE);
	udelay(2);
	
	ConfigureUsbHostPLL();
	
	set8(HW_USBFRCRST, 0xF6);
	udelay(50);
	set8(HW_USBFRCRST, 0xF4);
	udelay(1);
	set8(HW_USBFRCRST, 0xF0);
	udelay(1);
	set8(HW_USBFRCRST, 0x70);
	udelay(1);
	set8(HW_USBFRCRST, 0x60);
	udelay(1);
	set8(HW_USBFRCRST, 0x40);
	udelay(1);
	set8(HW_USBFRCRST, 0x00);
	udelay(1);

	_configureUsbController(hardwareRevision);
	if(hardwareRevision < 2)
	{
		set8(USB_REG_A4, 0x26);
		udelay(1);
		set16(USB_REG_A4, 0x2026);
		udelay(1);
		set16(USB_REG_A4, 0x4026);
	}
	else
	{
		set8(USB_REG_A4, 0x23);
		udelay(1);
		set16(USB_REG_A4, 0x2023);
		udelay(1);
		set16(USB_REG_A4, 0x4023);
	}
	
	udelay(20);
	set16(USB_REG_CC, 0x111);
}