/*
	StarStruck - a Free Software reimplementation for the Nintendo/BroadOn IOS.
	Copyright (C) 2022	DacoTaco

# This code is licensed to you under the terms of the GNU GPL, version 2;
# see file COPYING or http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt
*/

#include <ios/errno.h>
#include <string.h>
#include <ios/processor.h>
#include <ios/syscalls.h>
#include <ios/printk.h>
#include <ios/module.h>
#include <ios/keyring.h>

#include "cluster.h"
#include "nand.h"
#include "errors.h"

/* Max HMAC chunk size for processing large data */
#define MAX_HMAC_CHUNK_SIZE 0x10000

/* External globals from nand.c */
extern NandInformation SelectedNandChip;
extern NandSizeInformation SelectedNandSizeInfo;
extern s32 IoscMessageQueueId;
extern s32 IrqMessageQueueId;
extern u8 EccBuffer[0x100] __attribute__((aligned(0x80)));

/* Cluster module buffers */
static u8 _ivDataBuffer[0x10] __attribute__((aligned(0x20)));
static u8 _writePageBuffer[0x1000] __attribute__((aligned(0x40)));

static void GenerateFSAesIv(const u8 *salt, u8 *ivOut)
{
	// The original IOS code iterates through salt positions [0,16,32,48], [1,17,33,49], etc.
	// overwriting each output byte 4 times, keeping only the last value.
	// This effectively copies the last 16 bytes of the 64-byte salt to the IV.
	for (u32 destIndex = 0; destIndex < 0x10; destIndex++)
	{
		for (u32 saltIndex = destIndex; saltIndex < 0x40; saltIndex += 0x10)
		{
			ivOut[destIndex] = salt[saltIndex];
		}
	}
}

