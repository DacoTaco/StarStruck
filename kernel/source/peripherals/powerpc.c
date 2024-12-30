/*
	starstruck - a Free Software reimplementation for the Nintendo/BroadOn IOS.
	powerpc - manage the Hollywood CPU

	Copyright (C) 2021	DacoTaco

# This code is licensed to you under the terms of the GNU GPL, version 2;
# see file COPYING or http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt
*/

#include <types.h>
#include <ios/processor.h>
#include <ios/gecko.h>
#include <ios/printk.h>
#include <string.h>

#include "core/gpio.h"
#include "utils.h"
#include "powerpc_elf.h"
#include "core/hollywood.h"
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
	0x00000000,
	0x00000000,
	0x00000000,
};

//code that will let me PPC start execution @ 0x80000100, which is the exception vector
const u32 PPC_LaunchExceptionVector[0x0C] = {
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
	0x60000000,
	0x60000000,
};

//code that will let me PPC start execution @ 0x80000100, which is the exception vector
//no idea what this extra oris instruction is for though...
const u32 PPC_LaunchExceptionVector2[0x0D] = {
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
	0x60000000,
	0x60000000,
};

void PPCSoftReset(void)
{
	// enable the broadway IPC interrupt
	write32(HW_PPCIRQMASK, (1<<30));
	clear32(HW_RESETS, SRSTB_CPU | RSTB_CPU);
	udelay(0x0F);
	set32(HW_RESETS, SRSTB_CPU);
	udelay(0x96);
	set32(HW_RESETS, SRSTB_CPU | RSTB_CPU);
}

void PPCLoad(const void* code, u32 codeSize)
{
	if(codeSize <= 1)
		return;

	u32 oldExiValue = read32(HW_EXICTRL);
	u32 sizeToCopy = 0x0F;
	if(codeSize > 0x10)
	{
		write32(HW_EXICTRL, read32(HW_EXICTRL) | 1);
		printk("Warning: only loading %d instrs into EXI boot code", 0x10);	
	}	
	else
		sizeToCopy = codeSize;
	
	u32 codeAddr = (u32)code;
	for(u32 addr = EXI_BOOT_BASE; addr < addr+sizeToCopy; addr += 4)
	{
		write32(addr, codeAddr);
		codeAddr += 4;
	}

	write32(HW_EXICTRL, oldExiValue);
}

void PPCLoadCode(s8 mode, const void* code, u32 codeSize)
{
	const void* codeToLaunch = NULL;
	if(code == NULL || codeSize == 0)
	{
		if(mode == 0)
		{
			codeToLaunch = PPC_LaunchExceptionVector;
			codeSize = sizeof(PPC_LaunchExceptionVector);
		}
		else
		{
			codeToLaunch = PPC_LaunchExceptionVector2;
			codeSize = sizeof(PPC_LaunchExceptionVector2);
		}
	}
	else
		codeToLaunch = code;

	PPCLoad(codeToLaunch, codeSize);
	write32(HW_DIFLAGS, (read32(HW_DIFLAGS) & 0xFFEFFFFF) | DIFLAGS_BOOT_CODE);
}

void SetSemaphore(bool hasSemaphore)
{
	u32 oldValue = read32(HW_EXICTRL);
	write32(MEM1_INITSEMAPHORE, hasSemaphore ? 0 : 0xDEADBEEF);
	DCFlushRange(0, 4);
	write32(HW_EXICTRL, (oldValue & 0xFFFFFFFE) | (hasSemaphore != false));
}

