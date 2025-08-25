/*
        StarStruck - a Free Software reimplementation for the Nintendo/BroadOn
IOS. printk - printk implementation in ios

        Copyright (C) 2025	Alberto Mardegan <mardy@users.sourceforge.net>

# This code is licensed to you under the terms of the GNU GPL, version 2;
# see file COPYING or http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt
*/

#include "oh1.h"

#include <ios/errno.h>
#include <ios/printk.h>
#include <ios/syscalls.h>
#include <string.h>
#include <usb/ehci.h>

#define OHCI_REG_BASE            ((void *)0x0d060000)

#define OH1_TRANSFER_BASE        ((void *)0x13880400)
#define OH1_TRANSFER_SIZE        (0x1000)
#define OH1_HEAP_BASE            ((void *)0x13881400)
#define OH1_HEAP_SIZE            0x4000

#define MASK_PTR(ptr, mask)      ((void *)((u32)ptr & (mask)))
#define PTR_WITH_BITS(ptr, bits) ((void *)((u32)ptr | (bits)))
#define PADDED4_SIZEOF(type)     (((sizeof(type) + 3) / 4) * 4)

static s32 s_heap_16k;
static char s_usb_device_name[16];
static ohci_hcca *g_HCCA = (void *)0x13880000;
static ohci_endpoint_descriptor *s_ohci_intr_endpoints = OH1_TRANSFER_BASE;
static EhciRegisters *s_ehci_regs = (void *)EHCI_REG_BASE;
static u8 s_endp_indexes[] = { 62, 60, 56, 48, 32 };
static usb_ctrl_message *g_control_request = NULL;
static oh1_device g_devices[4];
static s16 s_root_hub_fd = 0x7fff;
static s32 s_device_queue_id;
static u32 s_device_queue_buffer[4];
static void *s_status_change_message = (void *)0xcafef00d;

static u32 s_worker_thread_stack[0x100]; /* That is, 0x400 bytes */

static inline void *swap_ptr(void *ptr)
{
	return (void *)__builtin_bswap32((u32)ptr);
}

static inline u32 swap_u32(u32 v)
{
	return __builtin_bswap32(v);
}

static inline u16 swap_u16(u16 v)
{
	return __builtin_bswap16(v);
}

static s32 create_heap_16K(void)
{
	memset(OH1_HEAP_BASE, 0, OH1_HEAP_SIZE);
	s_heap_16k = OSCreateHeap(OH1_HEAP_BASE, OH1_HEAP_SIZE);
	return s_heap_16k;
}

static s32 create_heap(void)
{
	s32 heap_handle;

	heap_handle = create_heap_16K();
	if (heap_handle < 0)
	{
		if (s_heap_16k > 0)
		{
			OSDestroyHeap(s_heap_16k);
		}
		return IPC_EINVAL;
	}
	return IPC_SUCCESS;
}

static void ohci_enable_interrupts(oh1_module_ctrl *module)
{
	module->hw_regs->inten = OHCI_INTR_MIE;
}

static void ohci_disable_interrupts(oh1_module_ctrl *module)
{
	module->hw_regs->intdis = OHCI_INTR_MIE;
}

static int usleept(oh1_module_ctrl *module, u32 timeout)
{
	int rc;
	void *message;

	rc = OSRestartTimer(module->timer, timeout, 0);
	if (rc < 0)
	{
		printk("usleept: RestartTimer (tmr = %d usec = %u) failed: %d\n",
		       module->timer, timeout, rc);
	}
	else
	{
		rc = OSReceiveMessage(module->timer_queue, &message, 0);
		OSStopTimer(module->timer);
	}
	return rc;
}

static void oh1_heap_free(void *ptr)
{
	OSFreeMemory(s_heap_16k, ptr);
}

static ohci_transfer_descriptor_wii *xfer_desc_alloc(void)
{
	ohci_transfer_descriptor_wii *xfer;

	xfer = OSAlignedAllocateMemory(s_heap_16k, sizeof(ohci_transfer_descriptor_wii), 0x10);
	if (xfer)
	{
		memset(xfer, 0, sizeof(ohci_transfer_descriptor_wii));
	}
	return xfer;
}

static bool xfer_desc_is_on_heap(void *ptr)
{
	return ptr && ((u32)ptr & 0xf) == 0 && ptr >= OH1_HEAP_BASE &&
	       ptr < OH1_HEAP_BASE + OH1_HEAP_SIZE;
}

static ohci_transfer_descriptor_isoc *xfer_desc_isoc_alloc(void)
{
	ohci_transfer_descriptor_isoc *ptr;

	ptr = OSAlignedAllocateMemory(s_heap_16k, sizeof(ohci_transfer_descriptor_isoc), 16);
	if (ptr)
	{
		memset(ptr, 0, sizeof(ohci_transfer_descriptor_isoc));
	}
	return ptr;
}

static ohci_endpoint_descriptor *endp_desc_alloc(void)
{
	void *ptr;

	ptr = OSAlignedAllocateMemory(s_heap_16k, sizeof(ohci_endpoint_descriptor), 16);
	if (ptr)
	{
		memset(ptr, 0, sizeof(ohci_endpoint_descriptor));
	}
	return ptr;
}

static s8 find_index_of_endpoint(s8 device_index, u8 bEndpoint)
{
	for (s8 i = 1; i < 16; i++)
	{
		if (g_devices[device_index].endpoints[i].bEndpointAddress == bEndpoint)
			return i;
	}
	return -1;
}

static void set_ctrl_stat_regs(oh1_module_ctrl *module, u32 ctrl_set, u32 cmdstat)
{
	volatile ohci_regs *module_data = module->hw_regs;

	if ((module_data->ctrl & ctrl_set) == 0)
	{
		module_data->ctrl |= ctrl_set;
	}
	module_data->cmdstat = cmdstat;
}

static void *sram_canonical(void *ptr)
{
	if ((ptr >= (void *)0xffff0000) && (ptr != (void *)0xffffffff))
	{
		ptr = (void *)((int)ptr + 0xd410000);
	}
	return ptr;
}

static int init_transfer_descriptors(oh1_module_ctrl *module)
{
	int rel_index_of_next;
	int index_of_next;
	ohci_endpoint_descriptor *endp;
	int index;
	int num_entries_in_level;
	int base_index;
	int base_index_of_next;

 /* This function fills the "next" pointers in all the interrupt endpoint
     * descriptors. The OHCI specs defines this table in figure 3-4 at page 10:
     * there are 32 entries in the interrupt table at the first level (which is
     * used for endpoints having poll interval of 32 ms), and their "next"
     * pointers converge into 16 entries (interval: 16 ms), which in turn
     * converge into 8 entries (interval: 8 ms), then 4, then 2, up to the final
     * entry, which is triggered at every millisecond. */
	memset(s_ohci_intr_endpoints, 0, OH1_TRANSFER_SIZE);
	base_index_of_next = 32;
	base_index = 0;
	num_entries_in_level = 32;
	do
	{
		if (num_entries_in_level != 0)
		{
			for (int index_in_level = 0; index_in_level < num_entries_in_level; index_in_level++)
			{
				index = base_index + index_in_level;
				s_ohci_intr_endpoints[index_in_level].dw0 =
				    (base_index == 0) ? 0 : swap_u32(ED_SKIP);

				rel_index_of_next = index_in_level % (num_entries_in_level / 2);
				index_of_next = rel_index_of_next + base_index_of_next;
				endp = s_ohci_intr_endpoints + index_of_next;
				s_ohci_intr_endpoints[index].ed_next = swap_ptr(endp);
			}
		}
		base_index += num_entries_in_level;
		num_entries_in_level /= 2;
		base_index_of_next += num_entries_in_level;
		num_entries_in_level = num_entries_in_level;
	}
	while (num_entries_in_level > 1);

	module->endpoint_descriptors = s_ohci_intr_endpoints;
	return 0;
}

static ohci_endpoint_descriptor *get_intr_endp_descriptor(oh1_module_ctrl *module, u8 bInterval)
{
	int level = 0;

	for (u32 halved = bInterval / 2; halved != 0; halved /= 2)
	{
		level++;
	}
	if (level > 5)
	{
		level = 5;
	}

 /* 63 = 32 + 16 + 8 + 4 + 2 + 1, that is the number of interrupt endpoint
	 * descriptors. */
	for (int index = s_endp_indexes[level]; index < 63; index++)
	{
		ohci_endpoint_descriptor *ed = &module->endpoint_descriptors[index];
		u32 dw0 = swap_u32(ed->dw0);
		if (!(dw0 & ED_WII31))
		{
			ed->dw0 = swap_u32(dw0 | ED_WII31);
			return ed;
		}
	}
	return NULL;
}

