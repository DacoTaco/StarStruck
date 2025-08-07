/*
        StarStruck - a Free Software reimplementation for the Nintendo/BroadOn
IOS. printk - printk implementation in ios

        Copyright (C) 2025	Alberto Mardegan <mardy@users.sourceforge.net>

# This code is licensed to you under the terms of the GNU GPL, version 2;
# see file COPYING or http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt
*/

#ifndef __USB_H__
#define __USB_H__

#include "types.h"

/*-------------------------------------------------------------------------*/

/* CONTROL REQUEST SUPPORT */

/*
 * USB directions
 *
 * This bit flag is used in endpoint descriptors' bEndpointAddress field.
 * It's also one of three fields in control requests bRequestType.
 */
#define USB_DIR_OUT                    0   /* to device */
#define USB_DIR_IN                     0x80 /* to host */

/*
 * USB types, the second of three bRequestType fields
 */
#define USB_TYPE_MASK                  (0x03 << 5)
#define USB_TYPE_STANDARD              (0x00 << 5)
#define USB_TYPE_CLASS                 (0x01 << 5)
#define USB_TYPE_VENDOR                (0x02 << 5)
#define USB_TYPE_RESERVED              (0x03 << 5)

/*
 * USB recipients, the third of three bRequestType fields
 */
#define USB_RECIP_MASK                 0x1f
#define USB_RECIP_DEVICE               0x00
#define USB_RECIP_INTERFACE            0x01
#define USB_RECIP_ENDPOINT             0x02
#define USB_RECIP_OTHER                0x03
/* From Wireless USB 1.0 */
#define USB_RECIP_PORT                 0x04
#define USB_RECIP_RPIPE                0x05

/*
 * Standard requests, for the bRequest field of a SETUP packet.
 *
 * These are qualified by the bRequestType field, so that for example
 * TYPE_CLASS or TYPE_VENDOR specific feature flags could be retrieved
 * by a GET_STATUS request.
 */
#define USB_REQ_GET_STATUS             0x00
#define USB_REQ_CLEAR_FEATURE          0x01
#define USB_REQ_SET_FEATURE            0x03
#define USB_REQ_SET_ADDRESS            0x05
#define USB_REQ_GET_DESCRIPTOR         0x06
#define USB_REQ_SET_DESCRIPTOR         0x07
#define USB_REQ_GET_CONFIGURATION      0x08
#define USB_REQ_SET_CONFIGURATION      0x09
#define USB_REQ_GET_INTERFACE          0x0A
#define USB_REQ_SET_INTERFACE          0x0B
#define USB_REQ_SYNCH_FRAME            0x0C
#define USB_REQ_SET_SEL                0x30
#define USB_REQ_SET_ISOCH_DELAY        0x31

#define USB_REQ_SET_ENCRYPTION         0x0D /* Wireless USB */
#define USB_REQ_GET_ENCRYPTION         0x0E
#define USB_REQ_RPIPE_ABORT            0x0E
#define USB_REQ_SET_HANDSHAKE          0x0F
#define USB_REQ_RPIPE_RESET            0x0F
#define USB_REQ_GET_HANDSHAKE          0x10
#define USB_REQ_SET_CONNECTION         0x11
#define USB_REQ_SET_SECURITY_DATA      0x12
#define USB_REQ_GET_SECURITY_DATA      0x13
#define USB_REQ_SET_WUSB_DATA          0x14
#define USB_REQ_LOOPBACK_DATA_WRITE    0x15
#define USB_REQ_LOOPBACK_DATA_READ     0x16
#define USB_REQ_SET_INTERFACE_DS       0x17

/* specific requests for USB Power Delivery */
#define USB_REQ_GET_PARTNER_PDO        20
#define USB_REQ_GET_BATTERY_STATUS     21
#define USB_REQ_SET_PDO                22
#define USB_REQ_GET_VDM                23
#define USB_REQ_SEND_VDM               24

/* The Link Power Management (LPM) ECN defines USB_REQ_TEST_AND_SET command,
 * used by hubs to put ports into a new L1 suspend state, except that it
 * forgot to define its number ...
 */

