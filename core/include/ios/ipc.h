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

#define IOS_OPEN			0x01
#define IOS_CLOSE			0x02
#define IOS_READ			0x03
#define IOS_WRITE			0x04
#define IOS_SEEK			0x05
#define IOS_IOCTL			0x06
#define IOS_IOCTLV			0x07
#define IOS_REPLY			0x08

#define IOS_OPEN_NONE		0x00
#define IOS_OPEN_READ		0x01
#define IOS_OPEN_WRITE		0x02
#define IOS_OPEN_RW			(IOS_OPEN_READ|IOS_OPEN_WRITE)

#define RELNCH_RELAUNCH 	0x01
#define RELNCH_BACKGROUND 	0x02

typedef struct 
{
	char *Filepath;
	u32 Mode;
} OpenMessage;
CHECK_SIZE(OpenMessage, 0x08);
CHECK_OFFSET(OpenMessage, 0x00, Filepath);
CHECK_OFFSET(OpenMessage, 0x04, Mode);

typedef struct {
	void *Data;
	u32 Length;
} ReadWriteMessage;
CHECK_SIZE(ReadWriteMessage, 0x08);
CHECK_OFFSET(ReadWriteMessage, 0x00, Data);
CHECK_OFFSET(ReadWriteMessage, 0x04, Length);

typedef struct {
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
	union {
		s32 FileDescriptor;
		u32 RequestCommand;
	};
	union {
		OpenMessage Open;
		ReadWriteMessage Read;
		ReadWriteMessage Write;
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
	u32 Relaunch;
} IpcMessage;
CHECK_SIZE(IpcMessage, 0x2C);
CHECK_OFFSET(IpcMessage, 0x00, Request);
CHECK_OFFSET(IpcMessage, 0x20, Callback);
CHECK_OFFSET(IpcMessage, 0x24, CallerData);
CHECK_OFFSET(IpcMessage, 0x28, Relaunch);

#endif