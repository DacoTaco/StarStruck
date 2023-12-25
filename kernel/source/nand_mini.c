/*
	mini - a Free Software replacement for the Nintendo/BroadOn IOS.
	low-level NAND support

Copyright (C) 2008, 2009	Haxx Enterprises <bushing@gmail.com>
Copyright (C) 2008, 2009	Sven Peter <svenpeter@gmail.com>
Copyright (C) 2008, 2009	Hector Martin "marcan" <marcan@marcansoft.com>

# This code is licensed to you under the terms of the GNU GPL, version 2;
# see file COPYING or http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt
*/
#include <string.h>
#include <ios/processor.h>
#include <ios/gecko.h>

#include "core/defines.h"
#include "core/hollywood.h"
#include "memory/memory.h"
#include "messaging/ipc.h"
#include "interrupt/irq.h"

#include "nand.h"

// #define	NAND_DEBUG	1
#define NAND_SUPPORT_WRITE 1
#define NAND_SUPPORT_ERASE 1

#ifdef NAND_DEBUG
#	include "peripherals/gecko.h"
#	define	NAND_debug(f, arg...) gecko_printf("NAND: " f, ##arg);
#else
#	define	NAND_debug(f, ...)
#endif

#define NAND_RESET      0xff
#define NAND_CHIPID     0x90
#define NAND_GETSTATUS  0x70
#define NAND_ERASE_PRE  0x60
#define NAND_ERASE_POST 0xd0
#define NAND_READ_PRE   0x00
#define NAND_READ_POST  0x30
#define NAND_WRITE_PRE  0x80
#define NAND_WRITE_POST 0x10

#define NAND_BUSY_MASK  0x80000000
#define NAND_ERROR      0x20000000

#define NAND_FLAGS_IRQ 	0x40000000
#define NAND_FLAGS_WAIT 0x8000
#define NAND_FLAGS_WR	0x4000
#define NAND_FLAGS_RD	0x2000
#define NAND_FLAGS_ECC	0x1000

static volatile int irq_flag;
static u32 last_page_read = 0;
static u32 nand_min_page = 0x200; // default to protecting boot1+boot2

void nand_irq(void)
{
	if(read32(NAND_CMD) & NAND_ERROR) {
		gecko_printf("NAND: Error on IRQ\n");
	}
	_ahb_flush_from(AHB_NAND);
	AhbFlushTo(AHB_STARLET);
	/*if (current_request.code != 0) {
		switch (current_request.req) {
			case IPC_NAND_GETID:
				memcpy((void*)current_request.args[0], ipc_data, 0x40);
				DCFlushRange((void*)current_request.args[0], 0x40);
				break;
			case IPC_NAND_STATUS:
				memcpy((void*)current_request.args[0], ipc_data, 0x40);
				DCFlushRange((void*)current_request.args[0], 0x40);
				break;
			case IPC_NAND_READ:
				err = nand_correct(last_page_read, ipc_data, ipc_ecc);

				if (current_request.args[1] != 0xFFFFFFFF) {
					memcpy((void*)current_request.args[1], ipc_data, PAGE_SIZE);
					DCFlushRange((void*)current_request.args[1], PAGE_SIZE);
				}
				if (current_request.args[2] != 0xFFFFFFFF) {
					memcpy((void*)current_request.args[2], ipc_ecc, PAGE_SPARE_SIZE);
					DCFlushRange((void*)current_request.args[2], PAGE_SPARE_SIZE);
				}
				break;
			case IPC_NAND_ERASE:
				// no action needed upon erase completion
				break;
			case IPC_NAND_WRITE:
				// no action needed upon write completion
				break;
			default:
				gecko_printf("Got IRQ for unknown NAND req %d\n", current_request.req);
		}
		code = current_request.code;
		tag = current_request.tag;
		current_request.code = 0;
		ipc_post(code, tag, 1, err);
	}*/
	irq_flag = 1;
}

static inline void __nand_wait(void) {
	while(read32(NAND_CMD) & NAND_BUSY_MASK);
	if(read32(NAND_CMD) & NAND_ERROR)
		gecko_printf("NAND: Error on wait\n");
	_ahb_flush_from(AHB_NAND);
	AhbFlushTo(AHB_STARLET);
}

void nand_send_command(u32 command, u32 bitmask, u32 flags, u32 num_bytes) {
	u32 cmd = NAND_BUSY_MASK | (bitmask << 24) | (command << 16) | flags | num_bytes;

	NAND_debug("nand_send_command(%x, %x, %x, %x) -> %x\n",
		command, bitmask, flags, num_bytes, cmd);

	write32(NAND_CMD, 0x7fffffff);
	write32(NAND_CMD, 0);
	write32(NAND_CMD, cmd);
}

void __nand_set_address(u32 page_off, u32 pageno) {
	if (0 < (s32)page_off) write32(NAND_ADDR0, page_off);
	if (0 < (s32)pageno)   write32(NAND_ADDR1, pageno);
}

void __nand_setup_dma(u8 *data, u8 *spare) {
	if (((s32)data) != -1) {
		write32(NAND_DATA, dma_addr(data));
	}
	if (((s32)spare) != -1) {
		u32 addr = dma_addr(spare);
		if(addr & 0x7f)
			gecko_printf("NAND: Spare buffer 0x%08x is not aligned, data will be corrupted\n", addr);
		write32(NAND_ECC, addr);
	}
}

int nand_reset(void) {
	NAND_debug("nand_reset()\n");
// IOS actually uses NAND_FLAGS_IRQ | NAND_FLAGS_WAIT here
	nand_send_command(NAND_RESET, 0, NAND_FLAGS_WAIT, 0);
	__nand_wait();
// enable NAND controller
	write32(NAND_CONF, 0x08000000);
// set configuration parameters for 512MB flash chips
	write32(NAND_CONF, 0x4b3e0e7f);
	return 0;
}

