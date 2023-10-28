/*
	starstruck - a Free Software reimplementation for the Nintendo/BroadOn IOS.
	calls_inner - internal filedescriptor syscalls handlers

	Copyright (C);

# This code is licensed to you under the terms of the GNU GPL, version 2;
# see file COPYING or http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt
*/

#ifndef __FILEDESC_CALLS_INNER_H__
#define __FILEDESC_CALLS_INNER_H__

#include "filedesc_types.h"
#include "interrupt/irq.h"

#ifndef WRAP_INNER_CALL
#define WRAP_INNER_CALL(...)
#endif

// begins the extraction:
// expands ARGEXTRACT_X arguments [aka ARGEXTRACT_X (abc, def)(ijk, lmn)] to
// ARGEXTRACT_X_LOOP_BODY(abc, def,) ARGEXTRACT_X_LOOP_B(ijk, lmn)_END
// aka ARGEXTRACT_X_LOOP_BODY(abc, def,) ARGEXTRACT_X_LOOP_BODY(ijk, lmn, 0) ARGEXTRACT_X_LOOP_C_END
// and so on for different lengths of 'arguments' tuple
#define ARGEXTRACT_DO(...) ARGEXTRACT_DO_(__VA_ARGS__)
#define ARGEXTRACT_DO_(...) __VA_ARGS__##_END

// runs ARGEXTRACT_X_LOOP_BODY, then puts the ARGEXTRACT_X_LOOP_Y name after.
// that name isn't expanded instantly because it's a function-like macro without parentheses after
// which avoids a recursive macro that wouldn't be expanded anymore once it reaches the same name
#define ARGEXTRACT_EVEN(...) ARGEXTRACT_EVEN_LOOP_BODY(__VA_ARGS__,) ARGEXTRACT_EVEN_LOOP_B
#define ARGEXTRACT_EVEN_LOOP_B(...) ARGEXTRACT_EVEN_LOOP_BODY(__VA_ARGS__,0) ARGEXTRACT_EVEN_LOOP_C
#define ARGEXTRACT_EVEN_LOOP_C(...) ARGEXTRACT_EVEN_LOOP_BODY(__VA_ARGS__,0) ARGEXTRACT_EVEN_LOOP_B
#define ARGEXTRACT_EVEN_END
#define ARGEXTRACT_EVEN_LOOP_B_END
#define ARGEXTRACT_EVEN_LOOP_C_END

#define ARGEXTRACT_FULL(...) ARGEXTRACT_FULL_LOOP_BODY(__VA_ARGS__,) ARGEXTRACT_FULL_LOOP_B
#define ARGEXTRACT_FULL_LOOP_B(...) ARGEXTRACT_FULL_LOOP_BODY(__VA_ARGS__,0) ARGEXTRACT_FULL_LOOP_C
#define ARGEXTRACT_FULL_LOOP_C(...) ARGEXTRACT_FULL_LOOP_BODY(__VA_ARGS__,0) ARGEXTRACT_FULL_LOOP_B
#define ARGEXTRACT_FULL_END
#define ARGEXTRACT_FULL_LOOP_B_END
#define ARGEXTRACT_FULL_LOOP_C_END

// used by the above macros to expand the tuples to a specific value, and have a marker for being the first
#define ARGEXTRACT_EVEN_LOOP_BODY(x, y, ...) __VA_OPT__(,) y
#define ARGEXTRACT_FULL_LOOP_BODY(x, y, ...) __VA_OPT__(,) x y

// this creates a function withe the _Inner suffix, and extracts every pair inside arguments to its argument list
#define DEFINE_FD_FUNCS(rettype, name, arguments) \
	rettype name ## FD_Inner(ARGEXTRACT_DO(ARGEXTRACT_FULL arguments), MessageQueue* messageQueue, IpcMessage* message); \
	WRAP_INNER_CALL(rettype, name, arguments)

DEFINE_FD_FUNCS(s32, Close, (s32, fd))
DEFINE_FD_FUNCS(s32, Read, (s32, fd)(void *, buf)(u32, len))
DEFINE_FD_FUNCS(s32, Write, (s32, fd)(const void *, buf)(u32, len))
DEFINE_FD_FUNCS(s32, Seek, (s32, fd)(s32, offset)(s32, origin))
DEFINE_FD_FUNCS(s32, Ioctl, (s32, fd)(u32, requestId)(void *, inputBuffer)(u32, inputBufferLength)(void *, outputBuffer)(u32, outputBufferLength))
DEFINE_FD_FUNCS(s32, Ioctlv, (s32, fd)(u32, requestId)(u32, vectorInputCount)(u32, vectorIOCount)(IoctlvMessageData *, vectors))

#endif

s32 OpenFD_Inner(const char* path, AccessMode mode);
