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
#include "filesystem.h"

// File system operations
s32 GetPathUsage(const char *path, u32 *clusters, u32 *inodes);
s32 DeletePath(const u32 uid, const u16 gid, const char *path);
s32 ReadDirectory(const u32 uid, const u16 gid, const char *path, char *files,
                  u32 *numberOfEntries);
s32 CreateDirectory(const u32 uid, const u16 gid, const char *path,
                    u8 attributes, u8 ownerPerm, u8 groupPerm, u8 otherPerm);
s32 Rename(const u32 userId, const u16 groupId, const char *source, const char *destination);

// Create an empty file at `path` with the given attributes and permissions.
// Mirrors IOS FS_CreateFile semantics: creates the FST entry and flushes
// the superblock on success.

// Batch operations
s32 MassCreateFiles(u32 userId, u16 groupId, IoctlvMessageData *paths,
                    u32 *sizes, u32 numberOfFiles);

// Administrative operations
// Format the filesystem. Called by devfs control handle (root only).
// unlike IOS we take in the filehandles list to clear, so no variables need to be exposed to devfs
s32 Format(u32 userId, FSHandle *fileHandles, u32 fileHandleCount);

// File operations (internal)
s32 CreateFile(const u32 userId, const u16 groupId, const char *path, u8 attributes,
               u8 ownerPermissions, u8 groupPermissions, u8 otherPermissions);
s32 SeekFile(FSHandle *handle, s32 offset, SeekMode whence);
s32 ReadFile(FSHandle *handle, u8 *data, u32 length);
s32 WriteFile(FSHandle *handle, const void *data, u32 length);
s32 SetFileVersionControl(u32 userId, const char *path, u32 enable);