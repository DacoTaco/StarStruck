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

#define IRQ_TIMER	0
#define IRQ_NAND	1
#define IRQ_AES		2
#define IRQ_SHA1	3
#define IRQ_EHCI	4
#define IRQ_OHCI0	5
#define IRQ_OHCI1	6
#define IRQ_SDHC	7
#define IRQ_WIFI	8
#define IRQ_GPIO1B	10
#define IRQ_GPIO1	11
#define IRQ_UNKN12	12
#define IRQ_RESET	17
#define IRQ_PPCIPC	30
#define IRQ_IPC		31

#define IRQF_TIMER	(1<<IRQ_TIMER)
#define IRQF_NAND	(1<<IRQ_NAND)
#define IRQF_AES	(1<<IRQ_AES)
#define IRQF_SHA1	(1<<IRQ_SHA1)
#define IRQF_SDHC	(1<<IRQ_SDHC)
#define IRQF_GPIO1B	(1<<IRQ_GPIO1B)
#define IRQF_GPIO1	(1<<IRQ_GPIO1)
#define IRQF_UNKN12	(1<<IRQ_UNKN12)
#define IRQF_RESET	(1<<IRQ_RESET)
#define IRQF_IPC	(1<<IRQ_IPC)

#define IRQF_ALL	( \
	IRQF_TIMER|IRQF_NAND|IRQF_GPIO1B|IRQF_GPIO1| \
	IRQF_RESET|IRQF_IPC|IRQF_AES|IRQF_SDHC \
	)

#define CPSR_IRQDIS 0x80
#define CPSR_FIQDIS 0x40

#ifndef __ASSEMBLER__

#include <types.h>

#define IRQ_ALARM_MS2REG(x)	(1898 * x)
#define MAX_DEVICE			0x20

void IrqInit(void);
u32 DisableInterrupts(void);
void RestoreInterrupts(u32 cookie);
s32 RegisterEventHandler(u8 device, int queueid, int message);
s32 UnregisterEventHandler(u8 device);

void irq_initialize(void);
void irq_shutdown(void);
void irq_enable(u32 irq);
void irq_disable(u32 irq);
void irq_wait(void);

#endif
#endif

