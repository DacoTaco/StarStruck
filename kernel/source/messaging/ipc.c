/*
	StarStruck - a Free Software replacement for the Nintendo/BroadOn IOS.
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
#include <string.h>
#include <types.h>
#include <ios/errno.h>
#include <ios/processor.h>
#include <ios/gecko.h>

#include "core/hollywood.h"
#include "core/defines.h"
#include "memory/memory.h"
#include "messaging/ipc.h"
#include "interrupt/irq.h"

#include "utils.h"
#include "nand.h"
#include "sdhc.h"
#include "sdmmc.h"
#include "crypto.h"
#include "boot2.h"
#include "powerpc.h"
#include "panic.h"

#define STARSTRUCK_VERSION__MAJOR 1
#define STARSTRUCK_VERSION__MINOR 4

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
#define IPC_PPC_ACK			IPC_PPC_Y2
#define IPC_TRIG_ACK		IPC_PPC_IY2

#define IPC_MAX_FILENAME	0x1300

extern char __mem2_area_start[];
static volatile u64 boot_titleID = 0;
static volatile IpcMessage* input_queue[IPC_IN_SIZE] ALIGNED(0x20) SRAM_BSS;
static volatile IpcMessage* output_queue[IPC_OUT_SIZE] ALIGNED(0x20) SRAM_BSS;
static u16 in_cnt = 0;
static u16 out_cnt = 0;

void ipc_send_ack(void)
{
	u32 ppc_regs = read32(HW_IPC_PPCCTRL);
	u32 arm_regs = read32(HW_IPC_ARMCTRL);
	
	//Send ACK to the PPC + remove the reply flag
	ppc_regs &= ~(IPC_TRIG_OUTGOING);
	ppc_regs |= IPC_TRIG_ACK | IPC_PPC_ACK;
	arm_regs |= IPC_ARM_ACK_OUT;
	
	write32(HW_IPC_PPCCTRL, ppc_regs );
	write32(HW_IPC_ARMCTRL, arm_regs );
}

void ipc_reply(IpcMessage* req)
{
	//Send Reply
	u32 regs = read32(HW_IPC_ARMCTRL);
	write32(HW_IPC_ARMMSG, (u32)req);
	write32(HW_IPC_PPCCTRL, read32(HW_IPC_PPCCTRL) | IPC_PPC_OUTGOING | IPC_TRIG_OUTGOING | IPC_TRIG_ACK );
	write32(HW_IPC_ARMCTRL, regs | IPC_ARM_OUTGOING );	
}

void enqueue_reply(IpcMessage* req)
{
	if(out_cnt >= IPC_OUT_SIZE)
	{
		gecko_printf("IPC: OUTPUT QUEUE OVERLOAD\n");
		return;
	}
	
	output_queue[out_cnt] = req;
	out_cnt++;
}

void ipc_enqueue_reuqest(IpcMessage* req)
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
	
	volatile IpcMessage* req = input_queue[in_cnt-1];
	if(req == NULL)
		goto exit_process;
	
	s32 return_value = IOS_EINVAL;	
	u8 reply = 1;
	/*ios_module* module = NULL;
	DCFlushRange((u32*)req, sizeof(IpcMessage));
	DCInvalidateRange((u32*)req, sizeof(IpcMessage));
	
	if(req->cmd == IOS_OPEN)
	{
		/*example on how we could with IOS_OPEN commands, except maybe look it up in a list?
			if(strncmp(req->open.filepath, es_module.device_name, IPC_MAX_FILENAME) == 0)
				ios_module = es_module;
			else
				...
		*//*
		gecko_printf("IPC: unknown open request 0x%04x-0x%04x - '%s'\n", req->cmd, req->fd, req->open.filepath);
	}
	else
	{	
		/*example on how we could with other commands, except maybe look it up in a list?
			switch(req->fd)
			{
				case IPC_DEV_ES:
					ios_module = es_module;
					break;
				...
		*//*
		gecko_printf("IPC: unknown request 0x%04x-0x%04x\n", req->cmd, req->fd);
	}
	
	if(module != NULL)
	{
		switch(req->cmd)
		{
			case IOS_IOCTLV:
			case IOS_IOCTL:
				return_value = module->request_handler((IpcMessage*)req, &reply);
				break;
			case IOS_OPEN:
				return_value = module->open_handler(req->open.filepath, req->open.mode, &reply);
				break;
			case IOS_CLOSE:
			case IOS_READ:
			case IOS_WRITE:
			case IOS_SEEK:
			//do nothing
			default:
				break;
		}
	}*/
	
	write32((u32)&req->result, return_value);
	DCFlushRange((void*)req, sizeof(IpcMessage));
	DCInvalidateRange((void*)req, sizeof(IpcMessage));
	ICInvalidateAll();

	if(reply)
		ipc_reply((IpcMessage*)req);
	else //only ack
		ipc_send_ack();

exit_process:
	in_cnt--;
	return;
}

void ipc_irq(void)
{
	int donebell = 0;	
	while(read32(HW_IPC_ARMCTRL) & IPC_ARM_INCOMING) 
	{		
		//Send ACK to the PPC + remove the reply flag
		ipc_send_ack();

		//enqueue command
		ipc_enqueue_reuqest((IpcMessage*)read32(HW_IPC_PPCMSG));
		
		//disable interrupt
		//this has to be done seperatly as the registers might have changed by now
		write32(HW_IPC_ARMCTRL, read32(HW_IPC_ARMCTRL) | IPC_ARM_OUTGOING );
		
		donebell++;
	}
	
	if(!donebell)
		gecko_printf("IPC: IRQ but no bell!\n");
}

void IpcInit(void)
{
	//init ipc interrupts
	write32(HW_IPC_ARMCTRL, ( IPC_ARM_IX1 | IPC_ARM_IX2 ));
}
void ipc_initialize(void)
{
	write32(HW_IPC_ARMMSG, 0);
	write32(HW_IPC_PPCMSG, 0);
	write32(HW_IPC_PPCCTRL, IPC_CTRL_RESET);
	write32(HW_IPC_ARMCTRL, IPC_CTRL_RESET);
	in_cnt = 0;
	out_cnt = 0;
	irq_enable(IRQ_IPC);
	write32(HW_IPC_ARMCTRL, IPC_ARM_IX1);
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
	while (boot_titleID == 0) 
	{
		u32 cookie = DisableInterrupts();		
		if(in_cnt > 0)
			ipc_process_input();
		RestoreInterrupts(cookie);
	}
	DisableInterrupts();
	
	return boot2_run(boot_titleID >> 32, boot_titleID & 0xFFFFFFFF);
}

