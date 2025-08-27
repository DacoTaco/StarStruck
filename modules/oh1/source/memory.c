/*
        StarStruck - a Free Software reimplementation for the Nintendo/BroadOn
IOS. oh1 - usb ohci implementation in ios

        Copyright (C) 2025	Alberto Mardegan <mardy@users.sourceforge.net>

# This code is licensed to you under the terms of the GNU GPL, version 2;
# see file COPYING or http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt
*/

#include <ios/errno.h>
#include <ios/syscalls.h>
#include <string.h>

#include "memory.h"

#define OH1_HEAP_BASE ((void *)0x13881400)
#define OH1_HEAP_SIZE 0x4000

s32 _moduleHeap;

static s32 CreateModuleHeap(void)
{
	memset(OH1_HEAP_BASE, 0, OH1_HEAP_SIZE);
	_moduleHeap = OSCreateHeap(OH1_HEAP_BASE, OH1_HEAP_SIZE);
	return _moduleHeap;
}

s32 CreateHeap(void)
{
	s32 heap_handle;

	heap_handle = CreateModuleHeap();
	if (heap_handle >= 0)
		return IPC_SUCCESS;

	//redundant if check, but whatever. you do you IOS xD
	if (_moduleHeap > 0)
		OSDestroyHeap(_moduleHeap);

	return IPC_EINVAL;
}

void FreeMemory(void *ptr)
{
	OSFreeMemory(_moduleHeap, ptr);
}

WiiTransferDescriptor *AllocateTransferDescriptor(void)
{
	WiiTransferDescriptor *transferDescriptor =
	    OSAlignedAllocateMemory(_moduleHeap, sizeof(WiiTransferDescriptor), 0x10);

	if (transferDescriptor)
		memset(transferDescriptor, 0, sizeof(WiiTransferDescriptor));

	return transferDescriptor;
}

bool IsTransferDescriptorOnHead(void *ptr)
{
	return ptr && ((u32)ptr & 0xf) == 0 && ptr >= OH1_HEAP_BASE &&
	       ptr < OH1_HEAP_BASE + OH1_HEAP_SIZE;
}

OhciTransferDescriptorIsoc *AllocateIsocTransferDescriptor(void)
{
	OhciTransferDescriptorIsoc *ptr;

	ptr = OSAlignedAllocateMemory(_moduleHeap, sizeof(OhciTransferDescriptorIsoc), 16);
	if (ptr)
		memset(ptr, 0, sizeof(OhciTransferDescriptorIsoc));

	return ptr;
}

OhciEndpointDescriptor *AllocateEndpointDescriptor(void)
{
	void *ptr;

	ptr = OSAlignedAllocateMemory(_moduleHeap, sizeof(OhciEndpointDescriptor), 16);
	if (ptr)
		memset(ptr, 0, sizeof(OhciEndpointDescriptor));

	return ptr;
}

void *ValidateMemoryAddress(void *ptr)
{
	//redirect SRAM to the dma address space
	if ((ptr >= (void *)0xffff0000) && (ptr != (void *)0xffffffff))
		ptr = (void *)((int)ptr + 0x0d410000);

	return ptr;
}

void CleanupIORequest(IORequestPacket *ioRequest)
{
	if (!ioRequest)
		return;

	if (ioRequest->Queue <= 0)
	{
		if (ioRequest->Size != 0)
			OSDCInvalidateRange(ioRequest->MessageData, ioRequest->Size);
		OSResourceReply(ioRequest->RequestMessage, ioRequest->Transferred);
	}
	else
	{
		OSSendMessage(ioRequest->Queue, (void *)(u32)ioRequest->Transferred, 0);
	}

	if (ioRequest->ControlMessage)
		OSFreeMemory(_moduleHeap, ioRequest->ControlMessage);

	/* Do we really need to zero this? */
	memset(ioRequest, 0, sizeof(*ioRequest));
	OSFreeMemory(_moduleHeap, ioRequest);
}