/*
 * USB feature flags are written using USB_REQ_{CLEAR,SET}_FEATURE, and
 * are read as a bit array returned by USB_REQ_GET_STATUS.  (So there
 * are at most sixteen features of each type.)  Hubs may also support a
 * new USB_REQ_TEST_AND_SET_FEATURE to put ports into L1 suspend.
 */
#define USB_DEVICE_SELF_POWERED        0      /* (read only) */
#define USB_DEVICE_REMOTE_WAKEUP       1     /* dev may initiate wakeup */
#define USB_DEVICE_TEST_MODE           2         /* (wired high speed only) */
#define USB_DEVICE_BATTERY             2           /* (wireless) */
#define USB_DEVICE_B_HNP_ENABLE        3      /* (otg) dev may initiate HNP */
#define USB_DEVICE_WUSB_DEVICE         3       /* (wireless)*/
#define USB_DEVICE_A_HNP_SUPPORT       4     /* (otg) RH port supports HNP */
#define USB_DEVICE_A_ALT_HNP_SUPPORT   5 /* (otg) other RH port does */
#define USB_DEVICE_DEBUG_MODE          6        /* (special devices only) */

/*
 * Test Mode Selectors
 * See USB 2.0 spec Table 9-7
 */
#define USB_TEST_J                     1
#define USB_TEST_K                     2
#define USB_TEST_SE0_NAK               3
#define USB_TEST_PACKET                4
#define USB_TEST_FORCE_ENABLE          5

/* Status Type */
#define USB_STATUS_TYPE_STANDARD       0
#define USB_STATUS_TYPE_PTM            1

/*
 * New Feature Selectors as added by USB 3.0
 * See USB 3.0 spec Table 9-7
 */
#define USB_DEVICE_U1_ENABLE           48 /* dev may initiate U1 transition */
#define USB_DEVICE_U2_ENABLE           49 /* dev may initiate U2 transition */
#define USB_DEVICE_LTM_ENABLE          50 /* dev may send LTM */
#define USB_INTRF_FUNC_SUSPEND         0 /* function suspend */

#define USB_INTR_FUNC_SUSPEND_OPT_MASK 0xFF00
/*
 * Suspend Options, Table 9-8 USB 3.0 spec
 */
#define USB_INTRF_FUNC_SUSPEND_LP      (1 << (8 + 0))
#define USB_INTRF_FUNC_SUSPEND_RW      (1 << (8 + 1))

/*
 * Interface status, Figure 9-5 USB 3.0 spec
 */
#define USB_INTRF_STAT_FUNC_RW_CAP     1
#define USB_INTRF_STAT_FUNC_RW         2

#define USB_ENDPOINT_HALT              0 /* IN/OUT will STALL */

/* Bit array elements as returned by the USB_REQ_GET_STATUS request. */
#define USB_DEV_STAT_U1_ENABLED        2 /* transition into U1 state */
#define USB_DEV_STAT_U2_ENABLED        3 /* transition into U2 state */
#define USB_DEV_STAT_LTM_ENABLED       4 /* Latency tolerance messages */

/*
 * Feature selectors from Table 9-8 USB Power Delivery spec
 */
#define USB_DEVICE_BATTERY_WAKE_MASK   40
#define USB_DEVICE_OS_IS_PD_AWARE      41
#define USB_DEVICE_POLICY_MODE         42
#define USB_PORT_PR_SWAP               43
#define USB_PORT_GOTO_MIN              44
#define USB_PORT_RETURN_POWER          45
#define USB_PORT_ACCEPT_PD_REQUEST     46
#define USB_PORT_REJECT_PD_REQUEST     47
#define USB_PORT_PORT_PD_RESET         48
#define USB_PORT_C_PORT_PD_CHANGE      49
#define USB_PORT_CABLE_PD_RESET        50
#define USB_DEVICE_CHARGING_POLICY     54

/*
 * Endpoints
 */
