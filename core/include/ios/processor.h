/*
	mini - a Free Software replacement for the Nintendo/BroadOn IOS.
	random utilities

Copyright (C) 2008, 2009	Hector Martin "marcan" <marcan@marcansoft.com>

# This code is licensed to you under the terms of the GNU GPL, version 2;
# see file COPYING or http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt
*/

#ifndef __PROCESSOR_H__
#define __PROCESSOR_H__

#ifndef STR_HELPER
#define STR_HELPER(x) #x
#endif

#ifndef STR
#define STR(x) STR_HELPER(x)
#endif

#define SPSR_USER_MODE        0x10
#define SPSR_FIQ_MODE         0x11
#define SPSR_IRQ_MODE         0x12
#define SPSR_SUPERVISOR_MODE  0x13
#define SPSR_ABORT_MODE       0x17
#define SPSR_UNDEFINED_MODE   0x1B
#define SPSR_SYSTEM_MODE      0x1F
#define SPSR_MODE_MASK(spsr)  (spsr & 0x1F)

#define SPSR_THUMB_MODE       0x20
#define SPSR_THUMB_MASK(spsr) (spsr & SPSR_THUMB_MODE)

#define SPSR_FIQ_ENABLE       0x40
#define SPSR_FIQ_MASK(spsr)   (spsr & SPSR_FIQ_ENABLE)

#define SPSR_IRQ_ENABLE       0x80
#define SPSR_IRQ_MASK(spsr)   (spsr & SPSR_IRQ_ENABLE)

#if !__ASSEMBLER__

#include "types.h"

static inline u32 read32(u32 addr)
{
	return *(vu32 *)addr;
}

static inline void write32(u32 addr, u32 data)
{
	*(vu32 *)addr = data;
}

static inline u32 set32(u32 addr, u32 set)
{
	u32 data = read32(addr) | set;
	write32(addr, data);
	return data;
}

static inline u32 clear32(u32 addr, u32 clear)
{
	u32 data = read32(addr) & ~clear;
	write32(addr, data);
	return data;
}

static inline u32 mask32(u32 addr, u32 clear, u32 set)
{
	u32 data = (read32(addr) & ~clear) | set;
	write32(addr, data);
	return data;
}

static inline u16 read16(u32 addr)
{
	return *(vu16 *)addr;
}

static inline void write16(u32 addr, u16 data)
{
	*(vu16 *)addr = data;
}

static inline u16 set16(u32 addr, u16 set)
{
	u16 data = read16(addr) | set;
	write16(addr, data);
	return data;
}

static inline u16 clear16(u32 addr, u16 clear)
{
	u16 data = read16(addr) & ~clear;
	write16(addr, data);
	return data;
}

static inline u16 mask16(u32 addr, u16 clear, u16 set)
{
	u16 data = (read16(addr) & ~clear) | set;
	write16(addr, data);
	return data;
}

static inline u8 read8(u32 addr)
{
	return *(vu8 *)addr;
}

static inline void write8(u32 addr, u8 data)
{
	*(vu8 *)addr = data;
}

static inline u8 set8(u32 addr, u8 set)
{
	u8 data = read8(addr) | set;
	write8(addr, data);
	return data;
}

static inline u8 clear8(u32 addr, u8 clear)
{
	u8 data = read8(addr) & ~clear;
	write8(addr, data);
	return data;
}

static inline u8 mask8(u32 addr, u8 clear, u8 set)
{
	u8 data = (read8(addr) & ~clear) | set;
	write8(addr, data);
	return data;
}

u32 GetCurrentStatusRegister(void);
u32 GetSavedStatusRegister();
void BusyDelay(u32 delay);
void debug_output(u8 byte);
int sprintf(char *str, const char *fmt, ...) __attribute__((format(printf, 2, 3)));
int snprintf(char *str, size_t size, const char *fmt, ...)
    __attribute__((format(printf, 3, 4)));

#endif
#endif
