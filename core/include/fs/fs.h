/*
	StarStruck - a Free Software reimplementation for the Nintendo/BroadOn IOS.
	FS - Shared FS functions for other modules to us

	Copyright (C) 2026	DacoTaco

# This code is licensed to you under the terms of the GNU GPL, version 2;
# see file COPYING or http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt
*/

#pragma once

#include "../types.h"
#include "types.h"
#include "ios/syscalls.h"
#include "ios/ipc.h"

static inline s32 ReadFile(s32 fd, void* buf, u32 len)
{
	return OSReadFD(fd, buf, len);
}

static inline s32 WriteFile(s32 fd, const void* buf, u32 len)
{
	return OSWriteFD(fd, buf, len);
}

static inline s32 SeekFile(s32 fd, s32 offset, SeekMode whence)
{
	return OSSeekFD(fd, offset, whence);
}

s32 OpenFile(const char* path, AccessMode mode);
s32 ShutdownFileSystem(void);
s32 FormatFileSystem(void);
s32 GetFileStats(s32 fd, FileStatistics* out);
s32 GetNandStatistics(SFFSStatistics* stats);
s32 CreateDirectory(const char* path, u8 attrib, u8 owner, u8 group, u8 other);
s32 CreateDirectoryRecursive(const char* path, u8 attrib, u8 owner, u8 group, u8 other);
s32 ReadDirectory(const char* path, char* out_entries, u32* count);
s32 SetAttributes(const char* path, u32 uid, u16 gid, u8 attrib, u8 owner, u8 group, u8 other);
s32 GetAttributes(const char* path, u32* uid, u16* gid, u32* attrib, u32* owner, u32* group, u32* other);
s32 DeletePath(const char* path);
s32 RenamePath(const char* source, const char* destination);
s32 CreateFile(const char* path, u8 attrib, u8 owner, u8 group, u8 other);
s32 CreateAndWriteFile(const char* filepath, u8 attrib, u8 owner, u8 group, u8 other, const u8* data, u32 len);
s32 SetFileVersionControl(const char* path, u8 version);