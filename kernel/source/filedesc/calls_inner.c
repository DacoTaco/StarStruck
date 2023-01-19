/*
	starstruck - a Free Software reimplementation for the Nintendo/BroadOn IOS.
	sha - the sha engine in starlet

	Copyright (C) 2021	DacoTaco

# This code is licensed to you under the terms of the GNU GPL, version 2{

}

# see file COPYING or http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt
*/

#include <string.h>
#include <ios/ipc.h>
#include <ios/processor.h>
#include <ios/errno.h>

#include "calls_inner.h"
#include "core/defines.h"
#include "messaging/ipc.h"
#include "memory/memory.h"

static AllProcessesFileDescriptors_t ProcessFileDescriptors SRAM_BSS;
FileDescriptor AesFileDescriptor SRAM_BSS;
FileDescriptor ShaFileDescriptor SRAM_BSS;

static s32 GetThreadSpecificMsgOrFreeFromExtra(int use_extra_messages_instead_of_thread_specific, IpcMessage **out)
{
	s32 ret = IPC_EMAX;

	const unsigned current_thread_id = IOS_GetThreadId();
	if (use_extra_messages_instead_of_thread_specific) {
		const int thread_open_msg_limit = (CurrentThread == IpcHandlerThread) ? 48 : 32;
		if (thread_msg_usage_arr[current_thread_id] != thread_open_msg_limit)
		{
			for(int i = 0; i < 128; ++i)
			{
				IpcMessage *destination = &ipc_message_array[100 + i];
				if(destination->IsInQueue == 0)
				{
					memset8(&destination->Request, 0, sizeof(destination->Request));
					*out = destination;
					ret = IPC_SUCCESS;

					thread_msg_usage_arr[current_thread_id]++;
					destination->IsInQueue = 1;
					destination->UsedByThreadId = current_thread_id;
					break;
				}
			}
		}
	}
	else {
		IpcMessage *destination = &ipc_message_array[current_thread_id];
		memset8(&destination->Request, 0, sizeof(destination->Request));
		*out = destination;
		ret = IPC_SUCCESS;
	}

	return ret;
}

// strncpy (with 0 padding if the source is shorter than length)
static void strncpy_with_mem1_caveat(FileDescriptorPath* into, const char* source, int length)
{
	// if in mem1, work in u32 sized chunks
	if((u32)into < 0x01800000)
	{
		u32 idx = 0;
		while(length != 0 && *source != '\0')
		{
			u32 value = 0;
			for(int i = 0; i < sizeof(u32) && length != 0 && *source != '\0'; ++i, --length, ++source)
			{
				value |= ((u32)(source[i])) << (24 - 8 * i);
			}
			into->DevicePathUINT[idx] = value;
			++idx;
		}
		for(; idx < ARRAY_LENGTH(into->DevicePathUINT); ++idx)
		{
			into->DevicePathUINT[idx] = 0;
		}
	}
	// otherwise, normal strncpy
	else
	{
		int i = 0;
		for(; i < length && source[i] != '\0'; ++i)
		{
			into->DevicePath[i] = source[i];
		}
		for(; i < length; ++i)
		{
			into->DevicePath[i] = 0;
		}
	}
}

