/*
	mini - a Free Software replacement for the Nintendo/BroadOn IOS.
	inter-processor communications

Copyright (C) 2008, 2009	Hector Martin "marcan" <marcan@marcansoft.com>
Copyright (C) 2008, 2009	Haxx Enterprises <bushing@gmail.com>
Copyright (C) 2008, 2009	Sven Peter <svenpeter@gmail.com>
Copyright (C) 2009			Andre Heider "dhewg" <dhewg@wiibrew.org>
Copyright (C) 2009		John Kelley <wiidev@kelley.ca>

# This code is licensed to you under the terms of the GNU GPL, version 2;
# see file COPYING or http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt
*/

#ifndef __IPC_H__
#define __IPC_H__

#include "types.h"
#include "ios_module.h"

/* For the sake of interface compatibility between mini and powerpc code,
   you should try to commit any enhancements you make back upstream so
   that they can be assigned a standard request number.  Otherwise, if
   you are creating a new device, you MUST assign it a device ID >= 0x80.

   Likewise, if you add functionality to any of the existing drivers,
   your must assign it to a request ID >= 0x8000.  This will prevent
   problems if a mismatch between ARM and PPC code occurs.  Similarly,
   if you add functions, you should always add them to the end, to
   prevent someone from calling the wrong function.

   Even still, you are encouraged to add in sanity checks and version
   checking to prevent strange bugs or even data loss.  --bushing */

#define IPC_OPEN_NONE	0x00
#define IPC_OPEN_READ	0x01
#define IPC_OPEN_WRITE	0x02
#define IPC_OPEN_RW		(IPC_OPEN_READ|IPC_OPEN_WRITE)

#define IPC_DEV_SYS		0x00
#define IPC_DEV_NAND	0x01
#define IPC_DEV_SDHC	0x02
#define IPC_DEV_KEYS	0x03
#define IPC_DEV_AES		0x04
#define IPC_DEV_BOOT2	0x05
#define IPC_DEV_PPC		0x06
#define IPC_DEV_SDMMC	0x07
#define IPC_DEV_ES		0x08

#define IPC_IN_SIZE		32
#define IPC_OUT_SIZE	32

typedef const struct {
	char magic[3];
	char version;
	void *mem2_boundary;
	volatile ipcreq **ipc_in;
	u32 ipc_in_size;
	volatile ipcreq **ipc_out;
	u32 ipc_out_size;
} ipc_infohdr;

void ipc_irq(void);
void ipc_send_ack(void);
void ipc_reply(ipcreq* req);
void ipc_enqueue_reuqest(ipcreq* req);
void ipc_initialize(void);
void ipc_shutdown(void);
void ipc_post(u32 code, u32 tag, u32 num_args, ...);
void ipc_flush(void);
u32  ipc_main(void);
void ipc_ppc_boot_title(u64 titleId);

#endif

