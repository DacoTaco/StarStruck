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
#include <ios/printk.h>
#include <ios/gecko.h>
#include <ios/module.h>

#include "core/hollywood.h"
#include "core/gpio.h"
#include "core/pll.h"
#include "core/iosElf.h"
#include "memory/memory.h"
#include "memory/heaps.h"
#include "memory/ahb.h"
#include "interrupt/exception.h"
#include "messaging/ipc.h"
#include "scheduler/timer.h"
#include "scheduler/threads.h"
#include "interrupt/irq.h"
#include "peripherals/usb.h"
#include "peripherals/powerpc.h"
#include "crypto/aes.h"
#include "crypto/iosc.h"
#include "crypto/sha.h"
#include "utils.h"

#include "sdhc.h"
#include "ff.h"
#include "panic.h"
#include "powerpc_elf.h"
#include "nand.h"
#include "boot2.h"

FATFS fatfs;
extern const u32 __ipc_heap_start[];
extern const u32 __ipc_heap_size[];
extern const u32 __headers_addr[];
extern const ModuleInfo __modules[];
extern const u32 __modules_size;

#ifdef MIOS

#define MAINSTACKSIZE 0x100
const u8 _mainStack[MAINSTACKSIZE] ALIGNED(32) = {0};

#else

#define MAINSTACKSIZE 0x00
const u8* _mainStack = NULL;

#endif

void DiThread()
{
	u32 messages[1];
	u32 msg;

	s32 ret = CreateMessageQueue((void**)&messages, 1);
	if(ret < 0)
		panic("Unable to create DI thread message queue: %d\n", ret);
	
	const u32 queueId = (u32)ret;
	CreateTimer(0, 2500, queueId, (void*)0xbabecafe);
	while(1)
	{
		//don't ask. i have no idea why this is here haha
		for(u32 i = 0; i < 0x1800000; i += 0x80000)
		{
			for(u32 y = 0; y < 6; y++){}
		}
		ReceiveMessage(queueId, (void **)&msg, None);
	}
}

#ifdef MIOS

void kernel_main( void )
{
	gecko_printf("Compat mode kernel thread init\n");
	//create IRQ Timer handler thread
	s32 ret = CreateThread((u32)TimerHandler, NULL, (u32*)TimerMainStack, TIMERSTACKSIZE, 0x7E, 1);
	u32 threadId = (u32)ret;
	//set thread to run as a system thread
	if(ret >= 0)
		Threads[threadId].ThreadContext.StatusRegister |= SPSR_SYSTEM_MODE;
	
	if( ret < 0 || StartThread(threadId) < 0 )
		panic("failed to start IRQ thread!\n");

	//Boot the PPC content
	PPCStart();

	udelay(1000);
	ClearAndEnableIPCInterrupt(IRQ_UNKNMIOS);
  	SetThreadPriority(0, 0);
	printk("Compat mode idle thread started\n");
	while( true ) {}
}

#else

