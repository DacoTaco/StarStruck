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
#include "scheduler/timer.h"
#include "memory/memory.h"
#include "messaging/message_queue.h"
#include "core/hollywood.h"
#include "messaging/ipc.h"

#include "crypto.h"
#include "nand.h"
#include "sdhc.h"

EventHandler eventHandlers[MAX_DEVICES];
void irq_setup_stack(void);

void IrqInit(void)
{
	//enable timer, nand, aes, sha1, reset & unknown12 interrupts
	write32(HW_ARMIRQMASK, IRQF_TIMER | IRQF_NAND | IRQF_AES | IRQF_SHA1 | IRQF_UNKN12 | IRQF_RESET);
	set32(HW_DIFLAGS, 6);
}

s32 RegisterEventHandler(u8 device, int queueid, void* message)
{
	u32 irqState = DisableInterrupts();
	s32 ret = 0;
	if(device >= MAX_DEVICES || queueid >= MAX_MESSAGEQUEUES)
	{
		ret = IPC_EINVAL;
		goto restore_and_return;	
	}

	if(MessageQueues[queueid].ProcessId != CurrentThread->ProcessId)
	{
		ret = IPC_EACCES;
		goto restore_and_return;
	}

	eventHandlers[device].Message = message;
	eventHandlers[device].ProcessId = CurrentThread->ProcessId;
	eventHandlers[device].MessageQueue = &MessageQueues[queueid];

restore_and_return:
	RestoreInterrupts(irqState);
	return ret;
}

s32 UnregisterEventHandler(u8 device)
{
	u32 irqState = DisableInterrupts();
	s32 ret = 0;
	if(device >= MAX_DEVICES)
	{
		ret = IPC_EINVAL;
		goto restore_and_return;	
	}

	if(eventHandlers[device].ProcessId != CurrentThread->ProcessId)
	{
		ret = IPC_EACCES;
		goto restore_and_return;
	}

	eventHandlers[device].MessageQueue = NULL;
	eventHandlers[device].Message = NULL;

restore_and_return:
	RestoreInterrupts(irqState);
	return ret;
}

void EnqueueEventHandler(s32 device)
{
	MessageQueue* queue = eventHandlers[device].MessageQueue;
	if(queue == NULL)
		return;

	if(queue->Used >= queue->QueueSize)
		return;

	s32 messageIndex = queue->Used + queue->First;
	queue->Used += 1;
	if(messageIndex > queue->QueueSize)
		messageIndex -= queue->QueueSize;

	queue->QueueHeap[messageIndex] = eventHandlers[device].Message;
	if(queue->ReceiveThreadQueue.NextThread != NULL)
	{
		ThreadInfo* handlerThread = ThreadQueue_PopThread(&queue->ReceiveThreadQueue);
		handlerThread->ThreadState = Ready;
		handlerThread->UserContext.Registers[0] = 0;
		ThreadQueue_PushThread(&SchedulerQueue, handlerThread);
	}
}

void irq_initialize(void)
{
	irq_setup_stack();
	write32(HW_ALARM, 0);
	write32(HW_ARMIRQMASK, 0);
	write32(HW_ARMIRQFLAG, 0xffffffff);
	RestoreInterrupts(CPSR_FIQDIS);

	//???
	write32(HW_ARMFIQMASK, 0);
	write32(HW_DBGINTEN, 0);
}

void irq_shutdown(void)
{
	write32(HW_ARMIRQMASK, 0);
	write32(HW_ARMIRQFLAG, 0xffffffff);
	DisableInterrupts();
}

void irq_handler(ThreadContext* context)
{
	//Enqueue current thread
	CurrentThread->ThreadState = Ready;
	ThreadQueue_PushThread(&SchedulerQueue, CurrentThread);
	//set dacr so we can access everything
	SetDomainAccessControlRegister(0x55555555);
	
	u32 flags = read32(HW_ARMIRQFLAG) & read32(HW_ARMIRQMASK);
	//gecko_printf("In IRQ handler: 0x%08x\n", flags);

	if(flags & IRQF_TIMER) 
	{
		EnqueueEventHandler(IRQ_TIMER);
		write32(HW_ALARM, 0);
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