s32 OpenFD_Inner(const char* path, int mode)
{
	const int current_thread_id = IOS_GetThreadId();
	const int current_process_id = CurrentThread == IpcHandlerThread ? 15 : GetProcessID();

	const int pathLength = strnlen(path, MAX_PATHLEN);
	if (pathLength < MAX_PATHLEN)
	{
		int ret = CheckMemoryPointer(path, pathLength, 3, current_process_id, current_process_id);
		if (ret != IPC_SUCCESS)
			return ret;

		strncpy_with_mem1_caveat(&fd_path_array[current_thread_id], path, pathLength + 1);
		ret = IPC_ENOENT;

		for(int i = 0; i < MAX_RESOURCES; ++i)
		{
			ResourceManager* current_resource = &ResourceManagers[i];
			if (current_process_id == 15 && current_resource->PpcHasAccessRights == 0 && current_resource->PathLength != 0 && strncmp(path, current_resource->DevicePath, current_resource->PathLength) == 0)
			{
				return IPC_EACCES;
			}

			if (current_resource->PathLength != 0 && strncmp(path, current_resource->DevicePath, current_resource->PathLength) == 0)
			{
				IpcMessage* message = &ipc_message_array[current_thread_id];
				message->Request.Command = IOS_OPEN;
				message->Request.Data.Open.Filepath = &fd_path_array[current_thread_id].DevicePath;
				message->Request.Data.Open.Mode = mode;
				message->Request.Data.Open.UID = GetUID();
				message->Request.Data.Open.GID = GetGID();
				message->Callback = NULL;
				message->CallerData = 0;

				SendMessageCheckReceive(message, current_resource);
				ret = message->Request.Result;

				if (ret != IPC_ENOENT)
				{
					if (ret < 0)
						return ret;

					for(int fd_id = 0; fd_id < MAX_PROCESS_FDS; ++fd_id)
					{
						FileDescriptor* current_fd = &ProcessFileDescriptors[current_process_id][fd_id];
						if(current_fd->belongs_to_resource == NULL)
						{
							current_fd->id = ret;
							current_fd->belongs_to_resource = current_resource;
							return fd_id;
						}
					}
	
					return IPC_EMAX;
				}
			}
		}

		return ret;
	}
	else
	{
		return IPC_EINVAL;
	}
}

int CloseFD_Inner(s32 fd, MessageQueue* messageQueue, IpcMessage* message)
{
	if (!(0x10000 <= fd && fd < 0x10002))
	{
		if (fd < MAX_PROCESS_FDS)
		{
			if (ProcessFileDescriptors[CurrentThread == IpcHandlerThread ? 15 : GetProcessID()][fd].belongs_to_resource == NULL)
			{
				return IPC_EINVAL;
			}
		}
		else
		{
			return IPC_EINVAL;
		}
	}
	
	IpcMessage* current_message = NULL;
	s32 ret = GetThreadSpecificMsgOrFreeFromExtra(messageQueue != NULL, &current_message);
	IpcMessage* got_msg_copy = current_message;

	if (ret == IPC_SUCCESS)
	{
		FileDescriptor* destination = NULL;
		if (fd == 0x10000) {
			destination = &AesFileDescriptor;
		}
		else if (fd == 0x10001) {
			destination = &ShaFileDescriptor;
		}
		else if (CurrentThread == IpcHandlerThread) {
			destination = &ProcessFileDescriptors[15][fd];
		}
		else {
			destination = &ProcessFileDescriptors[GetProcessID()][fd];
		}

		current_message->Request.Command = IOS_CLOSE;
		current_message->Request.FileDescriptor = destination->id;
		current_message->Callback = messageQueue;
		current_message->CallerData = message;

		ret = SendMessageCheckReceive(current_message, destination->belongs_to_resource);
		if (fd < 0x10000)
		{
			memset8(destination, 0, sizeof(*destination));
		}

		if (messageQueue == NULL && ret == IPC_SUCCESS)
		{
			ret = got_msg_copy->Request.Result;
		}
	}

	if (current_message != NULL && messageQueue != NULL && ret != IPC_SUCCESS)
	{
		current_message->IsInQueue = 0;
		thread_msg_usage_arr[current_message->UsedByThreadId]--;
	}

	return ret;
}

