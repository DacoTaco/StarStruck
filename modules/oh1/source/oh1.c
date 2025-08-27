/*
        StarStruck - a Free Software reimplementation for the Nintendo/BroadOn
IOS. oh1 - usb ohci implementation in ios

        Copyright (C) 2025	Alberto Mardegan <mardy@users.sourceforge.net>

# This code is licensed to you under the terms of the GNU GPL, version 2;
# see file COPYING or http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt
*/

#include <ios/irq.h>
#include <ios/errno.h>
#include <ios/printk.h>
#include <ios/syscalls.h>
#include <string.h>
#include <usb/ehci.h>

#include "memory.h"
#include "deviceManagement.h"
#include "communications.h"
#include "module.h"

#define OHCI_REG_BASE ((void *)0x0d060000)

static char _deviceName[16];
static OhciHcca *_hcca = (void *)0x13880000;
static EhciRegisters *_ehciRegisters = (EhciRegisters *)EHCI_REG_BASE;
static s16 _rootHubFileDescriptor = 0x7fff;
static s32 _deviceQueueId;
static u32 _deviceQueueBuffers[MAX_USB_DEVICES];
static void *_statusChangeMessage = (void *)0xcafef00d;

static u32 _workerThreadStack[0x100]; /* That is, 0x400 bytes */

//i dont know why they didn't just use strtol but ok, here we are
static int HexToInteger(const char *hexstring)
{
	int result, val;
	char ch;

	result = 0;
	if (hexstring[0] == '0' && (hexstring[1] == 'x' || hexstring[1] == 'X'))
		hexstring += 2;

	while (true)
	{
		ch = *hexstring;
		if (ch >= '0' && ch <= '9')
			val = ch - '0';
		else if (ch >= 'a' && ch <= 'f')
			val = ch - 'a' + 10;
		else if (ch >= 'A' && ch <= 'F')
			val = ch - 'A' + 10;
		else
			break;
		result = result * 16 + val;
		hexstring++;
	}

	return result;
}

static int SetModuleDeviceMessage(OH1ModuleControl *, IpcMessage *message)
{
	return SetDeviceIPCMessage(*(s8 *)((int)&message->Request.FileDescriptor + 3), message);
}

static int CreateUSBDeviceQueue(OH1ModuleControl *module, int deviceEvent)
{
	int ret = OSCreateMessageQueue(_deviceQueueBuffers,
	                               sizeof(_deviceQueueBuffers) / sizeof(u32));
	if (ret < 0)
		return ret;

	_deviceQueueId = ret;
	memset(_deviceName, 0, sizeof(_deviceName));

	const char *path = deviceEvent == IRQ_OHCI0 ? "/dev/usb/oh0" : "/dev/usb/oh1";
	strncpy(_deviceName, path, sizeof(_deviceName) - 1);
	module->QueueId = _deviceQueueId;
	return OSRegisterResourceManager(_deviceName, _deviceQueueId);
}

static int HandleClose(OH1ModuleControl *module, const IpcRequest *request)
{
	int result = IPC_SUCCESS;

	u16 fd;
	fd = *(u16 *)((int)&request->FileDescriptor + 2);
	if (fd != _rootHubFileDescriptor)
	{
		s8 deviceIndex = (s8)fd;
		u16 unusedPid, unusedVid;
		result = GetDeviceVendorAndProduct(deviceIndex, &unusedVid, &unusedPid);
		if (result == IPC_SUCCESS)
			CloseDevice(module, deviceIndex);
	}
	return result;
}

static int HandleOpen(OH1ModuleControl *, const IpcRequest *request)
{
	int result;
	char *device;
	size_t len;
	char subdevice_chr;
	s16 deviceIndex;
	char *nextToken;

	result = strncmp(request->Data.Open.Filepath, _deviceName, sizeof(_deviceName));
	if (result == 0)
		return _rootHubFileDescriptor;

	char product[16] = { 0 };
	char vendor[16] = { 0 };
	device = request->Data.Open.Filepath;
	// 13 is strlen("/dev/usb/oh1/")
	nextToken = device + 13;

	/* Parse vendor ID */
	len = 0;
	subdevice_chr = *nextToken;
	while (subdevice_chr != '\0' && subdevice_chr != '/')
		subdevice_chr = nextToken[++len];
	memcpy(vendor, nextToken, len);

	/* Parse product ID */
	nextToken += len + 1;
	len = 0;
	subdevice_chr = nextToken[len];
	while (subdevice_chr != '\0' && subdevice_chr != '/')
	{
		subdevice_chr = nextToken[++len];
	}
	memcpy(product, nextToken, len);

	u16 vid = (u16)HexToInteger(vendor);
	u16 pid = (u16)HexToInteger(product);
	deviceIndex = GetDeviceIndex(vid, pid);
	return deviceIndex >= 0 ? deviceIndex : IPC_EINVAL;
}