void kernel_main( void )
{
	//create IRQ Timer handler thread
	s32 ret = CreateThread((u32)TimerHandler, NULL, NULL, 0, 0x7E, 1);
	u32 threadId = (u32)ret;
	//set thread to run as a system thread
	if(ret >= 0)
		Threads[threadId].ThreadContext.StatusRegister |= SPSR_SYSTEM_MODE;
	
	if( ret < 0 || StartThread(threadId) < 0 )
		panic("failed to start IRQ thread!\n");

	//not sure what this is about, if you know please let us know.
	u32 hardwareVersion, hardwareRevision;
	GetHollywoodVersion(&hardwareVersion,&hardwareRevision);
	if (hardwareVersion == 0)
	{
		u32 dvdConfig = read32(HW_DI_CFG);
		u32 unknownConfig = dvdConfig >> 2 & 1;
		if ((unknownConfig != 0) && ((~(dvdConfig >> 3) & 1) == 0)) 
		{
			threadId = (u32)CreateThread((u32)DiThread, NULL, NULL, 0, 0x78, unknownConfig);
			Threads[threadId].ThreadContext.StatusRegister |= SPSR_SYSTEM_MODE;
			StartThread(threadId);
		}
	}

	//create AES Engine handler thread & also set it to run as system thread
	ret = CreateThread((u32)AesEngineHandler, NULL, NULL, 0, 0x7E, 1);
	threadId = (u32)ret;
	if(ret >= 0)
		Threads[threadId].ThreadContext.StatusRegister |= SPSR_SYSTEM_MODE;

	if( ret < 0 || StartThread(threadId) < 0 )
		panic("failed to start AES thread!\n");	

	//create SHA Engine handler thread & also set it to run as system thread
	ret = CreateThread((u32)ShaEngineHandler, NULL, NULL, 0, 0x7E, 1);
	threadId = (u32)ret;
	if(ret > 0)
		Threads[threadId].ThreadContext.StatusRegister |= SPSR_SYSTEM_MODE;

	if( ret < 0 || StartThread(threadId) < 0 )
		panic("failed to start SHA thread!\n");

	IOSC_InitInformation();

	//create IPC handler thread & also set it to run as system thread
	ret = CreateThread((u32)IpcHandler, NULL, NULL, 0, 0x5C, 1);
	threadId = (u32)ret;
	if(ret > 0)
	{
		Threads[threadId].ThreadContext.StatusRegister |= SPSR_SYSTEM_MODE;
		IpcHandlerThread = &Threads[threadId];
		IpcHandlerThreadId = (u32)threadId;
	}

	if( ret < 0 || StartThread(threadId) < 0 )
		panic("failed to start IPC thread!\n");

	//loop the program headers and map/launch all modules
	Elf32_Phdr* headers = (Elf32_Phdr*)__headers_addr;
	for(u32 index = 1; index < 0x0F; index++)
	{
		MemorySection section;
		Elf32_Phdr header = headers[index];
		if(header.p_type != PT_LOAD || (header.p_flags & 0x0FF00000) == 0 || header.p_vaddr == (u32)__headers_addr)
			continue;
		
		section.PhysicalAddress = header.p_paddr;
		section.VirtualAddress = header.p_vaddr;
		section.Domain = FLAGSTODOMAIN(header.p_flags);
		section.Size = (header.p_memsz + 0xFFF) & 0xFFFFF000;

		//set section access
		if(header.p_flags & PF_X)
			section.AccessRights = AP_ROUSER;
		else if(header.p_flags & PF_W)
			section.AccessRights = AP_RWUSER;
		else if(header.p_flags & PF_R)
			section.AccessRights = AP_ROM;
		else
			section.AccessRights = AP_ROUSER;

		section.IsCached = 1;
		s32 ret = MapMemory(&section);
		if(ret != 0)
			panic("Unable to map region %08x [%d bytes]\n", section.VirtualAddress, section.Size);
		
		//map cached version
		section.VirtualAddress |= 0x80000000;
		section.IsCached = 0;
		ret = MapMemory(&section);
		if(ret != 0)
			panic("Unable to map region %08x [%d bytes]\n", section.VirtualAddress, section.Size);
		
		printk("load segment @ [%08lx, %08lx] (%ld bytes)\n", header.p_vaddr, header.p_vaddr + header.p_memsz, header.p_memsz);

		//clear memory that didn't have stuff loaded in from the elf
		if(header.p_filesz < header.p_memsz)
			memset((void*)(header.p_vaddr + header.p_filesz), 0, header.p_memsz - header.p_filesz);
	}

	const u32 modules_cnt = __modules_size / sizeof(ModuleInfo);
	for(u32 i = 0; i < modules_cnt;i++)
	{
		u32 main = __modules[i].EntryPoint;
		u32 stackSize = __modules[i].StackSize;
		u32 priority = __modules[i].Priority;
		u32 stackTop = __modules[i].StackAddress;
		u32 arg = __modules[i].UserId;
		 
		printk("priority = %d, stackSize = %d, stackPtr = %d\n", priority, stackSize, stackTop);
		printk("starting thread entry: 0x%x\n", main);

		threadId = (u32)CreateThread(main, (void*)arg, (u32*)stackTop, stackSize, priority, 1);
		Threads[threadId].ProcessId = arg;
		StartThread(threadId);
	}

	KernelHeapId = CreateHeap((void*)__headers_addr, 0xC0000);
	printk("$IOSVersion: IOSP: %s %s 64M $", __DATE__, __TIME__);
	SetThreadPriority(0, 0);
	SetThreadPriority(IpcHandlerThreadId, 0x5C);
	u32 vector;
	FRESULT fres = 0;

	//while(1){}

	boot2_init();
	
	/*printk("Initializing SDHC...\n");
	sdhc_init();

	printk("Mounting SD...\n");
	fres = f_mount(0, &fatfs);*/

	if (read32(HW_CLOCKS) & 2) {
		printk("GameCube compatibility mode detected...\n");
		vector = boot2_run(1, 0x101);
		goto shutdown;
	}

	if(fres != FR_OK) {
		printk("Error %d while trying to mount SD\n", fres);
		panic2(0, PANIC_MOUNT);
	}

	DisableInterrupts();
	printk("rebooting into HBC...\n");
	vector = boot2_run(0x00010001, 0x4C554C5A);
	ipc_shutdown();

shutdown:
	printk("Shutting down...\n");
	printk("interrupts...\n");
	irq_shutdown();
	printk("caches and MMU...\n");
	mem_shutdown();

	printk("Vectoring to 0x%08x...\n", vector);
	//go to whatever address we got
	asm("bx\t%0": : "r" (vector));
}

