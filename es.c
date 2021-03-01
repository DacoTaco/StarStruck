/*
	starstruck - a Free Software reimplementation for the Nintendo/BroadOn IOS.
	E-Ticket Services module - Service for handling all ES device calls

Copyright (C) 2021	DacoTaco

# This code is licensed to you under the terms of the GNU GPL, version 2;
# see file COPYING or http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt
*/

#define DEVICE_NAME "/dev/es"
#define _DEBUG_ES

#include "string.h"
#include "ipc.h"
#include "es.h"

#ifdef _DEBUG_ES
#include "gecko.h"
#endif


#define IOCTL_ES_ADDTICKET				0x01
#define IOCTL_ES_ADDTITLESTART			0x02
#define IOCTL_ES_ADDCONTENTSTART		0x03
#define IOCTL_ES_ADDCONTENTDATA			0x04
#define IOCTL_ES_ADDCONTENTFINISH		0x05
#define IOCTL_ES_ADDTITLEFINISH			0x06
#define IOCTL_ES_GETDEVICEID			0x07
#define IOCTL_ES_LAUNCH					0x08
#define IOCTL_ES_OPENCONTENT			0x09
#define IOCTL_ES_READCONTENT			0x0A
#define IOCTL_ES_CLOSECONTENT			0x0B
#define IOCTL_ES_GETOWNEDTITLECNT		0x0C
#define IOCTL_ES_GETOWNEDTITLES			0x0D
#define IOCTL_ES_GETTITLECNT			0x0E
#define IOCTL_ES_GETTITLES				0x0F
#define IOCTL_ES_GETTITLECONTENTSCNT	0x10
#define IOCTL_ES_GETTITLECONTENTS		0x11
#define IOCTL_ES_GETVIEWCNT				0x12
#define IOCTL_ES_GETVIEWS				0x13
#define IOCTL_ES_GETTMDVIEWCNT			0x14
#define IOCTL_ES_GETTMDVIEWS			0x15
#define IOCTL_ES_GETCONSUMPTION			0x16
#define IOCTL_ES_DELETETITLE			0x17
#define IOCTL_ES_DELETETICKET			0x18
#define IOCTL_ES_DIGETTMDVIEWSIZE		0x19
#define IOCTL_ES_DIGETTMDVIEW			0x1A
#define IOCTL_ES_DIGETTICKETVIEW		0x1B
#define IOCTL_ES_DIVERIFY				0x1C
#define IOCTL_ES_GETTITLEDIR			0x1D
#define IOCTL_ES_GETDEVICECERT			0x1E
#define IOCTL_ES_IMPORTBOOT				0x1F
#define IOCTL_ES_GETTITLEID				0x20
#define IOCTL_ES_SETUID					0x21
#define IOCTL_ES_DELETETITLECONTENT		0x22
#define IOCTL_ES_SEEKCONTENT			0x23
#define IOCTL_ES_OPENTITLECONTENT		0x24
#define IOCTL_ES_LAUNCHBC				0x25
#define IOCTL_ES_EXPORTTITLEINIT		0x26
#define IOCTL_ES_EXPORTCONTENTBEGIN		0x27
#define IOCTL_ES_EXPORTCONTENTDATA		0x28
#define IOCTL_ES_EXPORTCONTENTEND		0x29
#define IOCTL_ES_EXPORTTITLEDONE		0x2A
#define IOCTL_ES_ADDTMD					0x2B
#define IOCTL_ES_ENCRYPT				0x2C
#define IOCTL_ES_DECRYPT				0x2D
#define IOCTL_ES_GETBOOT2VERSION		0x2E
#define IOCTL_ES_ADDTITLECANCEL			0x2F
#define IOCTL_ES_SIGN					0x30
#define IOCTL_ES_VERIFYSIGN				0x31
#define IOCTL_ES_GETSTOREDCONTENTCNT	0x32
#define IOCTL_ES_GETSTOREDCONTENTS		0x33
#define IOCTL_ES_GETSTOREDTMDSIZE		0x34
#define IOCTL_ES_GETSTOREDTMD			0x35
#define IOCTL_ES_GETSHAREDCONTENTCNT	0x36
#define IOCTL_ES_GETSHAREDCONTENTS		0x37

static char _opened = 0;
int _es_process_ioctlv(ipcreq* request, unsigned char* do_reply)
{
#ifdef _DEBUG_ES
	gecko_printf("ES IOCTLV : 0x%08X - 0x%08X\n", request->ioctlv.ioctl, request->relnch );
#endif

	switch(request->ioctl.ioctl)
	{
		case IOCTL_ES_LAUNCH:
			if(request->ioctlv.argcin != 2 || request->ioctlv.argcio != 0 || request->ioctlv.argv == NULL)
				return ES_INVALIDARGUMENT;

			ioctlv* vector = (ioctlv*)request->ioctlv.argv;
			if(vector->len != sizeof(u64))
				return ES_INVALIDARGUMENT;
			
			*do_reply = (request->relnch & RELNCH_RELAUNCH) > 0 ? 0 : 1;
			ipc_ppc_boot_title(*((u64*)vector->data));
			return 1;
		default:
#ifdef _DEBUG_ES
			gecko_printf("Unknown IOCTLV\n");
#endif
			return ES_IOSC_ENOENT;
	}
}

int _es_process_ioctl(ipcreq* request, unsigned char* do_reply)
{
	
#ifdef _DEBUG_ES
	gecko_printf("ES IOCTL : 0x%08X - 0x%08X\n", request->ioctl.ioctl, (u32)request->ioctl.buffer_in );
#endif
	switch(request->ioctl.ioctl)
	{
		default:
#ifdef _DEBUG_ES
			gecko_printf("Unknown IOCTL\n");
#endif
			if(request->ioctl.buffer_io != NULL)
				*(u32*)(request->ioctl.buffer_io) = 0xDEADBEEF;
			return ES_IOSC_ENOENT;
	}
}

int es_request_handler(ipcreq* request, unsigned char* do_reply)
{
	if(!request || !do_reply || !_opened || request->fd != IPC_DEV_ES)
		return ES_INVALIDARGUMENT;
	
	*do_reply = 1;
	switch(request->cmd)
	{
		case IOS_IOCTLV:
			return _es_process_ioctlv(request, do_reply);
		case IOS_IOCTL:
			return _es_process_ioctl(request, do_reply);
		default:
			gecko_printf("handle ES command : 0x%04x-0x%04x\n", request->cmd, request->fd);
			return ES_INVALIDARGUMENT;
	}
	
	return ES_IOSC_ENOENT;
}

int es_request_open(char* filepath, u32 mode, unsigned char* do_reply)
{
	if(!filepath || mode > IPC_OPEN_RW || !do_reply)
		return ES_INVALIDARGUMENT;
	
	if(_opened)
		return 1;
	
	if(strncmp(filepath, DEVICE_NAME, 32) != 0)
		return ES_FILENOTFOUND;
	
	_opened = 1;
	return IPC_DEV_ES;
}

ios_module es_module =
{
	DEVICE_NAME,
	es_request_handler,
	es_request_open
};