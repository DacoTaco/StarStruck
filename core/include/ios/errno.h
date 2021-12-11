/*
	starstruck - a Free Software reimplementation for the Nintendo/BroadOn IOS.
	syscallcore - internal communications over software interrupts

	Copyright (C) 2021	DacoTaco

# This code is licensed to you under the terms of the GNU GPL, version 2;
# see file COPYING or http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt
*/

#define IPC_SUCCESS					0					// Success
#define IPC_EACCES					-1					// Permission denied
#define IPC_EEXIST					-2					// File exists
#define IPC_EINVAL					-4					// Invalid argument or fd
#define IPC_EMAX					-5  				// Too many file descriptors open
#define IPC_ENOENT					-6					// File not found
#define IPC_EQUEUEFULL				-8					// Queue full
#define IPC_EIO						-12					// ECC error
#define IPC_ENOMEM					-22					// Alloc failed during request

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
#define FS_EIO						-114				// ECC error
#define FS_ENOTEMPTY				-115				// Directory not empty
#define FS_EDIRDEPTH				-116				// Max directory depth exceeded
#define FS_EBUSY					-118				// Resource busy

#define ES_SHORT_READ				-1009				// Short read
#define ES_EIO						-1010				// Write failure
#define ES_INVALID_SIGNATURE_TYPE	-1012
#define ES_FD_EXHAUSTED				-1016				// Max of 3 ES handles exceeded
#define ES_EINVAL					-1017				// Invalid argument
#define ES_DEVICE_ID_MISMATCH		-1020
#define ES_HASH_MISMATCH			-1022				// Decrypted content hash doesn't match with the hash from the TMD
#define ES_ENOMEM					-1024				// Alloc failed during request
#define ES_EACCES					-1026				// Incorrect access rights (according to TMD)
#define ES_UNKNOWN_ISSUER			-1027
#define ES_NO_TICKET				-1028
#define ES_INVALID_TICKET			-1029

#define IOSC_EACCES					-2000
#define IOSC_EEXIST					-2001
#define IOSC_EINVAL					-2002
#define IOSC_EMAX					-2003
#define IOSC_ENOENT					-2004
#define IOSC_INVALID_OBJTYPE		-2005
#define IOSC_INVALID_RNG			-2006
#define IOSC_INVALID_FLAG			-2007
#define IOSC_INVALID_FORMAT			-2008
#define IOSC_INVALID_VERSION		-2009
#define IOSC_INVALID_SIGNER			-2010
#define IOSC_FAIL_CHECKVALUE		-2011
#define IOSC_FAIL_INTERNAL			-2012
#define IOSC_FAIL_ALLOC				-2013
#define IOSC_INVALID_SIZE			-2014
#define IOSC_INVALID_ADDR			-2015
#define IOSC_INVALID_ALIGN			-2016

#define USB_ECANCELED				-7022