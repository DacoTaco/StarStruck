/*
	mini - a Free Software replacement for the Nintendo/BroadOn IOS.
	inter-processor communications

Copyright (C) 2008, 2009	Hector Martin "marcan" <marcan@marcansoft.com>
Copyright (C) 2008, 2009	Haxx Enterprises <bushing@gmail.com>
Copyright (C) 2008, 2009	Sven Peter <svenpeter@gmail.com>
Copyright (C) 2009			Andre Heider "dhewg" <dhewg@wiibrew.org>
Copyright (C) 2009		John Kelley <wiidev@kelley.ca>

# This code is licensed to you under the terms of the GNU GPL, version 2;
# see file COPYING or http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt
*/

#include <stdarg.h>
#include "string.h"
#include "types.h"
#include "irq.h"
#include "memory.h"
#include "utils.h"
#include "hollywood.h"
#include "gecko.h"
#include "ipc.h"
#include "nand.h"
#include "sdhc.h"
#include "sdmmc.h"
#include "crypto.h"
#include "boot2.h"
#include "powerpc.h"
#include "panic.h"
#include "ios_module.h"
#include "es.h"
#include "string.h"

#define MINI_VERSION_MAJOR 1
#define MINI_VERSION_MINOR 4

//Old stuff
static volatile ipc_request in_queue[IPC_IN_SIZE] ALIGNED(32) MEM2_BSS;
static volatile ipc_request out_queue[IPC_OUT_SIZE] ALIGNED(32) MEM2_BSS;
static volatile ipc_request slow_queue[IPC_SLOW_SIZE];

extern char __mem2_area_start[];
extern const char git_version[];

// These defines are for the ARMCTRL regs
// See http://wiibrew.org/wiki/Hardware/IPC

#define		IPC_CTRL_Y1	0x01
#define		IPC_CTRL_X2	0x02
#define		IPC_CTRL_X1	0x04
#define		IPC_CTRL_Y2	0x08

#define		IPC_CTRL_IX1	0x10
#define		IPC_CTRL_IX2	0x20

// Our definitions for this IPC interface
#define		IPC_CTRL_OUT	IPC_CTRL_Y1
#define		IPC_CTRL_IN		IPC_CTRL_X1
#define		IPC_CTRL_IRQ_IN	IPC_CTRL_IX1

static u16 slow_queue_head;
static vu16 slow_queue_tail;

static u16 in_head;
static u16 out_tail;

//New Stuff
// These defines are for the ARMCTRL regs
// See http://wiibrew.org/wiki/Hardware/IPC
#define IPC_ARM_Y1			0x01
#define IPC_ARM_X2			0x02
#define IPC_ARM_X1			0x04
#define IPC_ARM_Y2			0x08
#define IPC_ARM_IX1			0x10
#define IPC_ARM_IX2			0x20

#define IPC_PPC_X1			0x01
#define IPC_PPC_Y2			0x02
#define IPC_PPC_Y1			0x04
#define IPC_PPC_X2			0x08
#define IPC_PPC_IY1			0x10
#define IPC_PPC_IY2			0x20

// reset both flags (X* for ARM and Y* for PPC)
#define	IPC_CTRL_RESET		0x06

#define IPC_ARM_INCOMING	IPC_ARM_X1
#define IPC_ARM_OUTGOING	IPC_ARM_Y1
#define IPC_ARM_ACK_OUT		IPC_ARM_Y2

#define IPC_PPC_OUTGOING	IPC_PPC_Y1
#define IPC_TRIG_OUTGOING	IPC_PPC_IY1
#define IPC_TRIG_ACK		IPC_PPC_IY2

#define IPC_MAX_FILENAME	0x1300

static volatile u64 boot_titleID = 0;
static volatile ipcreq* input_queue[IPC_IN_SIZE] ALIGNED(0x20) MEM2_BSS;
static volatile ipcreq* output_queue[IPC_OUT_SIZE] ALIGNED(0x20) MEM2_BSS;
static u16 in_cnt = 0;
static u16 out_cnt = 0;

//code
const ipc_infohdr __ipc_info ALIGNED(32) MEM2_RODATA = {
	.magic = "IPC",
	.version = 1,
	.mem2_boundary = __mem2_area_start,
	.ipc_in = in_queue,
	.ipc_in_size = IPC_IN_SIZE,
	.ipc_out = out_queue,
	.ipc_out_size = IPC_OUT_SIZE,
};

