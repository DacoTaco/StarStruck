/*
	starstruck - a Free Software reimplementation for the Nintendo/BroadOn IOS.
	ios module template

Copyright (C) 2021	DacoTaco

# This code is licensed to you under the terms of the GNU GPL, version 2;
# see file COPYING or http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt
*/

#ifndef __IOS_MODULE_H__
#define __IOS_MODULE_H__

#include "types.h"

#define IOS_OPEN      0x01
#define IOS_CLOSE     0x02
#define IOS_READ      0x03
#define IOS_WRITE     0x04
#define IOS_SEEK      0x05
#define IOS_IOCTL     0x06
#define IOS_IOCTLV    0x07
#define IOS_REPLY     0x08
#define IOS_INTERRUPT 0x09

typedef enum
{
	NoAccess = 0x00,
	Read = 0x01,
	Write = 0x02,
	ReadWrite = 0x03,
	AccessModeSize = 0xFFFFFFFF
} AccessMode;
CHECK_SIZE(AccessMode, 4);

#define RELNCH_RELAUNCH   0x01
#define RELNCH_BACKGROUND 0x02

typedef struct
{
	char *Filepath;
	AccessMode Mode;
	u32 UID;
	u16 GID;
} OpenMessage;
CHECK_SIZE(OpenMessage, 0x10);
CHECK_OFFSET(OpenMessage, 0x00, Filepath);
CHECK_OFFSET(OpenMessage, 0x04, Mode);
CHECK_OFFSET(OpenMessage, 0x08, UID);
CHECK_OFFSET(OpenMessage, 0x0C, GID);

typedef struct
{
	void *Data;
	u32 Length;
} ReadMessage;
CHECK_SIZE(ReadMessage, 0x08);
CHECK_OFFSET(ReadMessage, 0x00, Data);
CHECK_OFFSET(ReadMessage, 0x04, Length);

typedef struct
{
	const void *Data;
	u32 Length;
} WriteMessage;
CHECK_SIZE(WriteMessage, 0x08);
CHECK_OFFSET(WriteMessage, 0x00, Data);
CHECK_OFFSET(WriteMessage, 0x04, Length);

typedef struct
{
	s32 Where;
	s32 Whence;
} SeekMessage;
CHECK_SIZE(SeekMessage, 0x08);
CHECK_OFFSET(SeekMessage, 0x00, Where);
CHECK_OFFSET(SeekMessage, 0x04, Whence);

typedef struct
{
	u32 Ioctl;
	void *InputBuffer;
	u32 InputLength;
	void *IoBuffer;
	u32 IoLength;
} IoctlMessage;
CHECK_OFFSET(IoctlMessage, 0x00, Ioctl);
CHECK_OFFSET(IoctlMessage, 0x04, InputBuffer);
CHECK_OFFSET(IoctlMessage, 0x08, InputLength);
CHECK_OFFSET(IoctlMessage, 0x0C, IoBuffer);
CHECK_OFFSET(IoctlMessage, 0x10, IoLength);
CHECK_SIZE(IoctlMessage, 0x14);

typedef struct
{
	u32 *Data;
	u32 Length;
} IoctlvMessageData;
CHECK_OFFSET(IoctlvMessageData, 0x00, Data);
CHECK_OFFSET(IoctlvMessageData, 0x04, Length);
CHECK_SIZE(IoctlvMessageData, 0x08);

typedef struct
{
	u32 Ioctl;
	u32 InputArgc;
	u32 IoArgc;
	IoctlvMessageData *Data;
} IoctlvMessage;
CHECK_OFFSET(IoctlvMessage, 0x00, Ioctl);
CHECK_OFFSET(IoctlvMessage, 0x04, InputArgc);
CHECK_OFFSET(IoctlvMessage, 0x08, IoArgc);
CHECK_OFFSET(IoctlvMessage, 0x0C, Data);
CHECK_SIZE(IoctlvMessage, 0x10);

typedef struct
{
	u32 Command;
	s32 Result;
	union
	{
		s32 FileDescriptor;
		u32 RequestCommand;
	};
	union
	{
		OpenMessage Open;
		ReadMessage Read;
		WriteMessage Write;
		SeekMessage Seek;
		IoctlMessage Ioctl;
		IoctlvMessage Ioctlv;
		u32 Arguments[5];
	} Data;
} IpcRequest;
CHECK_SIZE(IpcRequest, 0x20);
CHECK_OFFSET(IpcRequest, 0x00, Command);
CHECK_OFFSET(IpcRequest, 0x04, Result);
CHECK_OFFSET(IpcRequest, 0x08, FileDescriptor);
CHECK_OFFSET(IpcRequest, 0x08, RequestCommand);
//all message types and their data
CHECK_OFFSET(IpcRequest, 0x0C, Data);

typedef struct
{
	IpcRequest Request;
	void *Callback;
	u32 CallerData;
	s32 UsedByThreadId;
	u32 IsInQueue;
	u32 UsedByProcessId;
} IpcMessage;
CHECK_SIZE(IpcMessage, 0x34);
CHECK_OFFSET(IpcMessage, 0x00, Request);
CHECK_OFFSET(IpcMessage, 0x20, Callback);
CHECK_OFFSET(IpcMessage, 0x24, CallerData);
CHECK_OFFSET(IpcMessage, 0x28, UsedByThreadId);
CHECK_OFFSET(IpcMessage, 0x2C, IsInQueue);
CHECK_OFFSET(IpcMessage, 0x30, UsedByProcessId);

#endif
