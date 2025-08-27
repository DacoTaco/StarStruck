/*
        StarStruck - a Free Software reimplementation for the Nintendo/BroadOn
IOS. printk - printk implementation in ios

        Copyright (C) 2025	Alberto Mardegan <mardy@users.sourceforge.net>

# This code is licensed to you under the terms of the GNU GPL, version 2;
# see file COPYING or http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt
*/

#include <ios/errno.h>
#include "deviceManagement.h"

OH1Device Devices[MAX_USB_DEVICES];

s8 FindEndpointIndex(s8 deviceIndex, u8 endpointAddress)
{
	for (s8 i = 1; i < MAX_USB_ENDPOINTS; i++)
	{
		if (Devices[deviceIndex].Endpoints[i].EndpointAddress == endpointAddress)
			return i;
	}
	return -1;
}

void SetDeviceFunctionAddress(s8 deviceIndex)
{
	OhciEndpointDescriptor *endpoint;

	endpoint = Devices[deviceIndex].Endpoints[0].Descriptor;
	u32 dw0 = swap_u32(endpoint->dw0);
	endpoint->dw0 = swap_u32(dw0 | ED_SET(FA, deviceIndex));
}

void SetMaxPacketSize(s8 deviceIndex, u8 maxPacketSize)
{
	OhciEndpointDescriptor *endpoint;

	endpoint = Devices[deviceIndex].Endpoints[0].Descriptor;
	u32 dw0 = swap_u32(endpoint->dw0);
	endpoint->dw0 = swap_u32(ED_CLEAR(MPS, dw0) | ED_SET(MPS, maxPacketSize));
	Devices[deviceIndex].Endpoints[0].MaxPacketSize = maxPacketSize;
}

void SetDeviceVendor(s8 deviceIndex, u16 vendorId)
{
	Devices[deviceIndex].VendorId = vendorId;
}

void SetDeviceProductId(s8 deviceIndex, u16 productId)
{
	Devices[deviceIndex].ProductId = productId;
}

void SetDeviceClass(s8 deviceIndex, u8 class, u8 subclass, u8 protocol)
{
	Devices[deviceIndex].DeviceClass = class;
	Devices[deviceIndex].DeviceSubClass = subclass;
	Devices[deviceIndex].DeviceProtocol = protocol;
}

void SetMaxPower(s8 deviceIndex, u8 maxPower)
{
	Devices[deviceIndex].MaxPower = maxPower;
}

int SetDeviceIPCMessage(s8 deviceIndex, IpcMessage *message)
{
	if (Devices[deviceIndex].IpcMessage)
		return IPC_EEXIST;

	Devices[deviceIndex].IpcMessage = message;
	return IPC_SUCCESS;
}

s8 GetDeviceIndex(u16 vid, u16 pid)
{
	for (s8 deviceIndex = 0; deviceIndex < MAX_USB_DEVICES; deviceIndex++)
	{
		if (Devices[deviceIndex].VendorId == vid && Devices[deviceIndex].ProductId == pid)
			return deviceIndex;
	}
	return -1;
}

int GetDeviceVendorAndProduct(s8 deviceIndex, u16 *vendor, u16 *product)
{
	if (deviceIndex > MAX_USB_DEVICES || Devices[deviceIndex].DeviceType == DEV_TYPE_NONE)
		return IPC_EINVAL;

	if (vendor)
		*vendor = Devices[deviceIndex].VendorId;
	if (product)
		*product = Devices[deviceIndex].ProductId;

	return IPC_SUCCESS;
}

u8 GetPortIndex(s8 deviceIndex)
{
	return Devices[deviceIndex].PortIndex;
}

void AddDeviceToDevList(DeviceListEntry *deviceList, u8 entryIndex, s8 deviceIndex)
{
	/* It would be more logical if the first 4 bytes of the destination buffer
	 * were filled with the device ID, but they are left uninitialized. This is
	 * consistent with how libogc operates in USB_GetDeviceList(): only the
	 * vendor and product ID are being read. */
	deviceList[entryIndex].VendorId = Devices[deviceIndex].VendorId;
	deviceList[entryIndex].ProductId = Devices[deviceIndex].ProductId;
}

int PopulateDeviceList(DeviceListEntry *deviceList, u8 maxCount,
                       u8 interfaceClass, u8 *countOutput)
{
	u8 interfaceIndex;
	s8 deviceIndex;
	u8 addedCount = 0;

	for (deviceIndex = 1; deviceIndex < MAX_USB_DEVICES; deviceIndex++)
	{
		if (Devices[deviceIndex].DeviceType == DEV_TYPE_NONE)
			continue;

		if (interfaceClass == 0 || Devices[deviceIndex].DeviceClass == interfaceClass)
		{
			AddDeviceToDevList(deviceList, addedCount++, deviceIndex);
		}
		else
		{
			for (interfaceIndex = 0; interfaceIndex < Devices[deviceIndex].NumberOfInterfaces;
			     interfaceIndex++)
			{
				if (Devices[deviceIndex].Interfaces[interfaceIndex].InterfaceClass == interfaceClass)
				{
					AddDeviceToDevList(deviceList, addedCount++, deviceIndex);
					break;
				}
			}
		}

		if (addedCount == maxCount)
			break;
	}
	*countOutput = addedCount;
	return IPC_SUCCESS;
}

void AddInterface(s8 deviceIndex, u8 interfaceNumber, u8 alternateSetting,
                  u8 class, u8 subClass, u8 protocol)
{
	int i = Devices[deviceIndex].NumberOfInterfaces;
	DeviceInterface *iface = &Devices[deviceIndex].Interfaces[i];
	if (iface->InterfaceClass != 0)
		return;

	iface->InterfaceNumber = interfaceNumber;
	iface->AlternateSetting = alternateSetting;
	iface->InterfaceSubClass = subClass;
	iface->InterfaceClass = class;
	iface->InterfaceProtocol = protocol;
	Devices[deviceIndex].NumberOfInterfaces++;
}