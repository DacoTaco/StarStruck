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
#include "filedesc/filedesc_types.h"

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
#define IPC_ARM_RELAUNCH	IPC_ARM_X2
#define IPC_ARM_OUTGOING	IPC_ARM_Y1
#define IPC_ARM_ACK_OUT		IPC_ARM_Y2

#define IPC_PPC_OUTGOING	IPC_PPC_Y1
#define IPC_TRIG_OUTGOING	IPC_PPC_IY1
#define IPC_PPC_ACK			IPC_PPC_Y2
#define IPC_TRIG_ACK		IPC_PPC_IY2

#define IPC_MAX_FILENAME	0x1300

extern char __mem2_area_start[];
static volatile IpcMessage* input_queue[IPC_IN_SIZE] ALIGNED(0x20) SRAM_BSS;
static volatile IpcMessage* output_queue[IPC_OUT_SIZE] ALIGNED(0x20) SRAM_BSS;
static u16 in_cnt = 0;
static u16 out_cnt = 0;

IpcMessage* ipc_message_array = NULL;
FileDescriptorPath* fd_path_array = NULL;
MessageQueue ipc_message_queue_array[MAX_THREADS] SRAM_BSS;
unsigned thread_msg_usage_arr[MAX_THREADS] SRAM_BSS;
static void* message_queue_ptr_array[MAX_THREADS] SRAM_BSS;
static FileDescriptorPath* fd_path_ptr_array[MAX_THREADS] SRAM_BSS;

ThreadInfo* IpcHandlerThread = NULL;
s32 IpcHandlerThreadId = -1;

static IpcMessage* IpcHandlerMessageQueueData[50] SRAM_BSS;
static IpcRequest IpcHandlerRequest SRAM_BSS;

#define IPC_CIRCULAR_BUFFER_SIZE 0x30

typedef struct {
	u32 had_relaunch_flag;
	u32 waiting_in_buffer_cnt;
	u32 ready_to_send_cnt;
	u32 have_to_send_idx;
	u32 prepare_to_send_idx;
	IpcRequest* circular_backing_array[IPC_CIRCULAR_BUFFER_SIZE];
} IpcCircularBuffer;

static IpcCircularBuffer ipc_circ_buf SRAM_BSS;

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
	
	write32((u32)&req->Request.Result, return_value);
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


void DoSendIpcRequest(void)
{
	if (ipc_circ_buf.had_relaunch_flag && ipc_circ_buf.ready_to_send_cnt != 0)
	{
		IpcRequest* const ptr = ipc_circ_buf.circular_backing_array[ipc_circ_buf.have_to_send_idx];
		IOS_DCFlush(ptr, sizeof(IpcRequest));
		write32(HW_IPC_ARMMSG, (u32)ptr);
		ipc_circ_buf.have_to_send_idx = (ipc_circ_buf.have_to_send_idx + 1) % IPC_CIRCULAR_BUFFER_SIZE;
		ipc_circ_buf.ready_to_send_cnt--;
		ipc_circ_buf.waiting_in_buffer_cnt--;
		ipc_circ_buf.had_relaunch_flag = 0;
		write32(HW_IPC_ARMCTRL, read32(HW_IPC_ARMCTRL) & 0x30 | ((ipc_circ_buf.waiting_in_buffer_cnt == (IPC_CIRCULAR_BUFFER_SIZE - 1) ? IPC_ARM_ACK_OUT : 0) | IPC_ARM_INCOMING));
	}
}