#endif

void SetStarletClock()
{
#ifdef MIOS
	write32(HW_IOSTRCTRL0, 0x65244A);
	write32(HW_IOSTRCTRL1, 0x46A024);
#else
	u32 hardwareVersion = 0;
	u32 hardwareRevision = 0;
	GetHollywoodVersion(&hardwareVersion, &hardwareRevision);
	
	if(hardwareVersion < 2)
	{
		write32(HW_IOSTRCTRL0, 0x65244A);
		write32(HW_IOSTRCTRL1, 0x46A024);
	}
	else
	{
		write32(HW_IOSTRCTRL0, (read32(HW_IOSTRCTRL0) & 0xFF000000 ) | 0x292449);
		write32(HW_IOSTRCTRL1, (read32(HW_IOSTRCTRL1) & 0xFE000000) | 0x46A012);
	}
#endif
}

void InitialiseSystem( void )
{
	u32 hardwareVersion = 0;
	u32 hardwareRevision = 0;
	GetHollywoodVersion(&hardwareVersion, &hardwareRevision);
#ifdef MIOS
	IsWiiMode = 0;
#else
	IsWiiMode = 1;
#endif
	//Enable PPC EXI control
	set32(HW_EXICTRL, EXICTRL_ENABLE_EXI);
	
#ifndef MIOS
	//enable protection on our MEM2 addresses & SRAM
	ProtectMemory(1, (void*)0x13620000, (void*)0x1FFFFFFF);

	//????
	write32(HW_EXICTRL, read32(HW_EXICTRL) & 0xFFFFFFEF );
#endif
	
	//set some hollywood ahb registers????
	if(hardwareVersion == 1 && hardwareRevision == 0)
		write32(HW_ARB_CFG_CPU, (read32(HW_ARB_CFG_CPU) & 0xFFFF000F) | 1);
	
#ifndef MIOS
	// ¯\_(ツ)_/¯
	write32(HW_AHB_10, 0);
	
	//Set boot0 B10 & B11? found in IOS58.
	set32(HW_BOOT0, 0xC00);
#endif

//Configure PPL ( phase locked loop )
#ifdef MIOS
	ConfigureAiPLL(1, 1);
#else	
	ConfigureAiPLL(0, 0);
	ConfigureVideoInterfacePLL(0);
#endif

	//Configure USB Host
	ConfigureUsbController(hardwareRevision);

#ifndef MIOS	
	//Configure GPIO pins
	ConfigureGPIO();
	ResetGPIODevices();
#else
	write32(HW_RESETS, read32(HW_RESETS) | (u32)(~(RSTB_IODI | RSTB_DIRSTB | RSTB_CPU | SRSTB_CPU)));
#endif
	
	//Set clock speed
	SetStarletClock();
	
	//reset registers
#ifndef MIOS
	write32(HW_GPIO1OWNER, read32(HW_GPIO1OWNER) & (( 0xFF000000 | GP_ALL ) ^ GP_DISPIN));
	write32(HW_GPIO1DIR, read32(HW_GPIO1DIR) | GP_DISPIN);
#else
	InitializeGPIO();
#endif
	write32(HW_ALARM, 0);
	write32(NAND_CMD, 0);
	write32(AES_CMD, 0);
	write32(SHA_CMD, 0);
	
	//Enable all ARM irq's except for 2 unknown irq's ( 0x4200 )
	write32(HW_ARMIRQFLAG, 0xFFFFBDFF);
	write32(HW_ARMIRQMASK, 0);
	write32(HW_ARMFIQMASK, 0);

#ifndef MIOS
	gecko_printf("Configuring caches and MMU...\n");
	InitializeMemory();
#else
	//lol, mios explicitly disables the debug interface
	write32(HW_DBGINTEN, 0);
#endif
}

