/*
	StarStruck - a Free Software reimplementation for the Nintendo/BroadOn IOS.
	printk - printk implementation in ios

	Copyright (C) 2022	DacoTaco

# This code is licensed to you under the terms of the GNU GPL, version 2;
# see file COPYING or http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt
*/

#ifndef __MODULE_CORE_H__
#define __MODULE_CORE_H__

#include "types.h"

typedef struct ModuleInfo
{
    u32 _userIdHeader;
	u32 UserId;
	u32 _entrypointHeader;
	u32 EntryPoint;
	u32 _priorityHeader;
	u32 Priority;
	u32 _stackSizeHeader;
	u32 StackSize;
	u32 _stackAddressHeader;
	u32 StackAddress;
} __attribute__((packed)) ModuleInfo;

CHECK_OFFSET(struct ModuleInfo, 0x04, UserId);
CHECK_OFFSET(struct ModuleInfo, 0x0C, EntryPoint);
CHECK_OFFSET(struct ModuleInfo, 0x14, Priority);
CHECK_OFFSET(struct ModuleInfo, 0x1C, StackSize);
CHECK_OFFSET(struct ModuleInfo, 0x24, StackAddress);
CHECK_SIZE(struct ModuleInfo, 0x28);

#define MODULE_DATA __attribute__ ((section (".module.data")))

//unused as we generate the module info in link script, but shows how the module info is structured in the notes section of the elf
#define MODULE_INFO(userId, entrypoint, priority, stackAddress, stackSize) const struct ModuleInfo moduleInfo  __attribute__ ((section (".note"))) = \
{ ._userIdHeader = 0x0B, .UserId = userId, \
 ._entrypointHeader = 0x09, .EntryPoint = entrypoint, \
 ._priorityHeader = 0x7D, .Priority = priority, \
 ._stackAddressHeader = 0x7F, .StackAddress = stackAddress, \
 ._stackSizeHeader = 0x7E, .StackSize = stackSize }

#endif