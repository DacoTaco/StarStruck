/*
        StarStruck - a Free Software reimplementation for the Nintendo/BroadOn
IOS. oh1 - usb ohci implementation in ios

        Copyright (C) 2025	Alberto Mardegan <mardy@users.sourceforge.net>

# This code is licensed to you under the terms of the GNU GPL, version 2;
# see file COPYING or http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt
*/

#include <ios/syscalls.h>
#include <ios/printk.h>
#include <ios/errno.h>
#include <ios/irq.h>
#include <string.h>

#include "module.h"
#include "memory.h"
#include "communications.h"
#include "deviceManagement.h"

#define OH1_TRANSFER_BASE ((void *)0x13880400)
#define OH1_TRANSFER_SIZE (0x1000)
static OhciEndpointDescriptor *_ohciInterruptEndpoints = OH1_TRANSFER_BASE;
static USBControlMessage *_controlRequest = NULL;
static u8 _endpointIndexes[] = { 62, 60, 56, 48, 32 };

static void EnableModuleInterrupts(OH1ModuleControl *module)
{
	module->HardwareRegisters->EnableInterrupts = OHCI_INTR_MIE;
}

static void DisableModuleInterrupts(OH1ModuleControl *module)
{
	module->HardwareRegisters->DisableInterrupts = OHCI_INTR_MIE;
}

static void AppendControlEndpoint(OH1ModuleControl *module, OhciEndpointDescriptor *endpoint)
{
	OhciEndpointDescriptor *controlHead;
	OhciEndpointDescriptor *next;

	controlHead = module->HardwareRegisters->ControlHeadEndpoint;
	if (controlHead == NULL)
	{
		module->HardwareRegisters->ControlHeadEndpoint = endpoint;
	}
	else
	{
		for (next = controlHead->Next; next != NULL && endpoint != controlHead;
		     next = controlHead->Next)
		{
			controlHead = swap_ptr(next);
		}

		if (controlHead != endpoint)
		{
			controlHead->Next = swap_ptr(endpoint);
		}
	}
}

static void AppendBulkEndpoint(OH1ModuleControl *module, OhciEndpointDescriptor *endpoint)
{
	OhciEndpointDescriptor *last;
	OhciEndpointDescriptor *lastSwapped;

	last = module->HardwareRegisters->BulkHeadEndpoint;
	if (!last)
	{
		module->HardwareRegisters->BulkHeadEndpoint = endpoint;
	}
	else
	{
		if (endpoint != last)
		{
			lastSwapped = last->Next;
			while ((lastSwapped != NULL && (last = swap_ptr(lastSwapped), endpoint != last)))
			{
				lastSwapped = last->Next;
			}
		}
		if (last != endpoint)
		{
			last->Next = swap_ptr(endpoint);
		}
	}
}

static void AppendEndpoint(OH1ModuleControl *module, OhciEndpointDescriptor *endpoint)
{
	OhciEndpointDescriptor *head;
	OhciEndpointDescriptor *nextEndpoint;

	head = module->EndpointDescriptors + _endpointIndexes[0];
	nextEndpoint = head->Next;
	while (nextEndpoint != NULL)
	{
		head = swap_ptr(nextEndpoint);
		nextEndpoint = head->Next;
	}
	head->Next = swap_ptr(endpoint);
}

static void FlushEndpoint(OH1ModuleControl *module, OhciEndpointDescriptor *endpoint)
{
	u32 dw0 = swap_u32(endpoint->dw0);
	endpoint->dw0 = swap_u32(dw0 & ~ED_SKIP);
	OSDCFlushRange(endpoint, sizeof(*endpoint));
	OSAhbFlushTo(module->AHBDevice);
}

static int ResetDevice(OH1ModuleControl *module, u8 port)
{
	int attempts = 4;

	if (module->HardwareRegisters->RootHubPortStatus[port] & RH_PS_WII31)
	{
		module->HardwareRegisters->RootHubPortStatus[port] = RH_PS_PRS;
		SleepModule(module, 20000);
		do
		{
			if (module->HardwareRegisters->RootHubPortStatus[port] & RH_PS_PRSC)
			{
				module->HardwareRegisters->RootHubPortStatus[port] = RH_PS_PRSC;
				return IPC_SUCCESS;
			}
			attempts--;
			SleepModule(module, 10);
		}
		while (attempts != 0);
	}
	return IPC_NOTREADY;
}

static int HandleDisconnection(OH1ModuleControl *, u8 port)
{
	for (int deviceIndex = 1; deviceIndex < MAX_USB_DEVICES; deviceIndex++)
	{
		if (Devices[deviceIndex].DeviceType == DEV_TYPE_DEVICE &&
		    Devices[deviceIndex].PortIndex == port)
		{
			if (Devices[deviceIndex].IpcMessage != NULL)
				OSResourceReply(Devices[deviceIndex].IpcMessage, 0);

			return IPC_SUCCESS;
		}
	}
	return IPC_EINVAL;
}

static void SetControlStatusRegister(OH1ModuleControl *module, u32 controlFlag, u32 commandStatus)
{
	volatile OhciRegs *moduleRegisters = module->HardwareRegisters;

	if ((moduleRegisters->Control & controlFlag) == 0)
		moduleRegisters->Control |= controlFlag;

	moduleRegisters->CommandStatus = commandStatus;
}

