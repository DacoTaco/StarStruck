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
#include <ios/printk.h>

#include "core/hollywood.h"
#include "core/defines.h"
#include "memory/memory.h"
#include "messaging/ipc.h"
#include "interrupt/irq.h"
#include "filedesc/filedesc_types.h"
#include "filedesc/calls_async.h"

#include "utils.h"
#include "nand.h"
#include "sdhc.h"
#include "sdmmc.h"
#include "boot2.h"
#include "panic.h"

// These defines are for the ARMCTRL regs
// See http://wiibrew.org/wiki/Hardware/IPC
#define IPC_ARM_Y1        0x01
#define IPC_ARM_X2        0x02
#define IPC_ARM_X1        0x04
#define IPC_ARM_Y2        0x08
#define IPC_ARM_IX1       0x10
#define IPC_ARM_IX2       0x20

#define IPC_PPC_X1        0x01
#define IPC_PPC_Y2        0x02
#define IPC_PPC_Y1        0x04
#define IPC_PPC_X2        0x08
#define IPC_PPC_IY1       0x10
#define IPC_PPC_IY2       0x20

// reset both flags (X* for ARM and Y* for PPC)
#define IPC_CTRL_RESET    0x06

#define IPC_ARM_INCOMING  IPC_ARM_X1
#define IPC_ARM_RELAUNCH  IPC_ARM_X2
#define IPC_ARM_OUTGOING  IPC_ARM_Y1
#define IPC_ARM_ACK_OUT   IPC_ARM_Y2

#define IPC_PPC_OUTGOING  IPC_PPC_Y1
#define IPC_TRIG_OUTGOING IPC_PPC_IY1
#define IPC_PPC_ACK       IPC_PPC_Y2
#define IPC_TRIG_ACK      IPC_PPC_IY2

#define IPC_MAX_FILENAME  0x1300

#define MAX_IPCMESSAGES   (MAX_THREADS + IPC_EXTRA_MESSAGES)

extern const u32 __ipc_heap_start;
IpcMessage *IpcMessageArray = NULL;
FileDescriptorPath *FiledescPathArray = NULL;
MessageQueue IpcMessageQueueArray[MAX_THREADS] SRAM_BSS;
unsigned ThreadMessageUsageArray[MAX_THREADS] SRAM_BSS;
static void *IpcMessageQueueDataPtrArray[MAX_THREADS] SRAM_BSS;
static FileDescriptorPath *FiledescPathPointerArray[MAX_THREADS] SRAM_BSS;

ThreadInfo *IpcHandlerThread = NULL;
s32 IpcHandlerThreadId = -1;

static IpcMessage *IpcHandlerMessageQueueData[50] SRAM_BSS;
static IpcRequest IpcHandlerRequest SRAM_BSS;

#define IPC_CIRCULAR_BUFFER_SIZE 0x30

typedef struct
{
	u32 HadRelaunchFlag;
	u32 WaitingInBufferAmount;
	u32 ReadyToSendAmount;
	u32 SendingIndex;
	u32 PrepareToSendIndex;
	IpcRequest *BackingArray[IPC_CIRCULAR_BUFFER_SIZE];
} IpcCircularBuffer;

static IpcCircularBuffer IpcCircBuf SRAM_BSS;

void SendIpcRequest(void)
{
	if (!IpcCircBuf.HadRelaunchFlag || IpcCircBuf.ReadyToSendAmount == 0)
		return;

	IpcRequest *const ptr = IpcCircBuf.BackingArray[IpcCircBuf.SendingIndex];
	DCFlushRange(ptr, sizeof(IpcRequest));
	write32(HW_IPC_ARMMSG, (u32)ptr);
	IpcCircBuf.SendingIndex = (IpcCircBuf.SendingIndex + 1) % IPC_CIRCULAR_BUFFER_SIZE;
	IpcCircBuf.ReadyToSendAmount--;
	IpcCircBuf.WaitingInBufferAmount--;
	IpcCircBuf.HadRelaunchFlag = 0;
	mask32(HW_IPC_ARMCTRL, (u32) ~(IPC_ARM_IX1 | IPC_ARM_IX2),
	       (IpcCircBuf.WaitingInBufferAmount == (IPC_CIRCULAR_BUFFER_SIZE - 1) ? IPC_ARM_ACK_OUT : 0) |
	           IPC_ARM_OUTGOING);
}

