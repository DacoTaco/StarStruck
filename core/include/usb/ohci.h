/*
        StarStruck - a Free Software reimplementation for the Nintendo/BroadOn
IOS. printk - printk implementation in ios

        Copyright (C) 2025	Alberto Mardegan <mardy@users.sourceforge.net>

# This code is licensed to you under the terms of the GNU GPL, version 2;
# see file COPYING or http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt
*/

#ifndef __USB_OHCI_H__
#define __USB_OHCI_H__

#include "types.h"
#include "usb/usb.h"

#define ED_SET(field, value) \
	(((value) & ED_##field##_MASK) << ED_##field##_SHIFT)
#define ED_GET(field, var) (((var) >> ED_##field##_SHIFT) & ED_##field##_MASK)
#define ED_CLEAR(field, var) \
	((var) & (~ED_##field##_MASK << ED_##field##_SHIFT))

/* Upper 5 bits are free for driver's use */
#define ED_WII31     0x80000000
/* MaximumPacketSize */
#define ED_MPS_SHIFT 16
#define ED_MPS_MASK  (u32)(0x7ff) /* 11 bits */
/* 1-bit flags */
#define ED_ISO       (1U << 15)
#define ED_SKIP      (1U << 14)
#define ED_LOWSPEED  (1U << 13)
/* Direction */
#define ED_DIR_SHIFT 11
#define ED_DIR_MASK  (0x03 << ED_DIR_SHIFT)
#define ED_OUT       (0x01 << ED_DIR_SHIFT)
#define ED_IN        (0x02 << ED_DIR_SHIFT)
/* EndpointNumber */
#define ED_EN_SHIFT  7
#define ED_EN_MASK   (0x0f << ED_EN_SHIFT)
/* FunctionAddress */
#define ED_FA_SHIFT  0
#define ED_FA_MASK   (0x7f << ED_FA_SHIFT)
/* Head */
#define ED_C         (0x02U) /* toggle carry */
#define ED_H         (0x01U) /* halted */

typedef struct OhciEndpointDescriptor_t
{
	u32 dw0;

 /* Lower 4 bits are ignored and available for the driver */
	void *Tail;

 /* Bits 2 and 3 need to be zero; bits 0 and 1 are ED_C and ED_H */
	void *Head;

 /* Lower 4 bits are ignored and available for the driver */
	struct OhciEndpointDescriptor_t *Next;
} OhciEndpointDescriptor;

#define TD_SET(field, value) \
	(((u32)(value) & TD_##field##_MASK) << TD_##field##_SHIFT)
#define TD_GET(field, var) (((var) >> TD_##field##_SHIFT) & TD_##field##_MASK)

/* ConditionCode */
#define TD_CC_SHIFT        28
#define TD_CC_MASK         0x0f
/* ErrorCount */
#define TD_EC_SHIFT        26
#define TD_EC_MASK         0x03
/* DataToggle */
#define TD_DT_SHIFT        24
#define TD_DT_MASK         0x03
/* DelayInterrupt */
#define TD_DI_SHIFT        21
#define TD_DI_MASK         0x07
/* Direction/PID */
#define TD_DP_SHIFT        19
#define TD_DP_MASK         0x03
#define TD_DP_OUT          (0x1 << TD_DP_SHIFT)
#define TD_DP_IN           (0x2 << TD_DP_SHIFT)
/* BufferRounding */
#define TD_R               (1 << 18)

typedef struct OhciTransferDescriptor_t
{
	u32 dw0;
	void *CurrentBuffer;
	struct OhciTransferDescriptor_t *Next;
	void *LastBuffer;
} OhciTransferDescriptor;

/*
 * Hardware transfer status codes -- CC from td->hwINFO or td->hwPSW
 */
#define TD_CC_NOERROR     0x00
#define TD_CC_CRC         0x01
#define TD_CC_BITSTUFFING 0x02
#define TD_CC_DATATOGGLEM 0x03
#define TD_CC_STALL       0x04
#define TD_DEVNOTRESP     0x05
#define TD_PIDCHECKFAIL   0x06
#define TD_UNEXPECTEDPID  0x07
#define TD_DATAOVERRUN    0x08
#define TD_DATAUNDERRUN   0x09
    /* 0x0A, 0x0B reserved for hardware */
#define TD_BUFFEROVERRUN  0x0C
#define TD_BUFFERUNDERRUN 0x0D
    /* 0x0E, 0x0F reserved for HCD */
#define TD_NOTACCESSED    0x0F

typedef struct
{
	void *InterruptTable[32];
	u16 FrameNumber;
	u16 Padding;
	OhciTransferDescriptor *HeadDone;
	u8 Reserved[120];
} OhciHcca;
CHECK_SIZE(OhciHcca, 0x100);
CHECK_OFFSET(OhciHcca, 0x00, InterruptTable);
CHECK_OFFSET(OhciHcca, 0x80, FrameNumber);
CHECK_OFFSET(OhciHcca, 0x82, Padding);
CHECK_OFFSET(OhciHcca, 0x84, HeadDone);
CHECK_OFFSET(OhciHcca, 0x88, Reserved);

typedef struct
{
	OhciTransferDescriptor Header;
	u16 PwsOffset[8];
} OhciTransferDescriptorIsoc;

#define OHCI_SET(field, value) \
	(((value) & OHCI_##field##_MASK) << OHCI_##field##_SHIFT)
#define OHCI_GET(field, var) \
	(((var) >> OHCI_##field##_SHIFT) & OHCI_##field##_MASK)

#define OHCI_INTR_SO               0x1U
#define OHCI_INTR_WDH              0x2U
#define OHCI_INTR_SF               0x4U
#define OHCI_INTR_RD               0x8U
#define OHCI_INTR_UE               0x10U
#define OHCI_INTR_FNO              0x20U
#define OHCI_INTR_RHSC             0x40U
#define OHCI_INTR_OC               0x40000000U
#define OHCI_INTR_MIE              0x80000000U
#define OHCI_CTRL_CBSR_SHIFT       0
#define OHCI_CTRL_CBSR_MASK        0x3U
#define OHCI_CTRL_PLE              0x4U
#define OHCI_CTRL_CLE              0x10U
#define OHCI_CTRL_BLE              0x20U
#define OHCI_CTRL_HCFS_SHIFT       6
#define OHCI_CTRL_HCFS_MASK        0x3U
#define OHCI_CTRL_HCFS_RESET       0
#define OHCI_CTRL_HCFS_RESUME      1
#define OHCI_CTRL_HCFS_OPERATIONAL 2
#define OHCI_CTRL_HCFS_SUSPEND     3
#define OHCI_CTRL_IR               0x100U
#define OHCI_CTRL_RWC              0x200U
#define OHCI_CS_HCR                0x1U
#define OHCI_CS_CLF                0x2U
#define OHCI_CS_BLF                0x4U
#define OHCI_CS_OCR                0x8U

/* rev bits */
#define OHCI_REV_MASK              0xffU

/* HcFmInterval bits */
#define OHCI_FI_FI_MASK            0x3fff
#define OHCI_FI_FI_SHIFT           0
#define OHCI_FI_FSMPS_MASK         0x7fff
#define OHCI_FI_FSMPS_SHIFT        16

/* roothub.status bits */
#define RH_HS_LPS                  0x00000001  /* local power status */
#define RH_HS_OCI                  0x00000002  /* over current indicator */
#define RH_HS_DRWE                 0x00008000 /* device remote wakeup enable */
#define RH_HS_LPSC                 0x00010000 /* local power status change */
#define RH_HS_OCIC                 0x00020000 /* over current indicator change */
#define RH_HS_CRWE                 0x80000000 /* clear remote wakeup enable */
#define RH_HS_RESERVED             0x7ffc7ffcU

/* roothub.b masks */
#define RH_B_DR                    0x0000ffff /* device removable flags */
#define RH_B_PPCM                  0xffff0000 /* port power control mask */

/* roothub.a masks */
#define RH_A_NDP                   (0xff << 0) /* number of downstream ports */
#define RH_A_RESERVED              (0x00ffe000U) /* Reserved bits */
#define RH_A_PSM                   (1 << 8) /* power switching mode */
#define RH_A_NPS                   (1 << 9) /* no power switching */
#define RH_A_DT                    (1 << 10) /* device type (mbz) */
#define RH_A_OCPM                  (1 << 11) /* over current protection mode */
#define RH_A_NOCP                  (1 << 12) /* no over current protection */
/* power on to power good time */
#define RH_A_POTPGT_SHIFT          24
#define RH_A_POTPGT_MASK           0xff

/* roothub.portstatus [i] bits */
#define RH_PS_CCS                  0x00000001 /* current connect status */
#define RH_PS_PES                  0x00000002 /* port enable status*/
#define RH_PS_PSS                  0x00000004 /* port suspend status */
#define RH_PS_POCI                 0x00000008 /* port over current indicator */
#define RH_PS_PRS                  0x00000010 /* port reset status */
#define RH_PS_PPS                  0x00000100 /* port power status */
#define RH_PS_LSDA                 0x00000200 /* low speed device attached */
#define RH_PS_CSC                  0x00010000 /* connect status change */
#define RH_PS_PESC                 0x00020000 /* port enable status change */
#define RH_PS_PSSC                 0x00040000 /* port suspend status change */
#define RH_PS_OCIC                 0x00080000 /* over current indicator change */
#define RH_PS_PRSC                 0x00100000 /* port reset status change */
/* These bits are reserved; the Wii uses them for flagging */
#define RH_PS_WII31                0x80000000

typedef struct
{
	u32 Revision;
	u32 Control;
	u32 CommandStatus;
	u32 InterruptStatus;
	u32 EnableInterrupts;
	u32 DisableInterrupts;
	OhciHcca *Hcca;
	u32 PeriodicCurrentEndpoint;
	OhciEndpointDescriptor *ControlHeadEndpoint;
	u32 ControlCurrentEndpoint;
	OhciEndpointDescriptor *BulkHeadEndpoint;
	u32 BulkCurrentEndpoint;
	u32 DoneHead;
	u32 FrameInterrupt;
	u32 FrameRemaining;
	u32 FrameNumber;
	u32 PeriodicStart;
	u32 LowSpeedThresHold;
	u32 RootHubDiscriptorA;
	u32 RootHubDiscriptorB;
	u32 RootHubStatus;
	u32 RootHubPortStatus[107];
} OhciRegs;
CHECK_SIZE(OhciRegs, 0x200);
CHECK_OFFSET(OhciRegs, 0x00, Revision);
CHECK_OFFSET(OhciRegs, 0x04, Control);
CHECK_OFFSET(OhciRegs, 0x08, CommandStatus);
CHECK_OFFSET(OhciRegs, 0x0c, InterruptStatus);
CHECK_OFFSET(OhciRegs, 0x10, EnableInterrupts);
CHECK_OFFSET(OhciRegs, 0x14, DisableInterrupts);
CHECK_OFFSET(OhciRegs, 0x18, Hcca);
CHECK_OFFSET(OhciRegs, 0x1c, PeriodicCurrentEndpoint);
CHECK_OFFSET(OhciRegs, 0x20, ControlHeadEndpoint);
CHECK_OFFSET(OhciRegs, 0x24, ControlCurrentEndpoint);
CHECK_OFFSET(OhciRegs, 0x28, BulkHeadEndpoint);
CHECK_OFFSET(OhciRegs, 0x2c, BulkCurrentEndpoint);
CHECK_OFFSET(OhciRegs, 0x30, DoneHead);
CHECK_OFFSET(OhciRegs, 0x34, FrameInterrupt);
CHECK_OFFSET(OhciRegs, 0x38, FrameRemaining);
CHECK_OFFSET(OhciRegs, 0x3c, FrameNumber);
CHECK_OFFSET(OhciRegs, 0x40, PeriodicStart);
CHECK_OFFSET(OhciRegs, 0x44, LowSpeedThresHold);
CHECK_OFFSET(OhciRegs, 0x48, RootHubDiscriptorA);
CHECK_OFFSET(OhciRegs, 0x4c, RootHubDiscriptorB);
CHECK_OFFSET(OhciRegs, 0x50, RootHubStatus);
CHECK_OFFSET(OhciRegs, 0x54, RootHubPortStatus);

#endif