static void DoFlushIpcRequestAfterInvalidate(IpcRequest *request)
{
	const u32 req_cmd = request->RequestCommand;
	if (req_cmd == IOS_IOCTL)
	{
		DCFlushRange(request->Data.Ioctl.InputBuffer, request->Data.Ioctl.InputLength);
		DCFlushRange(request->Data.Ioctl.IoBuffer, request->Data.Ioctl.IoLength);
	}
	else if (req_cmd == IOS_READ)
	{
		DCFlushRange(request->Data.Read.Data, request->Result);
	}
	else if (req_cmd == IOS_IOCTLV)
	{
		const u32 total_arg_count = request->Data.Ioctlv.InputArgc + request->Data.Ioctlv.IoArgc;
		for(u32 i = 0; i < total_arg_count; ++i)
		{
			DCFlushRange(request->Data.Ioctlv.Data[i].Data, request->Data.Ioctlv.Data[i].Length);
		}
		DCFlushRange(request->Data.Ioctlv.Data, total_arg_count * sizeof(IoctlvMessageData));
	}
	
	ipc_circ_buf.circular_backing_array[ipc_circ_buf.prepare_to_send_idx] = request;
	ipc_circ_buf.prepare_to_send_idx = (ipc_circ_buf.prepare_to_send_idx + 1) % IPC_CIRCULAR_BUFFER_SIZE;
	ipc_circ_buf.ready_to_send_cnt++;

	DoSendIpcRequest();
	return;
}

static int CheckAddrIsFine_Inner(const u32 addr, const u32 size)
{
	const u32 addr_end = addr + size;
	return (addr < addr_end) && ((0x10000000 <= addr && addr_end < 0x13620000) || (addr_end < 0x01800000));
}
static int CheckAddrIsFine(const u32 addr, const u32 size)
{
	const int ret = CheckAddrIsFine_Inner(addr, size);
	if(!ret)
		printk("IPC: failed buf check: ptr=%08x len=%d\n", addr, size);
	return ret;
}
static int CheckPtrIsFine(const void* const addr, const u32 size)
{
	return CheckAddrIsFine((u32)addr, size);
}