static OhciEndpointDescriptor *GetInterruptEndpointDescriptor(OH1ModuleControl *module, u8 interval)
{
	int level = 0;

	for (u32 halved = interval / 2; halved != 0; halved /= 2) level++;

	if (level > 5)
		level = 5;

	/* 63 = 32 + 16 + 8 + 4 + 2 + 1, that is the number of interrupt endpoint
	 * descriptors. */
	for (int index = _endpointIndexes[level]; index < 63; index++)
	{
		OhciEndpointDescriptor *endpointDescriptor = &module->EndpointDescriptors[index];
		u32 dw0 = swap_u32(endpointDescriptor->dw0);
		if (!(dw0 & ED_WII31))
		{
			endpointDescriptor->dw0 = swap_u32(dw0 | ED_WII31);
			return endpointDescriptor;
		}
	}
	return NULL;
}

static s8 GetAvailableDeviceIndex(OH1ModuleControl *module, u8 port, u8 zero, bool isLowSpeedDevice)
{
	OhciEndpointDescriptor *endpoint;
	WiiTransferDescriptor *transfer;
	void *swappedTransfer;

	for (s8 deviceIndex = 1; deviceIndex < MAX_USB_DEVICES; deviceIndex++)
	{
		if (Devices[deviceIndex].DeviceType != DEV_TYPE_NONE)
			continue;
		Devices[deviceIndex].PortIndex = port;
		Devices[deviceIndex].DeviceType = DEV_TYPE_DEVICE;
		Devices[deviceIndex].Zero = zero;
		Devices[deviceIndex].Endpoints[0].EndpointAddress = 0;
		endpoint = AllocateEndpointDescriptor();
		Devices[deviceIndex].Endpoints[0].Descriptor = endpoint;
		transfer = AllocateTransferDescriptor();
		Devices[deviceIndex].Endpoints[0].TransferDescriptor = transfer;
		Devices[deviceIndex].IsLowSpeed = isLowSpeedDevice;
		u16 size = isLowSpeedDevice ? 8 : 32;
		Devices[deviceIndex].Endpoints[0].MaxPacketSize = size;
		for (int i_endp = 1; i_endp < MAX_USB_ENDPOINTS; i_endp++)
		{
			Devices[deviceIndex].Endpoints[i_endp].EndpointAddress = 0;
		}
		Devices[deviceIndex].NumberOfInterfaces = 0;
		swappedTransfer = swap_ptr(transfer);
		endpoint->Tail = swappedTransfer;
		endpoint->Head = swappedTransfer;
		endpoint->Next = NULL;
		/* TODO double check this: what's the relationship between being a
		 * low-speed device and the endpoint number? Maybe we misunderstood
		 * something in ghidra... */
		endpoint->dw0 = swap_u32(ED_SET(MPS, size) | ED_SET(EN, isLowSpeedDevice & 1));
		AppendControlEndpoint(module, endpoint);
		return deviceIndex;
	}
	return 0;
}

static void PopulateTransfer(WiiTransferDescriptor *transfer, IORequestPacket *ioRequest,
                             char *data, u16 length, u32 dw0, u32 interruptDelay)
{
	char *ptr;

	ptr = ValidateMemoryAddress(data);
	transfer->std.CurrentBuffer = swap_ptr(ptr);
	transfer->std.LastBuffer = swap_ptr(ptr + (length - 1));
	transfer->std.dw0 = swap_u32(dw0 | TD_SET(DI, interruptDelay));
	transfer->IORequestPacket = ioRequest;
	transfer->Length = length;
	ioRequest->Counter++;
}

static int SetupEndpoint(OH1ModuleControl *module, DeviceEndpoint *deviceEndpoint, s8 deviceIndex)
{
	OhciTransferDescriptor *transfer;
	OhciEndpointDescriptor *endpoint;
	u32 dw0;
	u8 transferType = deviceEndpoint->Attributes & USB_ENDPOINT_XFERTYPE_MASK;
	if (transferType == USB_ENDPOINT_XFER_ISOC)
	{
		deviceEndpoint->IsocTransferDescriptor = AllocateIsocTransferDescriptor();
		transfer = &deviceEndpoint->IsocTransferDescriptor->Header;
	}
	else
	{
		deviceEndpoint->TransferDescriptor = AllocateTransferDescriptor();
		transfer = &deviceEndpoint->TransferDescriptor->std;
	}

	if (!transfer)
		return IPC_ENOMEM;

	if (transferType == USB_ENDPOINT_XFER_INT)
	{
		endpoint = GetInterruptEndpointDescriptor(module, deviceEndpoint->Interval);
	}
	else
	{
		endpoint = AllocateEndpointDescriptor();
	}

	deviceEndpoint->Descriptor = endpoint;
	if (!endpoint)
		return IPC_SUCCESS;

	void *td_swapped = swap_ptr(transfer);
	endpoint->Head = td_swapped;
	endpoint->Tail = td_swapped;
	if (deviceEndpoint->EndpointAddress & USB_DIR_IN)
	{
		dw0 = ED_IN;
	}
	else
	{
		/* TODO investigate: this is weird. We'd expect it to set the
		 * endpoint direction to ED_OUT (0x800), instead this is setting the
		 * EndpointNumber (EN) field to 2 (the EN field starts at bit 7). */
		dw0 = 0x100;
	}
	dw0 |= ED_SET(EN, deviceEndpoint->EndpointAddress) | ED_SET(FA, deviceIndex) |
	       ED_SET(MPS, deviceEndpoint->MaxPacketSize) | ED_SKIP | ED_WII31;
	if (Devices[deviceIndex].IsLowSpeed)
		dw0 |= ED_LOWSPEED;
	if (transferType == USB_ENDPOINT_XFER_ISOC)
		dw0 |= ED_ISO;
	endpoint->dw0 = swap_u32(dw0);
	return IPC_SUCCESS;
}

