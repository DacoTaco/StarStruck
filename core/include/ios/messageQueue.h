/*
	starstruck - a Free Software reimplementation for the Nintendo/BroadOn IOS.
	messageQueue - message queue

	Copyright (C) 2021	DacoTaco

# This code is licensed to you under the terms of the GNU GPL, version 2;
# see file COPYING or http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt
*/

#ifndef __IOS_MESSAGE_QUEUE_H__
#define __IOS_MESSAGE_QUEUE_H__

typedef enum
{
	None = 0,
	RegisteredEventHandler = 1,
	Invalid = 2
} MessageQueueFlags;

#endif
