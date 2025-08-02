/*
	mini - a Free Software replacement for the Nintendo/BroadOn IOS.
	inter-processor communications

Copyright (C) 2008, 2009	Hector Martin "marcan" <marcan@marcansoft.com>
Copyright (C) 2008, 2009	Haxx Enterprises <bushing@gmail.com>
Copyright (C) 2008, 2009	Sven Peter <svenpeter@gmail.com>
Copyright (C) 2009			Andre Heider "dhewg" <dhewg@wiibrew.org>
Copyright (C) 2009		John Kelley <wiidev@kelley.ca>

# This code is licensed to you under the terms of the GNU GPL, version 2;
# see file COPYING or http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt
*/

#pragma once

#include "types.h"
#include "ios/ipc.h"
#include "scheduler/threads.h"
#include "messaging/messageQueue.h"
#include "messaging/resourceManager.h"

/* For the sake of interface compatibility between mini and powerpc code,
   you should try to commit any enhancements you make back upstream so
   that they can be assigned a standard request number.  Otherwise, if
   you are creating a new device, you MUST assign it a device ID >= 0x80.

   Likewise, if you add functionality to any of the existing drivers,
   your must assign it to a request ID >= 0x8000.  This will prevent
   problems if a mismatch between ARM and PPC code occurs.  Similarly,
   if you add functions, you should always add them to the end, to
   prevent someone from calling the wrong function.

   Even still, you are encouraged to add in sanity checks and version
   checking to prevent strange bugs or even data loss.  --bushing */

#define IPC_DEV_SYS        0x00
#define IPC_DEV_NAND       0x01
#define IPC_DEV_SDHC       0x02
#define IPC_DEV_KEYS       0x03
#define IPC_DEV_AES        0x04
#define IPC_DEV_BOOT2      0x05
#define IPC_DEV_PPC        0x06
#define IPC_DEV_SDMMC      0x07
#define IPC_DEV_ES         0x08

#define IPC_IN_SIZE        32
#define IPC_OUT_SIZE       32

// IpcMessageArray contains 1 message per thread (= MAX_THREADS), plus these extra messages
#define IPC_EXTRA_MESSAGES 128

extern IpcMessage *IpcMessageArray;
extern MessageQueue IpcMessageQueueArray[MAX_THREADS];
extern unsigned ThreadMessageUsageArray[MAX_THREADS];
extern ThreadInfo *IpcHandlerThread;
extern s32 IpcHandlerThreadId;

#ifndef MIOS
void IpcInit(void);
void IpcHandler(void);
#endif

s32 ResourceReply(IpcMessage *message, s32 requestReturnValue);
s32 SendMessageCheckReceive(IpcMessage *message, ResourceManager *resource);

void ipc_shutdown(void);