void nand_get_id(u8 *idbuf) {
	irq_flag = 0;
	__nand_set_address(0,0);

	DCInvalidateRange(idbuf, 0x40);

	__nand_setup_dma(idbuf, (u8 *)-1);
	nand_send_command(NAND_CHIPID, 1, NAND_FLAGS_IRQ | NAND_FLAGS_RD, 0x40);
}

void nand_get_status(u8 *status_buf) {
	irq_flag = 0;
	status_buf[0]=0;

	DCInvalidateRange(status_buf, 0x40);

	__nand_setup_dma(status_buf, (u8 *)-1);
	nand_send_command(NAND_GETSTATUS, 0, NAND_FLAGS_IRQ | NAND_FLAGS_RD, 0x40);
}

void nand_read_page(u32 pageno, void *data, void *ecc) {
	irq_flag = 0;
	last_page_read = pageno;  // needed for error reporting
	__nand_set_address(0, pageno);
	nand_send_command(NAND_READ_PRE, 0x1f, 0, 0);

	if (((s32)data) != -1) DCInvalidateRange(data, PAGE_SIZE);
	if (((s32)ecc) != -1)  DCInvalidateRange(ecc, ECC_BUFFER_SIZE);

	__nand_wait();
	__nand_setup_dma(data, ecc);
	nand_send_command(NAND_READ_POST, 0, NAND_FLAGS_IRQ | NAND_FLAGS_WAIT | NAND_FLAGS_RD | NAND_FLAGS_ECC, 0x840);
}

void nand_wait(void) {
// power-saving IRQ wait
	while(!irq_flag) {
		u32 cookie = DisableInterrupts();
		if(!irq_flag)
			irq_wait();
		RestoreInterrupts(cookie);
	}
}

#ifdef NAND_SUPPORT_WRITE
void nand_write_page(u32 pageno, void *data, void *ecc) {
	irq_flag = 0;
	NAND_debug("nand_write_page(%u, %p, %p)\n", pageno, data, ecc);

// this is a safety check to prevent you from accidentally wiping out boot1 or boot2.
	if ((pageno < nand_min_page) || (pageno >= NAND_MAX_PAGE)) {
		gecko_printf("Error: nand_write to page %d forbidden\n", pageno);
		return;
	}
	if (((s32)data) != -1) DCFlushRange(data, PAGE_SIZE);
	if (((s32)ecc) != -1)  DCFlushRange(ecc, PAGE_SPARE_SIZE);
	AhbFlushTo(AHB_NAND);
	__nand_set_address(0, pageno);
	__nand_setup_dma(data, ecc);
	nand_send_command(NAND_WRITE_PRE, 0x1f, NAND_FLAGS_WR, 0x840);
	__nand_wait();
	nand_send_command(NAND_WRITE_POST, 0, NAND_FLAGS_IRQ | NAND_FLAGS_WAIT, 0);
}
#endif

#ifdef NAND_SUPPORT_ERASE
void nand_erase_block(u32 pageno) {
	irq_flag = 0;
	NAND_debug("nand_erase_block(%d)\n", pageno);

// this is a safety check to prevent you from accidentally wiping out boot1 or boot2.
	if ((pageno < nand_min_page) || (pageno >= NAND_MAX_PAGE)) {
		gecko_printf("Error: nand_erase to page %d forbidden\n", pageno);
		return;
	}
	__nand_set_address(0, pageno);
	nand_send_command(NAND_ERASE_PRE, 0x1c, 0, 0);
	__nand_wait();
	nand_send_command(NAND_ERASE_POST, 0, NAND_FLAGS_IRQ | NAND_FLAGS_WAIT, 0);
	NAND_debug("nand_erase_block(%d) done\n", pageno);
}
#endif

void nand_initialize(void)
{
	nand_reset();
	irq_enable(IRQ_NAND);
}

int nand_correct(u32 pageno, void *data, void *ecc)
{
	(void) pageno;

	u8 *dp = (u8*)data;
	u32 *ecc_read = (u32*)((u8*)ecc+0x30);
	u32 *ecc_calc = (u32*)((u8*)ecc+0x40);
	int i;
	int uncorrectable = 0;
	int corrected = 0;
	
	for(i=0;i<4;i++) {
		u32 syndrome = *ecc_read ^ *ecc_calc; //calculate ECC syncrome
		// don't try to correct unformatted pages (all FF)
		if ((*ecc_read != 0xFFFFFFFF) && syndrome) {
			if(!((syndrome-1)&syndrome)) {
				// single-bit error in ECC
				corrected++;
			} else {
				// byteswap and extract odd and even halves
				u16 even = (u16)(syndrome >> 24) | ((syndrome >> 8) & 0xf00);
				u16 odd = (u16)((syndrome << 8) & 0xf00) | ((syndrome >> 8) & 0x0ff);
				if((even ^ odd) != 0xfff) {
					// oops, can't fix this one
					uncorrectable++;
				} else {
					// fix the bad bit
					dp[odd >> 3] ^= 1<<(odd&7);
					corrected++;
				}
			}
		}
		dp += 0x200;
		ecc_read++;
		ecc_calc++;
	}
	if(uncorrectable || corrected)
		gecko_printf("ECC stats for NAND page 0x%x: %d uncorrectable, %d corrected\n", pageno, uncorrectable, corrected);
	if(uncorrectable)
		return NAND_ECC_UNCORRECTABLE;
	if(corrected)
		return NAND_ECC_CORRECTED;
	return NAND_ECC_OK;
}