void IPC_Handler_Thread(void)
{
	IOS_SetThreadPriority(0, 0x40);
	IpcHandlerRequest.Command = IOS_INTERRUPT;

	const s32 messageQueue = IOS_CreateMessageQueue(IpcHandlerMessageQueueData, 50);
	if (messageQueue < 0)
		return;
	
	s32 ret = RegisterEventHandler(IRQ_IPC, messageQueue, &IpcHandlerRequest);
	if(ret != IPC_SUCCESS)
		return;

	// enable incoming & outgoing ipc messages from PPC
	if ((read32(HW_IPC_PPCCTRL) & 0x30) == 0)
	{
		write32(HW_IPC_PPCCTRL, 0);
	}

	IOS_enable_irq_iop();

	while(1)
	{
		IpcMessage *message_ptr = NULL;

		do {
			ret = ReceiveMessage(messageQueue, &message_ptr, None);
		} while (ret != IPC_SUCCESS);

		while (1)
		{
			if(message_ptr->Request.Command == IOS_REPLY)
			{
				DoFlushIpcRequestAfterInvalidate(&message_ptr->Request);
				break;
			}

			IpcRequest *ppc_message = (void*)read32(HW_IPC_PPCMSG);
			write32(HW_IPC_PPCMSG, (u32)ppc_message);

			if (message_ptr->Request.Command != IOS_INTERRUPT)
			{
				printk("UNKNOWN MESSAGE: %u\n", message_ptr->Request.Command);
				ret = ReceiveMessage(messageQueue, &message_ptr, None);
				if (ret != IPC_SUCCESS)
					break;
			}
			else
			{
				const u32 armctrl = read32(HW_IPC_ARMCTRL);
				if((armctrl & IPC_ARM_RELAUNCH) != 0)
				{
					ipc_circ_buf.had_relaunch_flag = 1;
					write32(HW_IPC_ARMCTRL, armctrl & 0x30 | IPC_ARM_RELAUNCH);
					IOS_enable_irq_iop();
					DoSendIpcRequest();
					break;
				}

				if ((armctrl & IPC_ARM_INCOMING) == 0)
				{
					printk("UNKNOWN INTERRUPT: %x / %x\n", read32(HW_ARMIRQFLAG), read32(HW_ARMIRQMASK));
					break;
				}

				write32(HW_IPC_ARMCTRL, armctrl & 0x30 | ((ipc_circ_buf.waiting_in_buffer_cnt < (IPC_CIRCULAR_BUFFER_SIZE - 1) ? IPC_ARM_ACK_OUT : 0) | IPC_ARM_INCOMING));

				IOS_enable_irq_iop();

				ipc_circ_buf.waiting_in_buffer_cnt++;
				if(!CheckPtrIsFine(ppc_message, sizeof(IpcRequest)))
				{
					break;
				}

				DCInvalidateRange(ppc_message, sizeof(IpcRequest));
				const int filedesc_id = ppc_message->FileDescriptor;
				ppc_message->RequestCommand = ppc_message->Command;
				ret = IPC_SUCCESS;
				switch(ppc_message->Command)
				{
				default:
					printk("Dispatch switch ERROR: %d cmd: %d\n", IPC_EINVAL, ppc_message->Command);
					ret = IPC_EINVAL;
					break;

				case IOS_OPEN:
					{
						if(!CheckPtrIsFine(ppc_message->Data.Open.Filepath, MAX_PATHLEN))
						{
							ret = IPC_EACCES;
							break;
						}
						DCInvalidateRange(ppc_message->Data.Open.Filepath, MAX_PATHLEN);
						const u32 pathlen = strnlen(ppc_message->Data.Open.Filepath, MAX_PATHLEN);
						if (pathlen >= MAX_PATHLEN)
						{
							printk("IPC: failed open path check: path=%s len=%d\n",ppc_message->Data.Open.Filepath, pathlen);
							ret = IPC_EINVAL;
							break;
						}
					}
					
					ret = OpenFDAsync(
						ppc_message->Data.Open.Filepath,
						ppc_message->Data.Open.Mode,
						messageQueue, ppc_message);
					break;

				case IOS_CLOSE:
					ret = CloseFDAsync(filedesc_id, messageQueue, ppc_message);
					break;

				case IOS_READ:
					{
						if (ppc_message->Read.Data != NULL)
						{
							if(!CheckPtrIsFine(ppc_message->Read.Data, ppc_message->Read.Length))
							{
								ret = IPC_EACCES;
								break;
							}
						}
						DCInvalidateRange(ppc_message->Read.Data, ppc_message->Read.Length);
					}

					ret = IOS_ReadAsync(filedesc_id, ppc_message->Read.Data, ppc_message->Read.Data == NULL ? 0 : ppc_message->Read.Length,messageQueue,ppc_message);
					break;

				case IOS_WRITE:
					{
						if (ppc_message->Write.Data != NULL)
						{
							if(!CheckPtrIsFine(ppc_message->Write.Data, ppc_message->Write.Length))
							{
								ret = IPC_EACCES;
								break;
							}
						}
						DCInvalidateRange(ppc_message->Write.Data, ppc_message->Write.Length);
					}
					
					ret = WriteFDAsync(filedesc_id,
						ppc_message->Write.Data,
						ppc_message->Write.Length,
						messageQueue, ppc_message);
					break;

				case IOS_SEEK:
					ret = SeekFDAsync(filedesc_id,
						ppc_message->Data.Seek.Where,
						ppc_message->Data.Seek.Whence,
						messageQueue, ppc_message);
					break;

				case IOS_IOCTL:
					{
						if (ppc_message->Data.Ioctl.InputLength != 0)
						{
							if(!CheckPtrIsFine(ppc_message->Data.Ioctl.InputBuffer, ppc_message->Data.Ioctl.InputLength))
							{
								ret = IPC_EACCES;
								break;
							}
						}
						if (ppc_message->Data.Ioctl.IoLength != 0)
						{
							if(!CheckPtrIsFine(ppc_message->Data.Ioctl.IoBuffer, ppc_message->Data.Ioctl.IoLength))
							{
								ret = IPC_EACCES;
								break;
							}
						}
						DCInvalidateRange(ppc_message->Data.Ioctl.InputBuffer, ppc_message->Data.Ioctl.InputLength);
						DCInvalidateRange(ppc_message->Data.Ioctl.IoBuffer, ppc_message->Data.Ioctl.IoLength);
					}

					ret = IoctlFDAsync(filedesc_id,
						ppc_message->Data.Ioctl.Ioctl,
						ppc_message->Data.Ioctl.InputBuffer,
						ppc_message->Data.Ioctl.InputLength,
						ppc_message->Data.Ioctl.IoBuffer,
						ppc_message->Data.Ioctl.IoLength,
						messageQueue, ppc_message);
					break;

				case IOS_IOCTLV:
					{
						const u32 total_arg_count = ppc_message->Data.Ioctl.InputArgc + ppc_message->Data.Ioctl.IoArgc;
						if (total_arg_count != 0)
						{
							if(!CheckPtrIsFine(ppc_message->Data.Ioctlv.Data, total_arg_count * sizeof(IoctlvMessageData)))
							{
								ret = IPC_EACCES;
								break;
							}
						}

						DCInvalidateRange(ppc_message->Data.Ioctlv.Data, total_arg_count * sizeof(IoctlvMessageData));
						u32 i = 0;
						for(; i < total_arg_count; ++i)
						{
							if (ppc_message->Data.Ioctlv.Data[i].Length != 0)
							{
								if(!CheckPtrIsFine(ppc_message->Data.Ioctlv.Data[i].Data, ppc_message->Data.Ioctlv.Data[i].Length))
								{
									break;
								}
							}
							DCInvalidateRange(ppc_message->Data.Ioctlv.Data[i].Data, ppc_message->Data.Ioctlv.Data[i].Length);
						}
						if(i != total_arg_count)
						{
							ret = IPC_EACCES;
							break;
						}
					}

					if(ret == IPC_SUCCESS)
						ret = IoctlvFDAsync(filedesc_id,
							ppc_message->Data.Ioctl.Ioctl,
							ppc_message->Data.Ioctl.InputArgc,
							ppc_message->Data.Ioctl.IoArgc,
							ppc_message->Data.Ioctl.Data,
							messageQueue, ppc_message);
					break;
				}

				if (ret < 0)
				{
					ppc_message->Result = ret;
					DoFlushIpcRequestAfterInvalidate(ppc_message);
				}
			}
		}
	}
}

