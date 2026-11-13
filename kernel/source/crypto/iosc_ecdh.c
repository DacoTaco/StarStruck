/*
	starstruck - a Free Software reimplementation for the Nintendo/BroadOn IOS.
	iosc_ecdh - iosc's eliptic curve Diffie-Hellman implementation

	Copyright (C) 2021	DacoTaco

# This code is licensed to you under the terms of the GNU GPL, version 2;
# see file COPYING or http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt
*/

#include <ios/iosc.h>
#include <ios/sha.h>
#include <string.h>

#include "crypto/iosc_ecdh.h"
#include "crypto/iosc_hash.h"
#include "crypto/ellipticCurve/curvePoint.h"
#include "crypto/ellipticCurve/uint233.h"

// the sect233r1 curve y^2 + xy = x^3 + x^2 + b and its generator, filled in by InitSect233r1Parameters
static CurveParameters _curveParameters;
static CurvePoint _generatorG;
static CurvePoint _curvePointCombTable[16];

static void InitSect233r1Parameters(void)
{
	_curveParameters = (CurveParameters){
		.Scalar = 1,
		.CoefficientA = { 0, 0, 0, 0, 0, 0, 0, 1 },
		.CoefficientB = { 0x66, 0x647ede6c, 0x332c7f8c, 0x923bb58, 0x213b333b, 0x20e9ce42, 0x81fe115f, 0x7d8f90ad },
	};
	_generatorG = (CurvePoint){
		.XCoordinate = { 0xfa, 0xc9dfcbac, 0x8313bb21, 0x39f1bb75, 0x5fef65bc, 0x391f8b36, 0xf8f8eb73, 0x71fd558b },
		.YCoordinate = { 0x100, 0x6a08a419, 0x3350678, 0xe58528be, 0xbf8a0bef, 0xf867a7ca, 0x36716f7e, 0x1f81052 },
	};

	// entry d = (d0 + d1 * 2^64 + d2 * 2^128 + d3 * 2^192) * G, where d0..d3 are the bits of d
	// 0: the point at infinity
	_curvePointCombTable[0] = (CurvePoint){
		.XCoordinate = { 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0 },
		.YCoordinate = { 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0 },
	};
	// 1: G
	_curvePointCombTable[1] = (CurvePoint){
		.XCoordinate = { 0xfa, 0xc9dfcbac, 0x8313bb21, 0x39f1bb75, 0x5fef65bc, 0x391f8b36, 0xf8f8eb73, 0x71fd558b },
		.YCoordinate = { 0x100, 0x6a08a419, 0x3350678, 0xe58528be, 0xbf8a0bef, 0xf867a7ca, 0x36716f7e, 0x1f81052 },
	};
	// 2: (2^64) * G
	_curvePointCombTable[2] = (CurvePoint){
		.XCoordinate = { 0x123, 0xd731a036, 0x6b9ed354, 0x2323a649, 0x7cf718ab, 0x12fe27b, 0x5cb0f320, 0xf7b10aed },
		.YCoordinate = { 0x163, 0x6760ce8f, 0x4eaae9ba, 0xf12a1514, 0x67a7b3f4, 0xb92a0e61, 0xfa190593, 0x100a80 },
	};
	// 3: (1 + 2^64) * G
	_curvePointCombTable[3] = (CurvePoint){
		.XCoordinate = { 0x1c6, 0x40b411fe, 0xa77c6cdc, 0x4a587fc3, 0x46a40358, 0x7856edb0, 0x193ea71f, 0xa7524b93 },
		.YCoordinate = { 0x104, 0xa659102f, 0x34b0bb8, 0xafb5afa6, 0x1b8adf8c, 0xb6cca538, 0x631bf941, 0x3c5d1bfa },
	};
	// 4: (2^128) * G
	_curvePointCombTable[4] = (CurvePoint){
		.XCoordinate = { 0x84, 0xc44003bb, 0x3da842d2, 0x7022ef1c, 0x3264a00e, 0x70ed2f1a, 0x42027637, 0xe48a3a6d },
		.YCoordinate = { 0x7b, 0x91e07dd6, 0x9ef7f6a2, 0xe1f72622, 0x135f0d1d, 0xbbcc886e, 0x85ccd0f7, 0xda1f6d98 },
	};
	// 5: (1 + 2^128) * G
	_curvePointCombTable[5] = (CurvePoint){
		.XCoordinate = { 0x96, 0x15d88a99, 0xceb0b782, 0xc4794801, 0xe0378bfb, 0x96b102be, 0x4d8bc62b, 0xf640fd06 },
		.YCoordinate = { 0x1d2, 0xa79fa10b, 0x9e8d0426, 0xb22d2db7, 0x410004c8, 0xc5ef7bca, 0x4bd020a2, 0x7bc255c3 },
	};
	// 6: (2^64 + 2^128) * G
	_curvePointCombTable[6] = (CurvePoint){
		.XCoordinate = { 0xf8, 0x9335641a, 0x6d52d1b7, 0x1fe5286a, 0xb5d18eb1, 0xb2a6bf0a, 0x47e4a3fb, 0x95b18299 },
		.YCoordinate = { 0x114, 0x11918fc, 0xe2afb564, 0x384e6a3f, 0x4fa20321, 0xf7dce7e2, 0x316149dd, 0x83816459 },
	};
	// 7: (1 + 2^64 + 2^128) * G
	_curvePointCombTable[7] = (CurvePoint){
		.XCoordinate = { 0xd, 0xb61258ca, 0x728a0565, 0x128ea0f8, 0x3a67e3e7, 0xcb752032, 0x1d1b0144, 0x2e390e0f },
		.YCoordinate = { 0x8a, 0x90174ec0, 0x8bcaef7c, 0xafedb681, 0x3817f429, 0x9167bb6d, 0x5345180f, 0x9f17791d },
	};
	// 8: (2^192) * G
	_curvePointCombTable[8] = (CurvePoint){
		.XCoordinate = { 0x41, 0xa7ab0b40, 0x47234421, 0x54a87fc5, 0x8278556d, 0x83618e53, 0x1f9c4c7c, 0x3816c879 },
		.YCoordinate = { 0xba, 0x7a211a5d, 0xbc8de196, 0x48a65332, 0x907318e2, 0xd9205a76, 0xc64a2da1, 0x9f6972f2 },
	};
	// 9: (1 + 2^192) * G
	_curvePointCombTable[9] = (CurvePoint){
		.XCoordinate = { 0x122, 0x9a00ca04, 0x6dc401be, 0x2335bb37, 0x59c7c243, 0xf884dea8, 0xd835dfed, 0xeabd47 },
		.YCoordinate = { 0x82, 0xfbe8c10b, 0x7532ac2b, 0x6be6db88, 0xebcc7739, 0x2b267e5e, 0xa23a88ea, 0x69da2f73 },
	};
	// 10: (2^64 + 2^192) * G
	_curvePointCombTable[10] = (CurvePoint){
		.XCoordinate = { 0x100, 0x85e45b07, 0xe6ef1d6e, 0x87d6d582, 0x623db5d0, 0x65182781, 0xe5c3f825, 0x138ad7e8 },
		.YCoordinate = { 0x19f, 0xf9609cd0, 0x4c8fa4c5, 0x9862e9cb, 0x864f72d4, 0x362665dd, 0xc30643db, 0x59e4b08a },
	};
	// 11: (1 + 2^64 + 2^192) * G
	_curvePointCombTable[11] = (CurvePoint){
		.XCoordinate = { 0x17f, 0xcd50381e, 0xa66df765, 0x5cf43cf7, 0x3d0788a6, 0x8938b3e5, 0x5012df2b, 0x8c1589e5 },
		.YCoordinate = { 0x17, 0xf5719186, 0xd0eefc0c, 0x4aac1fd7, 0x95ce8eda, 0x81d8b45a, 0x3b132ee2, 0x770851d2 },
	};
	// 12: (2^128 + 2^192) * G
	_curvePointCombTable[12] = (CurvePoint){
		.XCoordinate = { 0xc1, 0x4b5349c9, 0x4c748262, 0xe39e7051, 0x71e5a2e8, 0xaf258bc9, 0x7efa83eb, 0x8ed8777e },
		.YCoordinate = { 0x19e, 0x923caf64, 0x5601af9b, 0xf5d1c6c, 0x48de1ee6, 0xf438e699, 0xe0a7651, 0xf834cfd1 },
	};
	// 13: (1 + 2^128 + 2^192) * G
	_curvePointCombTable[13] = (CurvePoint){
		.XCoordinate = { 0x156, 0xa419bc4, 0x84828ae2, 0xd0114081, 0x697d40, 0xc2bb840a, 0xb626775f, 0x40955dde },
		.YCoordinate = { 0x1e0, 0xafcb6b6a, 0x2efb3ae6, 0x68ec3fac, 0x4d9524b9, 0x8503e4fe, 0x3d0c3039, 0xbf9410ac },
	};
	// 14: (2^64 + 2^128 + 2^192) * G
	_curvePointCombTable[14] = (CurvePoint){
		.XCoordinate = { 0x149, 0x9459c5b9, 0x935f6c3b, 0x223fe1c0, 0x76ccb5e9, 0x9d810fbb, 0x8d051e14, 0x3fc6ad47 },
		.YCoordinate = { 0x119, 0x7d175b61, 0x4da9494c, 0x3fe040b9, 0x43017773, 0xd2061ea2, 0xe9c9356c, 0xff0f3d83 },
	};
	// 15: (1 + 2^64 + 2^128 + 2^192) * G
	_curvePointCombTable[15] = (CurvePoint){
		.XCoordinate = { 0x10b, 0x47a49b1f, 0xf7c75c45, 0xa9d578f2, 0xb07cc152, 0x80b18075, 0x9ae882b1, 0x8bc66c86 },
		.YCoordinate = { 0x1b4, 0xd626657d, 0x510fdcf0, 0xc0cff138, 0x75653e50, 0xd1465cc5, 0x49571cda, 0xec46b60 },
	};
}