#ifdef MIOS
void PPCStart(void)
{
	u8 ppcInitCode[0x40];
	memcpy(ppcInitCode, PPC_LaunchBS1, sizeof(ppcInitCode));

	write32(MEM1_MEM2PHYSICALSIZE, 0x800000);
	write32(MEM1_MEM2SIMULATESIZE, 0x800000);
	write32(MEM1_MEM2INITLOW, MEM2_PHY2VIRT(0x10000800));
	write32(MEM1_MEM2INITHIGH, MEM2_PHY2VIRT(0x173E0000));
	write32(MEM1_IOSIPCLOW, MEM2_PHY2VIRT(0x173E0000));

	write32(MEM1_CPUVERSION, 0x101);
	write32(MEM1_IOSVERSION, 0x707);
	write32(MEM1_IOSBUILDDATE, 0x110206);
	write32(MEM1_GDDRVENDORCODE, 0xcafebabe);
	write32(MEM1_3114, 0xdeadbeef);
	write32(MEM1_312C, 0xdeadbeef);
	write32(MEM1_313C, 0xdeadbeef);
	write32(MEM1_IOSHEAPLOW, 0xdeadbeef);
	write32(MEM1_IOSHEAPHIGH, 0xdeadbeef);
	write32(MEM1_3154, 0xdeadbeef);
	write32(MEM1_3150, 0xdeadbeef);
	write32(MEM1_LOADMETHOD, 0xdeadbeef);
	write32(MEM1_INITSEMAPHORE, 0xdeadbeef);

	write32(MEM1_EXCEPTIONVECTOR, 0x1800000);
	write32(MEM1_EXCEPTIONVECTOR+4, 0x1800000);
	write32(MEM1_EXCEPTIONVECTOR+8, 0x81800000);
	write32(MEM1_MEM2BAT, MEM2_PHY2VIRT(0x17400000));
	write32(MEM1_IOSIPCHIGH, MEM2_PHY2VIRT(0x17400000));
	write32(MEM1_MEMORYSIZE, 0x1800000);
	write32(MEM1_SIMMEMORYSIZE, 0x1800000);
	write32(MEM1_HEAPLOW, 0x00);
	write32(MEM1_HEAPLOW, 0x81800000);
	
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
	write32(HW_RESETS, read32(HW_RESETS) | (u32)(~(RSTB_DSP | RSTB_IOPI | RSTB_IOSI | RSTB_AI_I2S3 | RSTB_GFX | RSTB_GFXTCPE | RSTB_PI)));
	udelay(1);
	mask32(HW_DIFLAGS, 0x07EF8F, 0x30);
	udelay(1);
	set32(HW_RESETS, (RSTB_DSP | RSTB_IOPI | RSTB_IOSI | RSTB_AI_I2S3 | RSTB_GFX | RSTB_GFXTCPE | RSTB_PI));
	udelay(1);

	//setup some GPIOS that make no sense for GC mode
	mask32(HW_GPIO1OUT, GP_AVE_SDA | GP_AVE_SCL | GP_SENSORBAR | GP_SLOTLED, read32(HW_GPIO1BOUT));
	write32(HW_GPIO1OWNER, 0);
	write32(HW_GPIO1DIR, GP_OUTPUTS);

	//setup PPC semaphore stuff
	SetSemaphore(true);
	PPCLoadCode(0, ppcInitCode, 0x10);
	SetSemaphore(false);
	PPCSoftReset();
	debug_output(0xCE);
	BusyDelay(8000);
	while(!read32(MEM1_30F8))
		AhbFlushTo(AHB_STARLET);
	
	mask32(HW_DIFLAGS, 0x601000, 0x600040);
	udelay(1);
	write32(MEM1_30F8, 0x00);
	AhbFlushFrom(AHB_1);
	SetMemoryCompatabilityMode();
	write32(HW_DIFLAGS, read32(HW_DIFLAGS) & (DIFLAGS_BOOT_CODE | 0x80000));
	
	return;
}
#else
void PPCStart(void)
{

}
#endif

//Old Mini stuff

void powerpc_upload_stub(u32 entry)
{
	u32 i;

	set32(HW_EXICTRL, EXICTRL_ENABLE_EXI);

	// lis r3, entry@h
	write32(EXI_BOOT_BASE + 4 * 0, 0x3c600000 | entry >> 16);
	// ori r3, r3, entry@l
	write32(EXI_BOOT_BASE + 4 * 1, 0x60630000 | (entry & 0xffff));
	// mtsrr0 r3
	write32(EXI_BOOT_BASE + 4 * 2, 0x7c7a03a6);
	// li r3, 0
	write32(EXI_BOOT_BASE + 4 * 3, 0x38600000);
	// mtsrr1 r3
	write32(EXI_BOOT_BASE + 4 * 4, 0x7c7b03a6);
	// rfi
	write32(EXI_BOOT_BASE + 4 * 5, 0x4c000064);

	for (i = 6; i < 0x10; ++i)
		write32(EXI_BOOT_BASE + 4 * i, 0);

	set32(HW_DIFLAGS, DIFLAGS_BOOT_CODE);
	set32(HW_AHBPROT, 0xFFFFFFFF);

	gecko_printf("disabling EXI now...\n");
	clear32(HW_EXICTRL, EXICTRL_ENABLE_EXI);
}

void powerpc_hang(void)
{
	clear32(HW_RESETS, 0x30);
	udelay(100);
	set32(HW_RESETS, 0x20);
	udelay(100);
}