#define USB_ENDPOINT_NUMBER_MASK       0x0f /* in bEndpointAddress */
#define USB_ENDPOINT_DIR_MASK          0x80

#define USB_ENDPOINT_XFERTYPE_MASK     0x03 /* in bmAttributes */
#define USB_ENDPOINT_XFER_CONTROL      0
#define USB_ENDPOINT_XFER_ISOC         1
#define USB_ENDPOINT_XFER_BULK         2
#define USB_ENDPOINT_XFER_INT          3
#define USB_ENDPOINT_MAX_ADJUSTABLE    0x80

#define USB_ENDPOINT_MAXP_MASK         0x07ff
#define USB_EP_MAXP_MULT_SHIFT         11
#define USB_EP_MAXP_MULT_MASK          (3 << USB_EP_MAXP_MULT_SHIFT)
#define USB_EP_MAXP_MULT(m) \
	(((m) & USB_EP_MAXP_MULT_MASK) >> USB_EP_MAXP_MULT_SHIFT)

/* The USB 3.0 spec redefines bits 5:4 of bmAttributes as interrupt ep type. */
#define USB_ENDPOINT_INTRTYPE          0x30
#define USB_ENDPOINT_INTR_PERIODIC     (0 << 4)
#define USB_ENDPOINT_INTR_NOTIFICATION (1 << 4)

#define USB_ENDPOINT_SYNCTYPE          0x0c
#define USB_ENDPOINT_SYNC_NONE         (0 << 2)
#define USB_ENDPOINT_SYNC_ASYNC        (1 << 2)
#define USB_ENDPOINT_SYNC_ADAPTIVE     (2 << 2)
#define USB_ENDPOINT_SYNC_SYNC         (3 << 2)

#define USB_ENDPOINT_USAGE_MASK        0x30
#define USB_ENDPOINT_USAGE_DATA        0x00
#define USB_ENDPOINT_USAGE_FEEDBACK    0x10
#define USB_ENDPOINT_USAGE_IMPLICIT_FB \
	0x20 /* Implicit feedback Data endpoint */

/*
 * Descriptor types ... USB 2.0 spec table 9.5
 */
#define USB_DT_DEVICE                   0x01
#define USB_DT_CONFIG                   0x02
#define USB_DT_STRING                   0x03
#define USB_DT_INTERFACE                0x04
#define USB_DT_ENDPOINT                 0x05
#define USB_DT_DEVICE_QUALIFIER         0x06
#define USB_DT_OTHER_SPEED_CONFIG       0x07
#define USB_DT_INTERFACE_POWER          0x08
/* these are from a minor usb 2.0 revision (ECN) */
#define USB_DT_OTG                      0x09
#define USB_DT_DEBUG                    0x0a
#define USB_DT_INTERFACE_ASSOCIATION    0x0b
/* these are from the Wireless USB spec */
#define USB_DT_SECURITY                 0x0c
#define USB_DT_KEY                      0x0d
#define USB_DT_ENCRYPTION_TYPE          0x0e
#define USB_DT_BOS                      0x0f
#define USB_DT_DEVICE_CAPABILITY        0x10
#define USB_DT_WIRELESS_ENDPOINT_COMP   0x11
/* From the eUSB2 spec */
#define USB_DT_EUSB2_ISOC_ENDPOINT_COMP 0x12
/* From Wireless USB spec */
#define USB_DT_WIRE_ADAPTER             0x21
/* From USB Device Firmware Upgrade Specification, Revision 1.1 */
#define USB_DT_DFU_FUNCTIONAL           0x21
/* these are from the Wireless USB spec */
#define USB_DT_RPIPE                    0x22
#define USB_DT_CS_RADIO_CONTROL         0x23
/* From the T10 UAS specification */
#define USB_DT_PIPE_USAGE               0x24
/* From the USB 3.0 spec */
#define USB_DT_SS_ENDPOINT_COMP         0x30
/* From the USB 3.1 spec */
#define USB_DT_SSP_ISOC_ENDPOINT_COMP   0x31

