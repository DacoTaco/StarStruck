/*
	starstruck - a Free Software reimplementation for the Nintendo/BroadOn IOS.
	curvePoint - the sect233r1 curve points of iosc's elliptic curve code

	Copyright (C) 2021	DacoTaco

# This code is licensed to you under the terms of the GNU GPL, version 2;
# see file COPYING or http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt
*/

#include "curvePoint.h"

static inline bool IsInfinity(const CurvePoint* point)
{
	u32 bits = 0;
	for (u32 index = 0; index < UINT233_WORDS; index++)
		bits |= point->XCoordinate[index] | point->YCoordinate[index];
	return bits == 0;
}

void CopyCurvePoint(const CurvePoint* src, CurvePoint* dst)
{
	CopyUInt233(src->XCoordinate, dst->XCoordinate);
	CopyUInt233(src->YCoordinate, dst->YCoordinate);
}

// dst = 2 * src. lambda = x + y / x, x3 = lambda^2 + lambda + a, y3 = x^2 + (lambda + 1) * x3. x = 0 gives the point at infinity.
// Like IOS dst is written before src.x is read again, so dst must not alias src.
// IOS only writes the result when parameters->Scalar != 0. It is always 1 (ECC_InitSect233r1Parameters), so that check is left out
void PointDouble(const CurvePoint* src, CurvePoint* dst, const CurveParameters* parameters)
{
	UInt233 inverse;
	UInt233 lambda;
	UInt233 yOverX;
	UInt233 lambdaSquared;
	UInt233 product;

	if (IsZeroUInt233(src->XCoordinate))
	{
		ClearUInt233(dst->XCoordinate);
		ClearUInt233(dst->YCoordinate);
		return;
	}

	InvertField(src->XCoordinate, inverse);
	MultiplyField(inverse, src->YCoordinate, yOverX);
	XorUInt233(src->XCoordinate, yOverX, lambda);
	MultiplyField(lambda, lambda, lambdaSquared);

	if (parameters->Scalar == 0)
		return;

	for (u32 index = 0; index < UINT233_WORDS; index++)
		dst->XCoordinate[index] = lambda[index] ^ lambdaSquared[index] ^ parameters->CoefficientA[index];

	// lambda + 1
	lambda[UINT233_WORDS - 1] ^= 1;
	MultiplyField(lambda, dst->XCoordinate, product);
	// x^2, the inverse is reused as the result
	MultiplyField(src->XCoordinate, src->XCoordinate, inverse);
	XorUInt233(inverse, product, dst->YCoordinate);
}

// out = a - b = a + (-b), -b = (b.x, b.x + b.y)
void PointSubtract(const CurvePoint* a, const CurvePoint* b, CurvePoint* out, const CurveParameters* parameters)
{
	CurvePoint negated;
	CopyUInt233(b->XCoordinate, negated.XCoordinate);
	ClearUInt233(negated.YCoordinate);
	XorUInt233(b->XCoordinate, b->YCoordinate, negated.YCoordinate);
	PointAdd(a, &negated, out, parameters);
}

