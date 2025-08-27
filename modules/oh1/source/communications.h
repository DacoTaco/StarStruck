/*
        StarStruck - a Free Software reimplementation for the Nintendo/BroadOn
IOS. oh1 - usb ohci implementation in ios

        Copyright (C) 2025	Alberto Mardegan <mardy@users.sourceforge.net>

# This code is licensed to you under the terms of the GNU GPL, version 2;
# see file COPYING or http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt
*/

#pragma once

#include <types.h>
#include <ios/ipc.h>
#include "usb/ohci.h"

#define USBV0_IOCTL_CTRLMSG        0x0
#define USBV0_IOCTL_BLKMSG         0x1
#define USBV0_IOCTL_INTRMSG        0x2
#define USBV0_IOCTL_SUSPENDDEV     0x5
#define USBV0_IOCTL_RESUMEDEV      0x6
#define USBV0_IOCTL_GETDEVLIST     0xc
#define USBV0_IOCTL_GETHUBSTATUS   0xf
#define USBV0_IOCTL_DEVREMOVALHOOK 0x1a
#define USBV0_IOCTL_GETPORTSTATUS  0x14
#define USBV0_IOCTL_SETPORTSTATUS  0x19

#define swap_ptr(ptr)              ((void *)__builtin_bswap32((u32)ptr))
#define swap_u32(value)            __builtin_bswap32(value)
#define swap_u16(value)            __builtin_bswap16(value)

typedef struct
{
	OhciTransferDescriptor std;
	struct IORequestPacket_t *IORequestPacket;
	u16 Length;
} WiiTransferDescriptor;

typedef struct IORequestPacket_t
{
	IpcMessage *RequestMessage;
	s8 Counter;
	u8 ErrorCount;
	u16 Transferred;
	char *MessageData;
	u32 Size;
	WiiTransferDescriptor *TransferDescriptor;
	OhciEndpointDescriptor *EndpointDescriptor;
	u32 Unused;
	s32 Queue;
	void *ControlMessage; /* Only set if queue == -1 */
} IORequestPacket;