static int SendControlMessageAsync(OH1ModuleControl *module, USBControlMessage *controlMessage,
                                   IpcMessage *ipcMessage, s32 queueId, s8 deviceIndex)
{
	WiiTransferDescriptor *nextTransfer;
	WiiTransferDescriptor *transferDescriptor;
	s8 endpointIndex;
	WiiTransferDescriptor *dataTransferDescriptor;
	OhciEndpointDescriptor *endpoint;
	u32 dw0;
	OhciTransferDescriptor *tail;
	WiiTransferDescriptor *transferTail;
	int rc = IPC_SUCCESS;

	DisableModuleInterrupts(module);
	OSAhbFlushFrom(module->AHBDeviceToFlush);
	OSAhbFlushTo(AHB_STARLET);
	OSDCFlushRange(controlMessage, sizeof(*controlMessage));
	IORequestPacket *ioRequest = OSAllocateMemory(_moduleHeap, sizeof(IORequestPacket));
	nextTransfer = AllocateTransferDescriptor();

	u16 length = swap_u16(controlMessage->Length);
	if (length == 0)
	{
		dataTransferDescriptor = NULL;
	}
	else
	{
		dataTransferDescriptor = AllocateTransferDescriptor();
		OSDCFlushRange(controlMessage->Data, length);
	}
	transferDescriptor = AllocateTransferDescriptor();
	if (!ioRequest || !nextTransfer || (!dataTransferDescriptor && length != 0) || !transferDescriptor)
	{
		rc = IPC_EMAX; /* TODO: why this error code? */
		goto error;
	}

	if (controlMessage->Oh1.Endpoint == 0)
	{
		endpointIndex = 0;
	}
	else
	{
		endpointIndex = FindEndpointIndex(deviceIndex, controlMessage->Oh1.Endpoint);
		if (endpointIndex < 1)
		{
			rc = IPC_EINVAL;
			/* The original code was not freeing the memory here! */
			goto error;
		}
	}
	endpoint = Devices[deviceIndex].Endpoints[endpointIndex].Descriptor;
	ioRequest->RequestMessage = ipcMessage;
	ioRequest->Queue = queueId;
	ioRequest->Counter = 0;
	ioRequest->ErrorCount = 0;
	ioRequest->Transferred = 0;
	ioRequest->Unused = 0;
	ioRequest->EndpointDescriptor = endpoint;
	ioRequest->MessageData = controlMessage->Oh1.Data;
	ioRequest->Size = length;
	ioRequest->ControlMessage = (queueId == -1) ? controlMessage : NULL;
	if (length != 0)
	{
		u32 dw0 = TD_SET(CC, TD_NOTACCESSED) |
		          TD_SET(DT, 0x3) | /* DataToggle is set to 1 for control transfers */
		          TD_R;
		dw0 |= (controlMessage->RequestType & USB_ENDPOINT_DIR_MASK) == USB_DIR_IN ?
		           TD_DP_IN :
		           TD_DP_OUT;
		PopulateTransfer(dataTransferDescriptor, ioRequest,
		                 controlMessage->Oh1.Data, length, dw0, 1);
	}
	/* TODO: check this condition: maybe it should be USB_DIR_IN? */
	if (length == 0 || (controlMessage->RequestType & USB_ENDPOINT_DIR_MASK) == USB_DIR_OUT)
	{
		dw0 = TD_DP_IN;
	}
	else
	{
		dw0 = TD_DP_OUT;
	}
	PopulateTransfer(transferDescriptor, ioRequest, NULL, 0,
	                 dw0 | TD_SET(CC, 0xf) | TD_SET(DT, 0x3), 0);
	void *td_swapped = endpoint->Tail;
	ioRequest->TransferDescriptor = nextTransfer;
	transferTail = swap_ptr(td_swapped);
	nextTransfer = ioRequest->TransferDescriptor;
	PopulateTransfer(transferTail, ioRequest, (char *)controlMessage, 8,
	                 TD_SET(CC, 0xf) | TD_SET(DT, 0x2), 2);
	ioRequest->TransferDescriptor = transferTail;
	if (dataTransferDescriptor)
	{
		transferTail->std.Next = swap_ptr(dataTransferDescriptor);
		dataTransferDescriptor->std.Next = swap_ptr(transferDescriptor);
	}
	else
	{
		transferTail->std.Next = swap_ptr(transferDescriptor);
	}
	tail = swap_ptr(nextTransfer);
	transferDescriptor->std.Next = tail;
	OSAhbFlushFrom(AHB_1);
	endpoint->Tail = tail;
	FlushEndpoint(module, endpoint);
	SetControlStatusRegister(module, OHCI_CTRL_CLE, OHCI_CS_CLF);

	EnableModuleInterrupts(module);
	return rc;

error:
	if (ioRequest != NULL)
		OSFreeMemory(_moduleHeap, ioRequest);
	if (nextTransfer != NULL)
		FreeMemory(nextTransfer);
	if (dataTransferDescriptor != NULL)
		FreeMemory(dataTransferDescriptor);
	if (transferDescriptor != NULL)
		FreeMemory(transferDescriptor);

	EnableModuleInterrupts(module);
	return rc;
}

static int SendControlMessage(OH1ModuleControl *module, s8 deviceIndex,
                              USBControlMessage *message)
{
	int rc;
	void *receivedMessage;

	rc = SendControlMessageAsync(module, message, NULL, module->TimerQueue, deviceIndex);
	if (rc >= 0)
		rc = OSReceiveMessage(module->TimerQueue, &receivedMessage, 0);

	return rc;
}