static s32 ReadClustersInner(u16 cluster, u32 count, u8 *clusterIv, u8 *clusterSalt,
                             u32 clusterSaltLength, u8 *data, u32 *hmacData)
{
	// Validate cluster is within NAND bounds
	const u32 maxClusters =
	    1 << ((SelectedNandChip.Info.SizeInfo.NandSizeBitShift - CLUSTER_SIZE_SHIFT) & 0xFF);
	if (cluster >= maxClusters || count == 0 || data == NULL ||
	    (hmacData != NULL && clusterSalt != NULL))
		return IPC_EINVAL;

	// Check if NAND is initialized
	if (!IsNandInitialized())
		return IPC_ENOENT;

	// Setup variables for reading
	s32 ret = IPC_SUCCESS;
	s32 functionResult = IPC_SUCCESS;
	bool nandCommandExecuted = false;
	bool ioscCommandExecuted = false;

	ShaContext hmacContext = { 0 };
	u8 hmacDigest[0x14] = { 0 };
	u8 hmacBuffer[0x40] = { 0 };
	u8 *hmacBufferPtr = NULL;
	if (clusterSalt == NULL)
	{
		hmacBufferPtr = (u8 *)hmacData;
	}
	else
	{
		// Initialize HMAC context with salt
		ret = OSIOSCGenerateBlockMAC(&hmacContext, NULL, 0, clusterSalt,
		                             clusterSaltLength, KEYRING_CONST_NAND_HMAC,
		                             InitHMacState, hmacDigest);
		if (ret != IPC_SUCCESS)
			goto waitAndReturn;

		hmacBufferPtr = hmacBuffer;
	}

	//read clusters
	s32 message = 0;
	u8 asyncBuffer[4] = { 0 };
	s32 asyncResult = 0;
	const u32 eccBufferBase = (u32)EccBuffer & 0xFFFFFF80;
	const u32 pageSizeShift = SelectedNandChip.Info.SizeInfo.PageSizeBitShift;
	const u32 pagesPerCluster = 1 << ((CLUSTER_SIZE_SHIFT - pageSizeShift) & 0xFF);
	const u32 pagesToRead = count * pagesPerCluster;
	const u32 hmacSizeShift = SelectedNandChip.Info.SizeInfo.HMACSizeShift;
	const u32 pageSize = 1 << (pageSizeShift & 0xFF);

	for (u32 pageIndex = 0; pageIndex < pagesToRead + 2; pageIndex++)
	{
		u32 cmdAddress = 0;

		// Read and optionally correct ecc data
		if (pageIndex != 0 && pageIndex < pagesToRead + 1)
		{
			nandCommandExecuted = false;

			s32 msgRet = OSReceiveMessage((s32)IrqMessageQueueId,
			                              (IpcMessage *)&message, None);
			if (msgRet != 0 || message != 1 || ((read32(0xd010000) >> 0x1d) & 1) != 0)
			{
				ret = IPC_UNKNOWN;
				goto returnRead;
			}

			OSAhbFlushFrom(AHB_NAND);
			OSAhbFlushTo(AHB_STARLET);

			u8 *databuffer =
			    data + ((pageIndex - 1)
			            << (SelectedNandChip.Info.SizeInfo.PageSizeBitShift & 0xFF));
			ret = CorrectNandData(databuffer, (u8 *)(((pageIndex - 1) & 1) * 0x80 + eccBufferBase));

			// Handle ECC results: SUCCESS, ECC (corrected), ECC_CRIT are acceptable
			// Any other error exits immediately
			if (ret != IPC_SUCCESS && ret != IPC_ECC && ret != IPC_ECC_CRIT)
				goto returnRead;

			// Track worst ECC status: ECC_CRIT takes priority over ECC
			if (ret == IPC_ECC_CRIT)
				functionResult = IPC_ECC_CRIT;
			else if (ret == IPC_ECC && functionResult != IPC_ECC_CRIT)
				functionResult = IPC_ECC;

			// Extract HMAC from spare data if needed
			// The HMAC is stored across the last few pages of a cluster's spare area.
			// pagesWithHmac: number of pages at end of cluster that contain HMAC data
			// pageOffsetInCluster: which page we're at within the current cluster
			// isLastCluster: for HMAC verification, only extract from the final cluster
			const u32 pagesWithHmac = 1 << ((6 - hmacSizeShift) & 0xFF);
			const u32 pageOffsetInCluster = (pageIndex - 1) & (pagesPerCluster - 1);
			const u32 clusterIndex =
			    (pageIndex - 1) >> ((CLUSTER_SIZE_SHIFT - pageSizeShift) & 0xFF);
			const bool isInHmacPages = (pagesPerCluster - pagesWithHmac) <= pageOffsetInCluster;
			const bool isLastCluster = (clusterSalt == NULL) ||
			                           (clusterIndex == count - 1);

			if (hmacBufferPtr != NULL && isInHmacPages && isLastCluster)
			{
				// Copy HMAC data from the spare area of this page's ECC buffer
				// eccSlot: alternates between 0 and 0x80 based on page parity (double buffering)
				// hmacOffset: offset within spare area where HMAC data starts
				// hmacChunkSize: bytes of HMAC data per page
				const u32 eccSlot = ((pageIndex - 1) & 1) * 0x80;
				const u32 hmacOffset =
				    SelectedNandChip.Info.SizeInfo.EccDataCheckByteOffset + 1;
				const u32 hmacChunkSize =
				    1 << (SelectedNandChip.Info.SizeInfo.HMACSizeShift & 0xFF);

				memcpy(hmacBufferPtr, (void *)(eccSlot + eccBufferBase + hmacOffset),
				       hmacChunkSize);
				hmacBufferPtr += hmacChunkSize;
			}
		}

		// Start NAND read for current page
		if (pageIndex < pagesToRead)
		{
			SetNandAddress(0, (u32)cluster * pagesPerCluster + pageIndex);
			if (SelectedNandChip.Info.Commands.ReadPrefix == 0xFE)
				cmdAddress = SelectedNandChip.Info.Commands.InputAddress;
			else
			{
				// Send read prefix command and wait for completion
#pragma GCC diagnostic ignored "-Wconversion"
				const NandCommand prefixCmd = {
					.Fields = { .Execute = 1,
					            .Address = SelectedNandChip.Info.Commands.InputAddress,
					            .Command = SelectedNandChip.Info.Commands.ReadPrefix }
				};
#pragma GCC diagnostic pop
				write32(NAND_CMD, prefixCmd.Value);
				while (READ_CMD().Fields.Execute)
				{
				}
			}

			void *eccBuffer = (void *)((pageIndex & 1) * 0x80 + eccBufferBase);
			const u32 eccSize =
			    1 << (SelectedNandChip.Info.SizeInfo.EccSizeBitShift & 0xFF);

			// Invalidate data and ECC buffers, then send the read command
			OSDCInvalidateRange(data + pageIndex * pageSize, pageSize);
			OSDCInvalidateRange(eccBuffer, eccSize);
			OSDCInvalidateRange((void *)((u32)eccBuffer + 0x40),
			                    4 << ((pageSizeShift - 9) & 0xFF));

			OSAhbFlushFrom(AHB_1);
			SetNandData(data + (pageIndex << (SelectedNandChip.Info.SizeInfo.PageSizeBitShift & 0xFF)),
			            eccBuffer);

			// Issue read command with ECC, IRQ, and wait flags
#pragma GCC diagnostic ignored "-Wconversion"
			const NandCommand readCmd = {
				.Fields = { .Execute = 1,
				            .GenerateIrq = 1,
				            .Wait = 1,
				            .ReadData = 1,
				            .CalculateEEC = 1,
				            .Address = cmdAddress,
				            .Command = SelectedNandChip.Info.Commands.Read,
				            .DataLength = eccSize + pageSize }
			};
#pragma GCC diagnostic pop
			write32(NAND_CMD, 0);
			write32(NAND_CMD, readCmd.Value);

			nandCommandExecuted = true;
		}

		// Decrypt page data if needed
		if (clusterIv == NULL)
			continue;

		//wait for previous decryption to finish
		if (pageIndex > 0)
		{
			ioscCommandExecuted = false;
			s32 msgRet = OSReceiveMessage((s32)IoscMessageQueueId,
			                              (IpcMessage *)&message, None);
			if (msgRet != IPC_SUCCESS || asyncResult != IPC_SUCCESS)
			{
				ret = IPC_UNKNOWN;
				goto returnRead;
			}
		}

		if (pageIndex != 0 && pageIndex < pagesToRead + 1)
		{
			// Calculate the buffer offset for the previous page
			const u32 prevPageOffset =
			    (pageIndex - 1)
			    << (SelectedNandChip.Info.SizeInfo.PageSizeBitShift & 0xFF);
			u8 *pageBuffer = data + prevPageOffset;

			ret = OSIOSCDecryptAsync(KEYRING_CONST_NAND_KEY, clusterIv, pageBuffer,
			                         pageSize, pageBuffer, (s32)IoscMessageQueueId,
			                         (IpcMessage *)asyncBuffer);
			if (ret != IPC_SUCCESS)
				goto waitAndReturn;

			ioscCommandExecuted = true;
		}
	}

	// Data Read, time to verify if salt was given
	ret = functionResult;
	if (clusterSalt == NULL)
		goto returnRead;

	// Create HMAC hash
	u32 offset = 0;
	u32 totalDataSize = pagesToRead * pageSize;
	while (offset < totalDataSize)
	{
		u32 chunkSize = totalDataSize - offset;
		if (MAX_HMAC_CHUNK_SIZE < chunkSize)
			chunkSize = MAX_HMAC_CHUNK_SIZE;

		ret = OSIOSCGenerateBlockMAC(&hmacContext, data + offset, chunkSize,
		                             NULL, 0, KEYRING_CONST_NAND_HMAC,
		                             ContributeHMacState, hmacDigest);
		if (ret != IPC_SUCCESS)
			goto waitAndReturn;

		offset += chunkSize;
	}

	// Finalize HMAC and compare
	ret = OSIOSCGenerateBlockMAC(&hmacContext, NULL, 0, NULL, 0, KEYRING_CONST_NAND_HMAC,
	                             FinalizeHmacState, hmacDigest);
	if (ret != IPC_SUCCESS)
		goto waitAndReturn;

	s32 cmpResult = memcmp(hmacDigest, hmacBuffer, 0x14);
	ret = functionResult;
	if (cmpResult == 0)
		goto returnRead;

	//hash does not match, check second half
	cmpResult = memcmp(hmacDigest, hmacBuffer + 0x14, 0x14);
	if (cmpResult == 0)
	{
		//second half matched, let's return a ECC error if it isn't set yet
		if (functionResult != IPC_ECC_CRIT)
			ret = functionResult = IPC_ECC;
	}
	else
		ret = IPC_CHECKVALUE;

	goto returnRead;

waitAndReturn:
	if (nandCommandExecuted)
	{
		OSReceiveMessage((s32)IrqMessageQueueId, (IpcMessage *)&message, None);
		OSAhbFlushFrom(AHB_NAND);
	}
	if (ioscCommandExecuted)
	{
		OSReceiveMessage((s32)IoscMessageQueueId, (IpcMessage *)&message, None);
	}

returnRead:
	if (ret == IPC_CHECKVALUE && functionResult == IPC_ECC_CRIT)
		ret = IPC_ECC_CRIT;

	return ret;
}