static inline void poke_outtail(u16 num)
{
	mask32(HW_IPC_ARMMSG, 0xFFFF, num);
}

static inline void poke_inhead(u16 num)
{
	mask32(HW_IPC_ARMMSG, 0xFFFF0000, num<<16);
}

static inline u16 peek_intail(void)
{
	return read32(HW_IPC_PPCMSG) & 0xFFFF;
}

static inline u16 peek_outhead(void)
{
	return read32(HW_IPC_PPCMSG) >> 16;
}


void ipc_post(u32 code, u32 tag, u32 num_args, ...)
{
	/*int arg = 0;
	va_list ap;
	u32 cookie = irq_kill();

	if(peek_outhead() == ((out_tail + 1)&(IPC_OUT_SIZE-1))) {
		gecko_printf("IPC: out queue full, PPC slow/dead/flooded\n");
		while(peek_outhead() == ((out_tail + 1)&(IPC_OUT_SIZE-1)));
	}
	out_queue[out_tail].code = code;
	out_queue[out_tail].tag = tag;
	if(num_args) {
		va_start(ap, num_args);
		while(num_args--) {
			out_queue[out_tail].args[arg++] = va_arg(ap, u32);
		}
		va_end(ap);
	}
	dc_flush_block_fast((void*)&out_queue[out_tail]);
	out_tail = (out_tail+1)&(IPC_OUT_SIZE-1);
	poke_outtail(out_tail);
	write32(HW_IPC_ARMCTRL, IPC_CTRL_IRQ_IN | IPC_CTRL_OUT);

	irq_restore(cookie);*/
}

void ipc_flush(void)
{
	while(peek_outhead() != out_tail);
}
/*
static u32 process_slow(volatile ipc_request *req)
{
	gecko_printf("IPC: process slow_queue @ %p\n",req);

	//gecko_printf("IPC: req %08x %08x [%08x %08x %08x %08x %08x %08x]\n", req->code, req->tag,
	//	req->args[0], req->args[1], req->args[2], req->args[3], req->args[4], req->args[5]);

	switch(req->device) {
		case IPC_DEV_SYS:
			switch(req->req) {
				case IPC_SYS_PING: //PING can be both slow and fast for testing purposes
					ipc_post(req->code, req->tag, 0);
					break;
				case IPC_SYS_JUMP:
					return req->args[0];
				case IPC_SYS_GETVERS:
					ipc_post(req->code, req->tag, 1, MINI_VERSION_MAJOR << 16 | MINI_VERSION_MINOR);
					break;
				case IPC_SYS_GETGITS:
					strlcpy((char *)req->args[0], git_version, 32);
					dc_flushrange((void *)req->args[0], 32);
					ipc_post(req->code, req->tag, 0);
					break;
				default:
					gecko_printf("IPC: unknown SLOW SYS request %04x\n", req->req);
			}
			break;
		case IPC_DEV_NAND:
			nand_ipc(req);
			break;
		case IPC_DEV_SDHC:
			sdhc_ipc(req);
			break;
		case IPC_DEV_SDMMC:
			sdmmc_ipc(req);
			break;
		case IPC_DEV_KEYS:
			crypto_ipc(req);
			break;
		case IPC_DEV_AES:
			aes_ipc(req);
			break;
		case IPC_DEV_BOOT2:
			return boot2_ipc(req);
			break;
		case IPC_DEV_PPC:
			powerpc_ipc(req);
			break;
		default:
			gecko_printf("IPC: unknown SLOW request %02x-%04x\n", req->device, req->req);
	}

	return 0;
}*/

