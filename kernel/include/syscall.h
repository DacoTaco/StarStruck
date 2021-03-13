/*
	starstruck - a Free Software reimplementation for the Nintendo/BroadOn IOS.
	syscalls - internal communications over software interrupts

	Copyright (C) 2021	DacoTaco

# This code is licensed to you under the terms of the GNU GPL, version 2;
# see file COPYING or http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt
*/

void handle_syscall(u16 syscall, u32 parameter1, void** parameter2, u32 parameter3, void** parameter4);