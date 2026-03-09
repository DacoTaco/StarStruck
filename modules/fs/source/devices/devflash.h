/*
	StarStruck - a Free Software reimplementation for the Nintendo/BroadOn IOS.
	Copyright (C) 2025	DacoTaco

# This code is licensed to you under the terms of the GNU GPL, version 2;
# see file COPYING or http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt
*/

#pragma once

#include <ios/ipc.h>

//enable /dev/flash access
//#define ENABLE_DEV_FLASH

#ifdef ENABLE_DEV_FLASH
s32 OpenFlashHandle(void);
#endif

// Check if file descriptor belongs to special devices
bool IsDevFlashFileHandle(s32 fd);

// Handle IPC messages for /dev/flash device
s32 HandleDevFlashMessage(IpcMessage *message);