static void ParseDescriptors(OH1ModuleControl *module, s8 deviceIndex,
                             const UsbConfigurationDescriptor *configuration, size_t totalLength)
{
	u8 endpointIndex = 1;
	u32 usedSlotsMask = 0;
	UsbDescriptor *lastDescriptor = (UsbDescriptor *)(&configuration->Length + totalLength);
	u8 interfaceNumber;
	DeviceEndpoint *deviceEndpoint;
	UsbDescriptor *descriptor;
	u8 numberOfEndpoints = 0;
	u8 alternateSetting;
	u8 class;
	u8 protocol;
	u8 subClass;

	for (descriptor = (UsbDescriptor *)((u32)configuration + configuration->Length);
	     descriptor < lastDescriptor;
	     descriptor = (UsbDescriptor *)((u32)descriptor + descriptor->Header.Length))
	{
		if (numberOfEndpoints == 0)
		{
			if (descriptor->Header.DescriptorType != USB_DT_INTERFACE)
				continue;

			interfaceNumber = descriptor->Interface.InterfaceNumber;
			if (interfaceNumber > MAX_USB_INTERFACES)
				break;

			numberOfEndpoints = descriptor->Interface.NumberOfEndpoints;
			if (numberOfEndpoints == 0)
				continue;

			alternateSetting = descriptor->Interface.AlternateSetting;
			class = descriptor->Interface.InterfaceClass;
			subClass = descriptor->Interface.InterfaceSubClass;
			protocol = descriptor->Interface.InterfaceProtocol;
			if ((1 << interfaceNumber & usedSlotsMask) == 0)
			{
				AddInterface(deviceIndex, interfaceNumber, alternateSetting,
				             class, subClass, protocol);
			}
		}
		else
		{
			/* Parse the endpoints defined in this interface */
			if (descriptor->Header.DescriptorType != USB_DT_ENDPOINT)
				continue;

			numberOfEndpoints--;
			if ((1 << interfaceNumber & usedSlotsMask) != 0)
				continue;

			deviceEndpoint = &Devices[deviceIndex].Endpoints[endpointIndex];

			deviceEndpoint->EndpointAddress = descriptor->Endpoint.EndpointAddress;
			deviceEndpoint->Attributes = descriptor->Endpoint.Attributes;
			deviceEndpoint->MaxPacketSize = descriptor->Endpoint.MaxPacketSize;
			deviceEndpoint->Interval = descriptor->Endpoint.Interval;
			SetupEndpoint(module, deviceEndpoint, deviceIndex);

			u8 xfer_type = deviceEndpoint->Attributes & USB_ENDPOINT_XFERTYPE_MASK;
			if (xfer_type == USB_ENDPOINT_XFER_ISOC)
			{
				AppendEndpoint(module, deviceEndpoint->Descriptor);
			}
			else if (xfer_type == USB_ENDPOINT_XFER_CONTROL)
			{
				AppendControlEndpoint(module, deviceEndpoint->Descriptor);
			}
			else if (xfer_type == USB_ENDPOINT_XFER_BULK)
			{
				AppendBulkEndpoint(module, deviceEndpoint->Descriptor);
			}
			endpointIndex++;
			if (endpointIndex > MAX_USB_ENDPOINTS)
				return;

			if (numberOfEndpoints == 0)
			{
				/* No more remaining enpoints: interface is defined */
				usedSlotsMask |= 1 << interfaceNumber;
			}
		}
	}
}

static int GetDeviceDescriptor(OH1ModuleControl *module, s8 deviceIndex,
                               s8 outputDeviceIndex, UsbDeviceDescriptor *output)
{
	USBControlMessage *msg = _controlRequest;

	memset(msg, 0, sizeof(*msg));
	msg->RequestType = USB_DIR_IN;
	msg->Request = USB_REQ_GET_DESCRIPTOR;
	msg->Value = USB_DT_DEVICE;
	msg->Index = 0;
	msg->Length = swap_u16(PADDED4_SIZEOF(*output));
	msg->Oh1.Data = output;
	msg->Oh1.Endpoint = 0;
	msg->Oh1.DeviceIndex = outputDeviceIndex;
	return SendControlMessage(module, deviceIndex, msg);
}

