/*
	starstruck - a Free Software reimplementation for the Nintendo/BroadOn IOS.
	powerpc - manage the Hollywood CPU

	Copyright (C) 2021	DacoTaco

# This code is licensed to you under the terms of the GNU GPL, version 2;
# see file COPYING or http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt
*/

#include <types.h>
#include <ios/processor.h>
#include <ios/printk.h>
#include <string.h>

#include "core/gpio.h"
#include "core/hollywood.h"
#include "utils/utils.h"
#include "memory/memory.h"

//code that will let me PPC start execution @ 0x80003400, which is the BS1 vector, where all dol content usually starts
const u32 PPC_LaunchBS1[0x10] = {
	0x3C600000, //lis r3, 0
	0x60633400, //ori r3, r3, 0x3400
	0x7C7A03A6, //mtspr 0x1a, r3
	0x38600000, //li r3, 0
	0x7C7B03A6, //mtspr 0x1b, r3
	0x4C000064, //rfi
	0x00000000, //padding
	0x00000000, 0x00000000, 0x00000000,
};

// GameCube compatibility mode boot stub: zeroes HID4, then jumps Broadway to 0x80000100 (exception vector base).
// HID4 = 0 leaves the Broadway in a minimal Gekko-compatible state (used by MIOS/BC path).
const u32 PPC_GameCubeResetVector[0x0C] = {
	0x7c631a78, //xor r3, r3, r3
	0x7c73fba6, //mtspr 0x3f3, r3
	0x4c00012c, //isync
	0x3c400000, //lis r2, 0
	0x60420100, //ori r2, r2, 0x100
	0x7c5a03a6, //mtspr 0x1a, r2
	0x38a00000, //li r5, 0
	0x7cbb03a6, //mtspr 0x1b, r5
	0x4c000064, //rfi
	0x60000000, //nop
	0x60000000, 0x60000000,
};

// Wii mode boot stub: sets HID4 = 0xD7B00000, then jumps Broadway to 0x80000100 (exception vector base).
// HID4 0xD7B00000 is the standard Broadway initialization enabling its full cache and memory controller features.
const u32 PPC_WiiResetVector[0x0D] = {
	0x7c631a78, //xor r3, r3, r3
	0x6463D7B0, //oris r3, r3, 0xd7b0
	0x7c73fba6, //mtspr 0x3f3, r3
	0x4c00012c, //isync
	0x3c400000, //lis r2, 0
	0x60420100, //ori r2, r2, 0x100
	0x7c5a03a6, //mtspr 0x1a, r2
	0x38a00000, //li r5, 0
	0x7cbb03a6, //mtspr 0x1b, r5
	0x4c000064, //rfi
	0x60000000, //nop
	0x60000000, 0x60000000,
};

#ifdef MIOS
#define MIOS_INLINE inline
#else
#define MIOS_INLINE
#endif

// Build date field: BCD-encoded DDMMYY packed as (DD << 16) | (MM << 8) | YY
// __DATE__ is "Mmm DD YYYY": [0-2]=month abbrev, [4-5]=day (space-padded), [7-10]=year
#define IOS_BUILDDATE(dd, mm, yy) (u32)(((dd) << 16) | ((mm) << 8) | (yy))
#define _BUILD_MONTH_BCD                                       \
	(__DATE__[2] == 'n' ? (__DATE__[1] == 'a' ? 0x01 : 0x06) : \
	 __DATE__[2] == 'b' ? 0x02 :                               \
	 __DATE__[2] == 'r' ? (__DATE__[0] == 'M' ? 0x03 : 0x04) : \
	 __DATE__[2] == 'y' ? 0x05 :                               \
	 __DATE__[2] == 'l' ? 0x07 :                               \
	 __DATE__[2] == 'g' ? 0x08 :                               \
	 __DATE__[2] == 'p' ? 0x09 :                               \
	 __DATE__[2] == 't' ? 0x10 :                               \
	 __DATE__[2] == 'v' ? 0x11 :                               \
	                      0x12)
#define _BUILD_DAY_BCD       ((__DATE__[4] == ' ' ? 0 : ((__DATE__[4] - '0') * 16)) + (__DATE__[5] - '0'))
#define _BUILD_YEAR_BCD      (((__DATE__[9] - '0') * 16) + (__DATE__[10] - '0'))
#define STARSTRUCK_BUILDDATE IOS_BUILDDATE(_BUILD_DAY_BCD, _BUILD_MONTH_BCD, _BUILD_YEAR_BCD)

void PPCHardReset(void)
{
	u32 resetValue = read32(HW_RESETS);
	clear32(HW_RESETS, SRSTB_CPU | RSTB_CPU);
	udelay(0x0F);
	set32(HW_RESETS, resetValue & SRSTB_CPU);
}

