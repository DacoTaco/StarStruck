/*
	starstruck - a Free Software reimplementation for the Nintendo/BroadOn IOS.
	seeprom - the seeprom chip

	Copyright (C) 2021	DacoTaco

# This code is licensed to you under the terms of the GNU GPL, version 2;
# see file COPYING or http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt
*/

#ifndef __SEEPROM_H__
#define __SEEPROM_H__

#include <types.h>
#include "crypto/otp.h"
#include "crypto/boot2.h"
#include "crypto/nand.h"

void SEEPROM_GetCommonKey(u8 data[OTP_COMMONKEY_SIZE]);
void SEEPROM_GetIdsAndNg(char ms_id_str[0x40], char ca_id_str[0x40], u32* ng_key_id, char ng_id_str[0x40], u8 ng_signature[60]);

s32 SEEPROM_GetPRNGSeed(void);
s32 SEEPROM_UpdatePRNGSeed(void);

s32 SEEPROM_BOOT2_GetCounter(BOOT2_Counter* data, s32* counter_write_index);
s32 SEEPROM_BOOT2_UpdateCounter(const BOOT2_Counter* data, s32 counter_write_index);

s32 SEEPROM_NAND_GetCounter(NAND_Counter* data, s32* counter_write_index);
s32 SEEPROM_NAND_UpdateCounter(const NAND_Counter* data, s32 counter_write_index);

#endif
