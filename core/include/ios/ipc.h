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
	char *filepath;
	u32 mode;
} OpenMessage;
CHECK_SIZE(OpenMessage, 0x08);
CHECK_OFFSET(OpenMessage, 0x00, filepath);
CHECK_OFFSET(OpenMessage, 0x04, mode);

typedef struct {
	void *data;
	u32 length;
} ReadWriteMessage;
CHECK_SIZE(ReadWriteMessage, 0x08);
CHECK_OFFSET(ReadWriteMessage, 0x00, data);
CHECK_OFFSET(ReadWriteMessage, 0x04, length);

typedef struct {
	s32 where;
	s32 whence;
} SeekMessage;
CHECK_SIZE(SeekMessage, 0x08);
CHECK_OFFSET(SeekMessage, 0x00, where);
CHECK_OFFSET(SeekMessage, 0x04, whence);

typedef struct
{
	u32 ioctl;
	void *inputBuffer;
	u32 inputLength;
	void *ioBuffer;
	u32 ioLength;
} IoctlMessage;
CHECK_OFFSET(IoctlMessage, 0x00, ioctl);
CHECK_OFFSET(IoctlMessage, 0x04, inputBuffer);
CHECK_OFFSET(IoctlMessage, 0x08, inputLength);
CHECK_OFFSET(IoctlMessage, 0x0C, ioBuffer);
CHECK_OFFSET(IoctlMessage, 0x10, ioLength);
CHECK_SIZE(IoctlMessage, 0x14);

typedef struct
{
	void *data;
	u32 length;
} IoctlvMessageData;
CHECK_OFFSET(IoctlvMessageData, 0x00, data);
CHECK_OFFSET(IoctlvMessageData, 0x04, length);
CHECK_SIZE(IoctlvMessageData, 0x08);

typedef struct
{
	u32 ioctl;
	u32 inputArgc;
	u32 ioArgc;
	IoctlvMessageData *data;
} IoctlvMessage;
CHECK_OFFSET(IoctlvMessage, 0x00, ioctl);
CHECK_OFFSET(IoctlvMessage, 0x04, inputArgc);
CHECK_OFFSET(IoctlvMessage, 0x08, ioArgc);
CHECK_OFFSET(IoctlvMessage, 0x0C, data);
CHECK_SIZE(IoctlvMessage, 0x10);

typedef struct
{
	u32 cmd;
	s32 result;
	union {
		s32 fileDescriptor;
		u32 requestCommand;
	};
	union {
		OpenMessage open;
		ReadWriteMessage read;
		ReadWriteMessage write;
		SeekMessage seek;
		IoctlMessage ioctl;
		IoctlvMessage ioctlv;
		u32 args[5];
	};
	void *callback;
	u32 callerData;
	u32 relaunch;
} IpcMessage;
CHECK_SIZE(IpcMessage, 0x2C);
CHECK_OFFSET(IpcMessage, 0x00, cmd);
CHECK_OFFSET(IpcMessage, 0x04, result);
CHECK_OFFSET(IpcMessage, 0x08, fileDescriptor);
CHECK_OFFSET(IpcMessage, 0x08, requestCommand);
CHECK_OFFSET(IpcMessage, 0x20, callback);
CHECK_OFFSET(IpcMessage, 0x24, callerData);
CHECK_OFFSET(IpcMessage, 0x28, relaunch);

//all message types and their data
CHECK_OFFSET(IpcMessage, 0x0C, open);
CHECK_OFFSET(IpcMessage, 0x0C, read);
CHECK_OFFSET(IpcMessage, 0x0C, write);
CHECK_OFFSET(IpcMessage, 0x0C, seek);
CHECK_OFFSET(IpcMessage, 0x0C, ioctl);
CHECK_OFFSET(IpcMessage, 0x0C, ioctlv);
CHECK_OFFSET(IpcMessage, 0x0C, args[0]);
CHECK_OFFSET(IpcMessage, 0x10, args[1]);
CHECK_OFFSET(IpcMessage, 0x14, args[2]);
CHECK_OFFSET(IpcMessage, 0x18, args[3]);
CHECK_OFFSET(IpcMessage, 0x1C, args[4]);

#endif