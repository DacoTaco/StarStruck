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

#ifndef MIOS
FileDescriptor AesFileDescriptor SRAM_BSS;
FileDescriptor ShaFileDescriptor SRAM_BSS;
#endif

static AllProcessesFileDescriptors_t ProcessFileDescriptors SRAM_BSS;

static s32 GetThreadSpecificMsgOrFreeFromExtra(const int useMsgFromExtraInsteadOfThread, IpcMessage **out)
{
	const s32 currentThreadId = GetThreadID();
	if (!useMsgFromExtraInsteadOfThread)
	{
		IpcMessage *destination = &IpcMessageArray[currentThreadId];
		memset(&destination->Request, 0, sizeof(destination->Request));
		*out = destination;
		return IPC_SUCCESS;
	}

	const u32 thread_open_msg_limit = (CurrentThread == IpcHandlerThread) ? 48 : 32;
	if (ThreadMessageUsageArray[currentThreadId] == thread_open_msg_limit)
		return IPC_EMAX;

	for (int i = 0; i < IPC_EXTRA_MESSAGES; ++i)
	{
		IpcMessage *destination = &IpcMessageArray[MAX_THREADS + i];
		if (destination->IsInQueue)
			continue;

		memset(&destination->Request, 0, sizeof(destination->Request));
		*out = destination;

		ThreadMessageUsageArray[currentThreadId]++;
		destination->IsInQueue = 1;
		destination->UsedByThreadId = currentThreadId;
		return IPC_SUCCESS;
	}
	
	return IPC_EMAX;
}

static bool IsIdValidForProcess(const s32 id)
{
#ifdef MIOS
	if (id >= MAX_PROCESS_FDS || ProcessFileDescriptors[GetProcessID()][id].BelongsToResource == NULL)
		return false;
#else
	if (!(0x10000 <= id && id < 0x10002))
	{
		if (id >= MAX_PROCESS_FDS || ProcessFileDescriptors[CurrentThread == IpcHandlerThread ? 15 : GetProcessID()][id].BelongsToResource == NULL)
			return false;
	}
#endif

	return true;
}

static FileDescriptor* GetProcessFd(const s32 id)
{
#ifdef MIOS
	return &ProcessFileDescriptors[GetProcessID()][id];
#else
	if (id == AES_STATIC_FILEDESC) {
		return &AesFileDescriptor;
	}
	else if (id == SHA_STATIC_FILEDESC) {
		return &ShaFileDescriptor;
	}
	else if (CurrentThread == IpcHandlerThread) {
		return &ProcessFileDescriptors[15][id];
	}
	else {
		return &ProcessFileDescriptors[GetProcessID()][id];
	}
#endif
}

s32 OpenFD_Inner(const char* path, AccessMode mode)
{
	const s32 currentThreadId = GetThreadID();
	const u32 currentProcessId = CurrentThread == IpcHandlerThread ? 15 : GetProcessID();

	const u32 pathLength = strnlen(path, MAX_PATHLEN);
	if (pathLength >= MAX_PATHLEN)
		return IPC_EINVAL;

#ifdef MIOS
	s32 ret = IPC_SUCCESS;
#else
	s32 ret = CheckMemoryPointer(path, pathLength, 3, currentProcessId, currentProcessId);
	if (ret != IPC_SUCCESS)
		return ret;
#endif

	strncpy(FiledescPathArray[currentThreadId].DevicePath, path, pathLength + 1);

	for (int i = 0; i < MAX_RESOURCES; ++i)
	{
		ResourceManager* current_resource = &ResourceManagers[i];
		if (currentProcessId == 15 && current_resource->PpcHasAccessRights == 0 && current_resource->PathLength != 0 && strncmp(path, current_resource->DevicePath, current_resource->PathLength) == 0)
			return IPC_EACCES;

		if (current_resource->PathLength == 0 || strncmp(path, current_resource->DevicePath, current_resource->PathLength) != 0)
			continue;

		IpcMessage* message = &IpcMessageArray[currentThreadId];
		message->Request.Command = IOS_OPEN;
		message->Request.Data.Open.Filepath = FiledescPathArray[currentThreadId].DevicePath;
		message->Request.Data.Open.Mode = mode;
		message->Request.Data.Open.UID = GetUID();
		message->Request.Data.Open.GID = GetGID();
		message->Callback = NULL;
		message->CallerData = 0;

		SendMessageCheckReceive(message, current_resource);
		ret = message->Request.Result;

		if (ret == IPC_ENOENT)
			continue;

		if (ret < 0)
			return ret;

		for (int fd_id = 0; fd_id < MAX_PROCESS_FDS; ++fd_id)
		{
			FileDescriptor* current_fd = &ProcessFileDescriptors[currentProcessId][fd_id];
			if (current_fd->BelongsToResource == NULL)
			{
				current_fd->Id = ret;
				current_fd->BelongsToResource = current_resource;
				return fd_id;
			}
		}

		return IPC_EMAX;
	}

	return IPC_ENOENT;
}