s32 ReadClusters(u16 cluster, u32 count, ClusterFlags flags, SaltData *salt,
                 u8 *data, u32 *hmacOut)
{
	s32 errno;
	u8 *clusterIv = NULL;
	u8 *clusterSalt = NULL;
	u32 clusterSaltLength = 0;

	// Calculate max clusters based on NAND size
	const u32 maxClusters =
	    1 << ((SelectedNandSizeInfo.NandSizeBitShift - CLUSTER_SIZE_SHIFT) & 0xFF);

	// Validate arguments
	if (((u32)cluster + count > maxClusters) || (data == NULL) ||
	    (flags != ClusterFlagsNone && salt == NULL))
	{
		errno = IPC_EINVAL;
	}
	else
	{
		// If decrypt flag is set, generate IV from salt
		if (flags & ClusterFlagsDecrypt)
		{
			GenerateFSAesIv((const u8 *)salt, _ivDataBuffer);
			clusterIv = _ivDataBuffer;
		}

		// If verify flag is set, pass salt data for HMAC
		if (flags & ClusterFlagsVerify)
		{
			clusterSaltLength = 0x40;
			clusterSalt = (u8 *)salt;
		}

		errno = ReadClustersInner(cluster, count, clusterIv, clusterSalt,
		                          clusterSaltLength, data, hmacOut);
	}

	// On error (except ECC corrected), zero the output buffer
	if (errno != IPC_SUCCESS && errno != IPC_ECC)
		memset(data, 0, count << CLUSTER_SIZE_SHIFT);

	return TranslateErrno(errno);
}

