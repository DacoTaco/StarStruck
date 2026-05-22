/*
	starstruck - a Free Software reimplementation for the Nintendo/BroadOn IOS.
	loader - loading binaries into memory and starting them

	Copyright (C) 2021	DacoTaco

# This code is licensed to you under the terms of the GNU GPL, version 2;
# see file COPYING or http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt
*/

#include <elf.h>
#include <ios/ahb.h>
#include <ios/ipc.h>
#include <ios/errno.h>
#include <ios/processor.h>
#include <ios/executables.h>
#include <string.h>

#include "loader.h"
#include "core/hollywood.h"
#include "core/gpio.h"
#include "filedesc/calls.h"
#include "memory/heaps.h"
#include "memory/memory.h"
#include "scheduler/threads.h"
#include "peripherals/powerpc.h"

static inline s32 _LoadELF(s32 fd, u32 *outArmEntrypoint, bool *outIsPPCBinary)
{
	s32 ret = SeekFD(fd, 0, SeekSet);
	if (ret != 0)
		return ret;

	Elf32_Ehdr *elfHeader = (Elf32_Ehdr *)AllocateOnHeap(KernelHeapId, sizeof(Elf32_Ehdr));
	if (elfHeader == NULL)
		return IPC_EMAX;

	bool isArmElf = false;
	ret = ReadFD(fd, elfHeader, sizeof(Elf32_Ehdr));
	if (ret != sizeof(Elf32_Ehdr))
		goto cleanup_loadElf;

   //Is this even an elf?
	if (elfHeader->e_ident[EI_MAG0] != ELFMAG0 || elfHeader->e_ident[EI_MAG1] != ELFMAG1 ||
	    elfHeader->e_ident[EI_MAG2] != ELFMAG2 || elfHeader->e_ident[EI_MAG3] != ELFMAG3)
	{
		ret = IPC_EINVAL;
		goto cleanup_loadElf;
	}

	//And is it a 32bit, MSB, version 1, ARM executable with no RELEXEC/EABI-v1 flags?
	if (elfHeader->e_ident[EI_CLASS] != ELFCLASS32 || elfHeader->e_ident[EI_DATA] != ELFDATA2MSB ||
	    elfHeader->e_ident[EI_VERSION] != EV_CURRENT || elfHeader->e_ident[EI_OSABI] != 0x61 ||
	    elfHeader->e_type != ET_EXEC || elfHeader->e_machine != EM_ARM ||
	    elfHeader->e_version != 1 || (elfHeader->e_flags & 0x21) != 0)
	{
		ret = IPC_EINVAL;
		goto cleanup_loadElf;
	}

	u32 phdrTotalSize = (u32)elfHeader->e_phnum * sizeof(Elf32_Phdr);
	Elf32_Phdr *phdrs = (Elf32_Phdr *)AllocateOnHeap(KernelHeapId, phdrTotalSize);
	if (phdrs == NULL)
	{
		ret = IPC_EMAX;
		goto cleanup_loadElf;
	}

	ret = ReadFD(fd, phdrs, phdrTotalSize);
	if (ret != (s32)phdrTotalSize)
	{
		FreeOnHeap(KernelHeapId, phdrs);
		goto cleanup_loadElf;
	}

	ret = IPC_SUCCESS;
	for (u16 i = 0; i < elfHeader->e_phnum; i++)
	{
		Elf32_Phdr *phdr = &phdrs[i];
		if (phdr->p_type != PT_LOAD)
			continue;

		// IOS encodes segment destination in p_flags bits 20-27 (PF_MASKOS):
		// 0x7f = PPC-destined; anything else is ARM-destined (no explicit ARM flag).
		if ((phdr->p_flags & PF_MASKOS) == ELF_PF_PPC)
		{
			if (!(*outIsPPCBinary))
			{
				PPCStart();
				*outIsPPCBinary = true;
			}
		}
		else
		{
			isArmElf = true;
		}

		ret = SeekFD(fd, (s32)phdr->p_offset, SeekSet);
		if (ret < 0)
			break;

		ret = ReadFD(fd, (void *)phdr->p_vaddr, phdr->p_filesz);
		if ((u32)ret != phdr->p_filesz)
			break;

		// Zero-fill BSS tail of segment if memsz > filesz
		if ((u32)ret < phdr->p_memsz)
			memset((void *)(phdr->p_vaddr + (u32)ret), 0, phdr->p_memsz - (u32)ret);

		ret = 0;
	}
	FreeOnHeap(KernelHeapId, phdrs);

	if (isArmElf)
		*outArmEntrypoint = elfHeader->e_entry;

cleanup_loadElf:
	FreeOnHeap(KernelHeapId, elfHeader);
	return ret;
}