int CloseFD_Inner(s32 fd, MessageQueue* messageQueue, IpcMessage* message)
{
	if (!IsIdValidForProcess(fd))
		return IPC_EINVAL;

	IpcMessage* currentMessage = NULL;
	s32 ret = GetThreadSpecificMsgOrFreeFromExtra(messageQueue != NULL, &currentMessage);
	IpcMessage* gotMessageCopy = currentMessage;

	if (ret != IPC_SUCCESS)
		goto finish;

	FileDescriptor* fd_ptr = GetProcessFd(fd);

	currentMessage->Request.Command = IOS_CLOSE;
	currentMessage->Request.FileDescriptor = fd_ptr->Id;
	currentMessage->Callback = messageQueue;
	currentMessage->CallerData = (u32)message;

	ret = SendMessageCheckReceive(currentMessage, fd_ptr->BelongsToResource);
	if (fd < 0x10000)
		memset(fd_ptr, 0, sizeof(*fd_ptr));

	if (messageQueue == NULL && ret == IPC_SUCCESS)
		ret = gotMessageCopy->Request.Result;

finish:
	if (currentMessage != NULL && messageQueue != NULL && ret != IPC_SUCCESS)
	{
		currentMessage->IsInQueue = 0;
		ThreadMessageUsageArray[currentMessage->UsedByThreadId]--;
	}

	return ret;
}

int ReadFD_Inner(s32 fd, void *buf, u32 len, MessageQueue* messageQueue, IpcMessage* message)
{
	if (!IsIdValidForProcess(fd))
		return IPC_EINVAL;

	IpcMessage* currentMessage = NULL;
	s32 ret = GetThreadSpecificMsgOrFreeFromExtra(messageQueue != NULL, &currentMessage);
	IpcMessage* gotMessageCopy = currentMessage;

	if (ret != IPC_SUCCESS)
		goto finish;

	FileDescriptor* fd_ptr = GetProcessFd(fd);

	currentMessage->Request.Command = IOS_READ;
	currentMessage->Request.FileDescriptor = fd_ptr->Id;
	currentMessage->Request.Data.Read.Data = buf;
	currentMessage->Request.Data.Read.Length = len;
	currentMessage->Callback = messageQueue;
	currentMessage->CallerData = (u32)message;

#ifndef MIOS
	ret = CheckMemoryPointer(buf, len, 4, CurrentThread == IpcHandlerThread ? 15 : GetProcessID(), fd_ptr->BelongsToResource->ProcessId);
	if (ret == IPC_SUCCESS)
		ret = SendMessageCheckReceive(currentMessage, fd_ptr->BelongsToResource);
#endif

	if (messageQueue == NULL && ret == IPC_SUCCESS)
		ret = gotMessageCopy->Request.Result;

finish:
	if (currentMessage != NULL && messageQueue != NULL && ret != IPC_SUCCESS)
	{
		currentMessage->IsInQueue = 0;
		ThreadMessageUsageArray[currentMessage->UsedByThreadId]--;
	}

	return ret;
}