static void FlushAndSendRequest(IpcRequest *request)
{
	const u32 requestCommand = request->RequestCommand;
	if (requestCommand == IOS_IOCTL)
	{
		DCFlushRange(request->Message.Ioctl.InputBuffer, request->Message.Ioctl.InputLength);
		DCFlushRange(request->Message.Ioctl.IoBuffer, request->Message.Ioctl.IoLength);
	}
	else if (requestCommand == IOS_READ)
	{
		DCFlushRange(request->Message.Read.MessageData, (u32)request->Result);
	}
	else if (requestCommand == IOS_IOCTLV)
	{
		const u32 totalArgc =
		    request->Message.Ioctlv.InputArgc + request->Message.Ioctlv.IoArgc;
		for (u32 i = 0; i < totalArgc; ++i)
		{
			DCFlushRange(request->Message.Ioctlv.MessageData[i].Data,
			             request->Message.Ioctlv.MessageData[i].Length);
		}
		DCFlushRange(request->Message.Ioctlv.MessageData,
		             totalArgc * sizeof(IoctlvMessageData));
	}

	IpcCircBuf.BackingArray[IpcCircBuf.PrepareToSendIndex] = request;
	IpcCircBuf.PrepareToSendIndex = (IpcCircBuf.PrepareToSendIndex + 1) % IPC_CIRCULAR_BUFFER_SIZE;
	IpcCircBuf.ReadyToSendAmount++;

	SendIpcRequest();
}

static int ValidateAddress(const void *const ptr, const u32 size)
{
	const u32 addr = (u32)ptr;
	const u32 addrEnd = addr + size;
	const int ret = (addr < addrEnd) && ((MEM2_BASE <= addr && addrEnd <= __ipc_heap_start) ||
	                                     (addrEnd <= MEM1_END));
	if (!ret)
		printk("IPC: failed buf check: ptr=%08x len=%d\n", addr, size);
	return ret;
}

#ifndef MIOS

