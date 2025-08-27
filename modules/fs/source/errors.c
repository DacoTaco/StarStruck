/*
	StarStruck - a Free Software reimplementation for the Nintendo/BroadOn IOS.
	Copyright (C) 2022	DacoTaco

# This code is licensed to you under the terms of the GNU GPL, version 2;
# see file COPYING or http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt
*/

#include <ios/errno.h>
#include <ios/processor.h>
#include <ios/syscalls.h>
#include <ios/printk.h>

#include "errors.h"

s32 TranslateErrno(s32 errno)
{
	switch (errno)
	{
		default:
			return FS_NOTIMPL;
		case IPC_SUCCESS:
			return IPC_SUCCESS;
		case IPC_EINVAL:
			return FS_EINVAL;
		case IPC_ECC:
			return FS_EAGAIN;
		case IPC_ECC_CRIT:
			return FS_EIO;
		case IPC_BADBLOCK:
			return FS_BADBLOCK;
		case IPC_CHECKVALUE:
			return FS_AUTHENTICATION;
	}
}