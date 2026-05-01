/*
	StarStruck - a Free Software reimplementation for the Nintendo/BroadOn IOS.
	FS Types - Shared FS types used by the FS module

	Copyright (C) 2026	DacoTaco

# This code is licensed to you under the terms of the GNU GPL, version 2;
# see file COPYING or http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt
*/

#pragma once

typedef enum
{
	IOCTLV_READDIR = 0x04,
	IOCTLV_GETUSAGE = 0x0C,
	IOCTLV_MASSCREATE = 0x0E,
} FSIoctlvCommands;

typedef enum
{
	IOCTL_FORMAT = 0x01,
	IOCTL_GETSTATS = 0x02,
	IOCTL_CREATEDIR = 0x03,
	IOCTL_SETATTR = 0x05,
	IOCTL_GETATTR = 0x06,
	IOCTL_DELETE = 0x07,
	IOCTL_RENAME = 0x08,
	IOCTL_CREATEFILE = 0x09,
	IOCTL_SETFILEVERCTRL = 0x0A,
	IOCTL_GETFILESTATS = 0x0B,
	IOCTL_SHUTDOWN = 0x0D,
} FSIoctlCommands;