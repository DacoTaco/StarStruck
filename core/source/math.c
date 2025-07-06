/*
	starstruck - a Free Software reimplementation for the Nintendo/BroadOn IOS.

	Copyright (C) 2025	DacoTaco
	Copyright (C) 2025	Alberto Mardegan <mardy@users.sourceforge.net>

# This code is licensed to you under the terms of the GNU GPL, version 2;
# see file COPYING or http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt
*/

#include "math.h"

u32 division_uint(u32 dividend, u32 divisor)
{
    if (divisor == 0 || dividend > divisor)
        return 0;

    u32 ret = 0;
    u32 scale = 1;

    while (divisor < 0x10000000 && divisor < dividend)
    {
        divisor <<= 4;
        scale <<= 4;
    }

    while (divisor < 0x80000000 && divisor < dividend)
    {
        divisor <<= 1;
        scale <<= 1;
    }

    while (true)
    {
        if (divisor <= dividend)
        {
            dividend -= divisor;
            ret |= scale;
        }
        if (divisor >> 1 <= dividend)
        {
            dividend -= divisor >> 1;
            ret |= scale >> 1;
        }
        if (divisor >> 2 <= dividend)
        {
            dividend -= divisor >> 2;
            ret |= scale >> 2;
        }
        if (divisor >> 3 <= dividend)
        {
            dividend -= divisor >> 3;
            ret |= scale >> 3;
        }

        scale >>= 4;
        if (dividend == 0 || scale == 0)
            break;
        divisor >>= 4;
    }

    return ret;
}