// the x coordinate that is hashed.
// aligned for sha engine
static u8 PointStorage[SHA_BLOCK_SIZE] ALIGNED(SHA_BLOCK_SIZE) = { 0 };

static void HashEccSharedPoint(const UInt233 x, void* digestOut)
{
	FinalShaHash hashOutput;
	IOSCHashContext context;

	StoreUInt233(PointStorage, x);
	GenerateHash(&context, NULL, 0, InitShaState, hashOutput, -1, NULL);
	GenerateHash(&context, PointStorage, UINT233_NUM_BYTES, FinalizeShaState, hashOutput, -1, NULL);
	memcpy(digestOut, hashOutput, sizeof(FinalShaHash));
}

// out = the first outSize bytes of SHA-1(x of privateKey * publicKey), outSize has to be below 0x1f. Returns true when it is not and
// nothing is written. The private key is 30 bytes (masked to 233 bits), the public key is x || y (60 bytes) and is not validated at all
bool ECDH_ComputeSharedSecretHash(const ECCPrivateKey privateKey, const ECCPublicKey* publicKey, u8* out, u32 outSize)
{
	UInt233 digest;
	CurvePoint point;
	UInt233 sharedX;
	UInt233 scalar;

	if (outSize >= sizeof(ECCPrivateKey) + 1)
		return true;

	InitSect233r1Parameters();
	LoadUInt233(privateKey, scalar);
	LoadUInt233Pair((const u8*)publicKey, sizeof(ECCPublicKey), (UInt233*)&point);
	MultiplyPointByScalarX(&_generatorG, &_curveParameters, &point, scalar, sharedX);
	HashEccSharedPoint(sharedX, digest);
	memcpy(out, digest, outSize);

	return false;
}
