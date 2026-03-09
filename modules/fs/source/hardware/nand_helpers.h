/*
	StarStruck - a Free Software reimplementation for the Nintendo/BroadOn IOS.
	Copyright (C) 2022	DacoTaco

# This code is licensed to you under the terms of the GNU GPL, version 2;
# see file COPYING or http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt
*/
#pragma once
#include <types.h>

#include "nand.h"

/*helpers for determining NAND size properties*/
static inline u32 GetNandEccSize(const NandSizeInformation *nandInformation)
{
	return 1 << (nandInformation->EccSizeBitShift & 0xFF);
}
static inline u32 GetEccSize(void)
{
	return GetNandEccSize(&SelectedNandSizeInfo);
}
static inline u32 GetNandPageSize(const NandSizeInformation *nandInformation)
{
	return 1 << (nandInformation->PageSizeBitShift & 0xFF);
}
static inline u32 GetPageSize(void)
{
	return GetNandPageSize(&SelectedNandSizeInfo);
}
static inline u32 GetMaxClusters(void)
{
	return 1 << ((SelectedNandSizeInfo.NandSizeBitShift - CLUSTER_SIZE_SHIFT) & 0xFF);
}
static inline u32 GetClustersPerBlock(void)
{
	return 1 << ((SelectedNandSizeInfo.BlockSizeBitShift - CLUSTER_SIZE_SHIFT) & 0xFF);
}
static inline u32 GetPagesPerCluster(void)
{
	return 1 << ((CLUSTER_SIZE_SHIFT - SelectedNandSizeInfo.PageSizeBitShift) & 0xFF);
}
static inline u32 GetNandMaxPages(const NandSizeInformation *nandInformation)
{
	return 1 << ((nandInformation->NandSizeBitShift - nandInformation->PageSizeBitShift) & 0xFF);
}
static inline u32 GetMaxPages(void)
{
	return GetNandMaxPages(&SelectedNandSizeInfo);
}
static inline u32 GetSpareSize(void)
{
	return 4 << ((SelectedNandSizeInfo.PageSizeBitShift - 9) & 0xFF);
}
static inline u32 GetPagesPerBlock(void)
{
	return 1 << ((SelectedNandSizeInfo.BlockSizeBitShift - SelectedNandSizeInfo.PageSizeBitShift) &
	             0xFF);
}

static inline u32 GetBlockIndexFromPage(u32 pageIndex)
{
	return pageIndex >>
	       ((SelectedNandSizeInfo.BlockSizeBitShift - SelectedNandSizeInfo.PageSizeBitShift) & 0xFF);
}