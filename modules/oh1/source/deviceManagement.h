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
#include "communications.h"

#define MAX_USB_DEVICES    4
#define MAX_USB_ENDPOINTS  16
#define MAX_USB_INTERFACES 8

typedef struct
{
	u32 Unused; /* probably meant to be used for the device ID */
	u16 VendorId;
	u16 ProductId;
} DeviceListEntry;
CHECK_SIZE(DeviceListEntry, 0x08);
CHECK_OFFSET(DeviceListEntry, 0x00, Unused);
CHECK_OFFSET(DeviceListEntry, 0x04, VendorId);
CHECK_OFFSET(DeviceListEntry, 0x06, ProductId);

typedef struct
{
	u8 InterfaceNumber;
	u8 AlternateSetting;
	u8 InterfaceClass;
	u8 InterfaceSubClass;
	u8 InterfaceProtocol;
} ALIGNED(4) DeviceInterface;
CHECK_SIZE(DeviceInterface, 0x08);
CHECK_OFFSET(DeviceInterface, 0x00, InterfaceNumber);
CHECK_OFFSET(DeviceInterface, 0x01, AlternateSetting);
CHECK_OFFSET(DeviceInterface, 0x02, InterfaceClass);
CHECK_OFFSET(DeviceInterface, 0x03, InterfaceSubClass);
CHECK_OFFSET(DeviceInterface, 0x04, InterfaceProtocol);

typedef struct
{
	u8 EndpointAddress;
	u8 Attributes;
	u16 MaxPacketSize;
	u8 Interval;
	OhciEndpointDescriptor *Descriptor;
	WiiTransferDescriptor *TransferDescriptor;
	OhciTransferDescriptorIsoc *IsocTransferDescriptor;
} DeviceEndpoint;
CHECK_SIZE(DeviceEndpoint, 0x14);
CHECK_OFFSET(DeviceEndpoint, 0x00, EndpointAddress);
CHECK_OFFSET(DeviceEndpoint, 0x01, Attributes);
CHECK_OFFSET(DeviceEndpoint, 0x02, MaxPacketSize);
CHECK_OFFSET(DeviceEndpoint, 0x04, Interval);
CHECK_OFFSET(DeviceEndpoint, 0x08, Descriptor);
CHECK_OFFSET(DeviceEndpoint, 0x0C, TransferDescriptor);
CHECK_OFFSET(DeviceEndpoint, 0x10, IsocTransferDescriptor);

typedef enum
{
	DEV_TYPE_NONE = 0,
	DEV_TYPE_HUB,
	DEV_TYPE_DEVICE,
} DeviceType;

typedef struct
{
	DeviceType DeviceType;
	u8 Zero;
	u8 PortIndex;
	u8 LowSpeed2;  // unused
	u8 IsLowSpeed;
	u8 MaxPower;
	u16 VendorId;
	u16 ProductId;
	u8 DeviceClass;
	u8 DeviceSubClass;
	u8 DeviceProtocol;
	u8 NumberOfInterfaces;
	IpcMessage *IpcMessage;
	DeviceInterface Interfaces[MAX_USB_INTERFACES];
	DeviceEndpoint Endpoints[MAX_USB_ENDPOINTS];
} OH1Device;

CHECK_SIZE(OH1Device, 0x194);
CHECK_OFFSET(OH1Device, 0x00, DeviceType);
CHECK_OFFSET(OH1Device, 0x01, Zero);
CHECK_OFFSET(OH1Device, 0x02, PortIndex);
CHECK_OFFSET(OH1Device, 0x03, LowSpeed2);
CHECK_OFFSET(OH1Device, 0x04, IsLowSpeed);
CHECK_OFFSET(OH1Device, 0x05, MaxPower);
CHECK_OFFSET(OH1Device, 0x06, VendorId);
CHECK_OFFSET(OH1Device, 0x08, ProductId);
CHECK_OFFSET(OH1Device, 0x0A, DeviceClass);
CHECK_OFFSET(OH1Device, 0x0B, DeviceSubClass);
CHECK_OFFSET(OH1Device, 0x0C, DeviceProtocol);
CHECK_OFFSET(OH1Device, 0x0D, NumberOfInterfaces);
CHECK_OFFSET(OH1Device, 0x10, IpcMessage);
CHECK_OFFSET(OH1Device, 0x14, Interfaces);
CHECK_OFFSET(OH1Device, 0x54, Endpoints);

extern OH1Device Devices[MAX_USB_DEVICES];

/* Sets the FunctionAddress field of the endpoint descriptor to be the value of
 * the device index. */
void SetDeviceFunctionAddress(s8 deviceIndex);
s8 FindEndpointIndex(s8 deviceIndex, u8 endpointAddress);
s8 GetDeviceIndex(u16 vid, u16 pid);
int GetDeviceVendorAndProduct(s8 deviceIndex, u16 *vendor, u16 *product);
u8 GetPortIndex(s8 deviceIndex);
void SetMaxPacketSize(s8 deviceIndex, u8 maxPacketSize);
void SetMaxPower(s8 deviceIndex, u8 maxPower);
void SetDeviceVendor(s8 deviceIndex, u16 vendorId);
void SetDeviceProductId(s8 deviceIndex, u16 productId);
void SetDeviceClass(s8 deviceIndex, u8 class, u8 subclass, u8 protocol);
int SetDeviceIPCMessage(s8 deviceIndex, IpcMessage *message);
void AddDeviceToDevList(DeviceListEntry *deviceList, u8 entryIndex, s8 deviceIndex);
int PopulateDeviceList(DeviceListEntry *deviceList, u8 maxCount,
                       u8 interfaceClass, u8 *countOutput);
void AddInterface(s8 deviceIndex, u8 interfaceNumber, u8 alternateSetting,
                  u8 class, u8 subClass, u8 protocol);