// out = p1 + p2. (0,0) is the identity: it gives the other point. p1 == p2 doubles, x1 == x2 with different y (p2 = -p1) gives (0,0).
// Otherwise lambda = (y1 + y2) / (x1 + x2), x3 = lambda^2 + lambda + x1 + x2 + a, y3 = lambda * (x1 + x3) + x3 + y1.
// Like IOS out.x is written before p1.x and p1.y are read for y3, so out must not alias p1.
// IOS only writes the result when parameters->Scalar != 0, see PointDouble
void PointAdd(const CurvePoint* p1, const CurvePoint* p2, CurvePoint* out, const CurveParameters* parameters)
{
	UInt233 xSum;
	UInt233 ySum;
	UInt233 inverse;
	UInt233 lambda;
	UInt233 lambdaSquared;

	if (IsInfinity(p1))
	{
		CopyCurvePoint(p2, out);
		return;
	}

	if (IsInfinity(p2))
	{
		CopyCurvePoint(p1, out);
		return;
	}

	if (EqualUInt233(p1->XCoordinate, p2->XCoordinate) && EqualUInt233(p1->YCoordinate, p2->YCoordinate))
	{
		PointDouble(p1, out, parameters);
		return;
	}

	ClearUInt233(xSum);
	ClearUInt233(ySum);
	XorUInt233(p1->XCoordinate, p2->XCoordinate, xSum);
	XorUInt233(p1->YCoordinate, p2->YCoordinate, ySum);
	if (IsZeroUInt233(xSum))
	{
		ClearUInt233(out->XCoordinate);
		ClearUInt233(out->YCoordinate);
		return;
	}

	InvertField(xSum, inverse);
	MultiplyField(inverse, ySum, lambda);
	MultiplyField(lambda, lambda, lambdaSquared);

	if (parameters->Scalar == 0)
		return;

	for (u32 index = 0; index < UINT233_WORDS; index++)
		out->XCoordinate[index] = lambda[index] ^ lambdaSquared[index] ^ xSum[index] ^ parameters->CoefficientA[index];

	// x1 + x3, the product with lambda reuses lambdaSquared
	XorUInt233(p1->XCoordinate, out->XCoordinate, xSum);
	MultiplyField(xSum, lambda, lambdaSquared);
	for (u32 index = 0; index < UINT233_WORDS; index++)
		out->YCoordinate[index] = lambdaSquared[index] ^ out->XCoordinate[index] ^ p1->YCoordinate[index];
}

// result = scalar * point with the signed digit (NAF) double-and-add: the scalar is recoded to digits -1, 0 and 1 (lowest first), then
// the point is doubled once per digit below the top one (always 1) and the point is added or subtracted for a 1 or -1.
// A digit of -1 is made by adding 1 to the scalar (the carry runs up through the words), so a run of ones becomes a single carry.
// IOS calls this "in place" but result is a separate point, IOS never passes the same point twice
void MultiplyPointByScalar(const UInt233 scalar, const CurvePoint* point, CurvePoint* result, const CurveParameters* parameters)
{
	CurvePoint doubled;
	UInt233 k;
	// 233 bits recode to at most 234 digits
	s8 digits[236];
	u16 digitCount;
	s16 top;
	s8 digit;
	s16 index;

	CopyUInt233(scalar, k);
	if (IsZeroUInt233(k))
	{
		ClearUInt233(result->XCoordinate);
		ClearUInt233(result->YCoordinate);
		return;
	}

	digitCount = 0;
	while (!IsZeroUInt233(k))
	{
		if ((k[UINT233_WORDS - 1] & 1) != 0)
		{
			// k mod 4 = 1 gives 1, k mod 4 = 3 gives -1
			digit = (s8)(2 - (k[UINT233_WORDS - 1] & 3));
			digits[digitCount] = digit;
			if ((digit * 0x1000000) < 0)
			{
				// k += 1, the carry runs up through the words
				for (index = UINT233_WORDS - 1; index >= 0; index--)
				{
					k[index]++;
					if (k[index] != 0)
						break;
				}
			}
		}
		else
		{
			digits[digitCount] = 0;
		}

		// k is even now, so this is k / 2
		k[UINT233_WORDS - 1] &= 0xFFFFFFFE;
		HalveUInt233(k);
		digitCount++;
	}

	// the top digit is 1 (k ended at 1), so it is the point itself
	top = (s16)(digitCount - 1);
	CopyCurvePoint(point, result);
	while (top > 0)
	{
		PointDouble(result, &doubled, parameters);
		top--;
		s8 digitCode = digits[top];
		if (digitCode == 0)
			CopyCurvePoint(&doubled, result);
		else if (digitCode < 1)
		{
			if (digitCode == -1)
				PointSubtract(&doubled, point, result, parameters);
		}
		else if (digitCode == 1)
			PointAdd(point, &doubled, result, parameters);
	}
}

s32 MultiplyPointByScalarX(const CurvePoint* generator __attribute__((unused)), const CurveParameters* parameters,
                           const CurvePoint* point, const UInt233 scalar, UInt233 outX)
{
	CurvePoint result;
	MultiplyPointByScalar(scalar, point, &result, parameters);
	CopyUInt233(result.XCoordinate, outX);
	return 0;
}