static s32 WriteClustersInner(u16 cluster, u32 count, u8 *clusterIv, u8 *clusterSalt,
                              u32 clusterSaltLength, u8 *data, u32 *hmacData)
{
	// Validate cluster is within NAND bounds
	const u32 maxClusters =
	    1 << ((SelectedNandChip.Info.SizeInfo.NandSizeBitShift - CLUSTER_SIZE_SHIFT) & 0xFF);
	if (cluster >= maxClusters || count == 0 || data == NULL ||
	    (hmacData != NULL && clusterSalt != NULL))
		return IPC_EINVAL;

	// Check if NAND is initialized
	if (!IsNandInitialized())
		return IPC_NOTREADY;

	// Setup variables for writing
	s32 ret = IPC_SUCCESS;
	bool ioscCommandExecuted = false;

	ShaContext hmacContext = { 0 };
	u8 hmacDigest[0x14] = { 0 };
	u8 hmacBuffer[0x40] = { 0 };
	u8 *hmacBufferPtr = NULL;

	if (clusterSalt == NULL)
	{
		// Use pre-computed HMAC data directly if provided
		hmacBufferPtr = (u8 *)hmacData;
	}
	else
	{
		// Initialize HMAC context with salt
		ret = OSIOSCGenerateBlockMAC(&hmacContext, NULL, 0, clusterSalt,
		                             clusterSaltLength, KEYRING_CONST_NAND_HMAC,
		                             InitHMacState, hmacDigest);
		if (ret != IPC_SUCCESS)
			goto waitAndReturn;

		hmacBufferPtr = hmacDigest;
	}

	// Calculate page and cluster dimensions
	const u32 pageSizeShift = SelectedNandChip.Info.SizeInfo.PageSizeBitShift;
	const u32 blockSizeShift = SelectedNandChip.Info.SizeInfo.BlockSizeBitShift;
	const u32 pagesPerCluster = 1 << ((CLUSTER_SIZE_SHIFT - pageSizeShift) & 0xFF);
	const u32 totalPages = count * pagesPerCluster;
	const u32 hmacSizeShift = SelectedNandChip.Info.SizeInfo.HMACSizeShift;
	const u32 pageSize = 1 << (pageSizeShift & 0xFF);

	// Generate HMAC over all data
	u32 offset = 0;
	const u32 totalDataSize = totalPages * pageSize;
	while (offset < totalDataSize)
	{
		u32 chunkSize = totalDataSize - offset;
		if (chunkSize > MAX_HMAC_CHUNK_SIZE)
			chunkSize = MAX_HMAC_CHUNK_SIZE;

		ret = OSIOSCGenerateBlockMAC(&hmacContext, data + offset, chunkSize,
		                             NULL, 0, KEYRING_CONST_NAND_HMAC,
		                             ContributeHMacState, hmacDigest);
		if (ret != IPC_SUCCESS)
			goto waitAndReturn;

		offset += chunkSize;
	}

	// Finalize HMAC
	ret = OSIOSCGenerateBlockMAC(&hmacContext, NULL, 0, NULL, 0, KEYRING_CONST_NAND_HMAC,
	                             FinalizeHmacState, hmacDigest);
	if (ret != IPC_SUCCESS)
		goto waitAndReturn;

	// Copy HMAC to buffer for embedding in spare area
	memcpy(hmacBuffer, hmacDigest, 0x14);

	// Write pages with optional encryption
	s32 message = 0;
	u8 asyncBuffer[4] = { 0 };
	s32 asyncResult = 0;

	for (u32 pageIndex = 0; pageIndex < totalPages + 1; pageIndex++)
	{
		// Handle encryption: encrypt current page asynchronously
		if (clusterIv != NULL)
		{
			// Wait for previous encryption to complete (if any)
			if (pageIndex > 0)
			{
				ioscCommandExecuted = false;
				s32 msgRet = OSReceiveMessage((s32)IoscMessageQueueId,
				                              (IpcMessage *)&message, None);
				if (msgRet != IPC_SUCCESS || asyncResult != IPC_SUCCESS)
				{
					ret = IPC_UNKNOWN;
					break;
				}
			}

			// Start encryption for current page if within bounds
			if (pageIndex < totalPages)
			{
				// Use double-buffering: alternate between two page-sized regions
				u8 *encryptBuffer = _writePageBuffer + (pageIndex & 1) * pageSize;
				ret = OSIOSCEncryptAsync(KEYRING_CONST_NAND_KEY, clusterIv,
				                         data + pageIndex * pageSize, pageSize,
				                         encryptBuffer, (s32)IoscMessageQueueId,
				                         (IpcMessage *)asyncBuffer);
				if (ret != IPC_SUCCESS)
					goto waitAndReturn;

				ioscCommandExecuted = true;
			}
		}

		// Write previous page (pageIndex - 1)
		if (pageIndex > 0)
		{
			const u32 prevPageIndex = pageIndex - 1;
			u8 eccData[0x40];
			memset(eccData, 0, 1 << (hmacSizeShift & 0xFF));

			// Embed HMAC in spare area for the last few pages of each cluster
			// pagesWithHmac: number of pages at end of cluster that contain HMAC data
			// pageOffsetInCluster: which page we're at within the current cluster
			// isLastCluster: for HMAC, only embed in the final cluster
			const u32 pagesWithHmac = 1 << ((6 - hmacSizeShift) & 0xFF);
			const u32 pageOffsetInCluster = prevPageIndex & (pagesPerCluster - 1);
			const u32 clusterIndex =
			    prevPageIndex >> ((CLUSTER_SIZE_SHIFT - pageSizeShift) & 0xFF);
			const bool isInHmacPages = (pagesPerCluster - pagesWithHmac) <= pageOffsetInCluster;
			const bool isLastCluster = (clusterSalt == NULL) ||
			                           (clusterIndex == count - 1);

			if (hmacBufferPtr != NULL && isInHmacPages && isLastCluster)
			{
				// Copy HMAC chunk into spare area
				const u32 hmacOffset =
				    SelectedNandChip.Info.SizeInfo.EccDataCheckByteOffset + 1;
				const u32 hmacChunkSize = 1 << (hmacSizeShift & 0xFF);
				memcpy(eccData + hmacOffset, hmacBufferPtr, hmacChunkSize);
				hmacBufferPtr += hmacChunkSize;
			}

			// Select data source: encrypted buffer or original data
			u8 *pageData;
			if (clusterIv == NULL)
				pageData = data + prevPageIndex * pageSize;
			else
				pageData = _writePageBuffer + (prevPageIndex & 1) * pageSize;

			// Determine unknownWriteflag: 0 at block boundaries or last page, 1 otherwise
			u8 unknownWriteflag;
			const u32 absolutePage = (u32)cluster * pagesPerCluster + pageIndex;
			const u32 pagesPerBlock = 1 << ((blockSizeShift - pageSizeShift) & 0xFF);
			if (pageIndex == totalPages || (absolutePage & (pagesPerBlock - 1)) == 0)
				unknownWriteflag = 0;
			else
				unknownWriteflag = 1;

			ret = WriteNandPage((u32)cluster * pagesPerCluster + prevPageIndex,
			                    pageData, eccData, unknownWriteflag, true);
			if (ret != IPC_SUCCESS)
				goto waitAndReturn;
		}
	}

	return ret;

waitAndReturn:
	if (ioscCommandExecuted)
	{
		OSReceiveMessage((s32)IoscMessageQueueId, (IpcMessage *)&message, None);
	}

	return ret;
}

