/*
	StarStruck - a Free Software reimplementation for the Nintendo/BroadOn IOS.
	Copyright (C) 2025	DacoTaco

# This code is licensed to you under the terms of the GNU GPL, version 2;
# see file COPYING or http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt
*/

#pragma once
#include <types.h>
#include "filesystem.h"

#define MAX_FILE_PATH 0x40
#define MAX_FILE_SIZE 0x0D

// Path utilities
u32 GetPathLength(const char *path);
s32 SplitPath(const char *path, char *directory, char *fileName);

// File system operations
s32 GetPathUsage(const char *path, u32 *clusters, u32 *inodes);
s32 DeletePath(u32 uid, u16 gid, const char *path);
s32 CreateDirectory(u32 uid, u16 gid, const char *path, u8 attributes,
                    u8 ownerPerm, u8 groupPerm, u8 otherPerm);