static int HandleIoctl(OH1ModuleControl *module, IpcMessage *message, bool *isAsync)
{
	const IpcRequest *request = &message->Request;
	const IoctlMessage *ioctl = &request->Data.Ioctl;
	int result = IPC_EINVAL;

	*isAsync = false;

	u16 fd = *(u16 *)((int)&request->FileDescriptor + 2);
	if (fd == _rootHubFileDescriptor)
	{
		if (ioctl->Ioctl == USBV0_IOCTL_GETHUBSTATUS)
		{
			u32 *status;
			status = ioctl->IoBuffer;
			if (status == NULL || ioctl->IoLength != 4)
				return IPC_EINVAL;

			*status = module->HardwareRegisters->RootHubDiscriptorA & 0xff001fff;
			return IPC_SUCCESS;
		}
		return IPC_EINVAL;
	}

	s8 deviceIndex = *(s8 *)((int)&request->FileDescriptor + 3);
	result = GetDeviceVendorAndProduct(deviceIndex, NULL, NULL);
	if (result != 0)
		return result;

	if (ioctl->Ioctl == USBV0_IOCTL_SUSPENDDEV || ioctl->Ioctl == USBV0_IOCTL_RESUMEDEV)
	{
		if (ioctl->InputBuffer || ioctl->InputLength != 0 || ioctl->IoBuffer ||
		    ioctl->IoLength != 0)
			return IPC_EINVAL;

		u8 port = GetPortIndex(deviceIndex);
		if (ioctl->Ioctl == USBV0_IOCTL_SUSPENDDEV)
			result = SuspendDevice(module, port);
		else
			result = ResumeDevice(module, port);
	}
	else if (ioctl->Ioctl == USBV0_IOCTL_DEVREMOVALHOOK)
	{
		result = SetModuleDeviceMessage(module, message);
		*isAsync = true;
	}
	return result;
}

static int HandleIoctlv(OH1ModuleControl *module, IpcMessage *message, bool *isAsync)
{
	const IpcRequest *request = &message->Request;
	const IoctlvMessage *ioctlv = &request->Data.Ioctlv;
	const IoctlvMessageData *vector;
	int result = IPC_EINVAL;

	u16 fd = *(u16 *)((int)&request->FileDescriptor + 2);
	if (fd != _rootHubFileDescriptor)
	{
		if (ioctlv->Ioctl == USBV0_IOCTL_CTRLMSG)
		{
			result = ProcessControlMessage(module, message);
		}
		else if (ioctlv->Ioctl == USBV0_IOCTL_INTRMSG || ioctlv->Ioctl == USBV0_IOCTL_BLKMSG)
		{
			result = ProcessInterruptBlockMessage(module, message);
		}
		else
			return IPC_EINVAL;
		*isAsync = true;
		return result;
	}

	/* All other ioctls are synchronous */
	*isAsync = false;

	if (ioctlv->Ioctl == USBV0_IOCTL_GETPORTSTATUS)
	{
		vector = ioctlv->Data;
		if (ioctlv->InputArgc != 1 || ioctlv->IoArgc != 1 || !vector[0].Data ||
		    !vector[1].Data)
			return IPC_EINVAL;

		u32 queryPort = *vector[0].Data;
		u32 *outptr = vector[1].Data;
		if (vector[0].Length == 1 && vector[1].Length == 4 && queryPort < module->NumberOfDownstreamPorts)
		{
			*outptr = module->HardwareRegisters->RootHubPortStatus[queryPort];
			OSDCFlushRange(outptr, sizeof(*outptr));
			result = IPC_SUCCESS;
		}
	}
	else if (ioctlv->Ioctl == USBV0_IOCTL_SETPORTSTATUS)
	{
		if (ioctlv->InputArgc != 2)
			return IPC_EINVAL;

		vector = ioctlv->Data;
		if (vector[0].Length != 1 || !vector[0].Data || vector[1].Length != 4 ||
		    !vector[1].Data)
			return IPC_EINVAL;

		u8 port = *(u8 *)vector[0].Data;
		if (port < module->NumberOfDownstreamPorts)
		{
			module->HardwareRegisters->RootHubPortStatus[port] =
			    *(u32 *)vector[1].Data;
			result = IPC_SUCCESS;
		}
	}
	else if (ioctlv->Ioctl == USBV0_IOCTL_GETDEVLIST)
	{
		u32 num_in = ioctlv->InputArgc;
		u32 num_io = ioctlv->IoArgc;
		if (num_in != 2 || num_io != 2)
		{
			printk("readcount[%u], writecount[%u] bad\n", num_in, num_io);
			return IPC_EINVAL;
		}

		vector = ioctlv->Data;
		if (vector[0].Length != 1 || !vector[0].Data || vector[1].Length != 1 ||
		    !vector[1].Data || vector[2].Length != 1 || !vector[2].Data)
			return IPC_EINVAL;

		u8 num_elements = *(u8 *)vector[0].Data;
		if (vector[3].Length != num_elements * sizeof(DeviceListEntry) ||
		    (num_elements != 0 && !vector[3].Data))
			return IPC_EINVAL;

		u8 *count = (u8 *)vector[2].Data;
		u8 iface_class = *(u8 *)vector[1].Data;
		DeviceListEntry *dest_ptr = (DeviceListEntry *)vector[3].Data;
		result = PopulateDeviceList(dest_ptr, num_elements, iface_class, count);
		/* TODO: shouldn't this pass "num_elements * sizeof(DeviceListEntry)"? */
		OSDCFlushRange(vector[3].Data, num_elements);
		OSDCFlushRange(count, sizeof(*count));
	}
	else
		return IPC_EINVAL;

	return result;
}

