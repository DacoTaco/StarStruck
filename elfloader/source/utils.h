/*
	mini - a Free Software replacement for the Nintendo/BroadOn IOS.
	ELF loader: random utilities
Copyright (C) 2008, 2009	Hector Martin "marcan" <marcan@marcansoft.com>
This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, version 2.
This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.
You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/
#ifndef __UTILS_H__
#define __UTILS_H__

static inline u32 read32(u32 addr)
{
	u32 data;
	__asm__ volatile ("ldr\t%0, [%1]" : "=r" (data) : "r" (addr));
	return data;
}

static inline void write32(u32 addr, u32 data)
{
	__asm__ volatile ("str\t%0, [%1]" : : "r" (data), "r" (addr));
}

static inline u32 set32(u32 addr, u32 set)
{
	u32 data;
	__asm__ volatile (
		"ldr\t%0, [%1]\n"
		"\torr\t%0, %2\n"
		"\tstr\t%0, [%1]"
		: "=&r" (data)
		: "r" (addr), "r" (set)
	);
	return data;
}

static inline u32 clear32(u32 addr, u32 clear)
{
	u32 data;
	__asm__ volatile (
		"ldr\t%0, [%1]\n"
		"\tbic\t%0, %2\n"
		"\tstr\t%0, [%1]"
		: "=&r" (data)
		: "r" (addr), "r" (clear)
	);
	return data;
}


static inline u32 mask32(u32 addr, u32 clear, u32 set)
{
	u32 data;
	__asm__ volatile (
		"ldr\t%0, [%1]\n"
		"\tbic\t%0, %3\n"
		"\torr\t%0, %2\n"
		"\tstr\t%0, [%1]"
		: "=&r" (data)
		: "r" (addr), "r" (set), "r" (clear)
	);
	return data;
}

static inline u16 read16(u32 addr)
{
	u32 data;
	__asm__ volatile ("ldrh\t%0, [%1]" : "=r" (data) : "r" (addr));
	return data;
}

static inline void write16(u32 addr, u16 data)
{
	__asm__ volatile ("strh\t%0, [%1]" : : "r" (data), "r" (addr));
}

static inline u16 set16(u32 addr, u16 set)
{
	u16 data;
	__asm__ volatile (
		"ldrh\t%0, [%1]\n"
		"\torr\t%0, %2\n"
		"\tstrh\t%0, [%1]"
		: "=&r" (data)
		: "r" (addr), "r" (set)
	);
	return data;
}

static inline u16 clear16(u32 addr, u16 clear)
{
	u16 data;
	__asm__ volatile (
		"ldrh\t%0, [%1]\n"
		"\tbic\t%0, %2\n"
		"\tstrh\t%0, [%1]"
		: "=&r" (data)
		: "r" (addr), "r" (clear)
	);
	return data;
}


static inline u16 mask16(u32 addr, u16 clear, u16 set)
{
	u16 data;
	__asm__ volatile (
		"ldrh\t%0, [%1]\n"
		"\tbic\t%0, %3\n"
		"\torr\t%0, %2\n"
		"\tstrh\t%0, [%1]"
		: "=&r" (data)
		: "r" (addr), "r" (set), "r" (clear)
	);
	return data;
}

static inline u8 read8(u32 addr)
{
	u32 data;
	__asm__ volatile ("ldrb\t%0, [%1]" : "=r" (data) : "r" (addr));
	return data;
}

static inline void write8(u32 addr, u8 data)
{
	__asm__ volatile ("strb\t%0, [%1]" : : "r" (data), "r" (addr));
}

static inline u8 set8(u32 addr, u8 set)
{
	u8 data;
	__asm__ volatile (
		"ldrb\t%0, [%1]\n"
		"\torr\t%0, %2\n"
		"\tstrb\t%0, [%1]"
		: "=&r" (data)
		: "r" (addr), "r" (set)
	);
	return data;
}

static inline u8 clear8(u32 addr, u8 clear)
{
	u8 data;
	__asm__ volatile (
		"ldrb\t%0, [%1]\n"
		"\tbic\t%0, %2\n"
		"\tstrb\t%0, [%1]"
		: "=&r" (data)
		: "r" (addr), "r" (clear)
	);
	return data;
}


static inline u8 mask8(u32 addr, u8 clear, u8 set)
{
	u8 data;
	__asm__ volatile (
		"ldrb\t%0, [%1]\n"
		"\tbic\t%0, %3\n"
		"\torr\t%0, %2\n"
		"\tstrb\t%0, [%1]"
		: "=&r" (data)
		: "r" (addr), "r" (set), "r" (clear)
	);
	return data;
}

void udelay(u32 d);
void panic(u8 v);

#endif