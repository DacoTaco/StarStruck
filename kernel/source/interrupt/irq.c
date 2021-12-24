/*
	mini - a Free Software replacement for the Nintendo/BroadOn IOS.
	IRQ support

Copyright (C) 2008, 2009	Hector Martin "marcan" <marcan@marcansoft.com>
Copyright (C) 2008, 2009	Sven Peter <svenpeter@gmail.com>
Copyright (C) 2009			Andre Heider "dhewg" <dhewg@wiibrew.org>

# This code is licensed to you under the terms of the GNU GPL, version 2;
# see file COPYING or http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt
*/

#include <ios/processor.h>
#include <ios/gecko.h>
#include <ios/errno.h>

#include "interrupt/irq.h"
#include "scheduler/threads.h"
#include "memory/memory.h"
#include "core/hollywood.h"
#include "messaging/ipc.h"

#include "crypto.h"
#include "nand.h"
#include "sdhc.h"

static u32 _alarm_frequency = 0;
void irq_setup_stack(void);

void IrqInit(void)
{
	//enable timer, nand, aes, sha1, reset & unknown12 interrupts
	write32(HW_ARMIRQMASK, IRQF_TIMER | IRQF_NAND | IRQF_AES | IRQF_SHA1 | IRQF_UNKN12 | IRQF_RESET);
	set32(HW_DIFLAGS, 6);
}

void irq_initialize(void)
{
	irq_setup_stack();
	write32(HW_ALARM, 0);
	write32(HW_ARMIRQMASK, 0);
	write32(HW_ARMIRQFLAG, 0xffffffff);
	irq_restore(CPSR_FIQDIS);

	//???
	write32(HW_ARMFIQMASK, 0);
	write32(HW_ARMIRQMASK+0x20, 0);
}

void irq_shutdown(void)
{
	write32(HW_ARMIRQMASK, 0);
	write32(HW_ARMIRQFLAG, 0xffffffff);
	irq_kill();
}

u32 GetTimerValue(void)
{
	return read32(HW_TIMER);
}

s32 RegisterEventHandler(u8 device, int queueid, int message)
{
	return IPC_EINVAL;
}

s32 UnregisterEventHandler(u8 device)
{
	return IPC_EINVAL;
}

void irq_handler(ThreadContext* context)
{
	//set dacr so we can access everything
	set_dacr(0x55555555);
	
	u32 enabled = read32(HW_ARMIRQMASK);
	u32 flags = read32(HW_ARMIRQFLAG);
	
	//gecko_printf("In IRQ handler: 0x%08x 0x%08x 0x%08x\n", enabled, flags, flags & enabled);	
	flags = flags & enabled;
	
	//TODO : once all irq handlers are threads and this works via threads, this state setting must be removed.
	if(currentThread != NULL)
		currentThread->threadState = Ready;

	if(flags & IRQF_TIMER) 
	{
		//clear Timer
		write32(HW_ALARM, 0);
		
		// Do Work
		//gecko_printf("Registers Init (%p):\n", context);
		/*gecko_printf("    R0-R3: %08x %08x %08x %08x\n", context->registers[0], context->registers[1], context->registers[2], context->registers[3]);
		gecko_printf("    R4-R7: %08x %08x %08x %08x\n", context->registers[4], context->registers[5], context->registers[6], context->registers[7]);
		gecko_printf("   R8-R11: %08x %08x %08x %08x\n", context->registers[8], context->registers[9], context->registers[10], context->registers[11]);
		gecko_printf("      R12: %08x\n", context->registers[12]);
		gecko_printf("       SP: %08x\n", context->stackPointer);
		gecko_printf("       LR: %08x\n", context->linkRegister);
		gecko_printf("       PC: %08x\n", context->programCounter);
		gecko_printf("     SPSR: %08x\n", context->statusRegister);
		*/

		//change thread queue? 
		//TODO : check with IOS
		
		//Reset Timer
		if (_alarm_frequency)
			write32(HW_ALARM, read32(HW_TIMER) + _alarm_frequency);

		write32(HW_ARMIRQFLAG, IRQF_TIMER);
	}
	if(flags & IRQF_NAND) {
//		gecko_printf("IRQ: NAND\n");
		write32(NAND_CMD, 0x7fffffff); // shut it up
		write32(HW_ARMIRQFLAG, IRQF_NAND);
		nand_irq();
	}
	if(flags & IRQF_GPIO1B) {
//		gecko_printf("IRQ: GPIO1B\n");
		write32(HW_GPIO1BINTFLAG, 0xFFFFFF); // shut it up
		write32(HW_ARMIRQFLAG, IRQF_GPIO1B);
	}
	if(flags & IRQF_GPIO1) {
//		gecko_printf("IRQ: GPIO1\n");
		write32(HW_GPIO1INTFLAG, 0xFFFFFF); // shut it up
		write32(HW_ARMIRQFLAG, IRQF_GPIO1);
	}
	if(flags & IRQF_RESET) {
//		gecko_printf("IRQ: RESET\n");
		write32(HW_ARMIRQFLAG, IRQF_RESET);
	}
	if(flags & IRQF_IPC) {
		//gecko_printf("IRQ: IPC\n");
		ipc_irq();
		write32(HW_ARMIRQFLAG, IRQF_IPC);
	}
	if(flags & IRQF_AES) {
//		gecko_printf("IRQ: AES\n");
		write32(HW_ARMIRQFLAG, IRQF_AES);
	}
	if (flags & IRQF_SDHC) {
//		gecko_printf("IRQ: SDHC\n");
		write32(HW_ARMIRQFLAG, IRQF_SDHC);
		sdhc_irq();
	}
	
	flags &= ~IRQF_ALL;
	if(flags) {
		gecko_printf("IRQ: unknown 0x%08x\n", flags);
		write32(HW_ARMIRQFLAG, flags);
	}
}

void irq_enable(u32 irq)
{
	set32(HW_ARMIRQMASK, 1<<irq);
}

void irq_disable(u32 irq)
{
	clear32(HW_ARMIRQMASK, 1<<irq);
}

void irq_set_alarm(u32 ms, u8 enable)
{
	_alarm_frequency = IRQ_ALARM_MS2REG(ms);

	if (enable)
		write32(HW_ALARM, read32(HW_TIMER) + _alarm_frequency);
}

