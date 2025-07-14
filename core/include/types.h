/*
	mini - a Free Software replacement for the Nintendo/BroadOn IOS.
	types, memory areas, etc

Copyright (C) 2008, 2009	Hector Martin "marcan" <marcan@marcansoft.com>

# This code is licensed to you under the terms of the GNU GPL, version 2;
# see file COPYING or http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt
*/

#ifndef __TYPES_H__
#define __TYPES_H__

#include <stddef.h>
#include <stdbool.h>

typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned int u32;
typedef unsigned long long u64;

typedef signed char s8;
typedef signed short s16;
typedef signed int s32;
typedef signed long long s64;

typedef volatile unsigned char vu8;
typedef volatile unsigned short vu16;
typedef volatile unsigned int vu32;
typedef volatile unsigned long long vu64;

typedef volatile signed char vs8;
typedef volatile signed short vs16;
typedef volatile signed int vs32;
typedef volatile signed long long vs64;

typedef u32 size_t;

#ifdef __cplusplus
#define StaticAssert static_assert
#else
#define StaticAssert _Static_assert
#endif

#define NULL       ((void *)0)
#define ALIGNED(x) __attribute__((aligned(x)))

#define STACK_ALIGN(type, name, cnt, alignment)                                   \
	u8 _al__##name[((sizeof(type) * (cnt)) + (alignment) +                        \
	                (((sizeof(type) * (cnt)) % (alignment)) > 0 ?                 \
	                     ((alignment) - ((sizeof(type) * (cnt)) % (alignment))) : \
	                     0))];                                                    \
	type *name = (type *)(((u32)(_al__##name)) +                                  \
	                      ((alignment) - (((u32)(_al__##name)) & ((alignment) - 1))))

#define INT_MAX             ((int)0x7fffffff)
#define UINT_MAX            ((unsigned int)0xffffffff)

#define LONG_MAX            INT_MAX
#define ULONG_MAX           UINT_MAX

#define LLONG_MAX           0x7fffffffffffffffLL
#define ULLONG_MAX          0xffffffffffffffffULL

#define ARRAY_LENGTH(array) (sizeof(array) / sizeof((array)[0]))

#define CHECK_SIZE(Type, Size) \
	StaticAssert(sizeof(Type) == Size, #Type " must be " #Size " bytes")

#define CHECK_OFFSET(Type, Offset, Field)         \
	StaticAssert(offsetof(Type, Field) == Offset, \
	             #Type "::" #Field " must be at offset " #Offset)

#endif
