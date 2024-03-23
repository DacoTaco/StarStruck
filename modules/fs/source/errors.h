/*
	StarStruck - a Free Software reimplementation for the Nintendo/BroadOn IOS.
	Copyright (C) 2022	DacoTaco

# This code is licensed to you under the terms of the GNU GPL, version 2;
# see file COPYING or http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt
*/

#ifndef __FS_ERRORS_H__
#define __FS_ERRORS_H__

#include "types.h"

#define NAND_RESULT_ACCESS			-1
#define NAND_RESULT_ECC_CRIT		-3
#define NAND_RESULT_CORRUPT			-4
#define NAND_RESULT_BUSY			-5
#define NAND_RESULT_EXISTS			-6
#define NAND_RESULT_INVALID			-8
#define NAND_RESULT_MAXBLOCKS		-9
#define NAND_RESULT_MAXFD			-10
#define NAND_RESULT_MAXFILES		-11
#define NAND_RESULT_NOEXISTS		-12
#define NAND_RESULT_NOTEMPTY		-13
#define NAND_RESULT_OPENFD			-14
#define NAND_RESULT_UNKNOWN			-64
#define NAND_RESULT_FATALERROR		-128

#define FS_EINVAL					-101				// Invalid path
#define FS_EACCESS					-102				// Permission denied
#define FS_ECORRUPT					-103				// Corrupted NAND
#define FS_EEXIST					-105				// File exists
#define FS_ENOENT					-106				// No such file or directory
#define FS_ENFILE					-107				// Too many fds open
#define FS_EFBIG					-108				// Max block count reached?
#define FS_EFDEXHAUSTED				-109				// Too many fds open
#define FS_ENAMELEN					-110				// Pathname is too long
#define FS_EFDOPEN					-111				// FD is already open
#define FS_EUNKN					-112				// ?????
#define FS_EAGAIN					-113				// ??????
#define FS_EIO						-114				// ECC error
#define FS_ENOTEMPTY				-115				// Directory not empty
#define FS_EDIRDEPTH				-116				// Max directory depth exceeded
#define FS_NOTIMPL					-117				// Not implemented error?
#define FS_EBUSY					-118				// Resource busy

s32 TranslateErrno(s32 errno);

#endif