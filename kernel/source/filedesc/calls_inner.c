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

static s32 GetThreadSpecificMsgOrFreeFromExtra(const int useMsgFromExtraInsteadOfThread, IpcMessage **out)
{
	const unsigned currentThreadId = GetThreadID();
	if (!useMsgFromExtraInsteadOfThread)
	{
		IpcMessage *destination = &IpcMessageArray[currentThreadId];
		memset8(&destination->Request, 0, sizeof(destination->Request));
		*out = destination;
		return IPC_SUCCESS;
	}

	const u32 thread_open_msg_limit = (CurrentThread == IpcHandlerThread) ? 48 : 32;
	if (ThreadMessageUsageArray[currentThreadId] == thread_open_msg_limit)
		return IPC_EMAX;

	for(int i = 0; i < 128; ++i)
	{
		IpcMessage *destination = &IpcMessageArray[100 + i];
		if(destination->IsInQueue)
			continue;

		memset8(&destination->Request, 0, sizeof(destination->Request));
		*out = destination;

		ThreadMessageUsageArray[currentThreadId]++;
		destination->IsInQueue = 1;
		destination->UsedByThreadId = currentThreadId;
		return IPC_SUCCESS;
	}
	
	return IPC_EMAX;
}

static void SpecialStrncpyForMEM1(FileDescriptorPath* into, const char* source, int length)
{
	u32 idx = 0;
	while(length != 0 && *source != '\0')
	{
		u32 value = 0;
		for(u32 i = 0; i < sizeof(u32) && length != 0 && *source != '\0'; ++i, --length, ++source)
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

static void SpecialStrncpyRegular(FileDescriptorPath* into, const char* source, int length)
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
// strncpy (with 0 padding if the source is shorter than length)
static void SpecialStrncpy(FileDescriptorPath* into, const char* source, int length)
{
	// if in mem1, work in u32 sized chunks
	if((u32)into < 0x01800000)
	{
		SpecialStrncpyForMEM1(into, source, length);
	}
	// otherwise, normal strncpy
	else
	{
		SpecialStrncpyRegular(into, source, length);
	}
}

s32 OpenFD_Inner(const char* path, int mode)
{
	const int currentThreadId = GetThreadID();
	const int currentProcessId = CurrentThread == IpcHandlerThread ? 15 : GetProcessID();

	const int pathLength = strnlen(path, MAX_PATHLEN);
	if (pathLength >= MAX_PATHLEN)
		return IPC_EINVAL;

	int ret = CheckMemoryPointer(path, pathLength, 3, currentProcessId, currentProcessId);
	if (ret != IPC_SUCCESS)
		return ret;

	SpecialStrncpy(&FiledescPathArray[currentThreadId], path, pathLength + 1);

	for(int i = 0; i < MAX_RESOURCES; ++i)
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

		if(ret == IPC_ENOENT)
			continue;

		if (ret < 0)
			return ret;

		for(int fd_id = 0; fd_id < MAX_PROCESS_FDS; ++fd_id)
		{
			FileDescriptor* current_fd = &ProcessFileDescriptors[currentProcessId][fd_id];
			if(current_fd->BelongsToResource == NULL)
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
	if (!(0x10000 <= fd && fd < 0x10002))
	{
		if (fd >= MAX_PROCESS_FDS || ProcessFileDescriptors[CurrentThread == IpcHandlerThread ? 15 : GetProcessID()][fd].BelongsToResource == NULL)
			return IPC_EINVAL;
	}
	
	IpcMessage* currentMessage = NULL;
	s32 ret = GetThreadSpecificMsgOrFreeFromExtra(messageQueue != NULL, &currentMessage);
	IpcMessage* gotMessageCopy = currentMessage;

	if (ret != IPC_SUCCESS)
		goto finish;

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

	currentMessage->Request.Command = IOS_CLOSE;
	currentMessage->Request.FileDescriptor = destination->Id;
	currentMessage->Callback = messageQueue;
	currentMessage->CallerData = (u32)message;

	ret = SendMessageCheckReceive(currentMessage, destination->BelongsToResource);
	if (fd < 0x10000)
		memset8(destination, 0, sizeof(*destination));

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
	if (!(0x10000 <= fd && fd < 0x10002))
	{
		if (fd >= MAX_PROCESS_FDS || ProcessFileDescriptors[CurrentThread == IpcHandlerThread ? 15 : GetProcessID()][fd].BelongsToResource == NULL)
			return IPC_EINVAL;
	}
	
	IpcMessage* currentMessage = NULL;
	s32 ret = GetThreadSpecificMsgOrFreeFromExtra(messageQueue != NULL, &currentMessage);
	IpcMessage* gotMessageCopy = currentMessage;

	if (ret != IPC_SUCCESS)
		goto finish;

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

	currentMessage->Request.Command = IOS_READ;
	currentMessage->Request.FileDescriptor = fd_ptr->Id;
	currentMessage->Request.Data.Read.Data = buf;
	currentMessage->Request.Data.Read.Length = len;
	currentMessage->Callback = messageQueue;
	currentMessage->CallerData = (u32)message;

	ret = CheckMemoryPointer(buf, len, 4, CurrentThread == IpcHandlerThread ? 15 : GetProcessID(), fd_ptr->BelongsToResource->ProcessId);
	if(ret == IPC_SUCCESS)
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

int WriteFD_Inner(s32 fd, const void *buf, u32 len, MessageQueue* messageQueue, IpcMessage* message)
{
	if (!(0x10000 <= fd && fd < 0x10002))
	{
		if (fd >= MAX_PROCESS_FDS || ProcessFileDescriptors[CurrentThread == IpcHandlerThread ? 15 : GetProcessID()][fd].BelongsToResource == NULL)
			return IPC_EINVAL;
	}
	
	IpcMessage* currentMessage = NULL;
	s32 ret = GetThreadSpecificMsgOrFreeFromExtra(messageQueue != NULL, &currentMessage);
	IpcMessage* gotMessageCopy = currentMessage;

	if (ret != IPC_SUCCESS)
		goto finish;

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

	currentMessage->Request.Command = IOS_WRITE;
	currentMessage->Request.FileDescriptor = fd_ptr->Id;
	currentMessage->Request.Data.Write.Data = buf;
	currentMessage->Request.Data.Write.Length = len;
	currentMessage->Callback = messageQueue;
	currentMessage->CallerData = (u32)message;

	ret = CheckMemoryPointer(buf, len, 3, CurrentThread == IpcHandlerThread ? 15 : GetProcessID(), fd_ptr->BelongsToResource->ProcessId);

	if(ret == IPC_SUCCESS)
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

int SeekFD_Inner(s32 fd, s32 offset, s32 origin, MessageQueue* messageQueue, IpcMessage* message)
{
	if (!(0x10000 <= fd && fd < 0x10002))
	{
		if (fd >= MAX_PROCESS_FDS || ProcessFileDescriptors[CurrentThread == IpcHandlerThread ? 15 : GetProcessID()][fd].BelongsToResource == NULL)
			return IPC_EINVAL;
	}
	
	IpcMessage* currentMessage = NULL;
	s32 ret = GetThreadSpecificMsgOrFreeFromExtra(messageQueue != NULL, &currentMessage);
	IpcMessage* gotMessageCopy = currentMessage;

	if (ret != IPC_SUCCESS)
		goto finish;

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

	currentMessage->Request.Command = IOS_SEEK;
	currentMessage->Request.FileDescriptor = destination->Id;
	currentMessage->Request.Data.Seek.Where = offset;
	currentMessage->Request.Data.Seek.Whence = origin;
	currentMessage->Callback = messageQueue;
	currentMessage->CallerData = (u32)message;

	ret = SendMessageCheckReceive(currentMessage, destination->BelongsToResource);
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
	if (!(0x10000 <= fd && fd < 0x10002))
	{
		if (fd >= MAX_PROCESS_FDS || ProcessFileDescriptors[CurrentThread == IpcHandlerThread ? 15 : GetProcessID()][fd].BelongsToResource == NULL)
			return IPC_EINVAL;
	}
	
	IpcMessage* currentMessage = NULL;
	s32 ret = GetThreadSpecificMsgOrFreeFromExtra(messageQueue != NULL, &currentMessage);
	IpcMessage* gotMessageCopy = currentMessage;

	if (ret != IPC_SUCCESS)
		goto finish;

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

	currentMessage->Request.Command = IOS_IOCTL;
	currentMessage->Request.FileDescriptor = fd_ptr->Id;
	currentMessage->Request.Data.Ioctl.Ioctl = requestId;
	currentMessage->Request.Data.Ioctl.InputBuffer = inputBuffer;
	currentMessage->Request.Data.Ioctl.InputLength = inputBufferLength;
	currentMessage->Request.Data.Ioctl.IoBuffer = outputBuffer;
	currentMessage->Request.Data.Ioctl.IoLength = outputBufferLength;
	currentMessage->Callback = messageQueue;
	currentMessage->CallerData = (u32)message;

	ret = CheckMemoryPointer(inputBuffer, inputBufferLength, 3, CurrentThread == IpcHandlerThread ? 15 : GetProcessID(), fd_ptr->BelongsToResource->ProcessId);

	if(ret == IPC_SUCCESS)
		ret = CheckMemoryPointer(outputBuffer, outputBufferLength, 4, CurrentThread == IpcHandlerThread ? 15 : GetProcessID(), fd_ptr->BelongsToResource->ProcessId);

	if(ret == IPC_SUCCESS)
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
	if (!(0x10000 <= fd && fd < 0x10002))
	{
		if (fd >= MAX_PROCESS_FDS || ProcessFileDescriptors[CurrentThread == IpcHandlerThread ? 15 : GetProcessID()][fd].BelongsToResource == NULL)
			return IPC_EINVAL;
	}
	
	IpcMessage* currentMessage = NULL;
	s32 ret = GetThreadSpecificMsgOrFreeFromExtra(messageQueue != NULL, &currentMessage);
	IpcMessage* gotMessageCopy = currentMessage;

	if (ret != IPC_SUCCESS)
		goto finish;

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

	currentMessage->Request.Command = IOS_IOCTLV;
	currentMessage->Request.FileDescriptor = fd_ptr->Id;
	currentMessage->Request.Data.Ioctlv.Ioctl = requestId;
	currentMessage->Request.Data.Ioctlv.InputArgc = vectorInputCount;
	currentMessage->Request.Data.Ioctlv.IoArgc = vectorIOCount;
	currentMessage->Request.Data.Ioctlv.Data = vectors;
	currentMessage->Callback = messageQueue;
	currentMessage->CallerData = (u32)message;

	if (checkBeforeSend)
	{
		const int pid = CurrentThread == IpcHandlerThread ? 15 : GetProcessID();
		ret = CheckMemoryPointer(vectors, (vectorInputCount + vectorIOCount) * sizeof(*vectors), 3, pid, fd_ptr->BelongsToResource->ProcessId);
		for(u32 i = 0; i < vectorInputCount && ret == IPC_SUCCESS; ++i)
		{
			ret = CheckMemoryPointer(vectors[i].Data, vectors[i].Length, 3, pid, fd_ptr->BelongsToResource->ProcessId);
		}
		for(u32 i = 0, j = vectorInputCount; i < vectorIOCount && ret == IPC_SUCCESS; ++i, ++j)
		{
			ret = CheckMemoryPointer(vectors[j].Data, vectors[j].Length, 4, pid, fd_ptr->BelongsToResource->ProcessId);
		}
	}

	if(ret == IPC_SUCCESS)
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
	return IoctlvFD_InnerWithFlag(fd, requestId, vectorInputCount, vectorIOCount, vectors, messageQueue, message, 1);
}
