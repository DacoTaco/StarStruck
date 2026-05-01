/*
	StarStruck - a Free Software reimplementation for the Nintendo/BroadOn IOS.
	Copyright (C) 2022	DacoTaco

# This code is licensed to you under the terms of the GNU GPL, version 2;
# see file COPYING or http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt
*/

#pragma once

#include <fs/errors.h>

#include "types.h"

#define NAND_RESULT_ACCESS     -1
#define NAND_RESULT_ECC_CRIT   -3
#define NAND_RESULT_CORRUPT    -4
#define NAND_RESULT_BUSY       -5
#define NAND_RESULT_EXISTS     -6
#define NAND_RESULT_INVALID    -8
#define NAND_RESULT_MAXBLOCKS  -9
#define NAND_RESULT_MAXFD      -10
#define NAND_RESULT_MAXFILES   -11
#define NAND_RESULT_NOEXISTS   -12
#define NAND_RESULT_NOTEMPTY   -13
#define NAND_RESULT_OPENFD     -14
#define NAND_RESULT_UNKNOWN    -64
#define NAND_RESULT_FATALERROR -128

s32 TranslateErrno(s32 errno);