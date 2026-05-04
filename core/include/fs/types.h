/*
	StarStruck - a Free Software reimplementation for the Nintendo/BroadOn IOS.
	FS Types - Shared FS types used by the FS module

	Copyright (C) 2026	DacoTaco

# This code is licensed to you under the terms of the GNU GPL, version 2;
# see file COPYING or http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt
*/

#pragma once

#include "../types.h"

// Maximum directory nesting depth for path validation
#define MAX_PATH_DEPTH 8
#define MAX_FILE_PATH  0x40
#define MAX_FILE_SIZE  0x0C

typedef struct
{
	u32 FileLength;
	u32 FilePosition;
} FileStatistics;
CHECK_SIZE(FileStatistics, 0x08);
CHECK_OFFSET(FileStatistics, 0x00, FileLength);
CHECK_OFFSET(FileStatistics, 0x04, FilePosition);

// SFFS filesystem statistics
typedef struct
{
	u32 ClusterSize;
	u32 FreeClusters;
	u32 UsedClusters;
	u32 BadClusters;
	u32 ReservedClusters;
	u32 FreeInodes;
	u32 UsedInodes;
} SFFSStatistics;
CHECK_SIZE(SFFSStatistics, 0x1C);
CHECK_OFFSET(SFFSStatistics, 0x00, ClusterSize);
CHECK_OFFSET(SFFSStatistics, 0x04, FreeClusters);
CHECK_OFFSET(SFFSStatistics, 0x08, UsedClusters);
CHECK_OFFSET(SFFSStatistics, 0x0C, BadClusters);
CHECK_OFFSET(SFFSStatistics, 0x10, ReservedClusters);
CHECK_OFFSET(SFFSStatistics, 0x14, FreeInodes);
CHECK_OFFSET(SFFSStatistics, 0x18, UsedInodes);

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

//I have no idea why this is a thing. Set attributes uses the path variable in FileOperationsParameter
//but GetAttributes uses a seperate path variable? really stupid haha
typedef struct
{
	char Path[MAX_FILE_PATH];
	FileOperationsParameter Parameters;
} GetAttributesParameters;
CHECK_SIZE(GetAttributesParameters, sizeof(FileOperationsParameter) + MAX_FILE_PATH);
CHECK_OFFSET(GetAttributesParameters, 0x00, Path);
CHECK_OFFSET(GetAttributesParameters, MAX_FILE_PATH, Parameters);