void ipc_enqueue_slow(u8 device, u16 req, u32 num_args, ...)
{
	/*int arg = 0;
	va_list ap;

	if(slow_queue_head == ((slow_queue_tail + 1)&(IPC_SLOW_SIZE-1))) {
		gecko_printf("IPC: Slowqueue1 overrun\n");
		panic2(0, PANIC_IPCOVF);
	}
	
	gecko_printf("IPC: process in %d @ 0x%08X\n", slow_queue_tail, slow_queue[slow_queue_tail].req);

	slow_queue[slow_queue_tail].flags = IPC_SLOW;
	slow_queue[slow_queue_tail].device = device;
	slow_queue[slow_queue_tail].req = req;
	slow_queue[slow_queue_tail].tag = 0;

	if(num_args) {
		va_start(ap, num_args);
		while(num_args--)
			slow_queue[slow_queue_tail].args[arg++] = va_arg(ap, u32);
		va_end(ap);
	}

	slow_queue_tail = (slow_queue_tail+1)&(IPC_SLOW_SIZE-1);*/
}
/*
static void process_in(void)
{
	volatile ipc_request *req = &in_queue[in_head];

	gecko_printf("IPC: process in %d @ %p\n",in_head,req);

	dc_inval_block_fast((void*)req);

	//gecko_printf("IPC: req %08x %08x [%08x %08x %08x %08x %08x %08x]\n", req->code, req->tag,
	//	req->args[0], req->args[1], req->args[2], req->args[3], req->args[4], req->args[5]);

	if(req->flags & IPC_FAST) {
		switch(req->device) {
			case IPC_DEV_SYS:
				// handle fast SYS requests here
				switch(req->req) {
					case IPC_SYS_PING:
						ipc_post(req->code, req->tag, 0);
						break;
					case IPC_SYS_WRITE32:
						write32(req->args[0], req->args[1]);
						break;
					case IPC_SYS_WRITE16:
						write16(req->args[0], req->args[1]);
						break;
					case IPC_SYS_WRITE8:
						write8(req->args[0], req->args[1]);
						break;
					case IPC_SYS_READ32:
						ipc_post(req->code, req->tag, 1, read32(req->args[0]));
						break;
					case IPC_SYS_READ16:
						ipc_post(req->code, req->tag, 1, read16(req->args[0]));
						break;
					case IPC_SYS_READ8:
						ipc_post(req->code, req->tag, 1, read8(req->args[0]));
						break;
					case IPC_SYS_SET32:
						set32(req->args[0], req->args[1]);
						break;
					case IPC_SYS_SET16:
						set16(req->args[0], req->args[1]);
						break;
					case IPC_SYS_SET8:
						set8(req->args[0], req->args[1]);
						break;
					case IPC_SYS_CLEAR32:
						clear32(req->args[0], req->args[1]);
						break;
					case IPC_SYS_CLEAR16:
						clear16(req->args[0], req->args[1]);
						break;
					case IPC_SYS_CLEAR8:
						clear8(req->args[0], req->args[1]);
						break;
					case IPC_SYS_MASK32:
						mask32(req->args[0], req->args[1], req->args[2]);
						break;
					case IPC_SYS_MASK16:
						mask16(req->args[0], req->args[1], req->args[2]);
						break;
					case IPC_SYS_MASK8:
						mask8(req->args[0], req->args[1], req->args[2]);
						break;
					default:
						gecko_printf("IPC: unknown FAST SYS request %04x\n", req->req);
						break;
				}
				break;
			default:
				gecko_printf("IPC: unknown FAST request %02x-%04x\n", req->device, req->req);
				break;
		}
	} else {
		if(slow_queue_head == ((slow_queue_tail + 1)&(IPC_SLOW_SIZE-1))) {
			gecko_printf("IPC: Slowqueue2 overrun\n");
			panic2(0, PANIC_IPCOVF);
		}

		slow_queue[slow_queue_tail] = *req;
		slow_queue_tail = (slow_queue_tail+1)&(IPC_SLOW_SIZE-1);
	}
}*/
void ipc_reply(ipcreq* req)
{
	//Send Reply
	u32 regs = read32(HW_IPC_ARMCTRL);
	write32(HW_IPC_ARMMSG, (u32)req);
	write32(HW_IPC_PPCCTRL, read32(HW_IPC_PPCCTRL) | IPC_PPC_OUTGOING | IPC_TRIG_OUTGOING | IPC_TRIG_ACK );
	write32(HW_IPC_ARMCTRL, regs | IPC_ARM_OUTGOING );	
}

void enqueue_reply(ipcreq* req)
{
	if(out_cnt >= IPC_OUT_SIZE)
	{
		gecko_printf("IPC: OUTPUT QUEUE OVERLOAD\n");
		return;
	}
	
	output_queue[out_cnt] = req;
	out_cnt++;
}

void enqueue_request(ipcreq* req)
{
	if(in_cnt >= IPC_IN_SIZE)
	{
		gecko_printf("IPC: INPUT QUEUE OVERLOAD\n");
		return;
	}
	
	input_queue[in_cnt] = req;
	in_cnt++;
}

