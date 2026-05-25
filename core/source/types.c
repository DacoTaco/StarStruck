/*
	StarStruck - a Free Software reimplementation for the Nintendo/BroadOn IOS.
	type utilities

	Copyright (C) 2024	DacoTaco

# This code is licensed to you under the terms of the GNU GPL, version 2;
# see file COPYING or http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt
*/

#include "types.h"

void hextou32(const char* str, u32* out)
{
	u32 shift = 28;
	*out = 0;

	while (*str != '\0')
	{
		u8 c = (u8)*str;
		u8 nibble;

		if (c >= '0' && c <= '9')
			nibble = c - '0';
		else if (c >= 'A' && c <= 'F')
			nibble = c - 'A' + 10;
		else if (c >= 'a' && c <= 'f')
			nibble = c - 'a' + 10;
		else
			break;

		*out |= (u32)nibble << shift;

		if (shift < 4)
			break;
		shift -= 4;
		str++;
	}
}