/* Conventional codes for class-specific descriptors.  The convention is
 * defined in the USB "Common Class" Spec (3.11).  Individual class specs
 * are authoritative for their usage, not the "common class" writeup.
 */
#define USB_DT_CS_DEVICE                (USB_TYPE_CLASS | USB_DT_DEVICE)
#define USB_DT_CS_CONFIG                (USB_TYPE_CLASS | USB_DT_CONFIG)
#define USB_DT_CS_STRING                (USB_TYPE_CLASS | USB_DT_STRING)
#define USB_DT_CS_INTERFACE             (USB_TYPE_CLASS | USB_DT_INTERFACE)
#define USB_DT_CS_ENDPOINT              (USB_TYPE_CLASS | USB_DT_ENDPOINT)

/* USB device class codes (bDeviceClass) */
#define USB_DEVICE_CLASS_DEVICE         0x0
#define USB_DEVICE_CLASS_AUDIO          0x1
#define USB_DEVICE_CLASS_COMM           0x2
#define USB_DEVICE_CLASS_HID            0x3
#define USB_DEVICE_CLASS_PHYSICAL       0x5
#define USB_DEVICE_CLASS_IMAGE          0x6
#define USB_DEVICE_CLASS_PRINTER        0x7
#define USB_DEVICE_CLASS_MASS_STORAGE   0x8
#define USB_DEVICE_CLASS_HUB            0x9

/* Just to remind us that such fields are in little-endian encoding */
typedef u16 le16;

typedef struct usb_configuration_desc_t usb_configuration_desc;

struct usb_configuration_desc_t
{
	u8 bLength;
	u8 bDescriptorType;
	le16 wTotalLength;
	u8 bNumInterfaces;
	u8 bConfigurationValue;
	u8 iConfiguration;
	u8 bmAttributes;
	u8 bMaxPower;
};

typedef struct usb_device_desc_t usb_device_desc;

struct usb_device_desc_t
{
	u8 bLength;
	u8 bDescriptorType;
	le16 bcdUSB;
	u8 bDeviceClass;
	u8 bDeviceSubClass;
	u8 bDeviceProtocol;
	u8 bMaxPacketSize0;
	le16 idVendor;
	le16 idProduct;
	le16 bcdDevice;
	u8 iManufacturer;
	u8 iProduct;
	u8 iSerialNumber;
	u8 bNumConfigurations;
};

typedef struct usb_ctrl_message_t usb_ctrl_message;

struct usb_ctrl_message_t
{
	u8 bmRequestType;
	u8 bmRequest;
	le16 wValue;
	le16 wIndex;
	le16 wLength;
	union
	{
		char data[8];
		/* This is the struct describing the data format for the OH1 module */
		struct
		{
			void *data;
			u8 endpoint;
			s8 device_index; /* TODO: It seems to be never read, though */
		} oh1;
	};
};

typedef struct usb_endpoint_desc_t usb_endpoint_desc;

struct usb_endpoint_desc_t
{
	u8 bLength;
	u8 bDescriptorType;
	u8 bEndpointAddress;
	u8 bmAttributes;
	u16 wMaxPacketSize;
	u8 bInterval;
};

typedef struct usb_interface_desc_t usb_interface_desc;

struct usb_interface_desc_t
{
	u8 bLength;
	u8 bDescriptorType;
	u8 bInterfaceNumber;
	u8 bAlternateSetting;
	u8 bNumEndpoints;
	u8 bInterfaceClass;
	u8 bInterfaceSubClass;
	u8 bInterfaceProtocol;
	u8 iInterface;
};

typedef struct usb_descriptor_header_t usb_descriptor_header;

struct usb_descriptor_header_t
{
	u8 bLength;
	u8 bDescriptorType;
};

typedef union usb_descriptor_u usb_descriptor;

union usb_descriptor_u
{ // Union for USB descriptors
	struct usb_descriptor_header_t header;
	struct usb_configuration_desc_t conf;
	struct usb_endpoint_desc_t endp;
	struct usb_interface_desc_t iface;
};

#endif