int ReadFD_Inner(s32 fd, void *buf, u32 len, MessageQueue* messageQueue, IpcMessage* message)
{
	if (!(0x10000 <= fd && fd < 0x10002))
	{
		if (fd < MAX_PROCESS_FDS)
		{
			if (ProcessFileDescriptors[CurrentThread == IpcHandlerThread ? 15 : GetProcessID()][fd].belongs_to_resource == NULL)
			{
				return IPC_EINVAL;
			}
		}
		else
		{
			return IPC_EINVAL;
		}
	}
	
	IpcMessage* current_message = NULL;
	s32 ret = GetThreadSpecificMsgOrFreeFromExtra(messageQueue != NULL, &current_message);
	IpcMessage* got_msg_copy = current_message;

	if (ret == IPC_SUCCESS)
	{
		FileDescriptor* fd_ptr = NULL;
		if (fd == 0x10000) {
			fd_ptr = &AesFileDescriptor;
		}
		else if (fd == 0x10001) {
			fd_ptr = &ShaFileDescriptor;
		}
		else if (CurrentThread == IpcHandlerThread) {
			fd_ptr = &ProcessFileDescriptors[15][fd];
		}
		else {
			fd_ptr = &ProcessFileDescriptors[GetProcessID()][fd];
		}

		current_message->Request.Command = IOS_READ;
		current_message->Request.FileDescriptor = fd_ptr->id;
		current_message->Request.Data.Read.Data = buf;
		current_message->Request.Data.Read.Length = len;
		current_message->Callback = messageQueue;
		current_message->CallerData = message;

		ret = CheckMemoryPointer(buf, len, 4, CurrentThread == IpcHandlerThread ? 15 : GetProcessID(), fd_ptr->belongs_to_resource->ProcessId);
		if(ret == IPC_SUCCESS)
			ret = SendMessageCheckReceive(current_message, fd_ptr->belongs_to_resource);

		if (messageQueue == NULL && ret == IPC_SUCCESS)
		{
			ret = got_msg_copy->Request.Result;
		}
	}

	if (current_message != NULL && messageQueue != NULL && ret != IPC_SUCCESS) {
		current_message->IsInQueue = 0;
		thread_msg_usage_arr[current_message->UsedByThreadId]--;
	}

	return ret;
}

int WriteFD_Inner(s32 fd, const void *buf, u32 len, MessageQueue* messageQueue, IpcMessage* message)
{
	if (!(0x10000 <= fd && fd < 0x10002))
	{
		if (fd < MAX_PROCESS_FDS)
		{
			if (ProcessFileDescriptors[CurrentThread == IpcHandlerThread ? 15 : GetProcessID()][fd].belongs_to_resource == NULL)
			{
				return IPC_EINVAL;
			}
		}
		else
		{
			return IPC_EINVAL;
		}
	}
	
	IpcMessage* current_message = NULL;
	s32 ret = GetThreadSpecificMsgOrFreeFromExtra(messageQueue != NULL, &current_message);
	IpcMessage* got_msg_copy = current_message;

	if (ret == IPC_SUCCESS)
	{
		FileDescriptor* fd_ptr = NULL;
		if (fd == 0x10000) {
			fd_ptr = &AesFileDescriptor;
		}
		else if (fd == 0x10001) {
			fd_ptr = &ShaFileDescriptor;
		}
		else if (CurrentThread == IpcHandlerThread) {
			fd_ptr = &ProcessFileDescriptors[15][fd];
		}
		else {
			fd_ptr = &ProcessFileDescriptors[GetProcessID()][fd];
		}

		current_message->Request.Command = IOS_WRITE;
		current_message->Request.FileDescriptor = fd_ptr->id;
		current_message->Request.Data.Write.Data = buf;
		current_message->Request.Data.Write.Length = len;
		current_message->Callback = messageQueue;
		current_message->CallerData = message;

		ret = CheckMemoryPointer(buf, len, 3, CurrentThread == IpcHandlerThread ? 15 : GetProcessID(), fd_ptr->belongs_to_resource->ProcessId);

		if(ret == IPC_SUCCESS)
			ret = SendMessageCheckReceive(current_message, fd_ptr->belongs_to_resource);

		if (messageQueue == NULL && ret == IPC_SUCCESS)
		{
			ret = got_msg_copy->Request.Result;
		}
	}

	if (current_message != NULL && messageQueue != NULL && ret != IPC_SUCCESS) {
		current_message->IsInQueue = 0;
		thread_msg_usage_arr[current_message->UsedByThreadId]--;
	}

	return ret;
}