s32 WriteClusters(u16 cluster, u32 count, ClusterFlags flags, SaltData *salt,
                  u8 *data, u32 *hmacData)
{
	s32 errno;
	// Calculate max clusters based on NAND size
	const u32 maxClusters =
	    1 << ((SelectedNandSizeInfo.NandSizeBitShift - CLUSTER_SIZE_SHIFT) & 0xFF);

	// Validate arguments
	if (((u32)cluster + count > maxClusters) || (data == NULL) ||
	    (flags != ClusterFlagsNone && salt == NULL))
	{
		errno = IPC_EINVAL;
		goto translateAndReturn;
	}

	// Erase blocks before writing - only at block boundaries
	const u32 clustersPerBlock =
	    1 << ((SelectedNandSizeInfo.BlockSizeBitShift - CLUSTER_SIZE_SHIFT) & 0xFF);

	for (u32 i = 0; i < count; i++)
	{
		// Check if this cluster is at a block boundary
		if (((cluster + i) & (clustersPerBlock - 1)) != 0)
			continue;

		errno = DeleteCluster((u16)(cluster + i));
		if (errno != IPC_SUCCESS)
			goto translateAndReturn;
	}

	u8 *clusterIv = NULL;
	u8 *clusterSalt = NULL;
	u32 clusterSaltLength = 0;
	// If decrypt flag is set (encrypt for write), generate IV from salt
	if (flags & ClusterFlagsDecrypt)
	{
		GenerateFSAesIv((const u8 *)salt, _ivDataBuffer);
		clusterIv = _ivDataBuffer;
	}

	// If verify flag is set (sign for write), pass salt data for HMAC
	if (flags & ClusterFlagsVerify)
	{
		clusterSaltLength = 0x40;
		clusterSalt = (u8 *)salt;
	}

	errno = WriteClustersInner(cluster, count, clusterIv, clusterSalt,
	                           clusterSaltLength, data, hmacData);

translateAndReturn:
	return TranslateErrno(errno);
}

