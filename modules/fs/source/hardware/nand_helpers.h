/*
	StarStruck - a Free Software reimplementation for the Nintendo/BroadOn IOS.
	Copyright (C) 2022	DacoTaco

# This code is licensed to you under the terms of the GNU GPL, version 2;
# see file COPYING or http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt
*/
#pragma once
#include <types.h>

#include "nand.h"
#define INLINE_HELPER static inline __attribute__((always_inline))

/*helpers for determining NAND size properties*/
INLINE_HELPER u32 GetNandEccSize(const NandSizeInformation *nandInformation)
{
	return 1 << (nandInformation->EccSizeBitShift & 0xFF);
}
INLINE_HELPER u32 GetEccSize(void)
{
	return GetNandEccSize(&SelectedNandSizeInfo);
}
INLINE_HELPER u32 GetNandPageSize(const NandSizeInformation *nandInformation)
{
	return 1 << (nandInformation->PageSizeBitShift & 0xFF);
}
INLINE_HELPER u32 GetPageSize(void)
{
	return GetNandPageSize(&SelectedNandSizeInfo);
}
INLINE_HELPER u32 GetMaxClusters(void)
{
	return 1 << ((SelectedNandSizeInfo.NandSizeBitShift - CLUSTER_SIZE_SHIFT) & 0xFF);
}
INLINE_HELPER u32 GetClustersPerBlock(void)
{
	return 1 << ((SelectedNandSizeInfo.BlockSizeBitShift - CLUSTER_SIZE_SHIFT) & 0xFF);
}
INLINE_HELPER u32 GetPagesPerCluster(void)
{
	return 1 << ((CLUSTER_SIZE_SHIFT - SelectedNandSizeInfo.PageSizeBitShift) & 0xFF);
}
INLINE_HELPER u32 GetNandMaxPages(const NandSizeInformation *nandInformation)
{
	return 1 << ((nandInformation->NandSizeBitShift - nandInformation->PageSizeBitShift) & 0xFF);
}
INLINE_HELPER u32 GetMaxPages(void)
{
	return GetNandMaxPages(&SelectedNandSizeInfo);
}
INLINE_HELPER u32 GetSpareSize(void)
{
	return 4 << ((SelectedNandSizeInfo.PageSizeBitShift - 9) & 0xFF);
}
INLINE_HELPER u32 GetPagesPerBlock(void)
{
	return 1 << ((SelectedNandSizeInfo.BlockSizeBitShift - SelectedNandSizeInfo.PageSizeBitShift) &
	             0xFF);
}
INLINE_HELPER u32 GetBlockIndexFromPage(u32 pageIndex)
{
	return pageIndex >>
	       ((SelectedNandSizeInfo.BlockSizeBitShift - SelectedNandSizeInfo.PageSizeBitShift) & 0xFF);
}