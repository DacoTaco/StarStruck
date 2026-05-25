/*
	StarStruck - a Free Software reimplementation for the Nintendo/BroadOn IOS.
	Copyright (C) 2025	DacoTaco

# This code is licensed to you under the terms of the GNU GPL, version 2;
# see file COPYING or http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt
*/

#pragma once

#include <types.h>
#include <ios/ipc.h>

#include "../handles.h"
#include "../sffs/filesystem.h"

// Global shutdown flag
extern s32 _fsShutdown;

// Handle IPC messages for /dev/fs device
s32 GetFSHandle(u32 userId, u16 groupId, u32 inode, AccessMode mode, u32 size);
s32 GetFileHandle(u32 userId, u16 groupId, const char* path, AccessMode mode);
s32 CloseHandle(FSHandle* handle);

s32 HandleDevFsRead(IpcMessage* message);
s32 HandleDevFsWrite(IpcMessage* message);
s32 HandleDevFsSeek(IpcMessage* message);
s32 HandleDevFsIoctl(IpcMessage* message);
s32 HandleDevFsIoctlv(IpcMessage* message);
s32 HandleDevFsClose(IpcMessage* message);
