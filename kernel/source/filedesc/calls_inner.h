/*
	starstruck - a Free Software reimplementation for the Nintendo/BroadOn IOS.
	resourceManager - manager to maintain all device resources

	Copyright (C);

# This code is licensed to you under the terms of the GNU GPL, version 2;
# see file COPYING or http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt
*/

#ifndef __FILEDESC_CALLS_INNER_H__
#define __FILEDESC_CALLS_INNER_H__

#include "filedesc_types.h"

#ifdef WRAP_INNER_CALL

#define ARGEXTRACT_END(...) ARGEXTRACT_END_(__VA_ARGS__)
#define ARGEXTRACT_END_(...) __VA_ARGS__##_END

#define ARGEXTRACT_LOOP_EVEN_A(...) ARGEXTRACT_LOOP_EVEN_BODY(__VA_ARGS__,) ARGEXTRACT_LOOP_EVEN_B
#define ARGEXTRACT_LOOP_EVEN_B(...) ARGEXTRACT_LOOP_EVEN_BODY(__VA_ARGS__,0) ARGEXTRACT_LOOP_EVEN_C
#define ARGEXTRACT_LOOP_EVEN_C(...) ARGEXTRACT_LOOP_EVEN_BODY(__VA_ARGS__,0) ARGEXTRACT_LOOP_EVEN_B
#define ARGEXTRACT_LOOP_EVEN_A_END
#define ARGEXTRACT_LOOP_EVEN_B_END
#define ARGEXTRACT_LOOP_EVEN_C_END

#define ARGEXTRACT_LOOP_FULL_A(...) ARGEXTRACT_LOOP_FULL_BODY(__VA_ARGS__,) ARGEXTRACT_LOOP_FULL_B
#define ARGEXTRACT_LOOP_FULL_B(...) ARGEXTRACT_LOOP_FULL_BODY(__VA_ARGS__,0) ARGEXTRACT_LOOP_FULL_C
#define ARGEXTRACT_LOOP_FULL_C(...) ARGEXTRACT_LOOP_FULL_BODY(__VA_ARGS__,0) ARGEXTRACT_LOOP_FULL_B
#define ARGEXTRACT_LOOP_FULL_A_END
#define ARGEXTRACT_LOOP_FULL_B_END
#define ARGEXTRACT_LOOP_FULL_C_END

#define ARGEXTRACT_LOOP_EVEN_BODY(x, y, ...) __VA_OPT__(,) y
#define ARGEXTRACT_LOOP_FULL_BODY(x, y, ...) __VA_OPT__(,) x y

#define DEFINE_FD_FUNCS(rettype, name, arguments) \
	rettype name ## FD_Inner(ARGEXTRACT_END( ARGEXTRACT_LOOP_FULL_A arguments ), MessageQueue* messageQueue, IpcMessage* message); \
	WRAP_INNER_CALL(rettype, name, arguments)

DEFINE_FD_FUNCS(int, Close, (s32, fd))
DEFINE_FD_FUNCS(int, Read, (s32, fd)(void *, buf)(u32, len))
DEFINE_FD_FUNCS(int, Write, (s32, fd)(const void *, buf)(u32, len))
DEFINE_FD_FUNCS(int, Seek, (s32, fd)(s32, offset)(s32, origin))
DEFINE_FD_FUNCS(int, Ioctl, (s32, fd)(u32, request_id)(void *, input_buffer)(u32, input_buffer_len)(void *, output_buffer)(u32, output_buffer_len))
DEFINE_FD_FUNCS(int, Ioctlv, (s32, fd)(u32, request_id)(u32, vector_count_in)(u32, vector_count_out)(IoctlvMessageData *, vectors))

#endif

s32 OpenFD_Inner(const char* path, int mode);

#endif