static int ConfigureDevice(OH1ModuleControl *module, s8 deviceIndex,
                           UsbDeviceDescriptor *device, UsbConfigurationDescriptor *configuration)
{
	USBControlMessage *message;
	UsbConfigurationDescriptor *configurationReply;
	int rc;

	memset(_controlRequest, 0, sizeof(*_controlRequest));
	message = _controlRequest;
	message->RequestType = USB_DIR_OUT | USB_TYPE_STANDARD;
	message->Request = USB_REQ_SET_ADDRESS;
	message->Value = swap_u16((u16)deviceIndex);
	message->Length = 0;
	message->Index = 0;
	message->Oh1.Data = NULL;
	message->Oh1.Endpoint = 0;
	message->Oh1.DeviceIndex = 0;
	rc = SendControlMessage(module, deviceIndex, message);
	if (rc != 0)
		return rc;

	SetDeviceFunctionAddress(deviceIndex);
	rc = GetDeviceDescriptor(module, deviceIndex, deviceIndex, device);
	if (rc != 0)
		return rc;

	SetMaxPacketSize(deviceIndex, device->MaxPacketSize0);
	//some devices' descriptors are too big for the standard packet size
	//this is why we do a fetch, set max packet size, then fetch again
	rc = GetDeviceDescriptor(module, deviceIndex, deviceIndex, device);
	if (rc != 0)
		return rc;

	SetDeviceVendor(deviceIndex, swap_u16(device->VendorId));
	SetDeviceProductId(deviceIndex, swap_u16(device->ProductId));
	u8 device_class = device->DeviceClass;
	if ((device_class != USB_DEVICE_CLASS_DEVICE) && (device_class != USB_DEVICE_CLASS_HUB))
	{
		SetDeviceClass(deviceIndex, device_class, device->DeviceSubClass, device->DeviceProtocol);
	}
	memset(_controlRequest, 0, sizeof(*_controlRequest));
	message->RequestType = USB_DIR_IN;
	message->Request = USB_REQ_GET_DESCRIPTOR;
	message = _controlRequest;
	message->Value = 2;
	message->Length = 0xc00;
	message->Index = 0;
	message->Oh1.Data = configuration;
	message->Oh1.Endpoint = 0;
	message->Oh1.DeviceIndex = deviceIndex;
	SendControlMessage(module, deviceIndex, message);

	configuration->TotalLength = swap_u16(configuration->TotalLength);
	SetMaxPower(deviceIndex, configuration->MaxPower);
	u16 total_length = configuration->TotalLength;
	configurationReply = OSAlignedAllocateMemory(_moduleHeap, total_length, 32);
	if (configurationReply)
	{
		memset(configurationReply, 0, total_length);
		memset(_controlRequest, 0, sizeof(*_controlRequest));
		message->RequestType = USB_DIR_IN;
		message->Request = USB_REQ_GET_DESCRIPTOR;
		message = _controlRequest;
		message->Value = USB_DT_CONFIG;
		message->Index = 0;
		message->Length = swap_u16(total_length);
		message->Oh1.Data = configurationReply;
		message->Oh1.Endpoint = 0;
		message->Oh1.DeviceIndex = deviceIndex;
		rc = SendControlMessage(module, deviceIndex, message);
		if (rc == 0)
			ParseDescriptors(module, deviceIndex, configurationReply, total_length);

		OSFreeMemory(_moduleHeap, configurationReply);
	}
	u8 conf_value = configuration->ConfigurationValue;
	memset(_controlRequest, 0, sizeof(*_controlRequest));
	message = _controlRequest;
	message->RequestType = USB_DIR_OUT;
	message->Request = USB_REQ_SET_CONFIGURATION;
	message->Value = swap_u16(conf_value);
	message->Index = 0;
	message->Length = 0;
	message->Oh1.Data = NULL;
	message->Oh1.Endpoint = 0;
	message->Oh1.DeviceIndex = 0;
	SendControlMessage(module, deviceIndex, message);
	u8 num_interfaces = Devices[deviceIndex].NumberOfInterfaces;
	for (int i_interface = 0; i_interface < num_interfaces; i_interface++)
	{
		DeviceInterface *iface = &Devices[deviceIndex].Interfaces[i_interface];
		u8 alternate_setting = iface->AlternateSetting;
		if (alternate_setting == 0)
			continue;

		u8 iface_num = iface->InterfaceNumber;
		memset(_controlRequest, 0, sizeof(*_controlRequest));
		message = _controlRequest;
		message->RequestType = USB_RECIP_INTERFACE;
		message->Request = USB_REQ_SET_INTERFACE;
		message->Index = swap_u16(iface_num);
		message->Value = swap_u16(alternate_setting);
		message->Length = 0;
		message->Oh1.Data = NULL;
		message->Oh1.Endpoint = 0;
		message->Oh1.DeviceIndex = 0;
		rc = SendControlMessage(module, deviceIndex, message);
		if (rc < 0)
			return rc;
	}
	return 0;
}

static void CloseDeviceEndpoint(OH1ModuleControl *module, s8 deviceIndex, u8 endpointIndex)
{
	DeviceEndpoint *endp;

	endp = &Devices[deviceIndex].Endpoints[endpointIndex];
	if (endpointIndex == 0 || endp->EndpointAddress != 0)
	{
		OhciEndpointDescriptor *endpoint;
		endpoint = Devices[deviceIndex].Endpoints[endpointIndex].Descriptor;
		OSAhbFlushFrom(module->AHBDeviceToFlush);
		OSAhbFlushTo(AHB_STARLET);
		DisableEndpoint(module, endpoint);
		CloseEndpoint(module, endpoint);
	}
}

int InitialiseModule(OH1ModuleControl *module)
{
	int relIndexOfNext;
	int indexOfNext;
	OhciEndpointDescriptor *endpoint;
	int index;
	int numberOfEntriesInLevel;
	int baseIndex;
	int baseIndexOfNext;

	/* This function fills the "next" pointers in all the interrupt endpoint
     * descriptors. The OHCI specs defines this table in figure 3-4 at page 10:
     * there are 32 entries in the interrupt table at the first level (which is
     * used for endpoints having poll interval of 32 ms), and their "next"
     * pointers converge into 16 entries (interval: 16 ms), which in turn
     * converge into 8 entries (interval: 8 ms), then 4, then 2, up to the final
     * entry, which is triggered at every millisecond. */
	memset(_ohciInterruptEndpoints, 0, OH1_TRANSFER_SIZE);
	baseIndexOfNext = 32;
	baseIndex = 0;
	numberOfEntriesInLevel = 32;
	do
	{
		if (numberOfEntriesInLevel != 0)
		{
			for (int index_in_level = 0; index_in_level < numberOfEntriesInLevel; index_in_level++)
			{
				index = baseIndex + index_in_level;
				_ohciInterruptEndpoints[index_in_level].dw0 =
				    (baseIndex == 0) ? 0 : swap_u32(ED_SKIP);

				relIndexOfNext = index_in_level % (numberOfEntriesInLevel / 2);
				indexOfNext = relIndexOfNext + baseIndexOfNext;
				endpoint = _ohciInterruptEndpoints + indexOfNext;
				_ohciInterruptEndpoints[index].Next = swap_ptr(endpoint);
			}
		}
		baseIndex += numberOfEntriesInLevel;
		numberOfEntriesInLevel /= 2;
		baseIndexOfNext += numberOfEntriesInLevel;
		numberOfEntriesInLevel = numberOfEntriesInLevel;
	}
	while (numberOfEntriesInLevel > 1);

	module->EndpointDescriptors = _ohciInterruptEndpoints;
	return 0;
}

