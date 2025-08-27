/*
        StarStruck - a Free Software reimplementation for the Nintendo/BroadOn
IOS. oh1 - usb ohci implementation in ios

        Copyright (C) 2025	Alberto Mardegan <mardy@users.sourceforge.net>

# This code is licensed to you under the terms of the GNU GPL, version 2;
# see file COPYING or http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt
*/

#pragma once

#include <types.h>
#include "communications.h"

extern s32 _moduleHeap;

s32 CreateHeap(void);
void FreeMemory(void *ptr);
bool IsTransferDescriptorOnHead(void *ptr);
void *ValidateMemoryAddress(void *ptr);
OhciEndpointDescriptor *AllocateEndpointDescriptor(void);
WiiTransferDescriptor *AllocateTransferDescriptor(void);
OhciTransferDescriptorIsoc *AllocateIsocTransferDescriptor(void);
void CleanupIORequest(IORequestPacket *ioRequest);