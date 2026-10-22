/*
	starstruck - a Free Software reimplementation for the Nintendo/BroadOn IOS.
	curvePoint - the sect233r1 curve points of iosc's elliptic curve code

	Copyright (C) 2021	DacoTaco

# This code is licensed to you under the terms of the GNU GPL, version 2;
# see file COPYING or http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt
*/

#pragma once

#include <types.h>

#include "uint233.h"

// an affine point, (0, 0) is the point at infinity
typedef struct
{
	UInt233 XCoordinate;
	UInt233 YCoordinate;
} CurvePoint;
CHECK_OFFSET(CurvePoint, 0x00, XCoordinate);
CHECK_OFFSET(CurvePoint, 0x20, YCoordinate);
CHECK_SIZE(CurvePoint, 0x40);