static int ProcessEvents(OH1ModuleControl *module)
{
	IpcMessage *message;
	int result;
	const IpcRequest *request;

	module->State |= OH1_STATE_PROCESSING_EVENTS;
	do
	{
		result = OSReceiveMessage(_deviceQueueId, &message, 0);
		if (result != 0)
			return result;

		request = &message->Request;

		if (request == _statusChangeMessage)
		{
			HandleStatusChange(module, 0);
			continue;
		}

		bool isAsync = false;
		switch (request->Command)
		{
			case IOS_CLOSE:
				result = HandleClose(module, request);
				break;
			case IOS_OPEN:
				result = HandleOpen(module, request);
				break;
			case IOS_IOCTL:
				result = HandleIoctl(module, message, &isAsync);
				break;
			case IOS_IOCTLV:
				result = HandleIoctlv(module, message, &isAsync);
				break;
			default:
				result = IPC_EINVAL;
		}

		if (result < 0 || !isAsync)
		{
			OSResourceReply(message, result);
		}
	}
	while (true);
}

static int WorkerThread(OH1ModuleControl *module)
{
	void *queueBuffer[4] ALIGNED(16);
	u32 interruptStatus;
	volatile OhciRegs *registers;
	OhciEndpointDescriptor *endpoint;
	WiiTransferDescriptor *lastTransfer;
	u16 length;
	u8 device = module->DeviceEvent;

	/* TODO: the original code was declaring the queue to be 0x10 in size. This
	 * seems a mistake, but it needs to be double-checked. */
	int queueId = OSCreateMessageQueue(queueBuffer, sizeof(queueBuffer) / sizeof(u32));
	int ret = OSRegisterEventHandler(device, queueId, NULL);
	if (ret != 0)
		return -1;

	registers = module->HardwareRegisters;
	registers->InterruptStatus = 0xffffffff;
	OSClearAndEnableEvent(device);
	while (true)
	{
		do
		{
			ret = OSReceiveMessage(queueId, NULL, 0);
		}
		while (ret != 0);

		interruptStatus = registers->InterruptStatus;
		// Check WritebackDoneHead flag
		if (interruptStatus & OHCI_INTR_WDH)
		{
			OSAhbFlushFrom(module->AHBDeviceToFlush);
			OSAhbFlushTo(AHB_STARLET);
			OhciTransferDescriptor *swappedTransfer =
			    MASK_PTR(module->Hcca->HeadDone, swap_u32(0xfffffff0));
			/* Reverse the list */
			OhciTransferDescriptor *previousSwapped = NULL;
			while (swappedTransfer != 0)
			{
				OhciTransferDescriptor *td = swap_ptr(swappedTransfer);
				OhciTransferDescriptor *nextSwapped = td->Next;
				td->Next = previousSwapped;
				previousSwapped = swappedTransfer;
				swappedTransfer = nextSwapped;
			}

			lastTransfer = swap_ptr(previousSwapped);
			while (lastTransfer)
			{
				WiiTransferDescriptor *td_next;

				td_next = swap_ptr(lastTransfer->std.Next);
				IORequestPacket *ioRequest = lastTransfer->IORequestPacket;
				ioRequest->Counter--;
				u32 conditionCode = TD_GET(CC, lastTransfer->std.dw0);
				if (conditionCode == 0)
				{
					length = lastTransfer->Length;
					if (length != 0)
					{
						char *currentBuffer = swap_ptr(lastTransfer->std.CurrentBuffer);
						if (currentBuffer)
						{
							/* The transfer is not complete: CurrentBuffer
							 * points to the byte after the last successfully
							 * transferred one */
							char *lastBuffer = swap_ptr(lastTransfer->std.LastBuffer);
							length += (u16)((currentBuffer - lastBuffer) - 1);
						}
						ioRequest->Transferred += length;
					}

					if (ioRequest->Counter == 0)
					{
						OSAhbFlushFrom(module->AHBDeviceToFlush);
						CleanupIORequest(ioRequest);
					}
					FreeMemory(lastTransfer);
				}
				else
				{
					ioRequest->ErrorCount++;
					printk("OHCI processing TD error: 0x%x\n", conditionCode);
					printk("OHCI processing TD error for td  %p with irp %p\n",
					       lastTransfer, ioRequest);
					endpoint = ioRequest->EndpointDescriptor;
					u32 dw0 = swap_u32(endpoint->dw0);
					OhciTransferDescriptor *head = swap_ptr(endpoint->Head);
					printk("TD error for ed  %p; ed flag = 0x%x headP = %p\n",
					       endpoint, dw0, head);
					printk("TD error for ed  %p\n", endpoint);
					OSAhbFlushFrom(module->AHBDeviceToFlush);
					OSAhbFlushTo(AHB_STARLET);
					DisableEndpoint(module, endpoint);
					CloseEndpoint(module, endpoint);
					if ((u32)head & ED_H)
					{
						printk("OHCI ED %p halted: headP = %p\n", endpoint, head);
						ResumeEndpoint(module, endpoint);
					}
				}
				lastTransfer = td_next;
			}
			/* Setting the bit clears it */
			registers->InterruptStatus = OHCI_INTR_WDH;
		}
		/* Check RootHubStatusChange flag */
		if (interruptStatus & OHCI_INTR_RHSC)
		{
			if ((module->State & OH1_STATE_DEVICE_QUERIED) != 0 &&
			    (module->State & OH1_STATE_PROCESSING_EVENTS) != 0)
			{
				OSSendMessage(module->QueueId, _statusChangeMessage, 0);
			}
			registers->InterruptStatus = OHCI_INTR_RHSC;
		}
		/* Clear all other bits */
		registers->InterruptStatus = interruptStatus & ~(OHCI_INTR_WDH | OHCI_INTR_RHSC);
		OSClearAndEnableEvent(device);
	}
}