void PPCSoftReset(void)
{
	// enable the broadway IPC interrupt
	write32(HW_PPCIRQMASK, (1 << 30));
	clear32(HW_RESETS, SRSTB_CPU | RSTB_CPU);
	udelay(0x0F);
	set32(HW_RESETS, SRSTB_CPU);
	udelay(0x96);
	set32(HW_RESETS, SRSTB_CPU | RSTB_CPU);
}

void PPCLoad(const void* code, u32 codeSize)
{
	if (codeSize <= 1)
		return;

	u32 oldExiValue = read32(HW_EXICTRL);
	u32 sizeToCopy = 0x0F;
	if (codeSize > 0x10)
	{
		write32(HW_EXICTRL, read32(HW_EXICTRL) | 1);
		printk("Warning: only loading %d instrs into EXI boot code", 0x10);
	}
	else
		sizeToCopy = codeSize;

	u32 codeAddr = (u32)code;
	for (u32 addr = EXI_BOOT_BASE; addr < addr + sizeToCopy; addr += 4)
	{
		write32(addr, codeAddr);
		codeAddr += 4;
	}

	write32(HW_EXICTRL, oldExiValue);
}

void PPCLoadCode(bool wiiMode, const void* code, u32 codeSize)
{
	const void* codeToLaunch = NULL;
	if (code == NULL || codeSize == 0)
	{
		if (!wiiMode)
		{
			codeToLaunch = PPC_GameCubeResetVector;
			codeSize = sizeof(PPC_GameCubeResetVector);
		}
		else
		{
			codeToLaunch = PPC_WiiResetVector;
			codeSize = sizeof(PPC_WiiResetVector);
		}
	}
	else
		codeToLaunch = code;

	PPCLoad(codeToLaunch, codeSize);
	write32(HW_DIFLAGS, (read32(HW_DIFLAGS) & 0xFFEFFFFF) | DIFLAGS_BOOT_CODE);
}

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wnonnull"
#pragma GCC diagnostic ignored "-Warray-bounds"
MIOS_INLINE void PPCSetMEM1(u32 hollywoodVersion, u32 gddrVendorCode)
{
#ifndef MIOS
	// save the ios version so we can set it back after the wipe
	u32 iosVersion = read32(MEM1_IOSVERSION);
	memset((void*)MEM1_BASE, 0, 0x3FFF);
#else
	u32 iosVersion = 0x707;
#endif

	// MEM1 size and bounds
	write32(MEM1_PHYSICALMEM1SIZE, 0x01800000);
	write32(MEM1_SIMULATEDMEM1SIZE, 0x01800000);
	write32(MEM1_3108, 0x81800000);
	write32(MEM1_MEMORYSIZE, 0x01800000);
	write32(MEM1_SIMMEMORYSIZE, 0x01800000);
	write32(MEM1_AREASTART, 0);
	write32(MEM1_AREAEND, 0x81800000);
	write32(MEM1_HEAPLOW, 0);
	write32(MEM1_HEAPHIGH, 0x81800000);

	// MEM2 layout
#ifdef MIOS
	write32(MEM1_MEM2PHYSICALSIZE, 0x800000);
	write32(MEM1_MEM2SIMULATESIZE, 0x800000);
	write32(MEM1_MEM2INITLOW, MEM2_PHY2VIRT(0x10000800));
	write32(MEM1_MEM2INITHIGH, MEM2_PHY2VIRT(0x173E0000));
	write32(MEM1_IOSIPCLOW, MEM2_PHY2VIRT(0x173E0000));
	write32(MEM1_MEM2BAT, MEM2_PHY2VIRT(0x17400000));
	write32(MEM1_IOSIPCHIGH, MEM2_PHY2VIRT(0x17400000));
#else
	write32(MEM1_MEM2PHYSICALSIZE, 0x04000000);
	write32(MEM1_MEM2SIMULATESIZE, 0x04000000);
	write32(MEM1_MEM2INITLOW, 0x90000800);
	write32(MEM1_MEM2INITHIGH, 0x935E0000);
	write32(MEM1_IOSIPCLOW, 0x935E0000);
	write32(MEM1_MEM2BAT, 0x93600000);
	write32(MEM1_IOSIPCHIGH, 0x93600000);
	write32(MEM1_IOSHEAPLOW, 0x93600000);
	write32(MEM1_IOSHEAPHIGH, 0x93620000);
#endif

	// IOS identification
	write32(MEM1_CPUVERSION, hollywoodVersion);
	write32(MEM1_IOSVERSION, iosVersion);

	write32(MEM1_IOSBUILDDATE, STARSTRUCK_BUILDDATE);
	write32(MEM1_GDDRVENDORCODE, gddrVendorCode);

	// MIOS/GC compat flag: 0 = GC compat mode (SRSTB_CPU asserted), 1 = Wii mode
	write32(MEM1_MIOSFLAG, (read32(HW_RESETS) & SRSTB_CPU) ? 0 : 1);

	// uninitialized sentinel fields
	write32(MEM1_3114, 0xDEADBEEF);
	write32(MEM1_312C, 0xDEADBEEF);
	write32(MEM1_313C, 0xDEADBEEF);
	write32(MEM1_3150, 0xDEADBEEF);
	write32(MEM1_3154, 0xDEADBEEF);
	write32(MEM1_LOADMETHOD, 0xDEADBEEF);
	write32(MEM1_INITSEMAPHORE, 0xDEADBEEF);
#ifdef MIOS
	write32(MEM1_IOSHEAPLOW, 0xDEADBEEF);
	write32(MEM1_IOSHEAPHIGH, 0xDEADBEEF);
#endif
}
#pragma GCC diagnostic pop