static void device_append_endpoint_descriptor(oh1_module_ctrl *module,
                                              ohci_endpoint_descriptor *endp_desc)
{
	ohci_endpoint_descriptor *ed;
	ohci_endpoint_descriptor *next;

	ed = module->endpoint_descriptors + s_endp_indexes[0];
	next = ed->ed_next;
	while (next != NULL)
	{
		ed = swap_ptr(next);
		next = ed->ed_next;
	}
	ed->ed_next = swap_ptr(endp_desc);
}

static void fill_transfer(ohci_transfer_descriptor_wii *xfer_desc, io_request_packet *irp,
                          char *data, u16 length, u32 dw0, u32 delay_interrupt)
{
	char *ptr;

	ptr = sram_canonical(data);
	xfer_desc->std.current_buffer = swap_ptr(ptr);
	xfer_desc->std.buffer_last = swap_ptr(ptr + (length - 1));
	xfer_desc->std.dw0 = swap_u32(dw0 | TD_SET(DI, delay_interrupt));
	xfer_desc->irp = irp;
	xfer_desc->length = length;
	irp->counter++;
}

static int reset_port(oh1_module_ctrl *module, u8 port)
{
	int attempts = 4;

	if (module->hw_regs->rhportXstat[port] & RH_PS_WII31)
	{
		module->hw_regs->rhportXstat[port] = RH_PS_PRS;
		usleept(module, 20000);
		do
		{
			if (module->hw_regs->rhportXstat[port] & RH_PS_PRSC)
			{
				module->hw_regs->rhportXstat[port] = RH_PS_PRSC;
				return 0;
			}
			attempts--;
			usleept(module, 10);
		}
		while (attempts != 0);
	}
	return IPC_NOTREADY;
}

static int suspend_device(oh1_module_ctrl *module, u8 port)
{
	volatile ohci_regs *regs;

	if (port > module->number_downstream_ports)
		return IPC_EINVAL;

	regs = module->hw_regs;
	if (!(regs->rhportXstat[port] & RH_PS_CCS))
		return IPC_NOTREADY;

 // PortSuspendStatus
	regs->rhportXstat[port] = RH_PS_PSS;
	if (!(regs->rhportXstat[port] & RH_PS_PSS))
		return IPC_UNKNOWN;

	return IPC_SUCCESS;
}

static int resume_device(oh1_module_ctrl *module, u8 port)
{
	volatile ohci_regs *regs;

	if (port > module->number_downstream_ports)
		return IPC_EINVAL;

	regs = module->hw_regs;
	if (regs->rhportXstat[port] & RH_PS_PSS &&
	    /* This gcc warning indeed reveals a bug: RH_PS_PSS is 0x4, so there's
		 * no way that the and'ed value can be equal to 1.
		 * TODO: fix it later, after checking that it brings no regressions. */
	    (regs->rhportXstat[port] = RH_PS_POCI, (regs->rhportXstat[port] & RH_PS_PSS) == 1))
		return IPC_UNKNOWN;

	return IPC_SUCCESS;
}

static void endp_flush(oh1_module_ctrl *module, ohci_endpoint_descriptor *endp_desc)
{
	u32 dw0 = swap_u32(endp_desc->dw0);
	endp_desc->dw0 = swap_u32(dw0 & ~ED_SKIP);
	OSDCFlushRange(endp_desc, sizeof(*endp_desc));
	OSAhbFlushTo(module->ahb_dev);
}

static int send_usb_ctrl_msg(oh1_module_ctrl *module, usb_ctrl_message *ctrl_msg,
                             IpcMessage *ipc_req, QueueId queue_id, s8 device_index)
{
	ohci_transfer_descriptor_wii *xfer_next;
	ohci_transfer_descriptor_wii *xfer_desc;
	s8 i_endp;
	ohci_transfer_descriptor_wii *xfer_desc_data;
	ohci_endpoint_descriptor *endp_desc;
	u32 dw0;
	ohci_transfer_descriptor *new_tail;
	ohci_transfer_descriptor_wii *xfer_tail;
	int rc = IPC_SUCCESS;

	ohci_disable_interrupts(module);
	OSAhbFlushFrom(module->ahb_dev_flush);
	OSAhbFlushTo(AHB_STARLET);
	OSDCFlushRange(ctrl_msg, sizeof(*ctrl_msg));
	io_request_packet *irp = OSAllocateMemory(s_heap_16k, sizeof(io_request_packet));
	xfer_next = xfer_desc_alloc();

	u16 length = swap_u16(ctrl_msg->wLength);
	if (length == 0)
	{
		xfer_desc_data = NULL;
	}
	else
	{
		xfer_desc_data = xfer_desc_alloc();
		OSDCFlushRange(ctrl_msg->data, length);
	}
	xfer_desc = xfer_desc_alloc();
	if (!irp || !xfer_next || (!xfer_desc_data && length != 0) || !xfer_desc)
	{
		rc = IPC_EMAX; /* TODO: why this error code? */
		goto error;
	}

	if (ctrl_msg->oh1.endpoint == 0)
	{
		i_endp = 0;
	}
	else
	{
		i_endp = find_index_of_endpoint(device_index, ctrl_msg->oh1.endpoint);
		if (i_endp < 1)
		{
			rc = IPC_EINVAL;
			/* The original code was not freeing the memory here! */
			goto error;
		}
	}
	endp_desc = g_devices[device_index].endpoints[i_endp].descriptor;
	irp->resource_req = ipc_req;
	irp->queue = queue_id;
	irp->counter = 0;
	irp->err_count = 0;
	irp->transferred = 0;
	irp->unused = 0;
	irp->endpoint_desc = endp_desc;
	irp->msg_data = ctrl_msg->oh1.data;
	irp->size = length;
	irp->ctrl_msg = (queue_id == -1) ? ctrl_msg : NULL;
	if (length != 0)
	{
		u32 dw0 = TD_SET(CC, TD_NOTACCESSED) |
		          TD_SET(DT, 0x3) | /* DataToggle is set to 1 for control transfers */
		          TD_R;
		dw0 |= (ctrl_msg->bmRequestType & USB_ENDPOINT_DIR_MASK) == USB_DIR_IN ?
		           TD_DP_IN :
		           TD_DP_OUT;
		fill_transfer(xfer_desc_data, irp, ctrl_msg->oh1.data, length, dw0, 1);
	}
	/* TODO: check this condition: maybe it should be USB_DIR_IN? */
	if (length == 0 || (ctrl_msg->bmRequestType & USB_ENDPOINT_DIR_MASK) == USB_DIR_OUT)
	{
		dw0 = TD_DP_IN;
	}
	else
	{
		dw0 = TD_DP_OUT;
	}
	fill_transfer(xfer_desc, irp, NULL, 0, dw0 | TD_SET(CC, 0xf) | TD_SET(DT, 0x3), 0);
	void *td_swapped = endp_desc->td_tail;
	irp->xfer_desc = xfer_next;
	xfer_tail = swap_ptr(td_swapped);
	xfer_next = irp->xfer_desc;
	fill_transfer(xfer_tail, irp, (char *)ctrl_msg, 8,
	              TD_SET(CC, 0xf) | TD_SET(DT, 0x2), 2);
	irp->xfer_desc = xfer_tail;
	if (xfer_desc_data)
	{
		xfer_tail->std.td_next = swap_ptr(xfer_desc_data);
		xfer_desc_data->std.td_next = swap_ptr(xfer_desc);
	}
	else
	{
		xfer_tail->std.td_next = swap_ptr(xfer_desc);
	}
	new_tail = swap_ptr(xfer_next);
	xfer_desc->std.td_next = new_tail;
	OSAhbFlushFrom(AHB_1);
	endp_desc->td_tail = new_tail;
	endp_flush(module, endp_desc);
	set_ctrl_stat_regs(module, OHCI_CTRL_CLE, OHCI_CS_CLF);

	ohci_enable_interrupts(module);
	return rc;

error:
	if (irp != NULL)
	{
		OSFreeMemory(s_heap_16k, irp);
	}
	if (xfer_next != NULL)
	{
		oh1_heap_free(xfer_next);
	}
	if (xfer_desc_data != NULL)
	{
		oh1_heap_free(xfer_desc_data);
	}
	if (xfer_desc != NULL)
	{
		oh1_heap_free(xfer_desc);
	}
	ohci_enable_interrupts(module);
	return rc;
}

static int send_usb_ctrl_msg_sync(oh1_module_ctrl *module, s8 i_device, usb_ctrl_message *msg)
{
	int rc;
	void *message;

	rc = send_usb_ctrl_msg(module, msg, NULL, module->timer_queue, i_device);
	if (rc >= 0)
	{
		rc = OSReceiveMessage(module->timer_queue, &message, 0);
	}
	return rc;
}

