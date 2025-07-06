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
 * documented in https://wiibrew.org/wiki/Hardware/USB_Host_Controller. Until
 * we have not double-checked them, we keep them as "unnamed".
 */
typedef struct
{
	u32 unnamed[51]; /* 51 * 4 = 204 (= 0xcc) */

 /* Mysterious register. The OH1 and OHCI0 modules write the 0xe1800 bits
     * into it during their initialization. */
	u32 chickenbits;
} EhciRegisters;

#endif
