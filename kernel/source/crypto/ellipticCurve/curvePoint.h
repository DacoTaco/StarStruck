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

// the curve y^2 + xy = x^3 + a*x^2 + b, only the coefficient a is read by the point arithmetic
typedef struct
{
	// 1 from ECC_InitSect233r1Parameters. IOS only writes the result of a point operation when it is not 0, its real purpose is unknown
	u16 Scalar;
	u16 padding;
	UInt233 CoefficientA;
	UInt233 CoefficientB;
} CurveParameters;
CHECK_OFFSET(CurveParameters, 0x00, Scalar);
CHECK_OFFSET(CurveParameters, 0x04, CoefficientA);
CHECK_OFFSET(CurveParameters, 0x24, CoefficientB);
CHECK_SIZE(CurveParameters, 0x44);

void CopyCurvePoint(const CurvePoint* src, CurvePoint* dst);
void PointDouble(const CurvePoint* src, CurvePoint* dst, const CurveParameters* parameters);
void PointAdd(const CurvePoint* p1, const CurvePoint* p2, CurvePoint* out, const CurveParameters* parameters);
void PointSubtract(const CurvePoint* a, const CurvePoint* b, CurvePoint* out, const CurveParameters* parameters);
void MultiplyPointByScalar(const UInt233 scalar, const CurvePoint* point, CurvePoint* result,
                           const CurveParameters* parameters);
s32 MultiplyPointByScalarX(const CurvePoint* generator, const CurveParameters* parameters,
                           const CurvePoint* point, const UInt233 scalar, UInt233 outX);