#ifndef MIOS

void PPCPrepareMEM1(void)
{
	PPCSetMEM1(GetHollywoodId(), GetGDDRVendorCode());
	DCFlushRange((void*)MEM1_PHYSICALMEM1SIZE, 0x68);
}

#endif

void PPCSetSDKSemaphore(bool hasSemaphore)
{
	u32 oldValue = read32(HW_EXICTRL);
	write32(MEM1_INITSEMAPHORE, hasSemaphore ? 0 : 0xDEADBEEF);
	DCFlushRange(0, 4);
	write32(HW_EXICTRL, (oldValue & 0xFFFFFFFE) | (hasSemaphore != false));
}

void PPCStart(void)
{
	u8 ppcInitCode[sizeof(PPC_LaunchBS1)];
	memcpy(ppcInitCode, PPC_LaunchBS1, sizeof(ppcInitCode));

#ifdef MIOS
	const bool isWiiMode = false;
	PPCSetMEM1(0x101, 0xcafebabe);

	write32(MEM_COMPAT, 0x00);
	udelay(1);
	write32(MEM1_0080, 0x09142001);

	//disable MemIO and SD interface IO and some AHB stuff
	write32(HW_RESETS, read32(HW_RESETS) | (u32)(~(RSTB_IOMEM | RSTB_IOSI)));
	write32(HW_RESET_AHB, read32(HW_RESET_AHB) & 0xffffbc71);

	ConfigureDDRMemory();

	//enable MemIO and SD interface, followed by the AHB stuff
	set32(HW_RESETS, RSTB_IOMEM | RSTB_IOSI);
	set32(HW_RESET_AHB, 0x438E);
	write32(MEM1_30F8, 0x00);
	AhbFlushFrom(AHB_1);

	//set some DIFlags that *might* have to do with the hardware its disabling & reenabling
	write32(HW_RESETS, read32(HW_RESETS) | (u32)(~(RSTB_DSP | RSTB_IOPI | RSTB_IOSI | RSTB_AI_I2S3 |
	                                               RSTB_GFX | RSTB_GFXTCPE | RSTB_PI)));
	udelay(1);
	mask32(HW_DIFLAGS, 0x07EF8F, 0x30);
	udelay(1);
	set32(HW_RESETS, (RSTB_DSP | RSTB_IOPI | RSTB_IOSI | RSTB_AI_I2S3 | RSTB_GFX | RSTB_GFXTCPE | RSTB_PI));
	udelay(1);

	//setup some GPIOS that make no sense for GC mode
	mask32(HW_GPIO1OUT, GP_AVE_SDA | GP_AVE_SCL | GP_SENSORBAR | GP_SLOTLED, read32(HW_GPIO1BOUT));
	write32(HW_GPIO1OWNER, 0);
	write32(HW_GPIO1DIR, GP_OUTPUTS);
#else
	const bool isWiiMode = true;
	PPCHardReset();
#endif

	//setup PPC semaphore stuff
	PPCSetSDKSemaphore(true);
	PPCLoadCode(isWiiMode, ppcInitCode, sizeof(ppcInitCode) / 4);
	PPCSetSDKSemaphore(false);

#ifdef MIOS
	PPCSoftReset();
	debug_output(0xCE);
	BusyDelay(8000);
	while (!read32(MEM1_30F8)) AhbFlushTo(AHB_STARLET);

	mask32(HW_DIFLAGS, 0x601000, 0x600040);
	udelay(1);
	write32(MEM1_30F8, 0x00);
	AhbFlushFrom(AHB_1);
	SetMemoryCompatabilityMode();
	write32(HW_DIFLAGS, read32(HW_DIFLAGS) & (DIFLAGS_BOOT_CODE | 0x80000));
#else

	PPCPrepareMEM1();

#endif

	return;
}
