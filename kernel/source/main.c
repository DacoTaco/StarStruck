/*
	mini - a Free Software replacement for the Nintendo/BroadOn IOS.

Copyright (C) 2008, 2009	Haxx Enterprises <bushing@gmail.com>
Copyright (C) 2008, 2009	Sven Peter <svenpeter@gmail.com>
Copyright (C) 2008, 2009	Hector Martin "marcan" <marcan@marcansoft.com>
Copyright (C) 2009			Andre Heider "dhewg" <dhewg@wiibrew.org>
Copyright (C) 2009		John Kelley <wiidev@kelley.ca>

# This code is licensed to you under the terms of the GNU GPL, version 2;
# see file COPYING or http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt
*/

#include <types.h>
#include <string.h>
#include <git_version.h>
#include <ios/processor.h>

#include "core/hollywood.h"
#include "core/gpio.h"
#include "core/pll.h"
#include "memory/memory.h"
#include "memory/ahb.h"
#include "handlers/exception.h"
#include "messaging/ipc.h"
#include "interrupt/threads.h"
#include "interrupt/irq.h"
#include "storage/usb.h"
#include "utils.h"

#include "sdhc.h"
#include "gecko.h"
#include "ff.h"
#include "panic.h"
#include "powerpc_elf.h"
#include "crypto.h"
#include "nand.h"
#include "boot2.h"

#define PPC_BOOT_FILE "/bootmii/ppcboot.elf"

FATFS fatfs;

void kernel_main( void )
{
	u32 vector;
	FRESULT fres = 0;

	irq_initialize();
//	irq_enable(IRQ_GPIO1B);
	irq_enable(IRQ_GPIO1);
	irq_enable(IRQ_RESET);
	irq_enable(IRQ_TIMER);
	irq_set_alarm(20, 1);
	gecko_printf("Interrupts initialized\n");
	
	crypto_initialize();
	gecko_printf("crypto support initialized\n");

	nand_initialize();
	gecko_printf("NAND initialized.\n");

	boot2_init();

	gecko_printf("Initializing IPC...\n");
	ipc_initialize();
	
	/*gecko_printf("Initializing SDHC...\n");
	sdhc_init();

	gecko_printf("Mounting SD...\n");
	fres = f_mount(0, &fatfs);*/

	if (read32(HW_CLOCKS) & 2) {
		gecko_printf("GameCube compatibility mode detected...\n");
		vector = boot2_run(1, 0x101);
		goto shutdown;
	}

	if(fres != FR_OK) {
		gecko_printf("Error %d while trying to mount SD\n", fres);
		panic2(0, PANIC_MOUNT);
	}

	ipc_ppc_boot_title(0x000100014C554C5ALL);
	//*(u32*)HW_RESETS &= ~1;
	gecko_printf("Going into IPC mainloop...\n");
	vector = ipc_main();
	gecko_printf("IPC mainloop done! killing IPC...\n");
	ipc_shutdown();

shutdown:
	gecko_printf("Shutting down...\ninterrupts...\n");
	irq_shutdown();
	gecko_printf("caches and MMU...\n");
	mem_shutdown();

	gecko_printf("Vectoring to 0x%08x...\n", vector);
	//go to whatever address we got
	asm("bx\t%0": : "r" (vector));
}

void SetStarletClock()
{
	u32 hardwareVersion = 0;
	u32 hardwareRevision = 0;
	GetHollywoodVersion(&hardwareVersion, &hardwareRevision);
	
	if(hardwareVersion < 2)
	{
		set32(HW_IOSTRCTRL0, 0x65244A);
		set32(HW_IOSTRCTRL1, 0x46A024);
	}
	else
	{
		set32(HW_IOSTRCTRL0, (read32(HW_IOSTRCTRL0) & 0xFF000000 ) | 0x292449);
		set32(HW_IOSTRCTRL1, (read32(HW_IOSTRCTRL1) & 0xFE000000) | 0x46A012);
	}
}

