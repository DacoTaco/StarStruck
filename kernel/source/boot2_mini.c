/*
	mini - a Free Software replacement for the Nintendo/BroadOn IOS.
	boot2 chainloader

Copyright (C) 2008, 2009	Hector Martin "marcan" <marcan@marcansoft.com>
Copyright (C) 2008, 2009	Sven Peter <svenpeter@gmail.com>
Copyright (C) 2009		Andre Heider "dhewg" <dhewg@wiibrew.org>

# This code is licensed to you under the terms of the GNU GPL, version 2;
# see file COPYING or http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt
*/

#include <types.h>
#include <ios/gecko.h>
#include <ios/processor.h>

#include "core/defines.h"
#include "core/iosElf.h"
#include "core/hollywood.h"
#include "memory/memory.h"
#include "crypto/otp.h"
#include "crypto/aes.h"
#include "string.h"
#include "peripherals/powerpc.h"
#include "utils.h"
#include "panic.h"
#include "boot2.h"
#include "filedesc/calls.h"
#include "filedesc/calls_inner.h"

#define FLASH_IOCTL_GET_STATS 1

// Mirrors the fields of NandSizeInformation from the FS module.
// IOCTL_GET_STATS on /dev/flash returns this struct
typedef struct
{
	u32 NandSizeBitShift;
	u32 BlockSizeBitShift;
	u32 PageSizeBitShift;
	u32 EccSizeBitShift;
	u32 HMACSizeShift;
	u16 PageCopyMask;
	u16 SupportPageCopy;
	u16 EccDataCheckByteOffset;
	u8 Padding[2];
} FlashNandStats;

#define MEM2_BSS __attribute__((section(".bss.mem2")))

static FlashNandStats stats MEM2_BSS ALIGNED(32);
static u8 boot2[0x80000] MEM2_BSS ALIGNED(64);
static u8 boot2_key[32] MEM2_BSS ALIGNED(32);
static u8 boot2_iv[32] MEM2_BSS ALIGNED(32);
static u8 sector_buf[0x800] MEM2_BSS ALIGNED(64);
static u32 nand_page_size;
static u32 nand_block_size;
static u8 boot2_initialized = 0;
static u8 boot2_copy;
static u8 pages_read;
static u8 *page_ptr;

typedef struct
{
	u32 len;
	u32 data_offset;
	u32 certs_len;
	u32 tik_len;
	u32 tmd_len;
	u32 padding[3];
} boot2header;

typedef struct
{
	u64 signature;
	u32 generation;
	u8 blocks[0x40];
} __attribute__((packed)) boot2blockmap;

static boot2blockmap good_blockmap MEM2_BSS;

#define BLOCKMAP_SIGNATURE 0x26f29a401ee684cfULL

#define BOOT2_START        1
#define BOOT2_END          7

static u8 boot2_blocks[BOOT2_END - BOOT2_START + 1] MEM2_BSS;
static u32 valid_blocks;

static tmd_t tmd MEM2_BSS;
static tik_t tik MEM2_BSS;
static u8 *boot2_content;
static u32 boot2_content_size;

static void do_aes_decrypt(u8 *src, u8 *dst, u32 size, u8 *key, u8 *iv)
{
	while (size > 0)
	{
		u32 chunk = size > AES_MAX_CHUNK_SIZE ? AES_MAX_CHUNK_SIZE : size;

		IoctlvMessageData vecs[4];
		vecs[0].Data = src;
		vecs[0].Length = chunk;
		vecs[1].Data = key;
		vecs[1].Length = 0x10;
		vecs[2].Data = dst;
		vecs[2].Length = chunk;
		vecs[3].Data = iv; // engine writes last ciphertext block back here for CBC chaining
		vecs[3].Length = 0x10;

		IoctlvFD(AES_STATIC_FILEDESC, AES_DECRYPT, 2, 2, vecs);

		src += chunk;
		dst += chunk;
		size -= chunk;
	}
}

// find two equal valid blockmaps from a set of three, return one of them
static int find_valid_map(const boot2blockmap *maps)
{
	if (maps[0].signature == BLOCKMAP_SIGNATURE)
	{
		if (!memcmp(&maps[0], &maps[1], sizeof(boot2blockmap)))
			return 0;
		if (!memcmp(&maps[0], &maps[2], sizeof(boot2blockmap)))
			return 0;
	}
	if (maps[1].signature == BLOCKMAP_SIGNATURE)
	{
		if (!memcmp(&maps[1], &maps[2], sizeof(boot2blockmap)))
			return 1;
	}
	return -1;
}

// translate a page offset into boot2 to a real NAND page number using blockmap
static inline u32 boot2_page_translate(u32 page)
{
	u32 subpage = page % nand_block_size;
	u32 block = page / nand_block_size;

	return boot2_blocks[block] * nand_block_size + subpage;
}

