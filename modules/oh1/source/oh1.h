/*
        StarStruck - a Free Software reimplementation for the Nintendo/BroadOn
IOS. printk - printk implementation in ios

        Copyright (C) 2025	Alberto Mardegan <mardy@users.sourceforge.net>

# This code is licensed to you under the terms of the GNU GPL, version 2;
# see file COPYING or http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt
*/

#ifndef __OH1_H__
#define __OH1_H__

#include "ios/ipc.h"
#include "types.h"
#include "usb/ohci.h"
#include "usb/usb.h"

#include <stdint.h>

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

typedef struct ohci_transfer_descriptor_wii_t ohci_transfer_descriptor_wii;

typedef struct io_request_packet_t
{
	IpcMessage *resource_req;
	s8 counter;
	u8 err_count;
	u16 transferred;
	char *msg_data;
	u32 size;
	ohci_transfer_descriptor_wii *xfer_desc;
	ohci_endpoint_descriptor *endpoint_desc;
	u32 unused;
	s32 queue;
	void *ctrl_msg; /* Only set if queue == -1 */
} io_request_packet;

struct ohci_transfer_descriptor_wii_t
{
	ohci_transfer_descriptor std;
	io_request_packet *irp;
	u16 length;
};

typedef struct device_interface_t
{
	u8 bInterfaceNumber;
	u8 bAlternateSetting;
	u8 bInterfaceClass;
	u8 bInterfaceSubClass;
	u8 bInterfaceProtocol;
} ALIGNED(4) device_interface;

typedef struct oh1_device_t oh1_device;

typedef struct device_endpoint_t device_endpoint;

struct device_endpoint_t
{
	u8 bEndpointAddress;
	u8 bmAttributes;
	u16 wMaxPacketSize;
	u8 bInterval;
	ohci_endpoint_descriptor *descriptor;
	ohci_transfer_descriptor_wii *xfer_desc;
	ohci_transfer_descriptor_isoc *xfer_desc_isoc;
};

typedef enum
{
	DEV_TYPE_NONE = 0,
	DEV_TYPE_HUB,
	DEV_TYPE_DEVICE,
} DeviceType;

struct oh1_device_t
{
	DeviceType device_type;
	u8 zero;
	u8 port_index;
	u8 low_speed_2;  // unused
	u8 is_low_speed;
	u8 max_power;
	u16 vendor_id;
	u16 product_id;
	u8 device_class;
	u8 device_subclass;
	u8 device_protocol;
	u8 num_interfaces;
	IpcMessage *ipc_message;
	struct device_interface_t interfaces[8];
	struct device_endpoint_t endpoints[16];
};

typedef s32 QueueId;

typedef struct
{
	u32 unused; /* probably meant to be used for the device ID */
	u16 vendor_id;
	u16 product_id;
} oh1_devlist_entry;

typedef struct oh1_module_ctrl oh1_module_ctrl;

struct oh1_module_ctrl
{
	volatile ohci_regs *hw_regs;
	ohci_hcca *hcca;
	u32 frame_interval;
	ohci_endpoint_descriptor *endpoint_descriptors;
	s32 timer;
	QueueId timer_queue;
	QueueId queue_id;
	u32 timer_queue_buffer;
	u8 device_event;
	u8 number_downstream_ports;
#define OH1_STATE_DEVICE_QUERIED    (1 << 0)
#define OH1_STATE_PROCESSING_EVENTS (1 << 1)
	u32 state;
	u8 ahb_dev;
	u8 ahb_dev_flush;
};

#endif
