/*  string.c -- standard C string-manipulation functions.

Copyright (C) 2008		Segher Boessenkool <segher@kernel.crashing.org>
Copyright (C) 2009		Haxx Enterprises <bushing@gmail.com>

Portions taken from the Public Domain C Library (PDCLib).
https://negix.net/trac/pdclib

# This code is licensed to you under the terms of the GNU GPL, version 2;
# see file COPYING or http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt
*/

#include "string.h"

size_t strlen(const char *s)
{
	size_t len;

	for (len = 0; s[len]; len++);

	return len;
}

size_t strnlen(const char *s, size_t count)
{
	size_t len;

	for (len = 0; s[len] && len < count; len++);

	return len;
}

void set_memory(void *dest, const unsigned char data, size_t len)
{
	if (dest < (void *)0x1800000)
	{
		for (; len != 0; len--)
		{
			u32 *address = (u32 *)((u32)dest & (u32)~0x03);
			u32 offset = 24 - ((u32)dest & 0x03) * 8;
			*address = (*address & (u32) ~(0xFF << offset)) | (data << offset);
			;
			dest++;
		}
	}
	else
	{
		for (; len != 0; len--)
		{
			*(u8 *)dest = data;
			dest++;
		}
	}
}

//this is more like a regular memset, but only used at the end of memset..
__attribute__((target("arm"))) void set_memory_short(void *dest, const unsigned char c, size_t len)
{
	u32 data = c | (c << 8) | (u32)(c << 16) | (u32)(c << 24);
	register u32 data1 asm("r3") = data;
	register u32 data2 asm("r4") = data;
	register u32 data3 asm("r5") = data;
	u32 *address = dest;
	for (u32 index = len & 0xFFFFFFF0; index != 0; index -= 0x10)
	{
		__asm__ volatile(
		    "stmia	%[address]!, {%[data],%[data1],%[data2],%[data3]}"
		    :
		    : [address] "r"(address), [data] "r"(data), [data1] "r"(data1), [data2] "r"(data2), [data3] "r"(data3));
	}

	for (u32 index = len & 0x0F; index != 0; index -= 4)
	{
		*address = data;
		address++;
	}
}
// memset
// IOS has a completely custom memset to deal with a MEM1 HW bug
// MEM1 accesses of 4 bytes (strb & strh) to MEM1 will thrash 4 bytes
// hence the weird access/seperation of code
void *memset(void *dest, int character, size_t length)
{
	if (length == 0)
		return dest;

	void *destination = dest;
	const u8 data = (u8)character & 0xff;
	u32 cnt = 0;

	//if destination isn't 4 byte aligned, do a seperate memset until we are aligned
	if (((u32)dest & 3) != 0)
	{
		cnt = 4 - ((u32)dest & 3);
		if (length <= cnt)
		{
			set_memory(dest, data, length);
			return dest;
		}

		set_memory(dest, data, cnt);
		length -= cnt;
		destination = (void *)((u32)dest & 0xFFFFFFFC) + 4;
	}

	//align destination to 16 bytes
	if (length < 0x101 || ((u32)destination & 0xf) == 0)
		cnt = length & 0xFFFFFFFC;
	else
	{
		cnt = 0x10 - ((u32)destination & 0x0F);
		u32 dataToCopy = length;
		if (length < cnt)
		{
			dataToCopy = 0;
			cnt = length;
		}

		u32 alignedCnt = cnt & 0x0C;
		if (alignedCnt != 0)
		{
			u16 alignedData = (u16)(data | (data << 8));
			u32 *address = destination;
			for (; 3 < alignedCnt; alignedCnt -= 4)
			{
				*address = (u32)(alignedData | alignedData << 0x10);
				address++;
			}
			cnt = cnt & 3;
		}

		if (cnt != 0)
			set_memory(destination, data, cnt);

		if (dataToCopy == 0)
			return dest;

		length = dataToCopy - 0x10 + ((u32)destination & 0x0F);
		destination = (void *)(((u32)destination & (u32)0xFFFFFFF0) + 0x10);
		cnt = length & 0xFFFFFFF0;
	}

	if (cnt != 0)
	{
		set_memory_short(destination, data, cnt);
		destination += cnt;
		length -= cnt;
	}

	if (length != 0)
		set_memory(destination, data, length);

	return dest;
}

