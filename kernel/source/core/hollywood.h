/*
	mini - a Free Software replacement for the Nintendo/BroadOn IOS.
	Hollywood register definitions

Copyright (C) 2008, 2009	Haxx Enterprises <bushing@gmail.com>
Copyright (C) 2008, 2009	Sven Peter <svenpeter@gmail.com>
Copyright (C) 2008, 2009	Hector Martin "marcan" <marcan@marcansoft.com>
Copyright (C) 2008, 2009	John Kelley <wiidev@kelley.ca>

# This code is licensed to you under the terms of the GNU GPL, version 2;
# see file COPYING or http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt
*/

#ifndef __HOLLYWOOD_H__
#define __HOLLYWOOD_H__

/* Hollywood Registers */

#define		HW_ADDR_REG_BASE	0x0d000000
#define		HW_AHB_REG_BASE		0x0d800000
#define		HW_REG_BASE			HW_AHB_REG_BASE

// The PPC can only see the first three IPC registers
#define		HW_IPC_PPCMSG		(HW_REG_BASE + 0x000)
#define		HW_IPC_PPCCTRL		(HW_REG_BASE + 0x004)
#define		HW_IPC_ARMMSG		(HW_REG_BASE + 0x008)
#define		HW_IPC_ARMCTRL		(HW_REG_BASE + 0x00c)

#define		HW_TIMER			(HW_REG_BASE + 0x010)
#define		HW_ALARM			(HW_REG_BASE + 0x014)

#define		HW_VISOLID			(HW_REG_BASE + 0x024)

#define		HW_PPCIRQFLAG		(HW_REG_BASE + 0x030)
#define		HW_PPCIRQMASK		(HW_REG_BASE + 0x034)

#define		HW_ARMIRQFLAG		(HW_REG_BASE + 0x038)
#define		HW_ARMIRQMASK		(HW_REG_BASE + 0x03c)
#define		HW_ARMFIQMASK		(HW_REG_BASE + 0x040)
#define		HW_DBGINTEN			(HW_REG_BASE + 0x05c)

#define		HW_MEMMIRR			(HW_REG_BASE + 0x060)
#define		HW_AHBPROT			(HW_REG_BASE + 0x064)

// something to do with PPCBOOT
// and legacy DI it seems ?!?
#define		HW_EXICTRL			(HW_REG_BASE + 0x070)
#define		EXICTRL_ENABLE_EXI	1

// USB controller
#define		HW_USBDBG0			(HW_REG_BASE + 0x080)
#define		HW_USBDBG1			(HW_REG_BASE + 0x084)
#define		HW_USBFRCRST		(HW_REG_BASE + 0x088)
#define		HW_USBIOTEST		(HW_REG_BASE + 0x08c)

// PPC side of GPIO1 (Starlet can access this too)
// Output state
#define		HW_GPIO1BOUT		(HW_REG_BASE + 0x0c0)
// Direction (1=output)
#define		HW_GPIO1BDIR		(HW_REG_BASE + 0x0c4)
// Input state
#define		HW_GPIO1BIN			(HW_REG_BASE + 0x0c8)
// Interrupt level
#define		HW_GPIO1BINTLVL		(HW_REG_BASE + 0x0cc)
// Interrupt flags (write 1 to clear)
#define		HW_GPIO1BINTFLAG	(HW_REG_BASE + 0x0d0)
// Interrupt propagation enable
// Do these interrupts go anywhere???
#define		HW_GPIO1BINTENABLE	(HW_REG_BASE + 0x0d4)
//??? seems to be a mirror of inputs at some point... power-up state?
#define		HW_GPIO1BINMIR		(HW_REG_BASE + 0x0d8)
// 0xFFFFFF by default, if cleared disables respective outputs. Top bits non-settable.
#define		HW_GPIO1ENABLE		(HW_REG_BASE + 0x0dc)

