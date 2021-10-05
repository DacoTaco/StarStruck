/*
	mini - a Free Software replacement for the Nintendo/BroadOn IOS.
	USBGecko support code

Copyright (c) 2008		Nuke - <wiinuke@gmail.com>
Copyright (C) 2008, 2009	Hector Martin "marcan" <marcan@marcansoft.com>
Copyright (C) 2008, 2009	Sven Peter <svenpeter@gmail.com>
Copyright (C) 2009		Andre Heider "dhewg" <dhewg@wiibrew.org>

# This code is licensed to you under the terms of the GNU GPL, version 2;
# see file COPYING or http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt
*/
#ifdef CAN_HAZ_USBGECKO

#include <types.h>
#include <vsprintf.h>
#include <string.h>
#include <ios/processor.h>

#include "core/hollywood.h"
#include "interrupt/irq.h"
#include "gecko.h"
#include "elf.h"
#include "powerpc.h"

// irq context

#define GECKO_STATE_NONE 0
#define GECKO_STATE_RECEIVE_BUFFER_SIZE 1
#define GECKO_STATE_RECEIVE_BUFFER 2

#define GECKO_BUFFER_MAX (20 * 1024 * 1024)

#define GECKO_CMD_BIN_ARM 0x4241524d
#define GECKO_CMD_BIN_PPC 0x42505043

static u32 _gecko_cmd = 0;
static u32 _gecko_cmd_start_time = 0;
static u32 _gecko_state = GECKO_STATE_NONE;
static u32 _gecko_receive_left = 0;
static u32 _gecko_receive_len = 0;
static u8 *_gecko_receive_buffer = NULL;

void gecko_process(void) {
	u8 b;

	if (!gecko_found)
		return;

	if (_gecko_cmd_start_time && read32(HW_TIMER) >
			(_gecko_cmd_start_time + IRQ_ALARM_MS2REG(5000)))
		goto cleanup;

	switch (_gecko_state) {
	case GECKO_STATE_NONE:
		if (!gecko_checkrecv() || !_gecko_recvbyte(&b))
			return;

		_gecko_cmd <<= 8;
		_gecko_cmd |= b;

		switch (_gecko_cmd) {
		case GECKO_CMD_BIN_ARM:
			_gecko_state = GECKO_STATE_RECEIVE_BUFFER_SIZE;
			_gecko_receive_len = 0;
			_gecko_receive_left = 4;
			_gecko_receive_buffer = (u8 *) 0x0; // yarly

			_gecko_cmd_start_time = read32(HW_TIMER);

			break;

		case GECKO_CMD_BIN_PPC:
			_gecko_state = GECKO_STATE_RECEIVE_BUFFER_SIZE;
			_gecko_receive_len = 0;
			_gecko_receive_left = 4;
			_gecko_receive_buffer = (u8 *) 0x10100000;

			_gecko_cmd_start_time = read32(HW_TIMER);

			break;
		}

		return;

	case GECKO_STATE_RECEIVE_BUFFER_SIZE:
		if (!gecko_checkrecv() || !_gecko_recvbyte(&b))
			return;

		_gecko_receive_len <<= 8;
		_gecko_receive_len |= b;
		_gecko_receive_left--;

		if (!_gecko_receive_left) {
			if (_gecko_receive_len > GECKO_BUFFER_MAX)
				goto cleanup;

			_gecko_state = GECKO_STATE_RECEIVE_BUFFER;
			_gecko_receive_left = _gecko_receive_len;

			// sorry pal, that memory is mine now
			powerpc_hang();
		}

		return;

	case GECKO_STATE_RECEIVE_BUFFER:
		while (_gecko_receive_left) {
			if (!gecko_checkrecv() || !_gecko_recvbyte(_gecko_receive_buffer))
				return;

			_gecko_receive_buffer++;
			_gecko_receive_left--;
		}

		if (!_gecko_receive_left)
			break;

		return;

	default:
		gecko_printf("MINI/GECKO: internal error\n");
		return;
	}

	ioshdr *h;

	// done receiving, handle the command
	switch (_gecko_cmd) {
	case GECKO_CMD_BIN_ARM:
		h = (ioshdr *) (u32 *) 0x0;

		if (h->hdrsize != sizeof (ioshdr))
			goto cleanup;

		if (memcmp("\x7F" "ELF\x01\x02\x01",
					(void *) (h->hdrsize + h->loadersize), 7))
			goto cleanup;

		/*enqueue ppc jump here*/
		break;

	case GECKO_CMD_BIN_PPC:
		/*enqueue ppc jump here*/
		break;
	}

cleanup:
	gecko_flush();

	_gecko_cmd = 0;
	_gecko_cmd_start_time = 0;
	_gecko_state = GECKO_STATE_NONE;
}

#endif

