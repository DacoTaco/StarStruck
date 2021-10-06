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
#include "memory/ahb.h"
#include "interrupt/irq.h"

// what is this thing doing anyway?
// and why only on reads?
u32 _mc_read32(u32 addr)
{
	u32 data;
	u32 tmp130 = 0;
	// this seems to be a bug workaround
	if(!(read32(HW_VERSION) & 0xF0))
	{
		tmp130 = read32(HW_130);
		write32(HW_130, tmp130 | 0x400);
		// Dummy reads?
		read32(HW_138);
		read32(HW_138);
		read32(HW_138);
		read32(HW_138);
	}
	data = read32(addr);
	read32(HW_VERSION); //???

	if(!(read32(HW_VERSION) & 0xF0))
		write32(HW_130, tmp130);

	return data;
}

// invalidate device and then starlet
void AhbFlushTo(AHBDEV type)
{
	u32 cookie = irq_kill();
	_ahb_flush_to(type);
	if(type != AHB_STARLET)
		_ahb_flush_to(AHB_STARLET);

	irq_restore(cookie);
}

// this is ripped from IOS, because no one can figure out just WTF this thing is doing
void _ahb_flush_to(AHBDEV dev) {
	u32 mask = 10;
	switch(dev) {
		case AHB_STARLET: mask = 0x8000; break;
		case AHB_1: mask = 0x4000; break;
		//case 2: mask = 0x0001; break;
		case AHB_NAND: mask = 0x0002; break;
		case AHB_AES: mask = 0x0004; break;
		case AHB_SHA1: mask = 0x0008; break;
		//case 6: mask = 0x0010; break;
		//case 7: mask = 0x0020; break;
		//case 8: mask = 0x0040; break;
		case AHB_SDHC: mask = 0x0080; break;
		//case 10: mask = 0x0100; break;
		//case 11: mask = 0x1000; break;
		//case 12: mask = 0x0000; break;
		default:
			gecko_printf("ahb_invalidate(%d): Invalid device\n", dev);
			return;
	}
	//NOTE: 0xd8b000x, not 0xd8b400x!
	u32 val = _mc_read32(0xd8b0008);
	if(!(val & mask)) {
		switch(dev) {
			// 2 to 10 in IOS, add more
			case AHB_NAND:
			case AHB_AES:
			case AHB_SHA1:
			case AHB_SDHC:
				while((read32(HW_18C) & 0xF) == 9)
					set32(HW_188, 0x10000);
				clear32(HW_188, 0x10000);
				set32(HW_188, 0x2000000);
				mask32(HW_124, 0x7c0, 0x280);
				set32(HW_134, 0x400);
				while((read32(HW_18C) & 0xF) != 9);
				set32(HW_100, 0x400);
				set32(HW_104, 0x400);
				set32(HW_108, 0x400);
				set32(HW_10c, 0x400);
				set32(HW_110, 0x400);
				set32(HW_114, 0x400);
				set32(HW_118, 0x400);
				set32(HW_11c, 0x400);
				set32(HW_120, 0x400);
				write32(0xd8b0008, _mc_read32(0xd8b0008) & (~mask));
				write32(0xd8b0008, _mc_read32(0xd8b0008) | mask);
				clear32(HW_134, 0x400);
				clear32(HW_100, 0x400);
				clear32(HW_104, 0x400);
				clear32(HW_108, 0x400);
				clear32(HW_10c, 0x400);
				clear32(HW_110, 0x400);
				clear32(HW_114, 0x400);
				clear32(HW_118, 0x400);
				clear32(HW_11c, 0x400);
				clear32(HW_120, 0x400);
				clear32(HW_188, 0x2000000);
				mask32(HW_124, 0x7c0, 0xc0);
				break;
			//0, 1, 11 in IOS, add more
			case AHB_STARLET:
			case AHB_1:
				write32(0xd8b0008, val & (~mask));
				// wtfux
				write32(0xd8b0008, val | mask);
				write32(0xd8b0008, val | mask);
				write32(0xd8b0008, val | mask);
		}
	}
}

// flush device and also invalidate memory
void AhbFlushFrom(AHBDEV type)
{
	u32 cookie = irq_kill();
	_ahb_flush_from(type);
	irq_restore(cookie);
}

void _ahb_flush_from(AHBDEV dev)
{
	u32 cookie = irq_kill();
	u16 req = 0;
	u16 ack;
	int i;

	switch(dev)
	{
		case AHB_STARLET:
		case AHB_1:
			req = 1;
			break;
		case AHB_AES:
		case AHB_SHA1:
			req = 2;
			break;
		case AHB_NAND:
		case AHB_SDHC:
			req = 8;
			break;
		default:
			gecko_printf("ahb_flush(%d): Invalid device\n", dev);
			goto done;
	}

	write16(MEM_FLUSHREQ, req);

	for(i=0;i<1000000;i++) {
		ack = read16(MEM_FLUSHACK);
		_ahb_flush_to(AHB_STARLET);
		if(ack == req)
			break;
	}
	write16(MEM_FLUSHREQ, 0);
	if(i>=1000000) {
		gecko_printf("ahb_flush(%d): Flush (0x%x) did not ack!\n", dev, req);
	}
done:
	irq_restore(cookie);
}