int WriteFD_Inner(s32 fd, const void *buf, u32 len, MessageQueue* messageQueue, IpcMessage* message)
{
	if (!IsIdValidForProcess(fd))
		return IPC_EINVAL;

	IpcMessage* currentMessage = NULL;
	s32 ret = GetThreadSpecificMsgOrFreeFromExtra(messageQueue != NULL, &currentMessage);
	IpcMessage* gotMessageCopy = currentMessage;

	if (ret != IPC_SUCCESS)
		goto finish;

	FileDescriptor* fd_ptr = GetProcessFd(fd);

	currentMessage->Request.Command = IOS_WRITE;
	currentMessage->Request.FileDescriptor = fd_ptr->Id;
	currentMessage->Request.Data.Write.Data = buf;
	currentMessage->Request.Data.Write.Length = len;
	currentMessage->Callback = messageQueue;
	currentMessage->CallerData = (u32)message;

#ifndef MIOS
	ret = CheckMemoryPointer(buf, len, 3, CurrentThread == IpcHandlerThread ? 15 : GetProcessID(), fd_ptr->BelongsToResource->ProcessId);
	if (ret == IPC_SUCCESS)
		ret = SendMessageCheckReceive(currentMessage, fd_ptr->BelongsToResource);
#endif

	if (messageQueue == NULL && ret == IPC_SUCCESS)
		ret = gotMessageCopy->Request.Result;

finish:
	if (currentMessage != NULL && messageQueue != NULL && ret != IPC_SUCCESS)
	{
		currentMessage->IsInQueue = 0;
		ThreadMessageUsageArray[currentMessage->UsedByThreadId]--;
	}

	return ret;
}

int SeekFD_Inner(s32 fd, s32 offset, s32 origin, MessageQueue* messageQueue, IpcMessage* message)
{
	if (!IsIdValidForProcess(fd))
		return IPC_EINVAL;

	IpcMessage* currentMessage = NULL;
	s32 ret = GetThreadSpecificMsgOrFreeFromExtra(messageQueue != NULL, &currentMessage);
	IpcMessage* gotMessageCopy = currentMessage;

	if (ret != IPC_SUCCESS)
		goto finish;

	FileDescriptor* fd_ptr = GetProcessFd(fd);

	currentMessage->Request.Command = IOS_SEEK;
	currentMessage->Request.FileDescriptor = fd_ptr->Id;
	currentMessage->Request.Data.Seek.Where = offset;
	currentMessage->Request.Data.Seek.Whence = origin;
	currentMessage->Callback = messageQueue;
	currentMessage->CallerData = (u32)message;

	ret = SendMessageCheckReceive(currentMessage, fd_ptr->BelongsToResource);
	if (messageQueue == NULL && ret == IPC_SUCCESS)
		ret = gotMessageCopy->Request.Result;

finish:
	if (currentMessage != NULL && messageQueue != NULL && ret != IPC_SUCCESS)
	{
		currentMessage->IsInQueue = 0;
		ThreadMessageUsageArray[currentMessage->UsedByThreadId]--;
	}

	return ret;
}

int IoctlFD_Inner(s32 fd, u32 requestId, void *inputBuffer, u32 inputBufferLength, void *outputBuffer, u32 outputBufferLength, MessageQueue* messageQueue, IpcMessage* message)
{
	if (!IsIdValidForProcess(fd))
		return IPC_EINVAL;

	IpcMessage* currentMessage = NULL;
	s32 ret = GetThreadSpecificMsgOrFreeFromExtra(messageQueue != NULL, &currentMessage);
	IpcMessage* gotMessageCopy = currentMessage;

	if (ret != IPC_SUCCESS)
		goto finish;

	FileDescriptor* fd_ptr = GetProcessFd(fd);

	currentMessage->Request.Command = IOS_IOCTL;
	currentMessage->Request.FileDescriptor = fd_ptr->Id;
	currentMessage->Request.Data.Ioctl.Ioctl = requestId;
	currentMessage->Request.Data.Ioctl.InputBuffer = inputBuffer;
	currentMessage->Request.Data.Ioctl.InputLength = inputBufferLength;
	currentMessage->Request.Data.Ioctl.IoBuffer = outputBuffer;
	currentMessage->Request.Data.Ioctl.IoLength = outputBufferLength;
	currentMessage->Callback = messageQueue;
	currentMessage->CallerData = (u32)message;

#ifndef MIOS
	ret = CheckMemoryPointer(inputBuffer, inputBufferLength, 3, CurrentThread == IpcHandlerThread ? 15 : GetProcessID(), fd_ptr->BelongsToResource->ProcessId);
	if (ret == IPC_SUCCESS)
		ret = CheckMemoryPointer(outputBuffer, outputBufferLength, 4, CurrentThread == IpcHandlerThread ? 15 : GetProcessID(), fd_ptr->BelongsToResource->ProcessId);
#endif

	if (ret == IPC_SUCCESS)
		ret = SendMessageCheckReceive(currentMessage, fd_ptr->BelongsToResource);

	if (messageQueue == NULL && ret == IPC_SUCCESS)
		ret = gotMessageCopy->Request.Result;

finish:
	if (currentMessage != NULL && messageQueue != NULL && ret != IPC_SUCCESS)
	{
		currentMessage->IsInQueue = 0;
		ThreadMessageUsageArray[currentMessage->UsedByThreadId]--;
	}

	return ret;
}