// read boot2 up to the specified number of bytes (aligned to the next page)
static int read_to(s32 flash_fd, u32 bytes)
{
	if (bytes > (valid_blocks * nand_block_size * nand_page_size))
	{
		gecko_printf("tried to read %d boot2 bytes (%d pages), but only %d blocks (%d pages) are valid!\n",
		             bytes, (bytes + (nand_page_size - 1)) / nand_page_size,
		             valid_blocks, valid_blocks * nand_block_size);
		return -1;
	}
	while (bytes > ((u32)pages_read * nand_page_size))
	{
		u32 page = boot2_page_translate(pages_read);

		s32 ret = SeekFD(flash_fd, (s32)page, SeekSet);
		if (ret >= 0)
			ret = ReadFD(flash_fd, page_ptr, nand_page_size);

		if (ret < 0)
		{
			gecko_printf("boot2 page %d (NAND 0x%x) seek failed\n", pages_read, page);
			return -1;
		}
		DCInvalidateRange(page_ptr, nand_page_size);
		page_ptr += nand_page_size;
		pages_read++;
	}
	return 0;
}

int boot2_load(u8 copy)
{
	boot2blockmap *maps = (boot2blockmap *)sector_buf;
	u32 block;
	u32 page;
	int mapno;
	u32 found = 0;
	boot2header *hdr;
	STACK_ALIGN(u8, iv, 16, 32);

	boot2_content = NULL;
	boot2_content_size = 0;
	pages_read = 0;
	memset(&good_blockmap, 0, sizeof(boot2blockmap));
	valid_blocks = 0;

	s32 flash_fd = OpenFD("/dev/flash", Read);
	if (flash_fd < 0)
	{
		gecko_printf("boot2_load: failed to open /dev/flash: %d\n", flash_fd);
		return -1;
	}

	// find the best blockmap
	for (block = BOOT2_START; block <= BOOT2_END; block++)
	{
		page = (block + 1) * nand_block_size - 1;
		if (SeekFD(flash_fd, (s32)page, SeekSet) < 0 ||
		    ReadFD(flash_fd, sector_buf, nand_page_size) != (s32)nand_page_size)
		{
			gecko_printf("boot2 map candidate page 0x%x is unreadable, trying anyway\n", page);
		}

		DCInvalidateRange(sector_buf, nand_page_size);
		mapno = find_valid_map(maps);
		if (mapno >= 0)
		{
			gecko_printf("found valid boot2 blockmap at page 0x%x, submap %d, generation %d\n",
			             page, mapno, maps[mapno].generation);
			if (maps[mapno].generation >= good_blockmap.generation)
			{
				memcpy(&good_blockmap, &maps[mapno], sizeof(boot2blockmap));
				found = 1;
			}
		}
	}

	if (!found)
	{
		gecko_printf("no valid boot2 blockmap found!\n");
		CloseFD(flash_fd);
		return -1;
	}

	// traverse the blockmap and make a list of the actual boot2 blocks, in order
	if (copy == 0)
	{
		for (block = BOOT2_START; block <= BOOT2_END; block++)
		{
			if (good_blockmap.blocks[block] == 0x00)
			{
				boot2_blocks[valid_blocks++] = (u8)block;
			}
		}
	}
	else if (copy == 1)
	{
		for (block = BOOT2_END; block >= BOOT2_START; block--)
		{
			if (good_blockmap.blocks[block] == 0x00)
			{
				boot2_blocks[valid_blocks++] = (u8)block;
			}
		}
	}
	else
	{
		gecko_printf("invalid boot2 copy %d\n", copy);
		CloseFD(flash_fd);
		return -1;
	}

	gecko_printf("boot2 blocks:");
	for (block = 0; block < valid_blocks; block++)
		gecko_printf(" %02x", boot2_blocks[block]);
	gecko_printf("\n");

	// read boot2 header
	page_ptr = boot2;
	if (read_to(flash_fd, sizeof(boot2header)) < 0)
	{
		gecko_printf("error while reading boot2 header");
		CloseFD(flash_fd);
		return -1;
	}

	hdr = (boot2header *)boot2;

	if (hdr->len != sizeof(boot2header))
	{
		gecko_printf("invalid boot2 header size 0x%x\n", hdr->len);
		CloseFD(flash_fd);
		return -1;
	}
	if (hdr->tmd_len != sizeof(tmd_t))
	{
		gecko_printf("boot2 tmd size mismatch: expected 0x%x, got 0x%x (more than one content?)\n",
		             sizeof(tmd_t), hdr->tmd_len);
		CloseFD(flash_fd);
		return -1;
	}
	if (hdr->tik_len != sizeof(tik_t))
	{
		gecko_printf("boot2 tik size mismatch: expected 0x%x, got 0x%x\n",
		             sizeof(tik_t), hdr->tik_len);
		CloseFD(flash_fd);
		return -1;
	}

	// read tmd, tik, certs
	if (read_to(flash_fd, hdr->data_offset) < 0)
	{
		gecko_printf("error while reading boot2 certs/tmd/ticket");
		CloseFD(flash_fd);
		return -1;
	}

	memcpy(&tik, &boot2[hdr->len + hdr->certs_len], sizeof(tik_t));
	memcpy(&tmd, &boot2[hdr->len + hdr->certs_len + hdr->tik_len], sizeof(tmd_t));

	memset(iv, 0, 16);
	memcpy(iv, &tik.titleid, 8);

	STACK_ALIGN(u8, commonKey, OTP_COMMONKEY_SIZE, 32);
	OTP_FetchData(5, commonKey, OTP_COMMONKEY_SIZE);
	memcpy(boot2_key, &tik.cipher_title_key, 16);
	do_aes_decrypt(boot2_key, boot2_key, 16, commonKey, iv);

	memset(boot2_iv, 0, 16);
	memcpy(boot2_iv, &tmd.contents.index, 2); //just zero anyway...

	u32 *kp = (u32 *)boot2_key;
	gecko_printf("boot2 title key: %08x%08x%08x%08x\n", kp[0], kp[1], kp[2], kp[3]);

	boot2_content_size = (tmd.contents.size + 15) & (u32)~15;
	gecko_printf("boot2 content size: 0x%x (padded: 0x%x)\n",
	             (u32)tmd.contents.size, boot2_content_size);

	// read content
	if (read_to(flash_fd, hdr->data_offset + boot2_content_size) < 0)
	{
		gecko_printf("error while reading boot2 content");
		CloseFD(flash_fd);
		return -1;
	}

	boot2_content = &boot2[hdr->data_offset];

	boot2_copy = copy;
	gecko_printf("boot2 copy %d loaded to %p\n", copy, boot2);
	CloseFD(flash_fd);
	return 0;
}