static inline s32 _LoadDOL(s32 fd)
{
	s32 ret = SeekFD(fd, 0, SeekSet);
	if (ret != 0)
		return ret;

	DolHeader *dolHeader = (DolHeader *)AllocateOnHeap(KernelHeapId, sizeof(DolHeader));
	if (dolHeader == NULL)
		return IPC_EMAX;

	ret = ReadFD(fd, dolHeader, sizeof(DolHeader));
	if (ret != sizeof(DolHeader))
		goto cleanup_dol;

	// Clear BSS section
	memset((void *)PPCVirtToPhys(dolHeader->BSS.Address), 0, dolHeader->BSS.Size);

	// Load all text segments
	for (int i = 0; i < DOL_TEXT_SEGMENTS; i++)
	{
		if (dolHeader->Offsets.Text[i] == 0)
			continue;

		//align the section size to 32 bytes, it is a dol requirement after all
		u32 size = (dolHeader->Sizes.Text[i] + 0x1f) & ~0x1fu;
		ret = SeekFD(fd, (s32)dolHeader->Offsets.Text[i], SeekSet);
		if (ret < 0)
			goto cleanup_dol;

		ret = ReadFD(fd, (void *)PPCVirtToPhys(dolHeader->Addresses.Text[i]), size);
		if ((u32)ret != size)
			goto cleanup_dol;
	}

	// Load all data segments
	for (int i = 0; i < DOL_DATA_SEGMENTS; i++)
	{
		if (dolHeader->Offsets.Data[i] == 0)
			continue;

		u32 size = (dolHeader->Sizes.Data[i] + 0x1f) & ~0x1fu;
		ret = SeekFD(fd, (s32)dolHeader->Offsets.Data[i], SeekSet);
		if (ret < 0)
			break;

		ret = ReadFD(fd, (void *)PPCVirtToPhys(dolHeader->Addresses.Data[i]), size);
		if ((u32)ret != size)
			break;

		ret = 0;
	}

cleanup_dol:
	FreeOnHeap(KernelHeapId, dolHeader);
	return ret;
}

s32 LoadBinary(const char *path)
{
	u32 armEntrypoint = 0;
	bool isPPCBinary = false;

	// Only UID 0 (kernel/root) is permitted to start the PPC
	if (GetUID() != 0)
		return IPC_EACCES;

	// Small staging buffer for the file-type magic pre-read
	u8 *magicBuf = (u8 *)AllocateOnHeap(KernelHeapId, SELFMAG);
	if (magicBuf == NULL)
		return IPC_EMAX;

	s32 fd = OpenFD(path, Read);
	s32 ret = fd;
	if (fd < 0)
		goto return_loadPPC;

	// Read SELFMAG bytes to detect ELF vs DOL
	ret = ReadFD(fd, magicBuf, SELFMAG);
	if (ret != SELFMAG)
		goto return_loadPPC;

	if (memcmp(magicBuf, ELFMAG, SELFMAG) == 0)
		ret = _LoadELF(fd, &armEntrypoint, &isPPCBinary);
	else
	{
		isPPCBinary = true;
		PPCStart();
		ret = _LoadDOL(fd);
	}

	if (ret != IPC_SUCCESS)
		goto return_loadPPC;

	DCFlushAll();
	ICInvalidateAll();
	AhbFlushFrom(AHB_1);
	AhbFlushTo(AHB_1);

	if (armEntrypoint != 0)
	{
		s32 threadId = CreateThread(armEntrypoint, NULL, NULL, 0, 10, 1);
		StartThread(threadId);
		Threads[threadId].ProcessId = KernelId;
	}

	if (isPPCBinary)
	{
		PPCSoftReset();
		debug_output(0xAA);
		set32(HW_GPIO1OWNER, GP_DEBUG);
		BusyDelay(8000);
		PPCSetSDKSemaphore(true);
	}

return_loadPPC:
	if (fd >= 0)
		CloseFD(fd);

	FreeOnHeap(KernelHeapId, magicBuf);
	return ret;
}