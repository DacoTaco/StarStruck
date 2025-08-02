/*
	starstruck - a Free Software reimplementation for the Nintendo/BroadOn IOS.
	aes - the aes engine in starlet

	Copyright (C) 2021	DacoTaco

# This code is licensed to you under the terms of the GNU GPL, version 2;
# see file COPYING or http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt
*/

#pragma once
#ifndef MIOS

#define AES_DEVICE_NAME      "/dev/aes"
#define AES_DEVICE_NAME_SIZE sizeof(AES_DEVICE_NAME)

void AesEngineHandler(void);

#endif