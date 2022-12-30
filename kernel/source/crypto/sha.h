/*
	starstruck - a Free Software reimplementation for the Nintendo/BroadOn IOS.
	sha - the sha engine in starlet

	Copyright (C) 2021	DacoTaco

# This code is licensed to you under the terms of the GNU GPL, version 2;
# see file COPYING or http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt
*/

#ifndef __SHA_H__
#define __SHA_H__

#define SHA_DEVICE_NAME "/dev/sha"
#define SHA_DEVICE_NAME_SIZE 9

void ShaEngineHandler(void);

#endif