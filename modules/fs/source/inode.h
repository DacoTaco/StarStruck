/*
	StarStruck - a Free Software reimplementation for the Nintendo/BroadOn IOS.
	Copyright (C) 2025	DacoTaco

# This code is licensed to you under the terms of the GNU GPL, version 2;
# see file COPYING or http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt
*/

#pragma once
#include <types.h>
#include <ios/ipc.h>
#include "filesystem.h"

// SFFS FAT entry special values
typedef enum
{
	SFFSLastNode = 0xFFFB,  // Last cluster in chain
	SFFSReservedNode = 0xFFFC,  // Reserved cluster
	SFFSBadNode = 0xFFFD,   // Bad node
	SFFSFreeNode = 0xFFFE,   // Empty/free node
	SFFSErasedNode = 0xFFFF  // Invalid Node
} SFFSINodeType;

typedef enum
{
	CheckIfOpenInode = 1,      // Check if any file in subtree is open
	UnlinkInode = 2,         // Delete files and free FAT clusters
	MarkInodePending = 3, // Mark inodes as pending delete (set bit 31 of FileSize)
	GetUsedClusters = 4, // Count total clusters used by subtree
	GetUsedInodes = 5 // Count total inodes used by subtree
} InodeAction;

// Inode tree operations
s32 ProcessInodeAction(SuperBlockInfo *superblock, u32 inode, InodeAction action);

// Path/inode resolution
u32 FindInode(SuperBlockInfo *superblock, u32 parentInode, const char *name);
u32 FindInodeByPath(SuperBlockInfo *superblock, const char *path);

// Inode manipulation
s32 UnlinkTargetInode(SuperBlockInfo *superblock, u32 parentInode, u32 targetInode);

// Access control and validation
s32 CheckUserPermissions(SuperBlockInfo *superblock, u32 inode, u32 uid,
                         u16 gid, AccessMode mode);
s32 CheckIfFileOpen(u32 inode);