int QueryModuleDevices(OH1ModuleControl *module)
{
	UsbDeviceDescriptor *device;
	UsbConfigurationDescriptor *configuration;
	s8 deviceIndex;
	int hasSucceeded;
	u32 rootHubDescriptionB;
	char *deviceName;
	u32 numberOfPorts;
	volatile OhciRegs *registers;
	u8 portIndex;
	u8 *downstream_ports_ptr;
	u32 rootHubDescription;
	u32 oldRootHubDescription;
	int ret;

	oldRootHubDescription = module->HardwareRegisters->RootHubDiscriptorA;
	rootHubDescription = oldRootHubDescription & ~RH_A_RESERVED;
	ret = IPC_SUCCESS;
	_controlRequest = OSAlignedAllocateMemory(_moduleHeap, sizeof(USBControlMessage), 32);
	if (_controlRequest == NULL)
		return IPC_EMAX;

	numberOfPorts = module->NumberOfDownstreamPorts = rootHubDescription & RH_A_NDP;
	downstream_ports_ptr = &module->NumberOfDownstreamPorts;
	Devices[0].DeviceType = DEV_TYPE_HUB;
	Devices[0].Interfaces[0].InterfaceNumber = *downstream_ports_ptr;
	registers = module->HardwareRegisters;
	if (!(rootHubDescription & RH_A_NPS))
	{
		/* Ports are power switched */
		rootHubDescriptionB = registers->RootHubDiscriptorB & RH_B_DR;
		/* Mark all our ports as removable */
		for (u8 i = 1; i < numberOfPorts; i++)
		{
			rootHubDescriptionB |= 1 << (i + 16);
		}

		rootHubDescription |= RH_A_PSM;
		if (!(rootHubDescription & RH_A_NOCP))
		{
			/* No current protection */
			rootHubDescription |= (RH_A_OCPM | RH_A_PSM);
		}
		registers->RootHubDiscriptorA = rootHubDescription;
		registers->RootHubDiscriptorB = rootHubDescriptionB;
	}
	registers->RootHubDiscriptorA =
	    (oldRootHubDescription & ~(RH_A_RESERVED | RH_A_NOCP | RH_A_OCPM)) | RH_A_NPS;
	registers->RootHubStatus = (registers->RootHubStatus & ~RH_HS_RESERVED) |
	                           RH_HS_OCIC | RH_HS_LPSC;
	for (u8 index = 0; index < numberOfPorts; index++)
	{
		/* Power up all ports */
		registers->RootHubPortStatus[index] = RH_PS_PPS;
	}
	/* power on to power good time */
	u32 potpgt = (registers->RootHubDiscriptorA >> RH_A_POTPGT_SHIFT) & RH_A_POTPGT_MASK;
	SleepModule(module, potpgt * 2000);
	numberOfPorts = module->NumberOfDownstreamPorts;
	for (portIndex = 0; portIndex < numberOfPorts; portIndex++)
	{
		hasSucceeded = ResetDevice(module, portIndex);
		if (hasSucceeded != 0)
			continue;

		// Last parameter is a boolean: "LowSpeedDeviceAttached"
		deviceIndex = GetAvailableDeviceIndex(
		    module, portIndex, 0,
		    (module->HardwareRegisters->RootHubPortStatus[portIndex] & RH_PS_LSDA) != 0);
		if (deviceIndex == 0)
		{
			printk("No device slots available!\n");
			break;
		}

		size_t descriptorSize = PADDED4_SIZEOF(*device);
		device = OSAlignedAllocateMemory(_moduleHeap, descriptorSize, 32);
		memset(device, 0, descriptorSize);

		size_t configurationSize = PADDED4_SIZEOF(*configuration);
		configuration = OSAlignedAllocateMemory(_moduleHeap, configurationSize, 32);
		memset(configuration, 0, configurationSize);

		ret = ConfigureDevice(module, deviceIndex, device, configuration);
		if (ret == 0)
		{
			// 5 is OH0/OHCI0, 6 is OH1
			deviceName = (module->DeviceEvent == IRQ_OHCI0) ? "OH0:" : "OH1:";
			printk("%s configured USB device at port %u, vid: 0x%04x "
			       "pid: 0x%04x\n",
			       deviceName, portIndex, swap_u16(device->VendorId),
			       swap_u16(device->ProductId));
		}
		OSFreeMemory(_moduleHeap, device);
		OSFreeMemory(_moduleHeap, configuration);
	}

	for (portIndex = 0; portIndex < numberOfPorts; portIndex++)
		registers->RootHubPortStatus[portIndex] = RH_PS_CSC | RH_PS_PESC |
		                                          RH_PS_PSSC | RH_PS_OCIC;

	module->State |= OH1_STATE_DEVICE_QUERIED;
	OSFreeMemory(_moduleHeap, _controlRequest);
	return ret;
}

