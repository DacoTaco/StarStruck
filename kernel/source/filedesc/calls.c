/*
	starstruck - a Free Software reimplementation for the Nintendo/BroadOn IOS.
	resourceManager - manager to maintain all device resources

	Copyright (C) 2021	DacoTaco

# This code is licensed to you under the terms of the GNU GPL, version 2;
# see file COPYING or http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt
*/

#include "calls.h"

#define WRAP_INNER_CALL(rettype, name, arguments) \
rettype name ## FD(ARGEXTRACT_END( ARGEXTRACT_LOOP_FULL_A arguments )) { \
	const s32 state = DisableInterrupts(); \
	const rettype ret = name ## FD_Inner(ARGEXTRACT_END( ARGEXTRACT_LOOP_EVEN_A arguments ), NULL, NULL); \
	RestoreInterrupts(state); \
	return ret; \
}

#include "calls_inner.h"

s32 OpenFD(const char* path, int mode)
{
	const s32 state = DisableInterrupts();
	const s32 ret = OpenFD_Inner(path, mode);
	RestoreInterrupts(state);
	return ret;
}
