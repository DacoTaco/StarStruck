/*
	mini - a Free Software replacement for the Nintendo/BroadOn IOS.
	USBGecko support code

Copyright (c) 2008		Nuke - <wiinuke@gmail.com>
Copyright (C) 2008, 2009	Hector Martin "marcan" <marcan@marcansoft.com>
Copyright (C) 2008, 2009	Sven Peter <svenpeter@gmail.com>
Copyright (C) 2009		Andre Heider "dhewg" <dhewg@wiibrew.org>

# This code is licensed to you under the terms of the GNU GPL, version 2;
# see file COPYING or http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt
*/
#include "ios/processor.h"
#include "ios/gecko.h"
#include "types.h"
#include "vsprintf.h"
#include "string.h"
#include "elf.h"

/* EXI Registers */
#ifndef EXI_REG_BASE

#define EXI_REG_BASE  0xd006800
#define EXI0_REG_BASE (EXI_REG_BASE + 0x000)
#define EXI1_REG_BASE (EXI_REG_BASE + 0x014)
#define EXI2_REG_BASE (EXI_REG_BASE + 0x028)

#define EXI0_CSR      (EXI0_REG_BASE + 0x000)
#define EXI0_MAR      (EXI0_REG_BASE + 0x004)
#define EXI0_LENGTH   (EXI0_REG_BASE + 0x008)
#define EXI0_CR       (EXI0_REG_BASE + 0x00c)
#define EXI0_DATA     (EXI0_REG_BASE + 0x010)

#define EXI1_CSR      (EXI1_REG_BASE + 0x000)
#define EXI1_MAR      (EXI1_REG_BASE + 0x004)
#define EXI1_LENGTH   (EXI1_REG_BASE + 0x008)
#define EXI1_CR       (EXI1_REG_BASE + 0x00c)
#define EXI1_DATA     (EXI1_REG_BASE + 0x010)

#define EXI2_CSR      (EXI2_REG_BASE + 0x000)
#define EXI2_MAR      (EXI2_REG_BASE + 0x004)
#define EXI2_LENGTH   (EXI2_REG_BASE + 0x008)
#define EXI2_CR       (EXI2_REG_BASE + 0x00c)
#define EXI2_DATA     (EXI2_REG_BASE + 0x010)

#define EXI_BOOT_BASE (EXI_REG_BASE + 0x040)

#endif

u8 gecko_found = 0;
static u8 gecko_console_enabled = 0;

static u32 _gecko_command(u32 command)
{
	u32 i;
 // Memory Card Port B (Channel 1, Device 0, Frequency 3 (32Mhz Clock))
	write32(EXI1_CSR, 0xd0);
	write32(EXI1_DATA, command);
	write32(EXI1_CR, 0x19);
	while (read32(EXI1_CR) & 1);
	i = read32(EXI1_DATA);
	write32(EXI1_CSR, 0);
	return i;
}

static u32 _gecko_getid(void)
{
	u32 i;
 // Memory Card Port B (Channel 1, Device 0, Frequency 3 (32Mhz Clock))
	write32(EXI1_CSR, 0xd0);
	write32(EXI1_DATA, 0);
	write32(EXI1_CR, 0x19);
	while (read32(EXI1_CR) & 1);
	write32(EXI1_CR, 0x39);
	while (read32(EXI1_CR) & 1);
	i = read32(EXI1_DATA);
	write32(EXI1_CSR, 0);
	return i;
}
static u32 _gecko_sendbyte(u8 sendbyte)
{
	u32 i = 0;
	i = _gecko_command(0xB0000000 | (sendbyte << 20));
	if (i & 0x04000000)
		return 1; // Return 1 if byte was sent
	return 0;
}

u32 _gecko_recvbyte(u8 *recvbyte)
{
	u32 i = 0;
	*recvbyte = 0;
	i = _gecko_command(0xA0000000);
	if (i & 0x08000000)
	{
  // Return 1 if byte was received
		*recvbyte = (i >> 16) & 0xff;
		return 1;
	}
	return 0;
}

#if defined(GECKO_SAFE)
static u32 _gecko_checksend(void)
{
	u32 i = 0;
	i = _gecko_command(0xC0000000);
	if (i & 0x04000000)
		return 1; // Return 1 if safe to send
	return 0;
}
#endif

u32 gecko_checkrecv(void)
{
	u32 i = 0;
	i = _gecko_command(0xD0000000);
	if (i & 0x04000000)
		return 1; // Return 1 if safe to recv
	return 0;
}

static int gecko_isalive(void)
{
	u32 i;

	i = _gecko_getid();
	if (i != 0x00000000)
		return 0;

	i = _gecko_command(0x90000000);
	if ((i & 0xFFFF0000) != 0x04700000)
		return 0;

	return 1;
}

void gecko_flush(void)
{
	u8 tmp;
	while (_gecko_recvbyte(&tmp));
}

#if !defined(GECKO_SAFE)
static u32 gecko_sendbuffer(const void *buffer, u32 size)
{
	u32 left = size;
	char *ptr = (char *)buffer;

	while (left > 0)
	{
		if (!_gecko_sendbyte(*ptr))
			break;
		if (*ptr == '\n')
			break;
		ptr++;
		left--;
	}
	return (size - left);
}
#endif

#if defined(GECKO_SAFE)
static u32 gecko_sendbuffer_safe(const void *buffer, u32 size)
{
	u32 left = size;
	char *ptr = (char *)buffer;

	if ((read32(HW_EXICTRL) & EXICTRL_ENABLE_EXI) == 0)
		return left;

	while (left > 0)
	{
		if (_gecko_checksend())
		{
			if (!_gecko_sendbyte(*ptr))
				break;
			if (*ptr == '\n')
				break;
			ptr++;
			left--;
		}
	}
	return (size - left);
}
#endif

void gecko_init(void)
{
	write32(EXI0_CSR, 0);
	write32(EXI1_CSR, 0);
	write32(EXI2_CSR, 0);

	if (!gecko_isalive())
		return;

	gecko_found = 1;

	gecko_flush();
	gecko_console_enabled = 1;
}

u8 gecko_enable_console(const u8 enable)
{
	if (enable)
	{
		if (gecko_isalive())
			gecko_console_enabled = 1;
	}
	else
		gecko_console_enabled = 0;

	return gecko_console_enabled;
}

#ifndef NDEBUG
u32 gecko_printf(const char *fmt, ...)
{
	if (!gecko_console_enabled)
		return 0;

	va_list args;
	char buffer[256];
	memset(buffer, 0, sizeof(buffer));
	s32 i;

	va_start(args, fmt);
	i = vsnprintf(buffer, sizeof(buffer), fmt, args);
	va_end(args);

#ifdef GECKO_SAFE
	return gecko_sendbuffer_safe(buffer, (u32)i);
#else
	return gecko_sendbuffer(buffer, (u32)i);
#endif
}
#endif