void IpcHandler(void)
{
	SetThreadPriority(0, 0x40);
	IpcHandlerRequest.Command = IOS_INTERRUPT;

	s32 ret = CreateMessageQueue((void **)IpcHandlerMessageQueueData,
	                             ARRAY_LENGTH(IpcHandlerMessageQueueData));
	if (ret < 0)
		return;

	const s32 messageQueue = ret;
	ret = RegisterEventHandler(IRQ_IPC, messageQueue, &IpcHandlerRequest);
	if (ret != IPC_SUCCESS)
		return;

 // enable incoming & outgoing ipc messages from PPC
	if ((read32(HW_IPC_PPCCTRL) & (IPC_PPC_IY1 | IPC_PPC_IY2)) == 0)
		write32(HW_IPC_PPCCTRL, read32(HW_IPC_PPCCTRL) & (IPC_PPC_IY1 | IPC_PPC_IY2));

	ClearAndEnableIPCInterrupt();

	IpcMessage *messagePointer = NULL;
	IpcMessage *messageFromPPC = NULL;
	while (1)
	{
  //wait for a valid message
		while (ReceiveMessage(messageQueue, (void **)&messagePointer, None) != IPC_SUCCESS)
			;
		messageFromPPC = (void *)read32(HW_IPC_PPCMSG);
		if (messagePointer->Request.Command == IOS_REPLY)
		{
			FlushAndSendRequest(&messagePointer->Request);
			continue;
		}

		if (messagePointer->Request.Command != IOS_INTERRUPT)
		{
			printk("UNKNOWN MESSAGE: %u\n", messagePointer->Request.Command);
			continue;
		}

		const u32 armctrl = read32(HW_IPC_ARMCTRL);
		if ((armctrl & IPC_ARM_RELAUNCH) != 0)
		{
			IpcCircBuf.HadRelaunchFlag = 1;
			mask32(HW_IPC_ARMCTRL, (u32) ~(IPC_ARM_IX1 | IPC_ARM_IX2), IPC_ARM_RELAUNCH);

			ClearAndEnableIPCInterrupt();
			SendIpcRequest();
			continue;
		}

		if ((armctrl & IPC_ARM_INCOMING) == 0)
		{
			printk("UNKNOWN INTERRUPT: %x / %x\n", read32(HW_ARMIRQFLAG),
			       read32(HW_ARMIRQMASK));
			continue;
		}

		u32 set = IpcCircBuf.WaitingInBufferAmount < (IPC_CIRCULAR_BUFFER_SIZE - 1) ?
		              IPC_ARM_ACK_OUT :
		              0;
		mask32(HW_IPC_ARMCTRL, (u32) ~(IPC_ARM_IX1 | IPC_ARM_IX2), set | IPC_ARM_INCOMING);
		ClearAndEnableIPCInterrupt();

		IpcCircBuf.WaitingInBufferAmount++;
		if (!ValidateAddress(messageFromPPC, sizeof(IpcRequest)))
			continue;

		DCInvalidateRange(messageFromPPC, sizeof(IpcRequest));
		const int filedescId = messageFromPPC->Request.FileDescriptor;
		messageFromPPC->Request.RequestCommand = messageFromPPC->Request.Command;
		ret = IPC_SUCCESS;

  // systematically check pointers for access and invalidate them
		switch (messageFromPPC->Request.Command)
		{
			default:
				printk("Dispatch switch ERROR: %d cmd: %d\n", IPC_EINVAL,
				       messageFromPPC->Request.Command);
				ret = IPC_EINVAL;
				break;

			case IOS_OPEN:
				if (!ValidateAddress(messageFromPPC->Request.Message.Open.Filepath, MAX_PATHLEN))
				{
					ret = IPC_EACCES;
					break;
				}
				DCInvalidateRange(messageFromPPC->Request.Message.Open.Filepath, MAX_PATHLEN);
				const u32 pathlen =
				    strnlen(messageFromPPC->Request.Message.Open.Filepath, MAX_PATHLEN);
				if (pathlen >= MAX_PATHLEN)
				{
					printk("IPC: failed open path check: path=%s len=%d\n",
					       messageFromPPC->Request.Message.Open.Filepath, pathlen);
					ret = IPC_EINVAL;
					break;
				}

				ret = OpenFDAsync(messageFromPPC->Request.Message.Open.Filepath,
				                  messageFromPPC->Request.Message.Open.Mode,
				                  messageQueue, messageFromPPC);
				break;

			case IOS_CLOSE:
				ret = CloseFDAsync(filedescId, messageQueue, messageFromPPC);
				break;

			case IOS_READ:
				if (messageFromPPC->Request.Message.Read.Length > 0 &&
				    !ValidateAddress(messageFromPPC->Request.Message.Read.MessageData,
				                     messageFromPPC->Request.Message.Read.Length))
				{
					ret = IPC_EACCES;
					break;
				}

				ret = ReadFDAsync(filedescId,
				                  messageFromPPC->Request.Message.Read.MessageData,
				                  messageFromPPC->Request.Message.Read.Length,
				                  messageQueue, messageFromPPC);
				break;

			case IOS_WRITE:
				if (messageFromPPC->Request.Message.Write.Length > 0 &&
				    !ValidateAddress(messageFromPPC->Request.Message.Write.MessageData,
				                     messageFromPPC->Request.Message.Write.Length))
				{
					ret = IPC_EACCES;
					break;
				}
				DCInvalidateRange(messageFromPPC->Request.Message.Write.MessageData,
				                  messageFromPPC->Request.Message.Write.Length);

				ret = WriteFDAsync(filedescId,
				                   messageFromPPC->Request.Message.Write.MessageData,
				                   messageFromPPC->Request.Message.Write.Length,
				                   messageQueue, messageFromPPC);
				break;

			case IOS_SEEK:
				ret = SeekFDAsync(filedescId,
				                  messageFromPPC->Request.Message.Seek.Where,
				                  messageFromPPC->Request.Message.Seek.Whence,
				                  messageQueue, messageFromPPC);
				break;

			case IOS_IOCTL:
				if (messageFromPPC->Request.Message.Ioctl.InputLength != 0 &&
				    !ValidateAddress(messageFromPPC->Request.Message.Ioctl.InputBuffer,
				                     messageFromPPC->Request.Message.Ioctl.InputLength))
				{
					ret = IPC_EACCES;
					break;
				}
				if (messageFromPPC->Request.Message.Ioctl.IoLength != 0 &&
				    !ValidateAddress(messageFromPPC->Request.Message.Ioctl.IoBuffer,
				                     messageFromPPC->Request.Message.Ioctl.IoLength))
				{
					ret = IPC_EACCES;
					break;
				}
				DCInvalidateRange(messageFromPPC->Request.Message.Ioctl.InputBuffer,
				                  messageFromPPC->Request.Message.Ioctl.InputLength);
				DCInvalidateRange(messageFromPPC->Request.Message.Ioctl.IoBuffer,
				                  messageFromPPC->Request.Message.Ioctl.IoLength);

				ret = IoctlFDAsync(filedescId,
				                   messageFromPPC->Request.Message.Ioctl.Ioctl,
				                   messageFromPPC->Request.Message.Ioctl.InputBuffer,
				                   messageFromPPC->Request.Message.Ioctl.InputLength,
				                   messageFromPPC->Request.Message.Ioctl.IoBuffer,
				                   messageFromPPC->Request.Message.Ioctl.IoLength,
				                   messageQueue, messageFromPPC);
				break;

			case IOS_IOCTLV:
				const u32 totalArgc = messageFromPPC->Request.Message.Ioctlv.InputArgc +

				                      messageFromPPC->Request.Message.Ioctlv.IoArgc;
				if (totalArgc != 0 &&
				    !ValidateAddress(messageFromPPC->Request.Message.Ioctlv.MessageData,
				                     totalArgc * sizeof(IoctlvMessageData)))
				{
					ret = IPC_EACCES;
					break;
				}

				DCInvalidateRange(messageFromPPC->Request.Message.Ioctlv.MessageData,
				                  totalArgc * sizeof(IoctlvMessageData));
				u32 i = 0;
				for (; i < totalArgc; ++i)
				{
					if (messageFromPPC->Request.Message.Ioctlv.MessageData[i].Length != 0 &&
					    !ValidateAddress(messageFromPPC->Request.Message.Ioctlv
					                         .MessageData[i]
					                         .Data,
					                     messageFromPPC->Request.Message.Ioctlv
					                         .MessageData[i]
					                         .Length))
						break;

					DCInvalidateRange(
					    messageFromPPC->Request.Message.Ioctlv.MessageData[i].Data,
					    messageFromPPC->Request.Message.Ioctlv.MessageData[i].Length);
				}

				if (i != totalArgc)
				{
					ret = IPC_EACCES;
					break;
				}

				ret = IoctlvFDAsync(filedescId,
				                    messageFromPPC->Request.Message.Ioctlv.Ioctl,
				                    messageFromPPC->Request.Message.Ioctlv.InputArgc,
				                    messageFromPPC->Request.Message.Ioctlv.IoArgc,
				                    messageFromPPC->Request.Message.Ioctlv.MessageData,
				                    messageQueue, messageFromPPC);
				break;
		}

		if (ret < 0)
		{
			messageFromPPC->Request.Result = ret;
			FlushAndSendRequest(&messageFromPPC->Request);
		}
	}
}

