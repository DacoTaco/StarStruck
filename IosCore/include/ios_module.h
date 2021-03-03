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

#define RELNCH_RELAUNCH 	0x01
#define RELNCH_BACKGROUND 	0x02

#define IOS_EINVAL			-0x3004
#define IOS_EBADVERSION		-0x3100
#define IOS_ETOOMANYVIEWS	-0x3101
#define IOS_EMISMATCH		-0x3102

typedef struct _ioctlv
{
	void *data;
	u32 len;
} ioctlv;

typedef struct _ipcreq
{						//ipc struct size: 32
	u32 cmd;			//0
	s32 result;			//4
	union {				//8
		s32 fd;
		u32 req_cmd;
	};
	union {
		struct {
			char *filepath;
			u32 mode;
		} open;
		struct {
			void *data;
			u32 len;
		} read, write;
		struct {
			s32 where;
			s32 whence;
		} seek;
		struct {
			u32 ioctl;
			void *buffer_in;
			u32 len_in;
			void *buffer_io;
			u32 len_io;
		} ioctl;
		struct {
			u32 ioctl;
			u32 argcin;
			u32 argcio;
			struct _ioctlv *argv;
		} ioctlv;
		u32 args[5];
	};
	
	void *cb;			//32
	void *usrdata;		//36
	u32 relnch;			//40
} ipcreq;

typedef int (*handle_request)(ipcreq* request, unsigned char* do_reply);
typedef int (*handle_open)(char* filepath, u32 mode, unsigned char* do_reply);

typedef struct _IosModule {
	char* device_name;
	handle_request request_handler; 
	handle_open open_handler;
} ios_module;

typedef int (*load_module)(ios_module* output);
#endif