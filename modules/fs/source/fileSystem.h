/*
	StarStruck - a Free Software reimplementation for the Nintendo/BroadOn IOS.
	Copyright (C) 2025	DacoTaco

# This code is licensed to you under the terms of the GNU GPL, version 2;
# see file COPYING or http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt
*/

#pragma once
#include <types.h>

//a FileSystemTableEntry describes a file in the filesystem
typedef struct {
	char Name[12];		// File name, 12 bytes
	u8 Attributes;		// File attributes
	u8 Unknown;			// Unknown, always 0x00
	u16 StartCluster;	// Start cluster, used to find the file in the FAT
	u16 Sibling;		// File sibling, used to find the next file in the directory
	u32 FileSize;		// File size
	u32 UserId;			// User ID
	u16 GroupId;		// Group ID
	u32 Unknown2;		// Unknown
} __attribute__((packed)) FileSystemTableEntry;
CHECK_SIZE(FileSystemTableEntry, 0x20);
CHECK_OFFSET(FileSystemTableEntry, 0x00, Name);
CHECK_OFFSET(FileSystemTableEntry, 0x0C, Attributes);
CHECK_OFFSET(FileSystemTableEntry, 0x0D, Unknown);
CHECK_OFFSET(FileSystemTableEntry, 0x0E, StartCluster);
CHECK_OFFSET(FileSystemTableEntry, 0x10, Sibling);
CHECK_OFFSET(FileSystemTableEntry, 0x12, FileSize);
CHECK_OFFSET(FileSystemTableEntry, 0x16, UserId);
CHECK_OFFSET(FileSystemTableEntry, 0x1A, GroupId);
CHECK_OFFSET(FileSystemTableEntry, 0x1C, Unknown2);

//the FS superblock is a special section of the fileSystem that contains information about the filesystem
//it contains the file allocation table (FAT) and the file system table (FST) together with a version
#define SuperblockIdentifier 0x53464653 		//"SFFS"
typedef struct {
	u32 Identifier;								// 0x53464653 or "SFFS"
	u32 Version;								// Version Number, used to pick the newest version
	u32 Unknown;
	u16 FatEntries[0x8000];						// File Allocation Table, existing of 0x8000 16 bit identifiers
	FileSystemTableEntry  FstEntries[0x17FF];	// File System Table, used to store the file information
	u8 Padding[0x14];							// Padding to 0x40000
} SuperBlockInfo;
CHECK_OFFSET(SuperBlockInfo, 0x00, Identifier);
CHECK_OFFSET(SuperBlockInfo, 0x04, Version);
CHECK_OFFSET(SuperBlockInfo, 0x08, Unknown);
CHECK_OFFSET(SuperBlockInfo, 0x0C, FatEntries);
CHECK_OFFSET(SuperBlockInfo, 0x1000C, FstEntries);
CHECK_OFFSET(SuperBlockInfo, 0x3FFEC, Padding);
CHECK_SIZE(SuperBlockInfo, 0x40000);

s32 InitializeSuperBlockInfo(s32 mode);