// Starlet side of GPIO1
// Output state
#define		HW_GPIO1OUT			(HW_REG_BASE + 0x0e0)
// Direction (1=output)
#define		HW_GPIO1DIR			(HW_REG_BASE + 0x0e4)
// Input state
#define		HW_GPIO1IN			(HW_REG_BASE + 0x0e8)
// Interrupt level
#define		HW_GPIO1INTLVL		(HW_REG_BASE + 0x0ec)
// Interrupt flags (write 1 to clear)
#define		HW_GPIO1INTFLAG		(HW_REG_BASE + 0x0f0)
// Interrupt propagation enable (interrupts go to main interrupt 0x800)
#define		HW_GPIO1INTENABLE	(HW_REG_BASE + 0x0f4)
//??? seems to be a mirror of inputs at some point... power-up state?
#define		HW_GPIO1INMIR		(HW_REG_BASE + 0x0f8)
// Owner of each GPIO bit. If 1, GPIO1B registers assume control. If 0, GPIO1 registers assume control.
#define		HW_GPIO1OWNER		(HW_REG_BASE + 0x0fc)

// ????
#define		HW_ARB_CFG_M0		(HW_REG_BASE + 0x100)
#define		HW_ARB_CFG_M1		(HW_REG_BASE + 0x104)
#define		HW_ARB_CFG_M2		(HW_REG_BASE + 0x108)
#define		HW_ARB_CFG_M3		(HW_REG_BASE + 0x10c)
#define		HW_ARB_CFG_M4		(HW_REG_BASE + 0x110)
#define		HW_ARB_CFG_M5		(HW_REG_BASE + 0x114)
#define		HW_ARB_CFG_M6		(HW_REG_BASE + 0x118)
#define		HW_ARB_CFG_M7		(HW_REG_BASE + 0x11c)
#define		HW_ARB_CFG_M8		(HW_REG_BASE + 0x120)
#define		HW_ARB_CFG_M9		(HW_REG_BASE + 0x124)
#define		HW_ARB_CFG_MC		(HW_REG_BASE + 0x130)
#define		HW_ARB_CFG_MD		(HW_REG_BASE + 0x134)
#define		HW_ARB_CFG_ME		(HW_REG_BASE + 0x138)
#define		HW_ARB_CFG_MF		(HW_REG_BASE + 0x13c)
#define		HW_ARB_CFG_CPU		(HW_REG_BASE + 0x140)
#define		HW_ARB_CFG_DMA		(HW_REG_BASE + 0x144)
#define		HW_ARB_PCNTCFG		(HW_REG_BASE + 0x148)
#define		HW_ARB_PCNTSTS		(HW_REG_BASE + 0x14c)

// ????
#define		HW_DIFLAGS			(HW_REG_BASE + 0x180)
#define		HW_RESET_AHB		(HW_REG_BASE + 0x184)
#define		HW_SPARE0			(HW_REG_BASE + 0x188)
#define		HW_BOOT0			(HW_REG_BASE + 0x18c)
#define		HW_CLOCKS			(HW_REG_BASE + 0x190)
#define		DIFLAGS_BOOT_CODE	0x100000