void ipc_process_input(void)
{
	if(in_cnt == 0)
		return;
	
	volatile ipcreq* req = input_queue[in_cnt-1];
	if(req == NULL)
		goto exit_process;
	
	s32 return_value = IOS_EINVAL;	
	u8 reply = 1;
	request_handler req_handler = NULL;
	request_open open_handler = NULL;
	dc_flushrange((u32*)req, sizeof(ipcreq));
	dc_invalidaterange((u32*)req, sizeof(ipcreq));
	
	if(req->cmd == IOS_OPEN)
	{
		if(strncmp(req->open.filepath, es_module.device_name, IPC_MAX_FILENAME) == 0)
			open_handler = es_module.handle_open;
		else
			gecko_printf("IPC: unknown open request 0x%04x-0x%04x - '%s'\n", req->cmd, req->fd, req->open.filepath);
	}
	else
	{	
		switch(req->fd)
		{
			case IPC_DEV_ES:
				req_handler = es_module.handle_request;
				break;
			default:
				gecko_printf("IPC: unknown request 0x%04x-0x%04x\n", req->cmd, req->fd);
				break;
		}
	}
	
	if(req_handler)
		return_value = req_handler((ipcreq*)req, &reply);
	else if(open_handler)
		return_value = open_handler(req->open.filepath, req->open.mode, &reply);
	
	write32((u32)&req->result, return_value);
	dc_flushrange((u32*)&req->result, 0x04);
	dc_invalidaterange((u32*)&req->result, 0x04);
	
	if(reply)
		ipc_reply((ipcreq*)req);
	else
	{
		//only ack
		u32 regs = read32(HW_IPC_PPCCTRL);
		regs &= ~(IPC_TRIG_OUTGOING | IPC_PPC_OUTGOING);
		write32(HW_IPC_PPCCTRL, IPC_TRIG_ACK );
	}

exit_process:
	in_cnt--;
	return;
}

void ipc_irq(void)
{
	int donebell = 0;	
	while(read32(HW_IPC_ARMCTRL) & IPC_ARM_INCOMING) 
	{
		u32 regs = read32(HW_IPC_ARMCTRL);
		regs &= ~(IPC_ARM_INCOMING);
		
		//Send ACK to the PPC
		regs |= IPC_ARM_ACK_OUT;
		write32(HW_IPC_ARMCTRL, regs );

		//enqueue command
		enqueue_request((ipcreq*)read32(HW_IPC_PPCMSG));
		
		//disable interrupt
		write32(HW_IPC_ARMCTRL, read32(HW_IPC_ARMCTRL) | IPC_ARM_OUTGOING );
		
		donebell++;
	}
	if(!donebell)
		gecko_printf("IPC: IRQ but no bell!\n");
}

void ipc_initialize(void)
{
	write32(HW_IPC_ARMMSG, 0);
	write32(HW_IPC_PPCMSG, 0);
	write32(HW_IPC_PPCCTRL, IPC_CTRL_RESET);
	write32(HW_IPC_ARMCTRL, IPC_CTRL_RESET);
	slow_queue_head = 0;
	slow_queue_tail = 0;
	in_head = 0;
	out_tail = 0;
	irq_enable(IRQ_IPC);
	write32(HW_IPC_ARMCTRL, IPC_CTRL_IRQ_IN);
}

void ipc_shutdown(void)
{
	// Don't kill message registers so our PPC side doesn't get confused
	//write32(HW_IPC_ARMMSG, 0);
	//write32(HW_IPC_PPCMSG, 0);
	// Do kill flags so Nintendo's SDK doesn't get confused
	write32(HW_IPC_PPCCTRL, IPC_CTRL_RESET);
	write32(HW_IPC_ARMCTRL, IPC_CTRL_RESET);
	irq_disable(IRQ_IPC);
}

void ipc_ppc_boot_title(u64 titleId)
{
	boot_titleID = titleId;
	return;
}

u32 ipc_main(void)
{
	while (!boot_titleID) 
	{
		u32 cookie = irq_kill();
		if(in_cnt > 0)
			ipc_process_input();
		irq_restore(cookie);
		
		/*while (!reset_vector && (slow_queue_head != slow_queue_tail)) {
			reset_vector = process_slow(&slow_queue[slow_queue_head]);
			slow_queue_head = (slow_queue_head+1)&(IPC_SLOW_SIZE-1);
		}

		if (!reset_vector)
		{
			gecko_process();

			u32 cookie = irq_kill();
			if(slow_queue_head == slow_queue_tail)
				irq_wait();
			irq_restore(cookie);
		}*/
	}
	irq_kill();
	return boot2_run(boot_titleID >> 32, boot_titleID & 0xFFFFFFFF);
}

