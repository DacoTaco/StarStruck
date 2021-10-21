/*
	starstruck - a Free Software reimplementation for the Nintendo/BroadOn IOS.
	E-Ticket Services module - Service for handling all ES device calls

Copyright (C) 2021	DacoTaco

# This code is licensed to you under the terms of the GNU GPL, version 2;
# see file COPYING or http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt
*/
#ifndef __ES_H__
#define __ES_H__

#include <ios_module.h>

#define ES_FILENOTFOUND			-106
#define ES_INVALIDARGUMENT		-1017
#define ES_IOSC_ENOENT			-2004

int es_request_handler(ipcreq* request, unsigned char* do_reply);
int es_request_open(char* filepath, u32 mode, unsigned char* do_reply);

extern ios_module es_module;

#endif