// Copy clusters from source to destination with block erase
s32 CopyClusters(u16 srcCluster, u16 dstCluster, u32 count)
{
	// Validate clusters are within NAND bounds
	const u32 maxClusters =
	    1 << ((SelectedNandChip.Info.SizeInfo.NandSizeBitShift - CLUSTER_SIZE_SHIFT) & 0xFF);

	if ((u32)srcCluster + count > maxClusters || (u32)dstCluster + count > maxClusters)
		return FS_EINVAL;

	// Get clusters per block for block-aligned erase check
	s32 errno = IPC_SUCCESS;
	const u32 clustersPerBlock =
	    1 << ((SelectedNandChip.Info.SizeInfo.BlockSizeBitShift - CLUSTER_SIZE_SHIFT) & 0xFF);

	// Copy each cluster
	for (u32 i = 0; i < count; i++)
	{
		// Check if destination cluster is at block boundary & erase it
		if (((dstCluster + i) & (clustersPerBlock - 1)) == 0)
		{
			errno = DeleteCluster((u16)(dstCluster + i));
			if (errno != IPC_SUCCESS)
				break;
		}

		// Copy single cluster (page-by-page)
		errno = CopyCluster((u16)(srcCluster + i), (u16)(dstCluster + i));
		if (errno != IPC_SUCCESS)
			break;
	}

	return TranslateErrno(errno);
}
