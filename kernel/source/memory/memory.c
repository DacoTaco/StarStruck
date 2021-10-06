/*
	mini - a Free Software replacement for the Nintendo/BroadOn IOS.
	memory management, MMU, caches, and flushing

Copyright (C) 2008, 2009	Hector Martin "marcan" <marcan@marcansoft.com>

# This code is licensed to you under the terms of the GNU GPL, version 2;
# see file COPYING or http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt
*/

#include <ios/processor.h>
#include <ios/gecko.h>

#include "core/hollywood.h"
#include "memory/memory.h"
#include "interrupt/irq.h"
#include "utils.h"

void _dc_inval_entries(void *start, int count);
void _dc_flush_entries(const void *start, int count);
void _dc_flush(void);
void _ic_inval(void);
void _drain_write_buffer(void);

#ifndef LOADER
extern u32 __page_table[4096];
void _dc_inval(void);
void _tlb_inval(void);
#endif

#define LINESIZE 0x20
#define CACHESIZE 0x4000

#define CR_MMU		(1 << 0)
#define CR_DCACHE	(1 << 2)
#define CR_ICACHE	(1 << 12)

void dc_flushrange(const void *start, u32 size)
{
	u32 cookie = irq_kill();
	if(size > 0x4000) {
		_dc_flush();
	} else {
		void *end = ALIGN_FORWARD(((u8*)start) + size, LINESIZE);
		start = ALIGN_BACKWARD(start, LINESIZE);
		_dc_flush_entries(start, (end - start) / LINESIZE);
	}
	_drain_write_buffer();
	_ahb_flush_from(AHB_1);
	irq_restore(cookie);
}

void dc_invalidaterange(void *start, u32 size)
{
	u32 cookie = irq_kill();
	void *end = ALIGN_FORWARD(((u8*)start) + size, LINESIZE);
	start = ALIGN_BACKWARD(start, LINESIZE);
	_dc_inval_entries(start, (end - start) / LINESIZE);
	AhbFlushTo(AHB_STARLET);
	irq_restore(cookie);
}

void dc_flushall(void)
{
	u32 cookie = irq_kill();
	_dc_flush();
	_drain_write_buffer();
	_ahb_flush_from(AHB_1);
	irq_restore(cookie);
}

void ic_invalidateall(void)
{
	u32 cookie = irq_kill();
	_ic_inval();
	AhbFlushTo(AHB_STARLET);
	irq_restore(cookie);
}

void mem_protect(int enable, void *start, void *end)
{
	write16(MEM_PROT, enable?1:0);
	write16(MEM_PROT_START, (((u32)start) & 0xFFFFFFF) >> 12);
	write16(MEM_PROT_END, (((u32)end) & 0xFFFFFFF) >> 12);
	udelay(10);
}

void mem_setswap(int enable)
{
	u32 d = read32(HW_MEMMIRR);

	if((d & 0x20) && !enable)
		write32(HW_MEMMIRR, d & ~0x20);
	if((!(d & 0x20)) && enable)
		write32(HW_MEMMIRR, d | 0x20);
}

u32 dma_addr(void *p)
{
	u32 addr = (u32)p;

	switch(addr>>20) {
		case 0xfff:
		case 0x0d4:
		case 0x0dc:
			if(read32(HW_MEMMIRR) & 0x20) {
				addr ^= 0x10000;
			}
			addr &= 0x0001FFFF;
			addr |= 0x0d400000;
			break;
	}
	//gecko_printf("DMA to %p: address %08x\n", p, addr);
	return addr;
}

#define SECTION				0x012

#define	NONBUFFERABLE		0x000
#define	BUFFERABLE			0x004
#define	WRITETHROUGH_CACHE	0x008
#define	WRITEBACK_CACHE		0x00C

#define DOMAIN(x)			((x)<<5)

#define AP_ROM				0x000
#define AP_NOUSER			0x400
#define AP_ROUSER			0x800
#define AP_RWUSER			0xC00

// from, to, size: units of 1MB
void map_section(u32 from, u32 to, u32 size, u32 attributes)
{
	attributes |= SECTION;
	while(size--) {
		__page_table[from++] = (to++<<20) | attributes;
	}
}

//#define NO_CACHES

void mem_initialize(void)
{
	u32 cr;
	u32 cookie = irq_kill();

	gecko_printf("MEM: cleaning up\n");

	_ic_inval();
	_dc_inval();
	_tlb_inval();

	gecko_printf("MEM: unprotecting memory\n");

	mem_protect(0,NULL,NULL);

	gecko_printf("MEM: mapping sections\n");

	memset32(__page_table, 0, 16384);

	map_section(0x000, 0x000, 0x018, WRITEBACK_CACHE | DOMAIN(0) | AP_RWUSER);
	map_section(0x100, 0x100, 0x040, WRITEBACK_CACHE | DOMAIN(0) | AP_RWUSER);
	map_section(0x0d0, 0x0d0, 0x001, NONBUFFERABLE | DOMAIN(0) | AP_RWUSER);
	map_section(0x0d8, 0x0d8, 0x001, NONBUFFERABLE | DOMAIN(0) | AP_RWUSER);
	map_section(0xfff, 0xfff, 0x001, WRITEBACK_CACHE | DOMAIN(0) | AP_RWUSER);

	set_dacr(0xFFFFFFFF); //manager access for all domains, ignore AP
	set_ttbr((u32)__page_table); //configure translation table

	_drain_write_buffer();

	cr = get_cr();

#ifndef NO_CACHES
	gecko_printf("MEM: enabling caches\n");

	cr |= CR_DCACHE | CR_ICACHE;
	set_cr(cr);

	gecko_printf("MEM: enabling MMU\n");

	cr |= CR_MMU;
	set_cr(cr);
#endif

	gecko_printf("MEM: init done\n");

	irq_restore(cookie);
}

void mem_shutdown(void)
{
	u32 cookie = irq_kill();
	_dc_flush();
	_drain_write_buffer();
	u32 cr = get_cr();
	cr &= ~(CR_MMU | CR_DCACHE | CR_ICACHE); //disable ICACHE, DCACHE, MMU
	set_cr(cr);
	_ic_inval();
	_dc_inval();
	_tlb_inval();
	irq_restore(cookie);
}