static int get_usb_descriptor0(oh1_module_ctrl *module, s8 i_device,
                               s8 i_device2, usb_device_desc *desc)
{
	usb_ctrl_message *msg = g_control_request;

	memset(msg, 0, sizeof(*msg));
	msg->bmRequestType = USB_DIR_IN;
	msg->bmRequest = USB_REQ_GET_DESCRIPTOR;
	msg->wValue = USB_DT_DEVICE;
	msg->wIndex = 0;
	msg->wLength = swap_u16(PADDED4_SIZEOF(*desc));
	msg->oh1.data = desc;
	msg->oh1.endpoint = 0;
	msg->oh1.device_index = i_device2;
	return send_usb_ctrl_msg_sync(module, i_device, msg);
}

static int setup_endpoint(oh1_module_ctrl *module, device_endpoint *endpoint, s8 device_index)
{
	ohci_transfer_descriptor *td;
	ohci_endpoint_descriptor *desc;
	u32 dw0;
	u8 xfer_type;

	xfer_type = endpoint->bmAttributes & 3;
	if (xfer_type == USB_ENDPOINT_XFER_ISOC)
	{
		endpoint->xfer_desc_isoc = xfer_desc_isoc_alloc();
		td = &endpoint->xfer_desc_isoc->header;
	}
	else
	{
		endpoint->xfer_desc = xfer_desc_alloc();
		td = &endpoint->xfer_desc->std;
	}

	if (!td)
		return IPC_ENOMEM;

	if (xfer_type == USB_ENDPOINT_XFER_INT)
	{
		desc = get_intr_endp_descriptor(module, endpoint->bInterval);
	}
	else
	{
		desc = endp_desc_alloc();
	}

	endpoint->descriptor = desc;
	if (!desc)
		return IPC_SUCCESS;

	void *td_swapped = swap_ptr(td);
	desc->td_head = td_swapped;
	desc->td_tail = td_swapped;
	if (endpoint->bEndpointAddress & USB_DIR_IN)
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
	dw0 |= ED_SET(EN, endpoint->bEndpointAddress) | ED_SET(FA, device_index) |
	       ED_SET(MPS, endpoint->wMaxPacketSize) | ED_SKIP | ED_WII31;
	if (g_devices[device_index].is_low_speed)
		dw0 |= ED_LOWSPEED;
	if (xfer_type == USB_ENDPOINT_XFER_ISOC)
		dw0 |= ED_ISO;
	desc->dw0 = swap_u32(dw0);
	return IPC_SUCCESS;
}

static void ohci_append_ctrl_endp(oh1_module_ctrl *module, ohci_endpoint_descriptor *endp_desc)
{
	ohci_endpoint_descriptor *ctrlhd_old;
	ohci_endpoint_descriptor *next;

	ctrlhd_old = module->hw_regs->ctrlhd;
	if (ctrlhd_old == NULL)
	{
		module->hw_regs->ctrlhd = endp_desc;
	}
	else
	{
		for (next = ctrlhd_old->ed_next;
		     next != NULL && endp_desc != ctrlhd_old; next = ctrlhd_old->ed_next)
		{
			ctrlhd_old = swap_ptr(next);
		}

		if (ctrlhd_old != endp_desc)
		{
			ctrlhd_old->ed_next = swap_ptr(endp_desc);
		}
	}
}

static void ohci_append_bulk_endp(oh1_module_ctrl *module, ohci_endpoint_descriptor *endp_desc)
{
	ohci_endpoint_descriptor *last;
	ohci_endpoint_descriptor *last_swapped;

	last = module->hw_regs->blkhd;
	if (!last)
	{
		module->hw_regs->blkhd = endp_desc;
	}
	else
	{
		if (endp_desc != last)
		{
			last_swapped = last->ed_next;
			while ((last_swapped != NULL && (last = swap_ptr(last_swapped), endp_desc != last)))
			{
				last_swapped = last->ed_next;
			}
		}
		if (last != endp_desc)
		{
			last->ed_next = swap_ptr(endp_desc);
		}
	}
}

/* Sets the FunctionAddress field of the endpoint descriptor to be the value of
 * the device index. */
static void device_set_function_address(s8 device_index)
{
	ohci_endpoint_descriptor *endp_desc;

	endp_desc = g_devices[device_index].endpoints[0].descriptor;
	u32 dw0 = swap_u32(endp_desc->dw0);
	endp_desc->dw0 = swap_u32(dw0 | ED_SET(FA, device_index));
}

static void device_set_max_packet_size(s8 device_index, u8 max_packet_size)
{
	ohci_endpoint_descriptor *endp_desc;

	endp_desc = g_devices[device_index].endpoints[0].descriptor;
	u32 dw0 = swap_u32(endp_desc->dw0);
	endp_desc->dw0 = swap_u32(ED_CLEAR(MPS, dw0) | ED_SET(MPS, max_packet_size));
	g_devices[device_index].endpoints[0].wMaxPacketSize = max_packet_size;
}

static void device_set_vendor(s8 device_index, u16 vendor)
{
	g_devices[device_index].vendor_id = vendor;
}

static void device_set_product(s8 device_index, u16 product)
{
	g_devices[device_index].product_id = product;
}

static void device_set_class(s8 device_index, u8 device_class, u8 subclass, u8 protocol)
{
	g_devices[device_index].device_class = device_class;
	g_devices[device_index].device_subclass = subclass;
	g_devices[device_index].device_protocol = protocol;
}

static void device_set_max_power(s8 device_index, u8 max_power)
{
	g_devices[device_index].max_power = max_power;
}

static void device_add_interface(s8 device_index, u8 bInterfaceNumber,
                                 u8 bAlternateSetting, u8 bInterfaceClass,
                                 u8 bInterfaceSubClass, u8 bInterfaceProtocol)
{
	int i = g_devices[device_index].num_interfaces;
	device_interface *iface = &g_devices[device_index].interfaces[i];
	if (iface->bInterfaceClass != 0)
		return;

	iface->bInterfaceNumber = bInterfaceNumber;
	iface->bAlternateSetting = bAlternateSetting;
	iface->bInterfaceSubClass = bInterfaceSubClass;
	iface->bInterfaceClass = bInterfaceClass;
	iface->bInterfaceProtocol = bInterfaceProtocol;
	g_devices[device_index].num_interfaces++;
}

static s8 device_get_available(oh1_module_ctrl *module, u8 port_index, u8 zero, bool low_speed_device)
{
	ohci_endpoint_descriptor *endp_desc;
	ohci_transfer_descriptor_wii *xfer_desc;
	void *xfer_desc_swapped;

	for (s8 i_device = 1; i_device < 4; i_device++)
	{
		if (g_devices[i_device].device_type != DEV_TYPE_NONE)
			continue;
		g_devices[i_device].port_index = port_index;
		g_devices[i_device].device_type = DEV_TYPE_DEVICE;
		g_devices[i_device].zero = zero;
		g_devices[i_device].endpoints[0].bEndpointAddress = 0;
		endp_desc = endp_desc_alloc();
		g_devices[i_device].endpoints[0].descriptor = endp_desc;
		xfer_desc = xfer_desc_alloc();
		g_devices[i_device].endpoints[0].xfer_desc = xfer_desc;
		g_devices[i_device].is_low_speed = low_speed_device;
		u16 size = low_speed_device ? 8 : 32;
		g_devices[i_device].endpoints[0].wMaxPacketSize = size;
		for (int i_endp = 1; i_endp < 16; i_endp++)
		{
			g_devices[i_device].endpoints[i_endp].bEndpointAddress = 0;
		}
		g_devices[i_device].num_interfaces = 0;
		xfer_desc_swapped = swap_ptr(xfer_desc);
		endp_desc->td_tail = xfer_desc_swapped;
		endp_desc->td_head = xfer_desc_swapped;
		endp_desc->ed_next = NULL;
		/* TODO double check this: what's the relationship between being a
		 * low-speed device and the endpoint number? Maybe we misunderstood
		 * something in ghidra... */
		endp_desc->dw0 = swap_u32(ED_SET(MPS, size) | ED_SET(EN, low_speed_device & 1));
		ohci_append_ctrl_endp(module, endp_desc);
		return i_device;
	}
	return 0;
}

static s8 device_index_by_vid_pid(u16 vid, u16 pid)
{
	for (s8 i_device = 0; i_device < 4; i_device++)
	{
		if (g_devices[i_device].vendor_id == vid && g_devices[i_device].product_id == pid)
			return i_device;
	}
	return -1;
}