/* Hardware resets */
#define		HW_RESETS			(HW_REG_BASE + 0x194)
//System reset. Set to zero to reboot system. 
#define		RSTBINB				0x00000001
//CRST? Also seems to reboot system. 
#define		CRSTB				0x00000002
//MEM reset B. Also seems to reboot system. 
#define		RSTB_MEMRSTB		0x00000004
//DSKPLL reset. Is cleared by IOS before modifying 1b8, and set again afterwards 
#define		RSTB_DSKPLL			0x00000008
//PowerPC SRESET (release first) 
#define		RSTB_CPU			0x00000010
//PowerPC HRESET (release second) 
#define		SRSTB_CPU			0x00000020
//SYSPLL reset. If cleared, kills EXI-based starlet experimental proxy. 
#define		RSTB_SYSPLL			0x00000040
//Unlock SYSPLL reset? 
#define		NLCKB_SYSPLL		0x00000080
//??????
#define		RSTB_MEMRSTB2		0x00000100
//???
#define		RSTB_PI				0x00000200
//Disk Interface reset B 
#define		RSTB_DIRSTB			0x00000400
//MEM reset. If cleared, kills EXI-based starlet experimental proxy. 
#define		RSTB_MEM			0x00000800
//GFX TCPE? 
#define		RSTB_GFXTCPE		0x00001000
//GFX reset? 
#define		RSTB_GFX			0x00002000
//Audio interface I2S3 reset 
#define		RSTB_AI_I2S3		0x00004000
//SI IO reset 
#define		RSTB_IOSI			0x00008000
//EXI IO reset 
#define		RSTB_IOEXI			0x00010000
//Disk Interface IO reset 
#define		RSTB_IODI			0x00020000
//MEM IO reset 
#define		RSTB_IOMEM			0x00040000
//Processor Interface IO
#define		RSTB_IOPI			0x00080000
//Video Interface reset 
#define		RSTB_VI				0x00100000
//VI1 reset? 
#define		RSTB_VI1			0x00200000
//DSP processor reset
#define		RSTB_DSP			0x00400000
//IOP/Starlet reset 
#define		RSTB_IOP			0x00800000
//ARM AHB reset. Kills DI, sets slot LED on, hangs starlet... 
#define		RSTB_AHB			0x01000000
//External DRAM reset 
#define		RSTB_EDRAM			0x02000000
//Unlock external DRAM reset? 
#define		NLCKB_EDRAM			0x04000000
#define		HW_RST_UNKN1		0x08000000
#define		HW_RST_UNKN2		0x10000000
#define		HW_RST_UNKN3		0x20000000
#define		HW_RST_UNKN4		0x40000000
#define		HW_RST_UNKN5		0x80000000


#define		HW_PLLAIEXT1		(HW_REG_BASE + 0x1a8)
#define		HW_PLLSYS			(HW_REG_BASE + 0x1b0)
#define		HW_PLLSYSEXT		(HW_REG_BASE + 0x1b4)
#define		HW_PLLVIEXT			(HW_REG_BASE + 0x1c8)
#define		HW_PLLAI			(HW_REG_BASE + 0x1cc)
#define		HW_PLLAIEXT			(HW_REG_BASE + 0x1d0)
#define		HW_PLLUSB			(HW_REG_BASE + 0x1d4)
#define		HW_PLLUSBEXT		(HW_REG_BASE + 0x1d8)

/* IOSP clock speeds? */
#define		HW_IOSTRCTRL0		(HW_REG_BASE + 0x1e0)
#define		HW_IOSTRCTRL1		(HW_REG_BASE + 0x1e4)
#define		HW_CLKSTRCTRL		(HW_REG_BASE + 0x1e8)

#define		HW_OTPCMD			(HW_REG_BASE + 0x1ec)
#define		HW_OTPDATA			(HW_REG_BASE + 0x1f0)
#define		HW_VERSION			(HW_REG_BASE + 0x214)

/* Drive Interface */

#define		HW_DI_BASE			(HW_REG_BASE + 0x6000)
#define		HW_DI_STATUS		(HW_DI_BASE + 0x00)
#define		HW_DI_COVER			(HW_DI_BASE + 0x04)
#define		HW_DI_CMDBUF0		(HW_DI_BASE + 0x08)
#define		HW_DI_CMDBUF1		(HW_DI_BASE + 0x0C)
#define		HW_DI_CMDBUF2		(HW_DI_BASE + 0x10)
#define		HW_DI_MEM_ADDR		(HW_DI_BASE + 0x14)
#define		HW_DI_LENGTH		(HW_DI_BASE + 0x18)
#define		HW_DI_CONTROL		(HW_DI_BASE + 0x1C)
#define		HW_DI_IMM_BUF		(HW_DI_BASE + 0x20)
#define		HW_DI_CFG			(HW_DI_BASE + 0x24)

/* NAND Registers */

#define		NAND_REG_BASE		0xd010000
#define		NAND_CMD			(NAND_REG_BASE + 0x000)