static void IpcInitMessageQueues(void)
{
	int i;

	IpcMessageArray = KMalloc(sizeof(IpcMessage) * MAX_IPCMESSAGES);
	FiledescPathArray = KMalloc(sizeof(FileDescriptorPath) * MAX_THREADS);

	for (i = 0; i < MAX_THREADS; ++i)
	{
		FiledescPathPointerArray[i] = &FiledescPathArray[i];
	}

	for (i = 0; i < MAX_THREADS; ++i)
	{
		MessageQueue *currentQueue = &IpcMessageQueueArray[i];
		currentQueue->QueueHeap = &IpcMessageQueueDataPtrArray[i];
		currentQueue->ReceiveThreadQueue.NextThread = &ThreadStartingState;
		currentQueue->SendThreadQueue.NextThread = &ThreadStartingState;
		currentQueue->Used = 0;
		currentQueue->First = 0;
		currentQueue->QueueSize = 1;
		currentQueue->ProcessId = 0;
	}
}

void IpcInit(void)
{
 //init ipc interrupts
	write32(HW_IPC_ARMCTRL, (IPC_ARM_IX1 | IPC_ARM_IX2));
	IpcInitMessageQueues();
}

#endif

s32 ResourceReply(IpcMessage *message, s32 requestReturnValue)
{
	u32 interrupts = DisableInterrupts();
	s32 ret = IPC_EINVAL;

	const int msgIndex = message - IpcMessageArray;
	if (!(0 <= msgIndex && msgIndex < MAX_IPCMESSAGES))
		goto restore_and_return;

	IpcMessage *messageToSend = NULL;
	MessageQueue *queue = message->Callback;
	if (!(queue == NULL || (message->IsInQueue != 0 &&
	                        message->UsedByProcessId == CurrentThread->ProcessId)))
		goto restore_and_return;

	message->Request.Result = requestReturnValue;
	const int flag = queue != NULL;
	if (queue != NULL)
	{
		messageToSend = (IpcMessage *)message->CallerData;
		message->IsInQueue = 0;
		messageToSend->Request.Command = IOS_REPLY;
		messageToSend->Request.Result = requestReturnValue;
		ThreadMessageUsageArray[message->UsedByThreadId]--;
	}
	else
	{
		queue = &IpcMessageQueueArray[msgIndex];
		messageToSend = message;
	}
	ret = SendMessageToQueue(queue, messageToSend, flag ? RegisteredEventHandler : None);

restore_and_return:
	RestoreInterrupts(interrupts);
	return ret;
}

s32 SendMessageCheckReceive(IpcMessage *message, ResourceManager *resource)
{
	void *const cb = message->Callback;
	message->UsedByProcessId = resource->ProcessId;
	s32 ret = SendMessageToQueue(resource->Queue, message, None);

	if (ret != IPC_SUCCESS || cb != NULL)
		return ret;

	IpcMessage *receivedMessage = NULL;
	ret = ReceiveMessageFromQueue(&IpcMessageQueueArray[message - IpcMessageArray],
	                              (void **)&receivedMessage, None);
	if (ret == IPC_SUCCESS && receivedMessage != message)
		ret = IPC_EINVAL;

	return ret;
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
