/*
	starstruck - a Free Software reimplementation for the Nintendo/BroadOn IOS.
	ios module template

Copyright (C) 2025	DacoTaco
Copyright (C) 2025	Alberto Mardegan <mardy@users.sourceforge.net>

# This code is licensed to you under the terms of the GNU GPL, version 2;
# see file COPYING or http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt
*/

#ifndef __USB_EHCI_H__
#define __USB_EHCI_H__

#include "types.h"

#define EHCI_REG_BASE         0x0d040000
#define EHCI_CHICKENBITS_INIT 0xe1800

/* This structure is located at address 0x0d040000 and its registers are
 * documented in https://wiibrew.org/wiki/Hardware/USB_Host_Controller. 
 * some of them had multiple names, so they were added as comments.
 */
typedef struct
{
	u32 CapLenVer; //0x0d040000
	u32 HcsParameters; //0x0d040004
	u32 HccParameters; //0x0d040008
	u32 HcspPortRoute; //0x0d04000c
	u32 UsbCommand; //0x0d040010
	u32 UsbStatus; //0x0d040014
	u32 UsbInterrupt; //0x0d040018 EHC0_PRUSBINTR or EHC0_USBINTR?
	u32 FrameIndex; //0x0d04001c FRINDEX
	u32 CtrlDsSegment; //0x0d040020
	u32 PeriodicListBase; //0x0d040024
	u32 AsyncIcListAddr; //0x0d040028
	u32 Unknown[0x09]; //0x0d04002c - 0x0d04004c
	u32 PrConfigFlag; //0x0d040050
	u32 PortControl; //0x0d040054 PORTSC or PORT_CTRL
	u32 Unknown2[0x0E]; //0x0d040058 - 0x0d04008C
	u32 MiscelaneousControl0; //0x0d040090
	u32 PacketBufferThreshold; //0x0d040094
	u32 PhysicalStatus0; //0x0d040098
	u32 PhysicalStatus1; //0x0d04009c
	u32 PhysicalStatus2; //0x0d0400a0
	u32 UtmiControl; //0x0d0400a4
	u32 BistControl; //0x0d0400a8
	u32 MiscelaneousControl1; //0x0d0400ac
	u32 PhycmnCal; //0x0d0400b0
	u32 PhysicalAdjacentControl; //0x0d0400b4
	u32 Unknown3[0x03]; //0x0d0400b8 - 0x0d0400c0
	u32 PacketBufferDepth; //0x0d0400c4
	u32 BreakMemoryXfr; //0x0d0400c8
	u32 ChickenBits; //0x0d0400cc
} EhciRegisters;

CHECK_SIZE(EhciRegisters, 0xD0);
CHECK_OFFSET(EhciRegisters, 0x00, CapLenVer);
CHECK_OFFSET(EhciRegisters, 0x04, HcsParameters);
CHECK_OFFSET(EhciRegisters, 0x08, HccParameters);
CHECK_OFFSET(EhciRegisters, 0x0C, HcspPortRoute);
CHECK_OFFSET(EhciRegisters, 0x10, UsbCommand);
CHECK_OFFSET(EhciRegisters, 0x14, UsbStatus);
CHECK_OFFSET(EhciRegisters, 0x18, UsbInterrupt);
CHECK_OFFSET(EhciRegisters, 0x1C, FrameIndex);
CHECK_OFFSET(EhciRegisters, 0x20, CtrlDsSegment);
CHECK_OFFSET(EhciRegisters, 0x24, PeriodicListBase);
CHECK_OFFSET(EhciRegisters, 0x28, AsyncIcListAddr);
CHECK_OFFSET(EhciRegisters, 0x2C, Unknown);
CHECK_OFFSET(EhciRegisters, 0x50, PrConfigFlag);
CHECK_OFFSET(EhciRegisters, 0x54, PortControl);
CHECK_OFFSET(EhciRegisters, 0x58, Unknown2);
CHECK_OFFSET(EhciRegisters, 0x90, MiscelaneousControl0);
CHECK_OFFSET(EhciRegisters, 0x94, PacketBufferThreshold);
CHECK_OFFSET(EhciRegisters, 0x98, PhysicalStatus0);
CHECK_OFFSET(EhciRegisters, 0x9C, PhysicalStatus1);
CHECK_OFFSET(EhciRegisters, 0xA0, PhysicalStatus2);
CHECK_OFFSET(EhciRegisters, 0xA4, UtmiControl);
CHECK_OFFSET(EhciRegisters, 0xA8, BistControl);
CHECK_OFFSET(EhciRegisters, 0xAC, MiscelaneousControl1);
CHECK_OFFSET(EhciRegisters, 0xB0, PhycmnCal);
CHECK_OFFSET(EhciRegisters, 0xB4, PhysicalAdjacentControl);
CHECK_OFFSET(EhciRegisters, 0xB8, Unknown3);
CHECK_OFFSET(EhciRegisters, 0xC4, PacketBufferDepth);
CHECK_OFFSET(EhciRegisters, 0xC8, BreakMemoryXfr);
CHECK_OFFSET(EhciRegisters, 0xCC, ChickenBits);

#endif