int ProcessControlMessage(OH1ModuleControl *module, IpcMessage *ipcMessage)
{
	const IpcRequest *request = &ipcMessage->Request;
	const IoctlvMessage *ioctlv = &request->Data.Ioctlv;
	u32 inputCount = ioctlv->InputArgc;
	u32 ioCount = ioctlv->IoArgc;
	if (inputCount != 6 || ioCount != 1)
	{
		printk("readcount[%u], writecount[%u] bad\n", inputCount, ioCount);
		return IPC_EINVAL;
	}

	const IoctlvMessageData *vector = ioctlv->Data;
	if (vector[0].Length != 1 || !vector[0].Data || vector[1].Length != 1 ||
	    !vector[1].Data || vector[2].Length != 2 || !vector[2].Data ||
	    vector[3].Length != 2 || !vector[3].Data || vector[4].Length != 2 ||
	    !vector[4].Data || vector[5].Length != 1 || !vector[5].Data ||
	    vector[6].Length != swap_u16(*(u16 *)vector[4].Data ||
	                                 (vector[6].Length != 0 && !vector[6].Data)))
	{
		printk("parameter validity check failed\n");
		return IPC_EINVAL;
	}

	USBControlMessage *controlMessage =
	    OSAlignedAllocateMemory(_moduleHeap, sizeof(*controlMessage), 32);
	if (!controlMessage)
	{
		printk("failed to allocate ohcctrlreq\n");
		return IPC_EMAX;
	}

	controlMessage->RequestType = *(u8 *)vector[0].Data;
	controlMessage->Request = *(u8 *)vector[1].Data;
	controlMessage->Value = *(u16 *)vector[2].Data;
	controlMessage->Index = *(u16 *)vector[3].Data;
	controlMessage->Length = *(u16 *)vector[4].Data;
	controlMessage->Oh1.Data = vector[6].Data;
	controlMessage->Oh1.Endpoint = *(u8 *)vector[5].Data;
	controlMessage->Oh1.DeviceIndex = (s8)request->FileDescriptor;
	return SendControlMessageAsync(module, controlMessage, ipcMessage, -1,
	                               *(s8 *)((int)&request->FileDescriptor + 3));
}

int ProcessInterruptBlockMessage(OH1ModuleControl *module, IpcMessage *ipcMessage)
{
	s8 deviceIndex;
	const IoctlvMessageData *vector;
	char *data;
	s8 endpointIndex;
	u32 num_in;
	u32 num_io;
	u32 command;
	OhciEndpointDescriptor *endpoint;
	OhciTransferDescriptor *new_tail_swapped;
	WiiTransferDescriptor *xfer_desc;
	int ret = 0;
	u8 bEndpoint;

	const IpcRequest *request = &ipcMessage->Request;
	const IoctlvMessage *ioctlv = &request->Data.Ioctlv;
	num_in = ioctlv->InputArgc;
	num_io = ioctlv->IoArgc;
	if (num_in != 2 || num_io == 1)
	{
		printk("readcount[%u], writecount[%u] bad\n", num_in, num_io);
		return IPC_EINVAL;
	}

	vector = ioctlv->Data;
	if (vector[0].Length != 1 || !vector[0].Data || vector[1].Length != 2 ||
	    !vector[1].Data)
	{
		return IPC_EINVAL;
	}

	u16 length = *(u16 *)vector[1].Data;
	if (length != vector[2].Length || (length != 0 && !vector[2].Data))
	{
		return IPC_EINVAL;
	}
	bEndpoint = *(u8 *)vector[0].Data;
	data = (void *)vector[2].Data;
	deviceIndex = *(s8 *)((int)&request->FileDescriptor + 3);
	DisableModuleInterrupts(module);
	OSAhbFlushFrom(module->AHBDeviceToFlush);
	OSAhbFlushTo(AHB_STARLET);
	command = ioctlv->Ioctl;
	IORequestPacket *irp = OSAllocateMemory(_moduleHeap, sizeof(*irp));
	if (!irp)
	{
		ret = IPC_EMAX;
		goto error_reenable_interrupts;
	}

	if (length == 0 || bEndpoint == 0)
	{
		ret = IPC_EINVAL;
		goto error_free_irp;
	}

	OSDCFlushRange(data, length);
	endpointIndex = FindEndpointIndex(deviceIndex, bEndpoint);
	if (endpointIndex < 1)
	{
		ret = IPC_EINVAL;
		goto error_free_irp;
	}

	endpoint = Devices[deviceIndex].Endpoints[endpointIndex].Descriptor;
	irp->Transferred = 0;
	irp->Counter = 0;
	irp->ErrorCount = 0;
	irp->Queue = -1;
	irp->Unused = 0;
	irp->EndpointDescriptor = endpoint;
	irp->RequestMessage = ipcMessage;
	irp->MessageData = data;
	irp->Size = length;
	irp->ControlMessage = NULL;
	WiiTransferDescriptor *new_tail = AllocateTransferDescriptor();
	if (!new_tail)
	{
		ret = IPC_EMAX;
		goto error_free_irp;
	}

	xfer_desc = swap_ptr(endpoint->Tail);
	if (xfer_desc == NULL)
	{
		printk("td dummy == NULL\n");
	}
	else if (xfer_desc->std.CurrentBuffer != NULL)
	{
		printk("warning: dummy TD is not an empty TD: td = %p cbp = %p "
		       "typ = %u\n",
		       xfer_desc, xfer_desc->std.CurrentBuffer, command);
	}

	u32 direction = (bEndpoint & USB_DIR_IN) ? TD_DP_IN : TD_DP_OUT;
	PopulateTransfer(xfer_desc, irp, data, length, direction | TD_SET(CC, 0xf) | TD_R, 0);
	irp->TransferDescriptor = xfer_desc;
	new_tail_swapped = swap_ptr(new_tail);
	xfer_desc->std.Next = new_tail_swapped;
	endpoint->Tail = new_tail_swapped;
	OSAhbFlushFrom(AHB_1);
	FlushEndpoint(module, endpoint);
	if (command == USBV0_IOCTL_INTRMSG)
	{
		u32 ctrl_reg = module->HardwareRegisters->Control;
		if (!(ctrl_reg & OHCI_CTRL_PLE))
		{
			module->HardwareRegisters->Control = ctrl_reg | OHCI_CTRL_PLE;
		}
	}
	else if (command == USBV0_IOCTL_BLKMSG)
	{
		SetControlStatusRegister(module, OHCI_CTRL_BLE, OHCI_CS_BLF);
	}
	irp = NULL; /* We don't want to free it */

error_free_irp:
	if (irp != NULL)
	{
		OSFreeMemory(_moduleHeap, irp);
	}

error_reenable_interrupts:
	EnableModuleInterrupts(module);
	return ret;
}

