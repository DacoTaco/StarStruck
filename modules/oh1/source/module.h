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
#include <usb/ohci.h>

#include "communications.h"

#define MASK_PTR(ptr, mask)         ((void *)((u32)ptr & (mask)))
#define PTR_WITH_BITS(ptr, bits)    ((void *)((u32)ptr | (bits)))
#define PADDED4_SIZEOF(type)        (((sizeof(type) + 3) / 4) * 4)

#define OH1_STATE_DEVICE_QUERIED    (1 << 0)
#define OH1_STATE_PROCESSING_EVENTS (1 << 1)
typedef struct
{
	volatile OhciRegs *HardwareRegisters;
	OhciHcca *Hcca;
	u32 FrameInterval;
	OhciEndpointDescriptor *EndpointDescriptors;
	s32 Timer;
	s32 TimerQueue;
	s32 QueueId;
	u32 TimerQueueBuffer;
	u8 DeviceEvent;
	u8 NumberOfDownstreamPorts;
	u32 State;
	u8 AHBDevice;
	u8 AHBDeviceToFlush;
} OH1ModuleControl;
CHECK_SIZE(OH1ModuleControl, 0x2C);
CHECK_OFFSET(OH1ModuleControl, 0x00, HardwareRegisters);
CHECK_OFFSET(OH1ModuleControl, 0x04, Hcca);
CHECK_OFFSET(OH1ModuleControl, 0x08, FrameInterval);
CHECK_OFFSET(OH1ModuleControl, 0x0C, EndpointDescriptors);
CHECK_OFFSET(OH1ModuleControl, 0x10, Timer);
CHECK_OFFSET(OH1ModuleControl, 0x14, TimerQueue);
CHECK_OFFSET(OH1ModuleControl, 0x18, QueueId);
CHECK_OFFSET(OH1ModuleControl, 0x1C, TimerQueueBuffer);
CHECK_OFFSET(OH1ModuleControl, 0x20, DeviceEvent);
CHECK_OFFSET(OH1ModuleControl, 0x21, NumberOfDownstreamPorts);
CHECK_OFFSET(OH1ModuleControl, 0x24, State);
CHECK_OFFSET(OH1ModuleControl, 0x28, AHBDevice);
CHECK_OFFSET(OH1ModuleControl, 0x29, AHBDeviceToFlush);

int InitialiseModule(OH1ModuleControl *module);
int QueryModuleDevices(OH1ModuleControl *module);
int SleepModule(OH1ModuleControl *module, u32 timeout);
int ProcessControlMessage(OH1ModuleControl *module, IpcMessage *ipcMessage);
int ProcessInterruptBlockMessage(OH1ModuleControl *module, IpcMessage *ipcMessage);
void CloseEndpoint(OH1ModuleControl *module, OhciEndpointDescriptor *endpoint);
void DisableEndpoint(OH1ModuleControl *module, OhciEndpointDescriptor *endpoint);
void ResumeEndpoint(OH1ModuleControl *module, OhciEndpointDescriptor *endpoint);
int SuspendDevice(OH1ModuleControl *module, u8 port);
int ResumeDevice(OH1ModuleControl *module, u8 port);
void HandleStatusChange(OH1ModuleControl *module, int /*unused*/);
void CloseDevice(OH1ModuleControl *module, s8 deviceIndex);
