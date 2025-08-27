/*
	starstruck - a Free Software reimplementation for the Nintendo/BroadOn IOS.
	keyring - key management

	Copyright (C) 2025	DacoTaco

# This code is licensed to you under the terms of the GNU GPL, version 2;
# see file COPYING or http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt
*/

#pragma once

#define MAX_DEVICES  32
#define IRQ_TIMER    0
#define IRQ_NAND     1
#define IRQ_AES      2
#define IRQ_SHA1     3
#define IRQ_EHCI     4
#define IRQ_OHCI0    5
#define IRQ_OHCI1    6
#define IRQ_SDHC     7
#define IRQ_WIFI     8
#define IRQ_GPIO1B   10
#define IRQ_GPIO1    11
#define IRQ_UNKN12   12
#define IRQ_UNKNMIOS 15
#define IRQ_RESET    17
#define IRQ_DI       18
#define IRQ_PPCIPC   30
#define IRQ_IPC      31