/*

priiloader/preloader 0.30 - A tool which allows to change the default boot up sequence on the Wii console

Copyright (C) 2008-2019  crediar & DacoTaco

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation version 2.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.


*/

#include "gecko.h"

u8 GeckoFound = 0;

void CheckForGecko( void )
{
	GeckoFound = usb_isgeckoalive( EXI_CHANNEL_1 );
	if(GeckoFound)
		usb_flush(EXI_CHANNEL_1);
	return;
}
void gprintf( const char *str, ... )
{
	char astr[2048];
	s32 size = 0;
	memset(astr,0,sizeof(astr));

	char nstr[2048];
	memset(nstr,0,2048);
	snprintf(nstr,2048, "%s", str);
	
	va_list ap;
	va_start(ap,str);
	size = vsnprintf( astr, 2047, nstr, ap );
	va_end(ap);

	if(GeckoFound)
	{
		usb_sendbuffer( 1, astr, size );
		usb_flush(EXI_CHANNEL_1);
	}
	
	printf(astr);
	return;
}