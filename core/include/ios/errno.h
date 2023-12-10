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
#define IPC_EINTR					-3					// Waiting operation was interrupted
#define IPC_EINVAL					-4					// Invalid argument or fd
#define IPC_EMAX					-5  				// Too many file descriptors open
#define IPC_ENOENT					-6					// File not found
#define IPC_EQUEUEEMPTY				-7					// Queue Empty
#define IPC_EQUEUEFULL				-8					// Queue full
#define IPC_EAGAIN					-11					// ?????
#define IPC_EIO						-12					// ECC error
#define IPC_EUNKN					-13					// ?????
#define IPC_EDIRDEPTH				-20					// Max directory depth exceeded
#define IPC_ENOMEM					-22					// Alloc failed during request

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

#define IOS_EINVAL					-0x3004
#define IOS_EBADVERSION				-0x3100
#define IOS_ETOOMANYVIEWS			-0x3101
#define IOS_EMISMATCH				-0x3102