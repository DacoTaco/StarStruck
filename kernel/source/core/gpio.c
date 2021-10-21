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
#include "core/gpio.h"

#include "utils.h"

void _initGPIO()
{
	//set who (ARM or PPC and ARM) can access which GPIO pin
	//1 = PPC & Starlet
	//0 = Starlet only
	set32(HW_GPIO1OWNER, (read32(HW_GPIO1OWNER) & ( 0xFF000000 | GP_PUBLIC )) | GP_OWNER_PPC );

	//set output values & directions of the starlet registers
	mask32(HW_GPIO1OUT, GP_ARM_OUTPUTS, GP_ARM_DEFAULT_ON);
	set32(HW_GPIO1DIR, read32(HW_GPIO1DIR) | GP_ARM_OUTPUTS);
	
	//set output values & directions for the PPC registers
	mask32(HW_GPIO1BOUT, GP_PPC_OUTPUTS, 0);
	set32(HW_GPIO1BDIR, (read32(HW_GPIO1BDIR) & (0xFF000000 | GP_PUBLIC) ) | GP_PPC_OUTPUTS );
	
	//enable all gpio
	set32(HW_GPIO1ENABLE, GP_ALL);
	udelay(2000);
	
	//enable the power interrupt
	set32(HW_GPIO1INTENABLE, read32(HW_GPIO1INTENABLE) | GP_POWER );
	set32(HW_GPIO1INTLVL, read32(HW_GPIO1INTLVL) | GP_POWER );
}

void ConfigureGPIO(void)
{
	_initGPIO();
	
	//why ios, why. you just enabled them all? -_-
	set32(HW_GPIO1ENABLE, read32(HW_GPIO1ENABLE) | GP_EEPROM);
	
	//setup eeprom pins
	set32(HW_GPIO1OUT, read32(HW_GPIO1OUT) & (( 0xFF000000 | GP_ALL )^ GP_EEPROM) );
	
	//set eeprom gpio direction
	set32(HW_GPIO1DIR, read32(HW_GPIO1DIR) | (GP_EEP_MOSI | GP_EEP_CLK | GP_EEP_CS));
	
	//enable eeprom interrupts
	set32(HW_GPIO1INTENABLE, read32(HW_GPIO1INTENABLE) & (( 0xFF000000 | GP_ALL )^ GP_EEPROM) );
}

void ResetGPIODevices(void)
{
	//enable devices
	set32(HW_GPIO1OUT, read32(HW_GPIO1OUT) & (( 0xFF000000 | GP_ALL ) ^ GP_DISPIN));
	
	//wakeup/reset all devices except some cpu stuff?
	set32(HW_RESETS, read32(HW_RESETS) | (0x0FFFFFFF ^ (HW_RST_UNKN1 | RSTB_CPU | SRSTB_CPU)) );
}