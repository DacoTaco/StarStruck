/*
	mini - a Free Software replacement for the Nintendo/BroadOn IOS.
	IRQ support

Copyright (C) 2008, 2009	Hector Martin "marcan" <marcan@marcansoft.com>
Copyright (C) 2008, 2009	Sven Peter <svenpeter@gmail.com>

# This code is licensed to you under the terms of the GNU GPL, version 2;
# see file COPYING or http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt
*/

#ifndef __IRQ_H__
#define __IRQ_H__

#define MAX_DEVICES			32
#define IRQ_TIMER			0
#define IRQ_NAND			1
#define IRQ_AES				2
#define IRQ_SHA1			3
#define IRQ_EHCI			4
#define IRQ_OHCI0			5
#define IRQ_OHCI1			6
#define IRQ_SDHC			7
#define IRQ_WIFI			8
#define IRQ_GPIO1B			10
#define IRQ_GPIO1			11
#define IRQ_UNKN12			12
#define IRQ_RESET			17
#define IRQ_DI				18
#define IRQ_PPCIPC			30
#define IRQ_IPC				31

#define IRQF_TIMER			(1<<IRQ_TIMER)
#define IRQF_NAND			(1<<IRQ_NAND)
#define IRQF_AES			(1<<IRQ_AES)
#define IRQF_SHA1			(1<<IRQ_SHA1)
#define IRQF_EHCI			(1<<IRQ_EHCI)
#define IRQF_OHCI0			(1<<IRQ_OHCI0)
#define IRQF_OHCI1			(1<<IRQ_OHCI1)
#define IRQF_SDHC			(1<<IRQ_SDHC)
#define IRQF_WIFI			(1<<IRQ_WIFI)
#define IRQF_GPIO1B			(1<<IRQ_GPIO1B)
#define IRQF_GPIO1			(1<<IRQ_GPIO1)
#define IRQF_UNKN12			(1<<IRQ_UNKN12)
#define IRQF_RESET			(1<<IRQ_RESET)
#define IRQF_DI				(1<<IRQ_DI)
#define IRQF_IPC			(1<<IRQ_IPC)

#define IRQF_ALL			( IRQF_TIMER|IRQF_NAND|IRQF_GPIO1B|IRQF_GPIO1|IRQF_RESET|IRQF_IPC|IRQF_AES|IRQF_SHA1|IRQF_SDHC )

#define CPSR_IRQDIS 0x80
#define CPSR_FIQDIS 0x40

#ifndef __ASSEMBLER__

#include <types.h>
#include "messaging/message_queue.h"

typedef struct
{
	MessageQueue* MessageQueue;
	void* Message;
	u32 ProcessId;
	u32 Unknown;
} EventHandler;
CHECK_SIZE(EventHandler, 0x10);
CHECK_OFFSET(EventHandler, 0x00, MessageQueue);
CHECK_OFFSET(EventHandler, 0x04, Message);
CHECK_OFFSET(EventHandler, 0x08, ProcessId);
CHECK_OFFSET(EventHandler, 0x0C, Unknown);


void IrqInit(void);
u32 DisableInterrupts(void);
void RestoreInterrupts(u32 cookie);
s32 RegisterEventHandler(u8 device, int queueid, void* message);
s32 UnregisterEventHandler(u8 device);

s32 ClearAndEnableEvent(u32 inter);
s32 ClearAndEnableSDInterrupt(const u8 sdio);
s32 ClearAndEnableDIInterrupt(void);
s32 ClearAndEnableIPCInterrupt(void);

void irq_shutdown(void);
void irq_enable(u32 irq);
void irq_disable(u32 irq);
void irq_wait(void);

#endif
#endif