void InitialiseSystem( void )
{
	u32 hardwareVersion = 0;
	u32 hardwareRevision = 0;
	GetHollywoodVersion(&hardwareVersion, &hardwareRevision);
	
	//something to do with flipper?
	set32(HW_EXICTRL, read32(HW_EXICTRL) | EXICTRL_ENABLE_EXI);
	
	//enable protection on our MEM2 addresses & SRAM
	ProtectMemory(1, (void*)0x13620000, (void*)0x1FFFFFFF);
	
	//????
	set32(HW_EXICTRL, read32(HW_EXICTRL) & 0xFFFFFFEF );
	
	//set some hollywood ahb registers????
	if(hardwareVersion == 1 && hardwareRevision == 0)
		set32(HW_ARB_CFG_CPU, (read32(HW_ARB_CFG_CPU) & 0xFFFF0F) | 1);
	
	// ¯\_(ツ)_/¯
	set32(HW_AHB_10, 0);
	
	//Set boot0 B10 & B11? found in IOS58.
	set32(HW_BOOT0, read32(HW_BOOT0) | 0xC00);
	
	//Configure PPL ( phase locked loop )
	ConfigureAiPLL(0, 0);
	ConfigureVideoInterfacePLL(0);
	
	//Configure USB Host
	ConfigureUsbController(hardwareRevision);
	
	//Configure GPIO pins
	ConfigureGPIO();
	ResetGPIODevices();
	
	//Set clock speed
	SetStarletClock();
	
	//reset registers
	set32(HW_GPIO1OWNER, read32(HW_GPIO1OWNER) & (( 0xFF000000 | GP_ALL ) ^ GP_DISPIN));
	set32(HW_GPIO1DIR, read32(HW_GPIO1DIR) | GP_DISPIN);
	set32(HW_ALARM, 0);
	set32(NAND_CMD, 0);
	set32(AES_CMD, 0);
	set32(SHA_CMD, 0);
	
	//Enable all ARM irq's except for 2 unknown irq's ( 0x4200 )
	set32(HW_ARMIRQFLAG, 0xFFFFBDFF);
	set32(HW_ARMIRQMASK, 0);
	set32(HW_ARMFIQMASK, 0);
	
	gecko_printf("Configuring caches and MMU...\n");
	InitiliseMemory();
}

u32 _main(void *base)
{
	(void)base;	
	gecko_init();
	gecko_printf("StarStruck %s loading\n", git_version);
	
	gecko_printf("Initializing exceptions...\n");
	exception_initialize();

	AhbFlushFrom(AHB_1);
	AhbFlushTo(AHB_1);
	
	InitialiseSystem();	

	gecko_printf("IOSflags: %08x %08x %08x\n",
		read32(0xffffff00), read32(0xffffff04), read32(0xffffff08));
	gecko_printf("          %08x %08x %08x\n",
		read32(0xffffff0c), read32(0xffffff10), read32(0xffffff14));

	IrqInit();
	IpcInit();
	
	//currently unknown if these values are used in the kernel itself.
	//if they are, these need to be replaced with actual stuff from the linker script!
	write32(MEM1_MEM2PHYSICALSIZE, 0x4000000);
	write32(MEM1_MEM2SIMULATESIZE, 0x4000000);
	write32(MEM1_MEM2INITLOW, MEM2_PHY2VIRT(0x10000800));
	write32(MEM1_MEM2INITHIGH, MEM2_PHY2VIRT(0x135e0000));
	write32(MEM1_IOSHEAPLOW, MEM2_PHY2VIRT(0x135e0000));
	write32(MEM1_MEM2BAT, MEM2_PHY2VIRT(0x13600000));
	write32(MEM1_IOSHEAPHIGH, MEM2_PHY2VIRT(0x13600000));
	write32(MEM1_3148, MEM2_PHY2VIRT(0x13600000));
	write32(MEM1_314C, MEM2_PHY2VIRT(0x13620000));
	DCFlushRange((void*)0x00003100, 0x68);
	gecko_printf("Updated DDR settings in lomem for current map\n");
	
	//init&start main code next : 
	//-------------------------------
	//init thread context handles
	InitializeThreadContext();
	
	//create main kernel thread
	s32 threadId = CreateThread((s32)kernel_main, NULL, NULL, 0, 0x7F, 1);
	//enable interrupts in this thread
	threads[threadId].threadContext.statusRegister |= 0x1f;
	
	if( threadId < 0 || StartThread(threadId) < 0 )
		gecko_printf("failed to start kernel(%d)!\n", threadId);

	gecko_printf("\npanic!\n");
	while(1){};
	return 0;
}