/* AES Registers */

#define		AES_REG_BASE		0xd020000

#define		AES_CMD				(AES_REG_BASE + 0x000)
#define		AES_SRC				(AES_REG_BASE + 0x004)
#define		AES_DEST			(AES_REG_BASE + 0x008)
#define		AES_KEY				(AES_REG_BASE + 0x00c)
#define		AES_IV				(AES_REG_BASE + 0x010)

/* SHA-1 Registers */

#define		SHA_REG_BASE		0xd030000

#define		SHA_CMD				(SHA_REG_BASE + 0x000)
#define		SHA_SRC				(SHA_REG_BASE + 0x004)
#define		SHA_H0				(SHA_REG_BASE + 0x008)
#define		SHA_H1				(SHA_REG_BASE + 0x00c)
#define		SHA_H2				(SHA_REG_BASE + 0x010)
#define		SHA_H3				(SHA_REG_BASE + 0x014)
#define		SHA_H4				(SHA_REG_BASE + 0x018)

/* USB Host Controller Registers */

#define		USB_REG_BASE		0xd040000

/* SD Host Controller Registers */

#define		SDHC_REG_BASE		0xd070000

/* EXI Registers */

#define		EXI_REG_BASE		(HW_REG_BASE+0x6800)
#define		EXI0_REG_BASE		(EXI_REG_BASE+0x000)
#define		EXI1_REG_BASE		(EXI_REG_BASE+0x014)
#define		EXI2_REG_BASE		(EXI_REG_BASE+0x028)

#define		EXI0_CSR		(EXI0_REG_BASE+0x000)
#define		EXI0_MAR		(EXI0_REG_BASE+0x004)
#define		EXI0_LENGTH		(EXI0_REG_BASE+0x008)
#define		EXI0_CR			(EXI0_REG_BASE+0x00c)
#define		EXI0_DATA		(EXI0_REG_BASE+0x010)

#define		EXI1_CSR		(EXI1_REG_BASE+0x000)
#define		EXI1_MAR		(EXI1_REG_BASE+0x004)
#define		EXI1_LENGTH		(EXI1_REG_BASE+0x008)
#define		EXI1_CR			(EXI1_REG_BASE+0x00c)
#define		EXI1_DATA		(EXI1_REG_BASE+0x010)

#define		EXI2_CSR		(EXI2_REG_BASE+0x000)
#define		EXI2_MAR		(EXI2_REG_BASE+0x004)
#define		EXI2_LENGTH		(EXI2_REG_BASE+0x008)
#define		EXI2_CR			(EXI2_REG_BASE+0x00c)
#define		EXI2_DATA		(EXI2_REG_BASE+0x010)

#define		EXI_BOOT_BASE		(EXI_REG_BASE+0x040)

/* MEMORY CONTROLLER Registers */

#define		MEM_REG_BASE		(HW_REG_BASE+0xb4000)
#define		MEM_REFRESH			(MEM_REG_BASE+0x26)

#define		MEM_CONTRL_BASE		(MEM_REG_BASE+0x200)
#define		MEM_COMPAT			(MEM_CONTRL_BASE)
#define		MEM_PROT			(MEM_CONTRL_BASE+0x0a)
#define		MEM_PROT_START		(MEM_CONTRL_BASE+0x0c)
#define		MEM_PROT_END		(MEM_CONTRL_BASE+0x0e)
#define		MEM_FLUSHREQ		(MEM_CONTRL_BASE+0x28)
#define		MEM_FLUSHACK		(MEM_CONTRL_BASE+0x2a)

/* MEMORY INTERFACE Registers */
/* https://wiibrew.org/wiki/Hardware/Memory_Interface */

#define		MEM_INTERFACE_BASE	(HW_REG_BASE)

#ifndef __ASSEMBLER__

#include <types.h>

void GetHollywoodVersion(u32* hardwareVersion, u32* hardwareRevision);
u32 GetCoreClock(void);

#endif

#endif
