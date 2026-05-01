/*
	StarStruck - a Free Software reimplementation for the Nintendo/BroadOn IOS.
	ES Module - /dev/fs thin wrappers

	Copyright (C) 2024	DacoTaco

# This code is licensed to you under the terms of the GNU GPL, version 2;
# see file COPYING or http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt
*/

#pragma once

#include <ios/syscalls.h>
#include <fs/types.h>

#include "types.h"

s32 GetTitleUserId(TitleType titleType, u32 titleId, u32 *userIdOutput);
s32 DeleteDirectoryIfEmpty(const char *path);
s32 ReadDirectoryEntries(const char *path, char **out_entries, u32 *count);
