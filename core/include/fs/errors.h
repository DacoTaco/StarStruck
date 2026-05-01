/*
	StarStruck - a Free Software reimplementation for the Nintendo/BroadOn IOS.
	Copyright (C) 2022	DacoTaco

# This code is licensed to you under the terms of the GNU GPL, version 2;
# see file COPYING or http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt
*/

#define FS_EINVAL         -101 // Invalid path
#define FS_EACCESS        -102 // Permission denied
#define FS_ECORRUPT       -103 // Corrupted NAND
#define FS_NOFILESYSTEM   -104 // No filesystem found
#define FS_EEXIST         -105 // File exists
#define FS_ENOENT         -106 // No such file or directory
#define FS_NO_INODES      -107 // No free inodes available
#define FS_EFBIG          -108 // Max block count reached?
#define FS_EFDEXHAUSTED   -109 // Too many fds open
#define FS_ENAMELEN       -110 // Pathname is too long
#define FS_EFDOPEN        -111 // FD is already open
#define FS_BADBLOCK       -112 // Bad block
#define FS_EAGAIN         -113 // ECC error, but corrected. retry operation
#define FS_EIO            -114 // ECC error
#define FS_ENOTEMPTY      -115 // Directory not empty
#define FS_AUTHENTICATION -116 // Authentication or access error
#define FS_NOTIMPL        -117 // Not implemented error?
#define FS_EBUSY          -118 // Resource busy
#define FS_ESHUTDOWN      -119 // FS is shutting down