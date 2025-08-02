/*
	starstruck - a Free Software reimplementation for the Nintendo/BroadOn IOS.
	calls - blocking filedescriptor syscalls

	Copyright (C) 2021	DacoTaco

# This code is licensed to you under the terms of the GNU GPL, version 2;
# see file COPYING or http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt
*/

#include "calls.h"

// tells calls_inner.h to produce the <name>FD syscall functions in its include, with this template
// they all share this exact shape, except Open
#define WRAP_INNER_CALL(rettype, name, arguments)                                 \
	rettype name##FD(ARGEXTRACT_DO(ARGEXTRACT_FULL arguments))                    \
	{                                                                             \
		const u32 state = DisableInterrupts();                                    \
		const rettype ret =                                                       \
		    name##FD_Inner(ARGEXTRACT_DO(ARGEXTRACT_EVEN arguments), NULL, NULL); \
		RestoreInterrupts(state);                                                 \
		return ret;                                                               \
	}

#include "calls_inner.h"

// OpenFD_Inner doesn't have or take a MessageQueue/IpcMessage pointer
s32 OpenFD(const char *path, int mode)
{
	const u32 state = DisableInterrupts();
	const s32 ret = OpenFD_Inner(path, mode);
	RestoreInterrupts(state);
	return ret;
}