static int handle_disconnection(oh1_module_ctrl *, u8 port)
{
	for (int i = 1; i < 4; i++)
	{
		if (g_devices[i].device_type == DEV_TYPE_DEVICE && g_devices[i].port_index == port)
		{
			if (g_devices[i].ipc_message != NULL)
			{
				OSResourceReply(g_devices[i].ipc_message, 0);
			}
			return IPC_SUCCESS;
		}
	}
	return IPC_EINVAL;
}

static void handle_status_changes(oh1_module_ctrl *module, int /*unused*/)
{
	volatile ohci_regs *regs;

	u8 num_ports = module->number_downstream_ports;
	regs = module->hw_regs;
	regs->intdis = OHCI_INTR_RHSC;
	for (u8 port = 0; port < num_ports; port++)
	{
		/* Nothing to do if the connection status didn't change */
		if (!(regs->rhportXstat[port] & RH_PS_CSC))
			continue;

		if (regs->rhportXstat[port] & RH_PS_CCS)
		{
			regs->rhportXstat[port] = RH_PS_PPS;
			reset_port(module, port);
		}
		else
		{
			handle_disconnection(module, port);
		}
		// ConnectStatusChange and PortEnableStatusChange
		regs->rhportXstat[port] = RH_PS_PESC & RH_PS_CSC;
	}
	regs->inten = OHCI_INTR_RHSC;
}

static void parse_descriptors(oh1_module_ctrl *module, s8 device_index,
                              const usb_configuration_desc *cfg_reply, size_t total_length)
{
	u8 endp_index;
	u32 used_slots_mask;
	usb_descriptor *descriptors_end;
	u8 iface_num;
	device_endpoint *endp;
	usb_descriptor *desc;
	u8 num_endpoints = 0;
	u8 bAlternateSetting;
	u8 bInterfaceClass;
	u8 bInterfaceProtocol;
	u8 bInterfaceSubClass;

	endp_index = 1;
	used_slots_mask = 0;
	descriptors_end = (usb_descriptor *)(&cfg_reply->bLength + total_length);

	for (desc = (usb_descriptor *)((u32)cfg_reply + cfg_reply->bLength); desc < descriptors_end;
	     desc = (usb_descriptor *)((u32)desc + desc->header.bLength))
	{
		if (num_endpoints == 0)
		{
			if (desc->header.bDescriptorType != USB_DT_INTERFACE)
				continue;

			iface_num = desc->iface.bInterfaceNumber;
			if (iface_num > 8)
				break;

			num_endpoints = desc->iface.bNumEndpoints;
			if (num_endpoints == 0)
				continue;

			bAlternateSetting = desc->iface.bAlternateSetting;
			bInterfaceClass = desc->iface.bInterfaceClass;
			bInterfaceSubClass = desc->iface.bInterfaceSubClass;
			bInterfaceProtocol = desc->iface.bInterfaceProtocol;
			if ((1 << iface_num & used_slots_mask) == 0)
			{
				device_add_interface(device_index, iface_num, bAlternateSetting, bInterfaceClass,
				                     bInterfaceSubClass, bInterfaceProtocol);
			}
		}
		else
		{
			/* Parse the endpoints defined in this interface */
			if (desc->header.bDescriptorType != USB_DT_ENDPOINT)
				continue;

			num_endpoints--;
			if ((1 << iface_num & used_slots_mask) != 0)
				continue;

			endp = &g_devices[device_index].endpoints[endp_index];

			endp->bEndpointAddress = desc->endp.bEndpointAddress;
			endp->bmAttributes = desc->endp.bmAttributes;
			endp->wMaxPacketSize = desc->endp.wMaxPacketSize;
			endp->bInterval = desc->endp.bInterval;
			setup_endpoint(module, endp, device_index);

			u8 xfer_type = endp->bmAttributes & USB_ENDPOINT_XFERTYPE_MASK;
			if (xfer_type == USB_ENDPOINT_XFER_ISOC)
			{
				device_append_endpoint_descriptor(module, endp->descriptor);
			}
			else if (xfer_type == USB_ENDPOINT_XFER_CONTROL)
			{
				ohci_append_ctrl_endp(module, endp->descriptor);
			}
			else if (xfer_type == USB_ENDPOINT_XFER_BULK)
			{
				ohci_append_bulk_endp(module, endp->descriptor);
			}
			endp_index++;
			if (endp_index > 16)
				return;

			if (num_endpoints == 0)
			{
				/* No more remaining enpoints: interface is defined */
				used_slots_mask |= 1 << iface_num;
			}
		}
	}
}

static int device_configure(oh1_module_ctrl *module, s8 device_index,
                            usb_device_desc *dev_desc, usb_configuration_desc *conf_desc)
{
	usb_ctrl_message *msg;
	usb_configuration_desc *cfg_reply;
	int rc;

	memset(g_control_request, 0, sizeof(*g_control_request));
	msg = g_control_request;
	msg->bmRequestType = USB_DIR_OUT | USB_TYPE_STANDARD;
	msg->bmRequest = USB_REQ_SET_ADDRESS;
	msg->wValue = swap_u16((u16)device_index);
	msg->wLength = 0;
	msg->wIndex = 0;
	msg->oh1.data = NULL;
	msg->oh1.endpoint = 0;
	msg->oh1.device_index = 0;
	rc = send_usb_ctrl_msg_sync(module, device_index, msg);
	if (rc != 0)
		return rc;

	device_set_function_address(device_index);
	rc = get_usb_descriptor0(module, device_index, device_index, dev_desc);
	if (rc != 0)
		return rc;

	device_set_max_packet_size(device_index, dev_desc->bMaxPacketSize0);
	/* TODO: why are we doing this twice? */
	rc = get_usb_descriptor0(module, device_index, device_index, dev_desc);
	if (rc != 0)
		return rc;

	device_set_vendor(device_index, swap_u16(dev_desc->idVendor));
	device_set_product(device_index, swap_u16(dev_desc->idProduct));
	u8 device_class = dev_desc->bDeviceClass;
	if ((device_class != USB_DEVICE_CLASS_DEVICE) && (device_class != USB_DEVICE_CLASS_HUB))
	{
		device_set_class(device_index, device_class, dev_desc->bDeviceSubClass,
		                 dev_desc->bDeviceProtocol);
	}
	memset(g_control_request, 0, sizeof(*g_control_request));
	msg->bmRequestType = USB_DIR_IN;
	msg->bmRequest = USB_REQ_GET_DESCRIPTOR;
	msg = g_control_request;
	msg->wValue = 2;
	msg->wLength = 0xc00;
	msg->wIndex = 0;
	msg->oh1.data = conf_desc;
	msg->oh1.endpoint = 0;
	msg->oh1.device_index = device_index;
	send_usb_ctrl_msg_sync(module, device_index, msg);

	conf_desc->wTotalLength = swap_u16(conf_desc->wTotalLength);
	device_set_max_power(device_index, conf_desc->bMaxPower);
	u16 total_length = conf_desc->wTotalLength;
	cfg_reply = OSAlignedAllocateMemory(s_heap_16k, total_length, 32);
	if (cfg_reply)
	{
		memset(cfg_reply, 0, total_length);
		memset(g_control_request, 0, sizeof(*g_control_request));
		msg->bmRequestType = USB_DIR_IN;
		msg->bmRequest = USB_REQ_GET_DESCRIPTOR;
		msg = g_control_request;
		msg->wValue = USB_DT_CONFIG;
		msg->wIndex = 0;
		msg->wLength = swap_u16(total_length);
		msg->oh1.data = cfg_reply;
		msg->oh1.endpoint = 0;
		msg->oh1.device_index = device_index;
		rc = send_usb_ctrl_msg_sync(module, device_index, msg);
		if (rc == 0)
		{
			parse_descriptors(module, device_index, cfg_reply, total_length);
		}
		OSFreeMemory(s_heap_16k, cfg_reply);
	}
	u8 conf_value = conf_desc->bConfigurationValue;
	memset(g_control_request, 0, sizeof(*g_control_request));
	msg = g_control_request;
	msg->bmRequestType = USB_DIR_OUT;
	msg->bmRequest = USB_REQ_SET_CONFIGURATION;
	msg->wValue = swap_u16(conf_value);
	msg->wIndex = 0;
	msg->wLength = 0;
	msg->oh1.data = NULL;
	msg->oh1.endpoint = 0;
	msg->oh1.device_index = 0;
	send_usb_ctrl_msg_sync(module, device_index, msg);
	u8 num_interfaces = g_devices[device_index].num_interfaces;
	for (int i_interface = 0; i_interface < num_interfaces; i_interface++)
	{
		device_interface *iface = &g_devices[device_index].interfaces[i_interface];
		u8 alternate_setting = iface->bAlternateSetting;
		if (alternate_setting == 0)
			continue;

		u8 iface_num = iface->bInterfaceNumber;
		memset(g_control_request, 0, sizeof(*g_control_request));
		msg = g_control_request;
		msg->bmRequestType = USB_RECIP_INTERFACE;
		msg->bmRequest = USB_REQ_SET_INTERFACE;
		msg->wIndex = swap_u16(iface_num);
		msg->wValue = swap_u16(alternate_setting);
		msg->wLength = 0;
		msg->oh1.data = NULL;
		msg->oh1.endpoint = 0;
		msg->oh1.device_index = 0;
		rc = send_usb_ctrl_msg_sync(module, device_index, msg);
		if (rc < 0)
			return rc;
	}
	return 0;
}

