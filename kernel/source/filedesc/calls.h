/*
	starstruck - a Free Software reimplementation for the Nintendo/BroadOn IOS.
	resourceManager - manager to maintain all device resources

	Copyright (C) 2021	DacoTaco

# This code is licensed to you under the terms of the GNU GPL, version 2;
# see file COPYING or http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt
*/

#ifndef __FILEDESC_CALLS_H__
#define __FILEDESC_CALLS_H__

#include "filedesc_types.h"

s32 OpenFD(const char* path, int mode);
int CloseFD(s32 fd);
int ReadFD(s32 fd, void *buf, u32 len);
int WriteFD(s32 fd, const void *buf, u32 len);
int SeekFD(s32 fd, s32 offset, s32 origin);
int IoctlFD(s32 fd, u32 request_id, void *input_buffer, u32 input_buffer_len, void *output_buffer, u32 output_buffer_len);
int IoctlvFD(s32 fd, u32 request_id, u32 vector_count_in, u32 vector_count_out, IoctlvMessageData *vectors);

#endif