int main(void)
{
	int rc;
	volatile OhciRegs *regs;
	u32 frame_interval;
	OH1ModuleControl *module = NULL;

	OSSetThreadPriority(0, 0x60);
	printk("%s\n", "$IOSVersion: OH1: " __DATE__ " " __TIME__ " 64M $");
	rc = CreateHeap();
	if (rc < 0)
		goto error;

	module = OSAllocateMemory(_moduleHeap, sizeof(*module));
	rc = CreateUSBDeviceQueue(module, IRQ_OHCI1);
	if (rc < 0)
		goto error;

	module->HardwareRegisters = OHCI_REG_BASE;
	module->DeviceEvent = IRQ_OHCI1;
	module->AHBDeviceToFlush = AHB_OHCI;
	module->AHBDevice = AHB_OHCI;
	module->Hcca = _hcca;
	memset(_hcca, 0, sizeof(*_hcca));
	rc = OSCreateMessageQueue(&module->TimerQueueBuffer, 1);
	if (rc < 0)
		goto error;

	module->TimerQueue = rc;
	/* Post a status change message request, in order to update the status of
	 * the hub ports. */
	rc = OSCreateTimer(0, 0, module->TimerQueue, _statusChangeMessage);
	if (rc < 0)
		goto error;

	module->Timer = rc;
	rc = InitialiseModule(module);
	if (rc < 0)
		goto error_destroy_timer_queue;

	OhciEndpointDescriptor *endpoints = module->EndpointDescriptors;
	OhciHcca *Hcca = module->Hcca;
	for (int i_endp = 0; i_endp < 32; i_endp++)
	{
		OhciEndpointDescriptor *endp = &endpoints[i_endp];
		Hcca->InterruptTable[i_endp] = swap_ptr(endp);
	}
	OSAhbFlushFrom(AHB_STARLET);

	regs = module->HardwareRegisters;
	regs->DisableInterrupts = OHCI_INTR_MIE;
	u8 rev = regs->Revision & OHCI_REV_MASK;
	if (rev != 0x10)
	{
		rc = IPC_EINVAL;
		goto error_destroy_timer_queue;
	}

	if ((regs->Control & OHCI_CTRL_IR) != 0)
	{
		/* OwnershipChangerequest */
		regs->EnableInterrupts = OHCI_INTR_OC;
		regs->CommandStatus = OHCI_CS_OCR;
		SleepModule(module, 50000);
		if ((regs->Control & OHCI_CTRL_IR) != 0)
		{
			rc = IPC_NOTREADY;
			goto error_destroy_timer_queue;
		}
		SleepModule(module, 20000);
		regs->Control &= OHCI_CTRL_RWC;
	}

	/* We expect the host controller to be in reset state */
	if (OHCI_GET(CTRL_HCFS, regs->Control) != OHCI_CTRL_HCFS_RESET)
	{
		rc = IPC_NOTREADY;
		goto error_destroy_timer_queue;
	}

	/* Nominal value of the frame interval, according to the specs */
	module->FrameInterval = 11999;
	/* Reset the controller; the loop below waits until the reset is
	 * complete */
	regs->CommandStatus |= OHCI_CS_HCR;
	SleepModule(module, 10000);
	for (int tries = 10000; tries > 0; tries--)
	{
		if ((regs->CommandStatus & OHCI_CS_HCR) == 0)
			break;
		SleepModule(module, 2);
	}
	if ((regs->CommandStatus & OHCI_CS_HCR) != 0)
	{
		rc = IPC_NOTREADY;
		goto error_destroy_timer_queue;
	}

	frame_interval = module->FrameInterval;
	u32 largest_data_packet = (frame_interval * 6 - 1260) / 7;
	regs->FrameInterrupt = OHCI_SET(FI_FI, frame_interval) |
	                       OHCI_SET(FI_FSMPS, largest_data_packet);
	/* Spec says that the periodic start should be a 10% off the
	 * HcFmInterval. */
	regs->PeriodicStart = (frame_interval * 9) / 10;
	regs->BulkHeadEndpoint = NULL;
	regs->ControlHeadEndpoint = NULL;
	regs->Hcca = module->Hcca;
	u32 desired_interrupts = OHCI_INTR_MIE | OHCI_INTR_OC | OHCI_INTR_RHSC |
	                         OHCI_INTR_FNO | OHCI_INTR_UE | OHCI_INTR_RD |
	                         OHCI_INTR_WDH | OHCI_INTR_SO;
	regs->InterruptStatus = desired_interrupts;
	regs->EnableInterrupts = desired_interrupts;
	regs->Control = OHCI_CTRL_RWC | OHCI_SET(CTRL_HCFS, OHCI_CTRL_HCFS_OPERATIONAL) |
	                OHCI_SET(CTRL_CBSR, 3);

	/* No idea what this does */
	_ehciRegisters->ChickenBits |= EHCI_CHICKENBITS_INIT;

	s32 priority = OSGetThreadPriority(0);
	rc = OSCreateThread((ThreadFunc)WorkerThread, module, _workerThreadStack,
	                    sizeof(_workerThreadStack), priority, 1);
	if (rc < 0)
		goto error_destroy_timer_queue;

	s32 thread_id = rc;
	OSStartThread(thread_id);
	priority = OSGetThreadPriority(0);
	OSSetThreadPriority(0, priority - 1);
	rc = QueryModuleDevices(module);
	if (rc < 0)
		goto error_destroy_timer_queue;

	ProcessEvents(module);

error_destroy_timer_queue:
	if (module->TimerQueue > 0)
		OSDestroyMessageQueue(module->TimerQueue);

error:
	printk("ohci_core: OHCI initialization failed: %d\n", rc);
	printk("ohci_core exits...\n");
	OSFreeMemory(_moduleHeap, module);
	OSStopThread(0, 0);
	return rc;
}