static int query_devices(oh1_module_ctrl *module)
{
	usb_device_desc *dev_desc;
	usb_configuration_desc *conf_desc;
	s8 device_index;
	int rc2;
	u32 rhdesc_b;
	char *device_name;
	u32 n_ports;
	volatile ohci_regs *regs;
	u8 i_port;
	u8 *downstream_ports_ptr;
	u32 rhdesc_a;
	u32 rhdesc_a_old;
	int rc;

	rhdesc_a_old = module->hw_regs->rhdesca;
	rhdesc_a = rhdesc_a_old & ~RH_A_RESERVED;
	rc = 0;
	g_control_request = OSAlignedAllocateMemory(s_heap_16k, sizeof(usb_ctrl_message), 32);
	if (g_control_request == NULL)
		return IPC_EMAX;

	n_ports = module->number_downstream_ports = rhdesc_a & RH_A_NDP;
	downstream_ports_ptr = &module->number_downstream_ports;
	g_devices[0].device_type = DEV_TYPE_HUB;
	g_devices[0].interfaces[0].bInterfaceNumber = *downstream_ports_ptr;
	regs = module->hw_regs;
	if (!(rhdesc_a & RH_A_NPS))
	{
		/* Ports are power switched */
		rhdesc_b = regs->rhdescb & RH_B_DR;
		/* Mark all our ports as removable */
		for (u8 i = 1; i < n_ports; i++)
		{
			rhdesc_b |= 1 << (i + 16);
		}

		rhdesc_a |= RH_A_PSM;
		if (!(rhdesc_a & RH_A_NOCP))
		{
			/* No current protection */
			rhdesc_a |= (RH_A_OCPM | RH_A_PSM);
		}
		regs->rhdesca = rhdesc_a;
		regs->rhdescb = rhdesc_b;
	}
	regs->rhdesca = (rhdesc_a_old & ~(RH_A_RESERVED | RH_A_NOCP | RH_A_OCPM)) | RH_A_NPS;
	regs->rhstat = (regs->rhstat & ~RH_HS_RESERVED) | RH_HS_OCIC | RH_HS_LPSC;
	for (u8 i = 0; i < n_ports; i++)
	{
		/* Power up all ports */
		regs->rhportXstat[i] = RH_PS_PPS;
	}
	/* power on to power good time */
	u32 potpgt = (regs->rhdesca >> RH_A_POTPGT_SHIFT) & RH_A_POTPGT_MASK;
	usleept(module, potpgt * 2000);
	n_ports = module->number_downstream_ports;
	for (i_port = 0; i_port < n_ports; i_port++)
	{
		rc2 = reset_port(module, i_port);
		if (rc2 != 0)
			continue;

		// Last parameter is a boolean: "LowSpeedDeviceAttached"
		device_index = device_get_available(
		    module, i_port, 0, (module->hw_regs->rhportXstat[i_port] & RH_PS_LSDA) != 0);
		if (device_index == 0)
		{
			printk("No device slots available!\n");
			break;
		}

		size_t dev_desc_size = PADDED4_SIZEOF(*dev_desc);
		dev_desc = OSAlignedAllocateMemory(s_heap_16k, dev_desc_size, 32);
		memset(dev_desc, 0, dev_desc_size);

		size_t conf_desc_size = PADDED4_SIZEOF(*conf_desc);
		conf_desc = OSAlignedAllocateMemory(s_heap_16k, conf_desc_size, 32);
		memset(conf_desc, 0, conf_desc_size);

		rc = device_configure(module, device_index, dev_desc, conf_desc);
		if (rc == 0)
		{
			// 5 is OH0/OHCI0, 6 is OH1
			device_name = (module->device_event == 5) ? "OH0:" : "OH1:";
			printk("%s configured USB device at port %u, vid: 0x%04x "
			       "pid: 0x%04x\n",
			       device_name, i_port, swap_u16(dev_desc->idVendor),
			       swap_u16(dev_desc->idProduct));
		}
		OSFreeMemory(s_heap_16k, dev_desc);
		OSFreeMemory(s_heap_16k, conf_desc);
	}

	for (i_port = 0; i_port < n_ports; i_port++)
	{
		regs->rhportXstat[i_port] = RH_PS_CSC | RH_PS_PESC | RH_PS_PSSC | RH_PS_OCIC;
	}
	module->state |= OH1_STATE_DEVICE_QUERIED;
	OSFreeMemory(s_heap_16k, g_control_request);
	return rc;
}

static void endp_disable(oh1_module_ctrl *module, ohci_endpoint_descriptor *endp)
{
	u32 dw0 = swap_u32(endp->dw0);
	endp->dw0 = swap_u32(dw0 | ED_SKIP);
	OSDCFlushRange(endp, sizeof(*endp));
	OSAhbFlushTo(module->ahb_dev);
	usleept(module, 1000);
}

static void endp_clear_halted_flag(oh1_module_ctrl *module, ohci_endpoint_descriptor *endp)
{
	void *td = MASK_PTR(swap_ptr(endp->td_head), ~(ED_H | ED_C));
	endp->td_head = swap_ptr(td);
	OSDCFlushRange(endp, sizeof(*endp));
	OSAhbFlushTo(module->ahb_dev);
	printk("Cleared halt for ed: %p (headP = %p)\n", endp, td);
}