int IoctlvFD_InnerWithFlag(s32 fd, u32 requestId, u32 vectorInputCount, u32 vectorIOCount, IoctlvMessageData *vectors, MessageQueue* messageQueue, IpcMessage* message, const int checkBeforeSend)
{
	if (!IsIdValidForProcess(fd))
		return IPC_EINVAL;

	IpcMessage* currentMessage = NULL;
	s32 ret = GetThreadSpecificMsgOrFreeFromExtra(messageQueue != NULL, &currentMessage);
	IpcMessage* gotMessageCopy = currentMessage;

	if (ret != IPC_SUCCESS)
		goto finish;

	FileDescriptor* fd_ptr = GetProcessFd(fd);

	currentMessage->Request.Command = IOS_IOCTLV;
	currentMessage->Request.FileDescriptor = fd_ptr->Id;
	currentMessage->Request.Data.Ioctlv.Ioctl = requestId;
	currentMessage->Request.Data.Ioctlv.InputArgc = vectorInputCount;
	currentMessage->Request.Data.Ioctlv.IoArgc = vectorIOCount;
	currentMessage->Request.Data.Ioctlv.Data = vectors;
	currentMessage->Callback = messageQueue;
	currentMessage->CallerData = (u32)message;

#ifndef MIOS
	if (checkBeforeSend)
	{
		const u32 pid = CurrentThread == IpcHandlerThread ? 15 : GetProcessID();
		ret = CheckMemoryPointer(vectors, (vectorInputCount + vectorIOCount) * sizeof(*vectors), 3, pid, fd_ptr->BelongsToResource->ProcessId);
		for (u32 i = 0; i < vectorInputCount && ret == IPC_SUCCESS; ++i)
		{
			ret = CheckMemoryPointer(vectors[i].Data, vectors[i].Length, 3, pid, fd_ptr->BelongsToResource->ProcessId);
		}
		for (u32 i = 0, j = vectorInputCount; i < vectorIOCount && ret == IPC_SUCCESS; ++i, ++j)
		{
			ret = CheckMemoryPointer(vectors[j].Data, vectors[j].Length, 4, pid, fd_ptr->BelongsToResource->ProcessId);
		}
	}
#endif

	if (ret == IPC_SUCCESS)
		ret = SendMessageCheckReceive(currentMessage, fd_ptr->BelongsToResource);
	
	if (messageQueue == NULL && ret == IPC_SUCCESS)
		ret = gotMessageCopy->Request.Result;

finish:
	if (currentMessage != NULL && messageQueue != NULL && ret != IPC_SUCCESS)
	{
		currentMessage->IsInQueue = 0;
		ThreadMessageUsageArray[currentMessage->UsedByThreadId]--;
	}

	return ret;
}

int IoctlvFD_Inner(s32 fd, u32 requestId, u32 vectorInputCount, u32 vectorIOCount, IoctlvMessageData *vectors, MessageQueue* messageQueue, IpcMessage* message)
{
	return IoctlvFD_InnerWithFlag(fd, requestId, vectorInputCount, vectorIOCount, vectors, messageQueue, message, RegisteredEventHandler);
}
