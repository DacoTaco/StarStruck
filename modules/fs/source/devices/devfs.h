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

typedef struct
{
	u32 UserId;
	u16 GroupId;
	char Path[0x40];
	u8 OwnerPermissions;
	u8 GroupPermissions;
	u8 OtherPermissions;
	u8 Attributes;
} __attribute__((packed)) FileOperationsParameter;
CHECK_SIZE(FileOperationsParameter, 0x4A);
CHECK_OFFSET(FileOperationsParameter, 0x00, UserId);
CHECK_OFFSET(FileOperationsParameter, 0x04, GroupId);
CHECK_OFFSET(FileOperationsParameter, 0x06, Path);
CHECK_OFFSET(FileOperationsParameter, 0x46, OwnerPermissions);
CHECK_OFFSET(FileOperationsParameter, 0x47, GroupPermissions);
CHECK_OFFSET(FileOperationsParameter, 0x48, OtherPermissions);
CHECK_OFFSET(FileOperationsParameter, 0x49, Attributes);

typedef struct
{
	char Source[MAX_FILE_PATH];
	char Destination[MAX_FILE_PATH];
} FileRenameParameter;
CHECK_SIZE(FileRenameParameter, 2 * MAX_FILE_PATH);
CHECK_OFFSET(FileRenameParameter, 0x00, Source);
CHECK_OFFSET(FileRenameParameter, 0x40, Destination);

typedef struct
{
	u32 FileLength;
	u32 FilePosition;
} FileStatistics;
CHECK_SIZE(FileStatistics, 0x08);
CHECK_OFFSET(FileStatistics, 0x00, FileLength);
CHECK_OFFSET(FileStatistics, 0x04, FilePosition);

// Global shutdown flag
extern s32 _fsShutdown;

// Handle IPC messages for /dev/fs device
s32 GetFSHandle(u32 userId, u16 groupId, u32 inode, u32 mode, u32 size);
s32 GetFileHandle(u32 userId, u16 groupId, const char *path, u32 mode);
s32 CloseHandle(FSHandle *handle);

s32 HandleDevFsRead(IpcMessage *message);
s32 HandleDevFsWrite(IpcMessage *message);
s32 HandleDevFsSeek(IpcMessage *message);
s32 HandleDevFsIoctl(IpcMessage *message);
s32 HandleDevFsIoctlv(IpcMessage *message);
s32 HandleDevFsClose(IpcMessage *message);