int SeekFD_Inner(s32 fd, s32 offset, s32 origin, MessageQueue* messageQueue, IpcMessage* message)
{
	if (!(0x10000 <= fd && fd < 0x10002))
	{
		if (fd < MAX_PROCESS_FDS)
		{
			if (ProcessFileDescriptors[CurrentThread == IpcHandlerThread ? 15 : GetProcessID()][fd].belongs_to_resource == NULL)
			{
				return IPC_EINVAL;
			}
		}
		else
		{
			return IPC_EINVAL;
		}
	}
	
	IpcMessage* current_message = NULL;
	s32 ret = GetThreadSpecificMsgOrFreeFromExtra(messageQueue != NULL, &current_message);
	IpcMessage* got_msg_copy = current_message;

	if (ret == IPC_SUCCESS)
	{
		FileDescriptor* destination = NULL;
		if (fd == 0x10000) {
			destination = &AesFileDescriptor;
		}
		else if (fd == 0x10001) {
			destination = &ShaFileDescriptor;
		}
		else if (CurrentThread == IpcHandlerThread) {
			destination = &ProcessFileDescriptors[15][fd];
		}
		else {
			destination = &ProcessFileDescriptors[GetProcessID()][fd];
		}

		current_message->Request.Command = IOS_SEEK;
		current_message->Request.FileDescriptor = destination->id;
		current_message->Request.Data.Seek.Where = offset;
		current_message->Request.Data.Seek.Whence = origin;
		current_message->Callback = messageQueue;
		current_message->CallerData = message;

		ret = SendMessageCheckReceive(current_message, destination->belongs_to_resource);
		if (messageQueue == NULL && ret == IPC_SUCCESS)
		{
			ret = got_msg_copy->Request.Result;
		}
	}

	if (current_message != NULL && messageQueue != NULL && ret != IPC_SUCCESS)
	{
		current_message->IsInQueue = 0;
		thread_msg_usage_arr[current_message->UsedByThreadId]--;
	}

	return ret;
}

int IoctlFD_Inner(s32 fd, u32 request_id, void *input_buffer, u32 input_buffer_len, void *output_buffer, u32 output_buffer_len, MessageQueue* messageQueue, IpcMessage* message)
{
	if (!(0x10000 <= fd && fd < 0x10002))
	{
		if (fd < MAX_PROCESS_FDS)
		{
			if (ProcessFileDescriptors[CurrentThread == IpcHandlerThread ? 15 : GetProcessID()][fd].belongs_to_resource == NULL)
			{
				return IPC_EINVAL;
			}
		}
		else
		{
			return IPC_EINVAL;
		}
	}
	
	IpcMessage* current_message = NULL;
	s32 ret = GetThreadSpecificMsgOrFreeFromExtra(messageQueue != NULL, &current_message);
	IpcMessage* got_msg_copy = current_message;

	if (ret == IPC_SUCCESS)
	{
		FileDescriptor* fd_ptr = NULL;
		if (fd == 0x10000) {
			fd_ptr = &AesFileDescriptor;
		}
		else if (fd == 0x10001) {
			fd_ptr = &ShaFileDescriptor;
		}
		else if (CurrentThread == IpcHandlerThread) {
			fd_ptr = &ProcessFileDescriptors[15][fd];
		}
		else {
			fd_ptr = &ProcessFileDescriptors[GetProcessID()][fd];
		}

		current_message->Request.Command = IOS_IOCTL;
		current_message->Request.FileDescriptor = fd_ptr->id;
		current_message->Request.Data.Ioctl.Ioctl = request_id;
		current_message->Request.Data.Ioctl.InputBuffer = input_buffer;
		current_message->Request.Data.Ioctl.InputLength = input_buffer_len;
		current_message->Request.Data.Ioctl.IoBuffer = output_buffer;
		current_message->Request.Data.Ioctl.IoLength = output_buffer_len;
		current_message->Callback = messageQueue;
		current_message->CallerData = message;

		ret = CheckMemoryPointer(input_buffer, input_buffer_len, 3, CurrentThread == IpcHandlerThread ? 15 : GetProcessID(), fd_ptr->belongs_to_resource->ProcessId);

		if(ret == IPC_SUCCESS)
			ret = CheckMemoryPointer(output_buffer, output_buffer_len, 4, CurrentThread == IpcHandlerThread ? 15 : GetProcessID(), fd_ptr->belongs_to_resource->ProcessId);
	
		if(ret == IPC_SUCCESS)
			ret = SendMessageCheckReceive(current_message, fd_ptr->belongs_to_resource);

		if (messageQueue == NULL && ret == IPC_SUCCESS)
		{
			ret = got_msg_copy->Request.Result;
		}
	}

	if (current_message != NULL && messageQueue != NULL && ret != IPC_SUCCESS) {
		current_message->IsInQueue = 0;
		thread_msg_usage_arr[current_message->UsedByThreadId]--;
	}

	return ret;
}