int SleepModule(OH1ModuleControl *module, u32 timeout)
{
	int rc;
	void *message;

	rc = OSRestartTimer(module->Timer, timeout, 0);
	if (rc < 0)
	{
		printk("usleept: RestartTimer (tmr = %d usec = %u) failed: %d\n",
		       module->Timer, timeout, rc);
	}
	else
	{
		rc = OSReceiveMessage(module->TimerQueue, &message, 0);
		OSStopTimer(module->Timer);
	}
	return rc;
}

void HandleStatusChange(OH1ModuleControl *module, int /*unused*/)
{
	volatile OhciRegs *regs;
	regs = module->HardwareRegisters;
	regs->DisableInterrupts = OHCI_INTR_RHSC;

	for (u8 portIndex = 0; portIndex < module->NumberOfDownstreamPorts; portIndex++)
	{
		/* Nothing to do if the connection status didn't change */
		if (!(regs->RootHubPortStatus[portIndex] & RH_PS_CSC))
			continue;

		if (regs->RootHubPortStatus[portIndex] & RH_PS_CCS)
		{
			regs->RootHubPortStatus[portIndex] = RH_PS_PPS;
			ResetDevice(module, portIndex);
		}
		else
		{
			HandleDisconnection(module, portIndex);
		}
		// ConnectStatusChange and PortEnableStatusChange
		regs->RootHubPortStatus[portIndex] = RH_PS_PESC & RH_PS_CSC;
	}
	regs->EnableInterrupts = OHCI_INTR_RHSC;
}

int SuspendDevice(OH1ModuleControl *module, u8 port)
{
	volatile OhciRegs *regs;

	if (port > module->NumberOfDownstreamPorts)
		return IPC_EINVAL;

	regs = module->HardwareRegisters;
	if (!(regs->RootHubPortStatus[port] & RH_PS_CCS))
		return IPC_NOTREADY;

	// PortSuspendStatus
	regs->RootHubPortStatus[port] = RH_PS_PSS;
	if (!(regs->RootHubPortStatus[port] & RH_PS_PSS))
		return IPC_UNKNOWN;

	return IPC_SUCCESS;
}

int ResumeDevice(OH1ModuleControl *module, u8 port)
{
	volatile OhciRegs *regs;

	if (port > module->NumberOfDownstreamPorts)
		return IPC_EINVAL;

	regs = module->HardwareRegisters;
	if (regs->RootHubPortStatus[port] & RH_PS_PSS &&
	    /* This gcc warning indeed reveals a bug: RH_PS_PSS is 0x4, so there's
		 * no way that the and'ed value can be equal to 1.
		 * TODO: fix it later, after checking that it brings no regressions. */
	    (regs->RootHubPortStatus[port] = RH_PS_POCI,
	     (regs->RootHubPortStatus[port] & RH_PS_PSS) == 1))
		return IPC_UNKNOWN;

	return IPC_SUCCESS;
}

void CloseEndpoint(OH1ModuleControl *module, OhciEndpointDescriptor *endpoint)
{
	WiiTransferDescriptor *head;
	WiiTransferDescriptor *tail;

	OSAhbFlushFrom(module->AHBDeviceToFlush);
	OSAhbFlushTo(module->AHBDevice);
	u32 headField = (u32)swap_ptr(endpoint->Head);
	head = (void *)(headField & 0xfffffff0);
	u32 headFlags = headField & (ED_C | ED_H);
	tail = swap_ptr(endpoint->Tail);
	if (head != tail)
	{
		while (IsTransferDescriptorOnHead(head) && (head != tail))
		{
			IORequestPacket *ioRequest = head->IORequestPacket;
			ioRequest->Counter--;
			endpoint->Head = head->std.Next;
			OSDCFlushRange(endpoint, sizeof(*endpoint));
			FreeMemory(head);
			if (ioRequest->Counter == 0)
				CleanupIORequest(ioRequest);

			head = MASK_PTR(swap_ptr(endpoint->Head), 0xfffffff0);
		}
		endpoint->Head = PTR_WITH_BITS(endpoint->Head, swap_u32(headFlags));
		OSDCFlushRange(endpoint, sizeof(*endpoint));
		OSAhbFlushTo(module->AHBDevice);
	}
}

void DisableEndpoint(OH1ModuleControl *module, OhciEndpointDescriptor *endpoint)
{
	u32 dw0 = swap_u32(endpoint->dw0);
	endpoint->dw0 = swap_u32(dw0 | ED_SKIP);
	OSDCFlushRange(endpoint, sizeof(*endpoint));
	OSAhbFlushTo(module->AHBDevice);
	SleepModule(module, 1000);
}

void ResumeEndpoint(OH1ModuleControl *module, OhciEndpointDescriptor *endpoint)
{
	void *td = MASK_PTR(swap_ptr(endpoint->Head), ~(ED_H | ED_C));
	endpoint->Head = swap_ptr(td);
	OSDCFlushRange(endpoint, sizeof(*endpoint));
	OSAhbFlushTo(module->AHBDevice);
	printk("Cleared halt for ed: %p (headP = %p)\n", endpoint, td);
}

void CloseDevice(OH1ModuleControl *module, s8 deviceIndex)
{
	for (u8 descriptorIndex = 0; descriptorIndex < MAX_USB_ENDPOINTS; descriptorIndex++)
	{
		CloseDeviceEndpoint(module, deviceIndex, descriptorIndex);
	}
}
