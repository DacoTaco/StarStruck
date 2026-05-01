/*
	StarStruck - a Free Software reimplementation for the Nintendo/BroadOn IOS.
	ES Module - shared type definitions.

	Copyright (C) 2026	DacoTaco

# This code is licensed to you under the terms of the GNU GPL, version 2;
# see file COPYING or http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt
*/

#pragma once

#include <types.h>

typedef enum
{
	SystemTitle = 0x00000001, /* IOS, System Menu, BC, MIOS, ... */
	ChannelTitle = 0x00010000, /* Installed via the Shop Channel */
	GameSaveTitle = 0x00010001, /* Disc-based game saves */
	SysChannelTitle = 0x00010002, /* System channels */
	WiiWareTitle = 0x00010004, /* WiiWare / game channels */
	DLCTitle = 0x00010005, /* Downloadable content */
	HiddenTitle = 0x00010008, /* Hidden channels (TOS, etc.) */
} TitleType;

typedef struct
{
	TitleType Type;
	u32 Id;
} TitleID;
CHECK_SIZE(TitleID, 0x08);
CHECK_OFFSET(TitleID, 0x00, Type);
CHECK_OFFSET(TitleID, 0x04, Id);

typedef struct
{
	TitleID Title;
	u32 UserId;
} TitleUIDEntry;
CHECK_SIZE(TitleUIDEntry, 0x0C);
CHECK_OFFSET(TitleUIDEntry, 0x00, Title);
CHECK_OFFSET(TitleUIDEntry, 0x08, UserId);