s32 boot2_init(void)
{
	boot2_copy = (u8)-1;
	boot2_initialized = 0;

	s32 fd = OpenFD("/dev/flash", Read);
	if (fd < 0)
	{
		gecko_printf("boot2_init: failed to open /dev/flash: %d\n", fd);
		return -1;
	}

	if (IoctlFD(fd, FLASH_IOCTL_GET_STATS, NULL, 0, &stats, sizeof(stats)) < 0)
	{
		gecko_printf("boot2_init: IOCTL_GET_STATS failed\n");
		CloseFD(fd);
		return -2;
	}
	CloseFD(fd);

	nand_page_size = 1u << stats.PageSizeBitShift;
	nand_block_size = 1u << (stats.BlockSizeBitShift - stats.PageSizeBitShift);
	if (boot2_load(0) < 0)
	{
		gecko_printf("failed to load boot2 copy 0, trying copy 1...\n");
		if (boot2_load(1) < 0)
		{
			gecko_printf("failed to load boot2 copy 1!\n");
			return -3;
		}
	}

	// boot2 content flush would flush entire cache anyway so just do it all
	DCFlushAll();
	boot2_initialized = 1;
	return 1;
}

static u32 match[] = {
	0xBC024708,
	1,
	2,
};

static u32 patch[] = {
	0xBC024708,
	0, // tid hi
	0, // tid low
};

static u32 boot2_patch(ioshdr *hdr)
{
	u32 i, num_matches = 0;
	u8 *ptr = (u8 *)hdr + hdr->hdrsize + hdr->loadersize;

	for (i = 0; i < hdr->elfsize; i += 1)
	{
		if (memcmp(ptr + i, match, sizeof(match)) == 0)
		{
			num_matches++;
			memcpy(ptr + i, patch, sizeof(patch));
			gecko_printf("patched data @%08x\n", (u32)ptr + i);
		}
	}

	return num_matches;
}

u32 boot2_run(u32 tid_hi, u32 tid_lo)
{
	u32 num_matches;
	ioshdr *hdr;

	gecko_printf("booting boot2 with title %08x-%08x\n", tid_hi, tid_lo);
	ProtectMemory(1, (void *)0x11000000, (void *)0x13FFFFFF);

	do_aes_decrypt(boot2_content, (void *)0x11000000, boot2_content_size, boot2_key, boot2_iv);

	hdr = (ioshdr *)0x11000000;

	if ((tid_hi != match[1]) || (tid_lo != match[2]))
	{
		patch[1] = tid_hi;
		patch[2] = tid_lo;

		num_matches = boot2_patch(hdr);

		if (num_matches != 1)
		{
			gecko_printf("Wrong number of patches (matched %d times, expected 1), panicking\n",
			             num_matches);
			panic2(0, PANIC_PATCHFAIL);
		}
	}

	hdr->argument = 0x42;

	u32 vector = 0x11000000 + hdr->hdrsize;
	gecko_printf("boot2 is at 0x%08x\n", vector);
	return vector;
}
