/*
	starstruck - a Free Software reimplementation for the Nintendo/BroadOn IOS.
	Copyright (C) 2023	DacoTaco

# This code is licensed to you under the terms of the GNU GPL, version 2;
# see file COPYING or http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt
*/

PHDRS
{
	crypto		PT_LOAD FLAGS(0x00000007) /* Read | Write | Execute */;
}

MEMORY 
{
	crypto		: ORIGIN = 0x13A70000, LENGTH = 128K
}

__crypto_addr = ORIGIN(crypto);
__crypto_size = LENGTH(crypto);

SECTIONS
{
	.crypto.bss (NOLOAD):
	{
		aes* sha* hmac* iosc* keyring* (.dynbss)
		aes* sha* hmac* iosc* keyring* (.gnu.linkonce.b*)
		aes* sha* hmac* iosc* keyring* (.bss*)
		aes* sha* hmac* iosc* keyring* (COMMON)
		. = ALIGN(4);
	} > crypto :crypto

	.crypto : ALIGN(0x40)
	{
		*(.crypto.text*)
		aes* sha* hmac* iosc* keyring* (.text .text.* .gnu.linkonce.t*)
		. = ALIGN(4);
		*(.crypto.data*)
		aes* sha* hmac* iosc* keyring* (.rodata)
		aes* sha* hmac* iosc* keyring* (.roda)
		aes* sha* hmac* iosc* keyring* (.rodata.*)
		aes* sha* hmac* iosc* keyring* (.data)
		aes* sha* hmac* iosc* keyring* (.data.*)
		aes* sha* hmac* iosc* keyring* (.gnu.linkonce.d*)
		. = ALIGN(4);
	} > crypto :crypto
}