/*
	StarStruck - a Free Software reimplementation for the Nintendo/BroadOn IOS.
	Copyright (C) 2022	DacoTaco

# This code is licensed to you under the terms of the GNU GPL, version 2;
# see file COPYING or http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt
*/

#pragma once

#include <types.h>

typedef struct
{
	u32 Uid;
	u8 Filename[0x0C];
	u32 ChainIndex;
	u32 Inode;
	u32 Miscellaneous;
	u8 Unknown[0x18];
} SaltData;
CHECK_OFFSET(SaltData, 0x00, Uid);
CHECK_OFFSET(SaltData, 0x04, Filename);
CHECK_OFFSET(SaltData, 0x10, ChainIndex);
CHECK_OFFSET(SaltData, 0x14, Inode);
CHECK_OFFSET(SaltData, 0x18, Miscellaneous);
CHECK_OFFSET(SaltData, 0x1C, Unknown);
CHECK_SIZE(SaltData, 0x34);

typedef enum
{
	ClusterFlagsNone = 0,
	ClusterFlagsDecrypt = 1,  // bit 0: decrypt on read, encrypt on write
	ClusterFlagsVerify = 2,   // bit 1: HMAC verify on read, sign on write
} ClusterFlags;

/* Cluster-level operations with optional encryption/HMAC */
s32 ReadClusters(u16 cluster, u32 count, ClusterFlags flags, SaltData *salt,
                 u8 *data, u32 *hmacOut);
s32 WriteClusters(u16 cluster, u32 count, ClusterFlags flags, SaltData *salt,
                  u8 *data, u32 *hmacData);
s32 CopyClusters(u16 srcCluster, u16 dstCluster, u32 count);