static int handle_ioctlv_ctrlmsg(oh1_module_ctrl *module, IpcMessage *ipc_message)
{
	u32 num_in;
	u32 num_io;

	const IpcRequest *request = &ipc_message->Request;
	const IoctlvMessage *ioctlv = &request->Data.Ioctlv;
	num_in = ioctlv->InputArgc;
	num_io = ioctlv->IoArgc;
	if (num_in != 6 || num_io != 1)
	{
		printk("readcount[%u], writecount[%u] bad\n", num_in, num_io);
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

	usb_ctrl_message *ctrl_msg =
	    OSAlignedAllocateMemory(s_heap_16k, sizeof(*ctrl_msg), 32);
	if (!ctrl_msg)
	{
		printk("failed to allocate ohcctrlreq\n");
		return IPC_EMAX;
	}

	ctrl_msg->bmRequestType = *(u8 *)vector[0].Data;
	ctrl_msg->bmRequest = *(u8 *)vector[1].Data;
	ctrl_msg->wValue = *(u16 *)vector[2].Data;
	ctrl_msg->wIndex = *(u16 *)vector[3].Data;
	ctrl_msg->wLength = *(u16 *)vector[4].Data;
	ctrl_msg->oh1.data = vector[6].Data;
	ctrl_msg->oh1.endpoint = *(u8 *)vector[5].Data;
	ctrl_msg->oh1.device_index = (s8)request->FileDescriptor;
	return send_usb_ctrl_msg(module, ctrl_msg, ipc_message, -1,
	                         *(s8 *)((int)&request->FileDescriptor + 3));
}

static int handle_intrblk_request(oh1_module_ctrl *module, IpcMessage *ipc_message)
{
	s8 device_index;
	const IoctlvMessageData *vector;
	char *data;
	s8 i_endp;
	u32 num_in;
	u32 num_io;
	u32 command;
	ohci_endpoint_descriptor *endp_desc;
	ohci_transfer_descriptor *new_tail_swapped;
	ohci_transfer_descriptor_wii *xfer_desc;
	int ret = 0;
	u8 bEndpoint;

	const IpcRequest *request = &ipc_message->Request;
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
	device_index = *(s8 *)((int)&request->FileDescriptor + 3);
	ohci_disable_interrupts(module);
	OSAhbFlushFrom(module->ahb_dev_flush);
	OSAhbFlushTo(AHB_STARLET);
	command = ioctlv->Ioctl;
	io_request_packet *irp = OSAllocateMemory(s_heap_16k, sizeof(*irp));
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
	i_endp = find_index_of_endpoint(device_index, bEndpoint);
	if (i_endp < 1)
	{
		ret = IPC_EINVAL;
		goto error_free_irp;
	}

	endp_desc = g_devices[device_index].endpoints[i_endp].descriptor;
	irp->transferred = 0;
	irp->counter = 0;
	irp->err_count = 0;
	irp->queue = -1;
	irp->unused = 0;
	irp->endpoint_desc = endp_desc;
	irp->resource_req = ipc_message;
	irp->msg_data = data;
	irp->size = length;
	irp->ctrl_msg = NULL;
	ohci_transfer_descriptor_wii *new_tail = xfer_desc_alloc();
	if (!new_tail)
	{
		ret = IPC_EMAX;
		goto error_free_irp;
	}

	xfer_desc = swap_ptr(endp_desc->td_tail);
	if (xfer_desc == NULL)
	{
		printk("td dummy == NULL\n");
	}
	else if (xfer_desc->std.current_buffer != NULL)
	{
		printk("warning: dummy TD is not an empty TD: td = %p cbp = %p "
		       "typ = %u\n",
		       xfer_desc, xfer_desc->std.current_buffer, command);
	}

	u32 direction = (bEndpoint & USB_DIR_IN) ? TD_DP_IN : TD_DP_OUT;
	fill_transfer(xfer_desc, irp, data, length, direction | TD_SET(CC, 0xf) | TD_R, 0);
	irp->xfer_desc = xfer_desc;
	new_tail_swapped = swap_ptr(new_tail);
	xfer_desc->std.td_next = new_tail_swapped;
	endp_desc->td_tail = new_tail_swapped;
	OSAhbFlushFrom(AHB_1);
	endp_flush(module, endp_desc);
	if (command == USBV0_IOCTL_INTRMSG)
	{
		u32 ctrl_reg = module->hw_regs->ctrl;
		if (!(ctrl_reg & OHCI_CTRL_PLE))
		{
			module->hw_regs->ctrl = ctrl_reg | OHCI_CTRL_PLE;
		}
	}
	else if (command == USBV0_IOCTL_BLKMSG)
	{
		set_ctrl_stat_regs(module, OHCI_CTRL_BLE, OHCI_CS_BLF);
	}
	irp = NULL; /* We don't want to free it */

error_free_irp:
	if (irp != NULL)
	{
		OSFreeMemory(s_heap_16k, irp);
	}

error_reenable_interrupts:
	ohci_enable_interrupts(module);
	return ret;
}

static void done_irp_and_free(io_request_packet *irp)
{
	if (!irp)
		return;

	if (irp->queue <= 0)
	{
		if (irp->size != 0)
		{
			OSDCInvalidateRange(irp->msg_data, irp->size);
		}
		OSResourceReply(irp->resource_req, irp->transferred);
	}
	else
	{
		OSSendMessage(irp->queue, (void *)(u32)irp->transferred, 0);
	}

	if (irp->ctrl_msg)
	{
		OSFreeMemory(s_heap_16k, irp->ctrl_msg);
	}

	/* Do we really need to zero this? */
	memset(irp, 0, sizeof(*irp));
	OSFreeMemory(s_heap_16k, irp);
}

static void endp_close(oh1_module_ctrl *module, ohci_endpoint_descriptor *endp_desc)
{
	ohci_transfer_descriptor_wii *head;
	ohci_transfer_descriptor_wii *tail;

	OSAhbFlushFrom(module->ahb_dev_flush);
	OSAhbFlushTo(module->ahb_dev);
	u32 head_field = (u32)swap_ptr(endp_desc->td_head);
	head = (void *)(head_field & 0xfffffff0);
	u32 head_flags = head_field & (ED_C | ED_H);
	tail = swap_ptr(endp_desc->td_tail);
	if (head != tail)
	{
		while (xfer_desc_is_on_heap(head) && (head != tail))
		{
			io_request_packet *irp = head->irp;
			irp->counter--;
			endp_desc->td_head = head->std.td_next;
			OSDCFlushRange(endp_desc, sizeof(*endp_desc));
			oh1_heap_free(head);
			if (irp->counter == 0)
			{
				done_irp_and_free(irp);
			}
			head = MASK_PTR(swap_ptr(endp_desc->td_head), 0xfffffff0);
		}
		endp_desc->td_head = PTR_WITH_BITS(endp_desc->td_head, swap_u32(head_flags));
		OSDCFlushRange(endp_desc, sizeof(*endp_desc));
		OSAhbFlushTo(module->ahb_dev);
	}
}

static int thread_worker(oh1_module_ctrl *module)
{
	void *queue_buffer[4] ALIGNED(16);
	u8 device;
	int queue_id;
	int rc;
	u32 intstat;
	volatile ohci_regs *regs;
	ohci_endpoint_descriptor *endp_desc;
	ohci_transfer_descriptor_wii *td_last;
	u16 length;

	device = module->device_event;
	/* TODO: the original code was declaring the queue to be 0x10 in size. This
	 * seems a mistake, but it needs to be double-checked. */
	queue_id = OSCreateMessageQueue(queue_buffer, sizeof(queue_buffer) / sizeof(u32));
	rc = OSRegisterEventHandler(device, queue_id, NULL);
	if (rc != 0)
		return -1;

	regs = module->hw_regs;
	regs->intstat = 0xffffffff;
	OSClearAndEnableEvent(device);
	while (true)
	{
		do
		{
			rc = OSReceiveMessage(queue_id, NULL, 0);
		}
		while (rc != 0);

		intstat = regs->intstat;
		// Check WritebackDoneHead flag
		if (intstat & OHCI_INTR_WDH)
		{
			OSAhbFlushFrom(module->ahb_dev_flush);
			OSAhbFlushTo(AHB_STARLET);
			ohci_transfer_descriptor *td_swapped =
			    MASK_PTR(module->hcca->done_head, swap_u32(0xfffffff0));
			/* Reverse the list */
			ohci_transfer_descriptor *prev_swapped = NULL;
			while (td_swapped != 0)
			{
				ohci_transfer_descriptor *td = swap_ptr(td_swapped);
				ohci_transfer_descriptor *next_swapped = td->td_next;
				td->td_next = prev_swapped;
				prev_swapped = td_swapped;
				td_swapped = next_swapped;
			}

			td_last = swap_ptr(prev_swapped);
			while (td_last)
			{
				ohci_transfer_descriptor_wii *td_next;

				td_next = swap_ptr(td_last->std.td_next);
				io_request_packet *irp = td_last->irp;
				irp->counter--;
				u32 condition_code = TD_GET(CC, td_last->std.dw0);
				if (condition_code == 0)
				{
					length = td_last->length;
					if (length != 0)
					{
						char *current_buffer = swap_ptr(td_last->std.current_buffer);
						if (current_buffer)
						{
							/* The transfer is not complete: current_buffer
							 * points to the byte after the last successfully
							 * transferred one */
							char *buffer_last = swap_ptr(td_last->std.buffer_last);
							length += (u16)((current_buffer - buffer_last) - 1);
						}
						irp->transferred += length;
					}

					if (irp->counter == 0)
					{
						OSAhbFlushFrom(module->ahb_dev_flush);
						done_irp_and_free(irp);
					}
					oh1_heap_free(td_last);
				}
				else
				{
					irp->err_count++;
					printk("OHCI processing TD error: 0x%x\n", condition_code);
					printk("OHCI processing TD error for td  %p with irp %p\n", td_last, irp);
					endp_desc = irp->endpoint_desc;
					u32 dw0 = swap_u32(endp_desc->dw0);
					ohci_transfer_descriptor *head = swap_ptr(endp_desc->td_head);
					printk("TD error for ed  %p; ed flag = 0x%x headP = %p\n",
					       endp_desc, dw0, head);
					printk("TD error for ed  %p\n", endp_desc);
					OSAhbFlushFrom(module->ahb_dev_flush);
					OSAhbFlushTo(AHB_STARLET);
					endp_disable(module, endp_desc);
					endp_close(module, endp_desc);
					if ((u32)head & ED_H)
					{
						printk("OHCI ED %p halted: headP = %p\n", endp_desc, head);
						endp_clear_halted_flag(module, endp_desc);
					}
				}
				td_last = td_next;
			}
			/* Setting the bit clears it */
			regs->intstat = OHCI_INTR_WDH;
		}
		/* Check RootHubStatusChange flag */
		if (intstat & OHCI_INTR_RHSC)
		{
			if ((module->state & OH1_STATE_DEVICE_QUERIED) != 0 &&
			    (module->state & OH1_STATE_PROCESSING_EVENTS) != 0)
			{
				OSSendMessage(module->queue_id, s_status_change_message, 0);
			}
			regs->intstat = OHCI_INTR_RHSC;
		}
		/* Clear all other bits */
		regs->intstat = intstat & ~(OHCI_INTR_WDH | OHCI_INTR_RHSC);
		OSClearAndEnableEvent(device);
	}
}

static int read_vendor_and_product(s8 device_index, u16 *vendor, u16 *product)
{
	int rc = IPC_SUCCESS;

	if (device_index < 5 && g_devices[device_index].device_type != DEV_TYPE_NONE)
	{
		if (vendor)
			*vendor = g_devices[device_index].vendor_id;
		if (product)
			*product = g_devices[device_index].product_id;
	}
	else
	{
		rc = IPC_EINVAL;
	}
	return rc;
}

static u8 device_get_port_index(s8 device_index)
{
	return g_devices[device_index].port_index;
}

static int device_set_ipc_message(s8 device_index, IpcMessage *msg)
{
	if (g_devices[device_index].ipc_message)
		return IPC_EEXIST;

	g_devices[device_index].ipc_message = msg;
	return IPC_SUCCESS;
}

static void close_device_descriptors(oh1_module_ctrl *module, s8 device_index, u8 i_endp)
{
	device_endpoint *endp;

	endp = &g_devices[device_index].endpoints[i_endp];
	if (i_endp == 0 || endp->bEndpointAddress != 0)
	{
		ohci_endpoint_descriptor *endp_desc;
		endp_desc = g_devices[device_index].endpoints[i_endp].descriptor;
		OSAhbFlushFrom(module->ahb_dev_flush);
		OSAhbFlushTo(AHB_STARLET);
		endp_disable(module, endp_desc);
		endp_close(module, endp_desc);
	}
}

static void close_device(oh1_module_ctrl *module, s8 device_index)
{
	for (u8 i_desc = 0; i_desc < 16; i_desc++)
	{
		close_device_descriptors(module, device_index, i_desc);
	}
}

static void add_device_to_dev_getlist_reply(oh1_devlist_entry *dest_ptr, u8 i_entry, s8 device_index)
{
	/* It would be more logical if the first 4 bytes of the destination buffer
	 * were filled with the device ID, but they are left uninitialized. This is
	 * consistent with how libogc operates in USB_GetDeviceList(): only the
	 * vendor and product ID are being read. */
	dest_ptr[i_entry].vendor_id = g_devices[device_index].vendor_id;
	dest_ptr[i_entry].product_id = g_devices[device_index].product_id;
}

static int handle_get_devlist(oh1_devlist_entry *dest_ptr, u8 num_descr,
                              u8 iface_class, u8 *count)
{
	u32 num_interfaces;
	u8 i_iface;
	s8 i_device;
	u8 num_added;

	num_added = 0;
	for (i_device = 1; i_device < 4; i_device++)
	{
		if (g_devices[i_device].device_type == DEV_TYPE_NONE)
			continue;

		if (iface_class == 0 || g_devices[i_device].device_class == iface_class)
		{
			add_device_to_dev_getlist_reply(dest_ptr, num_added++, i_device);
		}
		else
		{
			num_interfaces = g_devices[i_device].num_interfaces;
			for (i_iface = 0; i_iface < num_interfaces; i_iface++)
			{
				if (g_devices[i_device].interfaces[i_iface].bInterfaceClass == iface_class)
				{
					add_device_to_dev_getlist_reply(dest_ptr, num_added++, i_device);
					break;
				}
			}
		}

		if (num_added == num_descr)
			break;
	}
	*count = num_added;
	return 0;
}

static int hexstring_to_int(const char *hexstring)
{
	int result, val;
	char ch;

	result = 0;
	if (hexstring[0] == '0' && (hexstring[1] == 'x' || hexstring[1] == 'X'))
	{
		hexstring += 2;
	}

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

static int oh1_set_ipc_message_on_device(oh1_module_ctrl *, IpcMessage *msg)
{
	return device_set_ipc_message(*(s8 *)((int)&msg->Request.FileDescriptor + 3), msg);
}

static int create_usb_device_queue(oh1_module_ctrl *module, int device_event)
{
	int rc;

	rc = OSCreateMessageQueue(s_device_queue_buffer,
	                          sizeof(s_device_queue_buffer) / sizeof(u32));
	if (rc < 0)
		return rc;

	s_device_queue_id = rc;
	memset(s_usb_device_name, 0, sizeof(s_usb_device_name));

	const char *path = device_event == 5 ? /* IRQ_OHCI0 */
	                       "/dev/usb/oh0" :
	                       "/dev/usb/oh1";
	strncpy(s_usb_device_name, path, sizeof(s_usb_device_name) - 1);
	module->queue_id = s_device_queue_id;
	return OSRegisterResourceManager(s_usb_device_name, s_device_queue_id);
}

static int event_close(oh1_module_ctrl *module, const IpcRequest *request)
{
	int result = IPC_SUCCESS;

	u16 usb_fd;
	usb_fd = *(u16 *)((int)&request->FileDescriptor + 2);
	if (usb_fd != s_root_hub_fd)
	{
		s8 device_index = (s8)usb_fd;
		u16 unused_pid, unused_vid;
		result = read_vendor_and_product(device_index, &unused_vid, &unused_pid);
		if (result == 0)
		{
			close_device(module, device_index);
		}
	}
	return result;
}

static int event_open(oh1_module_ctrl *, const IpcRequest *request)
{
	int result;
	char *device;
	size_t len;
	char subdevice_chr;
	s16 device_index;
	char *next_token;

	result = strncmp(request->Data.Open.Filepath, s_usb_device_name,
	                 sizeof(s_usb_device_name));
	if (result == 0)
		return s_root_hub_fd;

	char product[16] = { 0 };
	char vendor[16] = { 0 };
	device = request->Data.Open.Filepath;
	// 13 is strlen("/dev/usb/oh1/")
	next_token = device + 13;

	/* Parse vendor ID */
	len = 0;
	subdevice_chr = *next_token;
	while (subdevice_chr != '\0' && subdevice_chr != '/')
		subdevice_chr = next_token[++len];
	memcpy(vendor, next_token, len);

	/* Parse product ID */
	next_token += len + 1;
	len = 0;
	subdevice_chr = next_token[len];
	while (subdevice_chr != '\0' && subdevice_chr != '/')
	{
		subdevice_chr = next_token[++len];
	}
	memcpy(product, next_token, len);

	u16 vid = (u16)hexstring_to_int(vendor);
	u16 pid = (u16)hexstring_to_int(product);
	device_index = device_index_by_vid_pid(vid, pid);
	return device_index >= 0 ? device_index : IPC_EINVAL;
}

static int event_ioctl(oh1_module_ctrl *module, IpcMessage *message, bool *is_async)
{
	const IpcRequest *request = &message->Request;
	const IoctlMessage *ioctl = &request->Data.Ioctl;
	int result = IPC_EINVAL;

	*is_async = false;

	u16 usb_fd = *(u16 *)((int)&request->FileDescriptor + 2);
	if (usb_fd == s_root_hub_fd)
	{
		if (ioctl->Ioctl == USBV0_IOCTL_GETHUBSTATUS)
		{
			u32 *status;
			status = ioctl->IoBuffer;
			if (status == NULL || ioctl->IoLength != 4)
				return IPC_EINVAL;

			*status = module->hw_regs->rhdesca & 0xff001fff;
			return IPC_SUCCESS;
		}
		return IPC_EINVAL;
	}

	s8 device_index = *(s8 *)((int)&request->FileDescriptor + 3);
	result = read_vendor_and_product(device_index, NULL, NULL);
	if (result != 0)
		return result;

	if (ioctl->Ioctl == USBV0_IOCTL_SUSPENDDEV || ioctl->Ioctl == USBV0_IOCTL_RESUMEDEV)
	{
		if (ioctl->InputBuffer || ioctl->InputLength != 0 || ioctl->IoBuffer ||
		    ioctl->IoLength != 0)
			return IPC_EINVAL;

		u8 port = device_get_port_index(device_index);
		if (ioctl->Ioctl == USBV0_IOCTL_SUSPENDDEV)
			result = suspend_device(module, port);
		else
			result = resume_device(module, port);
	}
	else if (ioctl->Ioctl == USBV0_IOCTL_DEVREMOVALHOOK)
	{
		result = oh1_set_ipc_message_on_device(module, message);
		*is_async = true;
	}
	return result;
}

static int event_ioctlv(oh1_module_ctrl *module, IpcMessage *message, bool *is_async)
{
	const IpcRequest *request = &message->Request;
	const IoctlvMessage *ioctlv = &request->Data.Ioctlv;
	const IoctlvMessageData *vector;
	int result = IPC_EINVAL;

	u16 usb_fd = *(u16 *)((int)&request->FileDescriptor + 2);
	if (usb_fd != s_root_hub_fd)
	{
		if (ioctlv->Ioctl == USBV0_IOCTL_CTRLMSG)
		{
			result = handle_ioctlv_ctrlmsg(module, message);
		}
		else if (ioctlv->Ioctl == USBV0_IOCTL_INTRMSG || ioctlv->Ioctl == USBV0_IOCTL_BLKMSG)
		{
			result = handle_intrblk_request(module, message);
		}
		else
			return IPC_EINVAL;
		*is_async = true;
		return result;
	}

	/* All other ioctls are synchronous */
	*is_async = false;

	if (ioctlv->Ioctl == USBV0_IOCTL_GETPORTSTATUS)
	{
		vector = ioctlv->Data;
		if (ioctlv->InputArgc != 1 || ioctlv->IoArgc != 1 || !vector[0].Data ||
		    !vector[1].Data)
			return IPC_EINVAL;

		u32 query_port = *vector[0].Data;
		u32 *outptr = vector[1].Data;
		if (vector[0].Length == 1 && vector[1].Length == 4 &&
		    query_port < module->number_downstream_ports)
		{
			*outptr = module->hw_regs->rhportXstat[query_port];
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
		if (port < module->number_downstream_ports)
		{
			module->hw_regs->rhportXstat[port] = *(u32 *)vector[1].Data;
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
		if (vector[3].Length != num_elements * sizeof(oh1_devlist_entry) ||
		    (num_elements != 0 && !vector[3].Data))
			return IPC_EINVAL;

		u8 *count = (u8 *)vector[2].Data;
		u8 iface_class = *(u8 *)vector[1].Data;
		oh1_devlist_entry *dest_ptr = (oh1_devlist_entry *)vector[3].Data;
		result = handle_get_devlist(dest_ptr, num_elements, iface_class, count);
		/* TODO: shouldn't this pass "num_elements * sizeof(oh1_devlist_entry)"? */
		OSDCFlushRange(vector[3].Data, num_elements);
		OSDCFlushRange(count, sizeof(*count));
	}
	else
		return IPC_EINVAL;

	return result;
}

static int process_events(oh1_module_ctrl *module)
{
	IpcMessage *message;
	int result;
	const IpcRequest *request;

	module->state |= OH1_STATE_PROCESSING_EVENTS;
	do
	{
		result = OSReceiveMessage(s_device_queue_id, &message, 0);
		if (result != 0)
			return result;

		request = &message->Request;

		if (request == s_status_change_message)
		{
			handle_status_changes(module, 0);
			continue;
		}

		bool is_async = false;
		switch (request->Command)
		{
			case IOS_CLOSE:
				result = event_close(module, request);
				break;
			case IOS_OPEN:
				result = event_open(module, request);
				break;
			case IOS_IOCTL:
				result = event_ioctl(module, message, &is_async);
				break;
			case IOS_IOCTLV:
				result = event_ioctlv(module, message, &is_async);
				break;
			default:
				result = IPC_EINVAL;
		}

		if (result < 0 || !is_async)
		{
			OSResourceReply(message, result);
		}
	}
	while (true);
}

int main(void)
{
	int rc;
	volatile ohci_regs *regs;
	u32 frame_interval;
	oh1_module_ctrl *module = NULL;

	OSSetThreadPriority(0, 0x60);
	printk("%s\n", "$IOSVersion: OH1: " __DATE__ " " __TIME__ " 64M $");
	rc = create_heap();
	if (rc < 0)
		goto error;

	module = OSAllocateMemory(s_heap_16k, sizeof(*module));
	rc = create_usb_device_queue(module, 6); /* IRQ_OHCI1 */
	if (rc < 0)
		goto error;

	module->hw_regs = OHCI_REG_BASE;
	module->device_event = 6; /* IRQ_OHCI1 */
	module->ahb_dev_flush = AHB_UNKN11;
	module->ahb_dev = AHB_UNKN11;
	module->hcca = g_HCCA;
	memset(g_HCCA, 0, sizeof(*g_HCCA));
	rc = OSCreateMessageQueue(&module->timer_queue_buffer, 1);
	if (rc < 0)
		goto error;

	module->timer_queue = rc;
	/* Post a status change message request, in order to update the status of
	 * the hub ports. */
	rc = OSCreateTimer(0, 0, module->timer_queue, s_status_change_message);
	if (rc < 0)
		goto error;

	module->timer = rc;
	rc = init_transfer_descriptors(module);
	if (rc < 0)
		goto error_destroy_timer_queue;

	ohci_endpoint_descriptor *endpoints = module->endpoint_descriptors;
	ohci_hcca *hcca = module->hcca;
	for (int i_endp = 0; i_endp < 32; i_endp++)
	{
		ohci_endpoint_descriptor *endp = &endpoints[i_endp];
		hcca->interrupt_table[i_endp] = swap_ptr(endp);
	}
	OSAhbFlushFrom(AHB_STARLET);

	regs = module->hw_regs;
	regs->intdis = OHCI_INTR_MIE;
	u8 rev = regs->rev & OHCI_REV_MASK;
	if (rev != 0x10)
	{
		rc = IPC_EINVAL;
		goto error_destroy_timer_queue;
	}

	if ((regs->ctrl & OHCI_CTRL_IR) != 0)
	{
		/* OwnershipChangerequest */
		regs->inten = OHCI_INTR_OC;
		regs->cmdstat = OHCI_CS_OCR;
		usleept(module, 50000);
		if ((regs->ctrl & OHCI_CTRL_IR) != 0)
		{
			rc = IPC_NOTREADY;
			goto error_destroy_timer_queue;
		}
		usleept(module, 20000);
		regs->ctrl &= OHCI_CTRL_RWC;
	}

	/* We expect the host controller to be in reset state */
	if (OHCI_GET(CTRL_HCFS, regs->ctrl) != OHCI_CTRL_HCFS_RESET)
	{
		rc = IPC_NOTREADY;
		goto error_destroy_timer_queue;
	}

	/* Nominal value of the frame interval, according to the specs */
	module->frame_interval = 11999;
	/* Reset the controller; the loop below waits until the reset is
	 * complete */
	regs->cmdstat |= OHCI_CS_HCR;
	usleept(module, 10000);
	for (int tries = 10000; tries > 0; tries--)
	{
		if ((regs->cmdstat & OHCI_CS_HCR) == 0)
			break;
		usleept(module, 2);
	}
	if ((regs->cmdstat & OHCI_CS_HCR) != 0)
	{
		rc = IPC_NOTREADY;
		goto error_destroy_timer_queue;
	}

	frame_interval = module->frame_interval;
	u32 largest_data_packet = (frame_interval * 6 - 1260) / 7;
	regs->fmint = OHCI_SET(FI_FI, frame_interval) | OHCI_SET(FI_FSMPS, largest_data_packet);
	/* Spec says that the periodic start should be a 10% off the
	 * HcFmInterval. */
	regs->perst = (frame_interval * 9) / 10;
	regs->blkhd = NULL;
	regs->ctrlhd = NULL;
	regs->hcca = module->hcca;
	u32 desired_interrupts = OHCI_INTR_MIE | OHCI_INTR_OC | OHCI_INTR_RHSC |
	                         OHCI_INTR_FNO | OHCI_INTR_UE | OHCI_INTR_RD |
	                         OHCI_INTR_WDH | OHCI_INTR_SO;
	regs->intstat = desired_interrupts;
	regs->inten = desired_interrupts;
	regs->ctrl = OHCI_CTRL_RWC | OHCI_SET(CTRL_HCFS, OHCI_CTRL_HCFS_OPERATIONAL) |
	             OHCI_SET(CTRL_CBSR, 3);

	/* No idea what this does */
	s_ehci_regs->ChickenBits |= EHCI_CHICKENBITS_INIT;

	s32 priority = OSGetThreadPriority(0);
	rc = OSCreateThread((ThreadFunc)thread_worker, module, s_worker_thread_stack,
	                    sizeof(s_worker_thread_stack), priority, 1);
	if (rc < 0)
		goto error_destroy_timer_queue;

	s32 thread_id = rc;
	OSStartThread(thread_id);
	priority = OSGetThreadPriority(0);
	OSSetThreadPriority(0, priority - 1);
	rc = query_devices(module);
	if (rc < 0)
		goto error_destroy_timer_queue;

	process_events(module);

error_destroy_timer_queue:
	if (module->timer_queue > 0)
		OSDestroyMessageQueue(module->timer_queue);

error:
	printk("ohci_core: OHCI initialization failed: %d\n", rc);
	printk("ohci_core exits...\n");
	OSFreeMemory(s_heap_16k, module);
	OSStopThread(0, 0);
	return rc;
}