// memcpy
// IOS has a completely custom memcpy to deal with a MEM1 HW bug
// MEM1 accesses of 4 bytes (strb & strh) to MEM1 will thrash 4 bytes
// hence the weird access/seperation of code
__attribute__((target("arm"))) void *memcpy(void *dest, const void *src, size_t len)
{
	if (len == 0)
		return dest;

	//these are normal for ARM (r0 - r2 = function arguments)
	//however, to make sure we have the values in these registers we will redefine them here
	//nintendo's memcpy is very optimised and custom written and its lovely lol
	register size_t length asm("r3") = len;
	register void *source asm("r2") = (void *)src;
	register void *destination asm("r1") = dest;
	register void *ret asm("r0") = dest;

	if ((((u32)destination | (u32)source) & 0x03) == 0)
	{
		__asm__ volatile(
		    "\
			#Save the register values that GCC might have used before the asm \n\
			stmdb		sp!,{r4,r5,r6,r7,r8,r9,r10,r11} \n\
			#check if we can do a 0x10 copy paste \n\
			cmp			%[length], #0x0F \n\
			bls			memcpy_end \n\
			#check if we can do a 0x20 copy/paste \n\
			cmp			%[length], #0x1F \n\
			bls			memcpy_By16 \n\
			memcpy_by32: \n\
			ldmia		%[source]!,{r4,r5,r6,r7,r8,r9,r10,r11} \n\
			stmia		%[destination]!,{r4,r5,r6,r7,r8,r9,r10,r11} \n\
			sub			%[length],%[length], #0x20 \n\
			cmp			%[length], #0x1F \n\
			bls			memcpy_By16 \n\
			b			memcpy_by32 \n\
			memcpy_By16: \n\
			cmp			%[length], #0x0F \n\
			bls			memcpy_end \n\
			ldmia		%[source]!,{r4,r5,r6,r7} \n\
			stmia		%[destination]!,{r4,r5,r6,r7} \n\
			sub			%[length],%[length], #0x10 \n\
			b			memcpy_By16 \n\
			memcpy_end: \n\
			ldmia		sp!,{r4,r5,r6,r7,r8,r9,r10,r11} \n"
		    :
		    : [source] "r"(source), [destination] "r"(destination), [length] "r"(length));

		//ok, nintendo's beauty has run, now we are left to copy 4 bytes at a time
		while (length >= 4)
		{
			*(u32 *)destination = *(u32 *)source;
			destination += 4;
			source += 4;
			length -= 4;
		}
	}

	//and then there is the MEM1 issue, which means single bytes they also need to be copied by 4 bytes
	if (destination < (void *)0x1800000)
	{
		for (; length != 0; length--)
		{
			u32 *address = (u32 *)((u32)dest & (u32)~0x03);
			u32 offset = 24 - ((u32)dest & 0x03) * 8;
			u8 data = *(u8 *)source;
			*address = (*address & (u32) ~(0xFF << (offset & 0xFF))) |
			           (data << (offset & 0xFF));
			destination++;
			source++;
		}
	}
	else
	{
		while (length != 0)
		{
			*(u8 *)destination = *(u8 *)source;
			destination++;
			source++;
			length--;
		}
	}

	return ret;
}

int memcmp(const void *s1, const void *s2, size_t len)
{
	size_t i;
	const unsigned char *p1 = (const unsigned char *)s1;
	const unsigned char *p2 = (const unsigned char *)s2;

	for (i = 0; i < len; i++)
		if (p1[i] != p2[i])
			return p1[i] - p2[i];

	return 0;
}

int strcmp(const char *s1, const char *s2)
{
	size_t i;

	for (i = 0; s1[i] && s1[i] == s2[i]; i++);

	return s1[i] - s2[i];
}

int strncmp(const char *s1, const char *s2, size_t n)
{
	size_t i;

	for (i = 0; i < n && s1[i] && s1[i] == s2[i]; i++);
	if (i == n)
		return 0;
	return s1[i] - s2[i];
}

// strncpy (with 0 padding if the source is shorter than length)
// IOS has a completely custom strncpy to deal with a MEM1 HW bug
// MEM1 accesses of 4 bytes (strb & strh) to MEM1 will thrash 4 bytes
// hence the weird access/seperation of code
char *strncpy(char *dest, const char *src, size_t maxlen)
{
	// if in mem1, work in u32 sized chunks
	if ((u32)dest < 0x01800000)
	{
		u32 index = 0;
		u32 destination = (u32)dest;
		while (index < maxlen)
		{
			u32 *address = (u32 *)(destination & (u32)~0x03);
			u32 offset = 24 - (destination & 0x03) * 8;
			u32 value = (*address & (u32) ~(0xFF << offset)) | ((src[index]) << offset);
			*address = value;
			destination++;
			if (src[index] == '\0')
				break;
			index++;
		}

		//add padding
		while (index < maxlen)
		{
			u32 *address = (u32 *)(destination & (u32)~0x03);
			u32 offset = 24 - ((u32)destination & 0x03) * 8;
			*address = (*address & (u32) ~(0xFF << offset));

			destination++;
			index++;
		}
	}
	// otherwise, normal strncpy
	else
	{
		size_t i = 0;
		for (; i < maxlen && src[i] != '\0'; ++i)
		{
			dest[i] = src[i];
		}

		//padding
		for (; i < maxlen; ++i)
		{
			dest[i] = 0;
		}
	}

	return dest;
}

size_t strlcpy(char *dest, const char *src, size_t maxlen)
{
	size_t len, needed;

	len = needed = strnlen(src, maxlen - 1) + 1;
	if (len >= maxlen)
		len = maxlen - 1;

	memcpy(dest, src, len);
	dest[len] = '\0';

	return needed - 1;
}

size_t strlcat(char *dest, const char *src, size_t maxlen)
{
	size_t used;

	used = strnlen(dest, maxlen - 1);
	return used + strlcpy(dest + used, src, maxlen - used);
}

char *strchr(const char *s, int c)
{
	size_t i;

	for (i = 0; s[i]; i++)
		if (s[i] == (char)c)
			return (char *)s + i;

	return NULL;
}

size_t strspn(const char *s1, const char *s2)
{
	size_t len = 0;
	const char *p;

	while (s1[len])
	{
		p = s2;
		while (*p)
		{
			if (s1[len] == *p)
				break;

			++p;
		}
		if (!*p)
			return len;

		++len;
	}

	return len;
}

size_t strcspn(const char *s1, const char *s2)
{
	size_t len = 0;
	const char *p;

	while (s1[len])
	{
		p = s2;
		while (*p)
			if (s1[len] == *p++)
				return len;

		++len;
	}

	return len;
}