static void IpcInitMessageQueues(void)
{
	int i;

	ipc_message_array = KMalloc(sizeof(IpcMessage) * (MAX_THREADS + 128));
	fd_path_array = KMalloc(sizeof(FileDescriptorPath) * MAX_THREADS);

	for(i = 0; i < MAX_THREADS; ++i)
	{
		fd_path_ptr_array[i] = &fd_path_array[i];
	}

	for(i = 0; i < MAX_THREADS; ++i)
	{
		MessageQueue *currentQueue = &ipc_message_queue_array[i];
		currentQueue->QueueHeap = &message_queue_ptr_array[i];
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
	write32(HW_IPC_ARMCTRL, ( IPC_ARM_IX1 | IPC_ARM_IX2 ));
	IpcInitMessageQueues();
}

s32 ResourceReply(IpcMessage* message, u32 requestReturnValue)
{
	u32 interrupts = DisableInterrupts();
	s32 ret = 0;


return_resourceReply:
	RestoreInterrupts(interrupts);
	return ret;
}

s32 SendMessageCheckReceive(IpcMessage *message, ResourceManager* resource)
{
	void* const cb = message->Callback;
	message->UsedByProcessId = resource->ProcessId;
	s32 ret = SendMessageToQueue(resource->MessageQueue, message, None);

	if (ret == IPC_SUCCESS && cb == NULL)
	{
		IpcMessage* rcvd_msg = NULL;
		ret = ReceiveMessageFromQueue(&ipc_message_queue_array[message - ipc_message_array], &rcvd_msg, None);
		if(ret == IPC_SUCCESS && rcvd_msg != message)
		{
			ret = IPC_EINVAL;
		}
	}

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