u32 _main(void)
{
	gecko_init();
	//don't use printk before our main thread started. our stackpointers are god knows were at that point & thread context isn't init yet
	gecko_printf("StarStruck %s loading\n", git_version);	
	gecko_printf("Initializing exceptions...\n");
	initializeExceptions();

	AhbFlushFrom(AHB_1);
	AhbFlushTo(AHB_1);
	
	InitialiseSystem();

#ifndef MIOS
	gecko_printf("IOSflags: %08x %08x %08x\n",
		read32(0xffffff00), read32(0xffffff04), read32(0xffffff08));
	gecko_printf("          %08x %08x %08x\n",
		read32(0xffffff0c), read32(0xffffff10), read32(0xffffff14));
#endif

#ifdef MIOS
	ClearAndEnableIPCInterrupt(IRQ_GPIO1);

	gecko_printf("Compat mode IOS...\n");
#else
	IrqInit();
	IpcInit();
	IOSC_Init();

	//currently unknown if these values are used in the kernel itself.
	//if they are, these need to be replaced with actual stuff from the linker script!
	write32(MEM1_MEM2PHYSICALSIZE, 0x4000000);
	write32(MEM1_MEM2SIMULATESIZE, 0x4000000);
	write32(MEM1_MEM2INITLOW, MEM2_PHY2VIRT(0x10000800));
	write32(MEM1_MEM2INITHIGH, MEM2_PHY2VIRT(0x135e0000));
	write32(MEM1_IOSIPCLOW, MEM2_PHY2VIRT(0x135e0000));
	write32(MEM1_MEM2BAT, MEM2_PHY2VIRT((u32)__ipc_heap_start));
	write32(MEM1_IOSIPCHIGH, MEM2_PHY2VIRT((u32)__ipc_heap_start));
	write32(MEM1_IOSHEAPLOW, MEM2_PHY2VIRT((u32)__ipc_heap_start));
	write32(MEM1_IOSHEAPHIGH, MEM2_PHY2VIRT((u32)__ipc_heap_start + (u32)__ipc_heap_size));
	DCFlushRange((void*)0x00003100, 0x68);
	gecko_printf("Updated DDR settings in lomem for current map\n");
#endif

	//init&start main code next : 
	//-------------------------------
	//init thread context handles
	InitializeThreadContext();

	//create main kernel thread
	u32 threadId = (u32)CreateThread((u32)kernel_main, NULL, (u32*)_mainStack, MAINSTACKSIZE, 0x7F, 1);
	//set thread to run as a system thread
	Threads[threadId].ThreadContext.StatusRegister |= SPSR_SYSTEM_MODE;

	if( threadId != 0 || StartThread(threadId) < 0 )
		gecko_printf("failed to start kernel(%d)!\n", threadId);

	panic("\npanic!\n");
	return 0;
}