int IoctlvFD_InnerWithFlag(s32 fd, u32 request_id, u32 vector_count_in, u32 vector_count_out, IoctlvMessageData *vectors, MessageQueue* messageQueue, IpcMessage* message, const int checkBeforeSend)
{
	if (!(0x10000 <= fd && fd < 0x10002))
	{
		if (fd < MAX_PROCESS_FDS)
		{
			if (ProcessFileDescriptors[CurrentThread == IpcHandlerThread ? 15 : GetProcessID()][fd].belongs_to_resource == NULL)
			{
				return IPC_EINVAL;
			}
		}
		else
		{
			return IPC_EINVAL;
		}
	}
	
	IpcMessage* current_message = NULL;
	s32 ret = GetThreadSpecificMsgOrFreeFromExtra(messageQueue != NULL, &current_message);
	IpcMessage* got_msg_copy = current_message;

	if (ret == IPC_SUCCESS)
	{
		FileDescriptor* fd_ptr = NULL;
		if (fd == 0x10000) {
			fd_ptr = &AesFileDescriptor;
		}
		else if (fd == 0x10001) {
			fd_ptr = &ShaFileDescriptor;
		}
		else if (CurrentThread == IpcHandlerThread) {
			fd_ptr = &ProcessFileDescriptors[15][fd];
		}
		else {
			fd_ptr = &ProcessFileDescriptors[GetProcessID()][fd];
		}

		current_message->Request.Command = IOS_IOCTLV;
		current_message->Request.FileDescriptor = fd_ptr->id;
		current_message->Request.Data.Ioctlv.Ioctl = request_id;
		current_message->Request.Data.Ioctlv.InputArgc = vector_count_in;
		current_message->Request.Data.Ioctlv.IoArgc = vector_count_out;
		current_message->Request.Data.Ioctlv.Data = vectors;
		current_message->Callback = messageQueue;
		current_message->CallerData = message;

		if (checkBeforeSend)
		{
			const int pid = CurrentThread == IpcHandlerThread ? 15 : GetProcessID();
			ret = CheckMemoryPointer(vectors, (vector_count_in + vector_count_out) * sizeof(*vectors), 3, pid, fd_ptr->belongs_to_resource->ProcessId);
			for(u32 i = 0; i < vector_count_in && ret == IPC_SUCCESS; ++i)
			{
				ret = CheckMemoryPointer(vectors[i].Data, vectors[i].Length, 3, pid, fd_ptr->belongs_to_resource->ProcessId);
			}
			for(u32 i = 0, j = vector_count_in; i < vector_count_out && ret == IPC_SUCCESS; ++i, ++j)
			{
				ret = CheckMemoryPointer(vectors[j].Data, vectors[j].Length, 4, pid, fd_ptr->belongs_to_resource->ProcessId);
			}
		}

		if(ret == IPC_SUCCESS)
			ret = SendMessageCheckReceive(current_message, fd_ptr->belongs_to_resource);
		
		if (messageQueue == NULL && ret == IPC_SUCCESS) {
			ret = got_msg_copy->Request.Result;
		}
}

	if (current_message != NULL && messageQueue != NULL && ret != IPC_SUCCESS) {
		current_message->IsInQueue = 0;
		thread_msg_usage_arr[current_message->UsedByThreadId]--;
	}

	return ret;
}

int IoctlvFD_Inner(s32 fd, u32 request_id, u32 vector_count_in, u32 vector_count_out, IoctlvMessageData *vectors, MessageQueue* messageQueue, IpcMessage* message)
{
	return IoctlvFD_InnerWithFlag(fd, request_id, vector_count_in, vector_count_out, vectors, messageQueue, message, 1);
}
