#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <unistd.h>
#include <string.h>
#include <string>
#include <malloc.h>
#include <gccore.h>
#include <wiiuse/wpad.h>
#include <ogc/machine/processor.h>
#include <ogc/machine/asm.h>

#include "gecko.h"

#include "armboot_bin.h"

#define IOS_TO_LOAD 0x00000001000000FEULL
#define SRAMADDR(x) (0x0d400000 | ((x) & 0x000FFFFF))

static void *xfb = NULL;
static GXRModeObj *vmode = NULL;

//---------------------------------------------------------------------------------
int main(int argc, char **argv) 
//---------------------------------------------------------------------------------
{
	CheckForGecko();
	// Initialise the video system
	VIDEO_Init();

	vmode = VIDEO_GetPreferredMode(NULL);
	xfb = MEM_K0_TO_K1(SYS_AllocateFramebuffer(vmode));

	VIDEO_Configure(vmode);
	VIDEO_SetNextFramebuffer(xfb);
	VIDEO_SetBlack(false);
	VIDEO_Flush();

	VIDEO_WaitVSync();
	if (vmode->viTVMode & VI_NON_INTERLACE) VIDEO_WaitVSync();

	int x = 20, y = 20, w, h;
	w = vmode->fbWidth - (x * 2);
	h = vmode->xfbHeight - (y + 20);

	// Initialize the console
	CON_InitEx(vmode, x, y, w, h);

	VIDEO_ClearFrameBuffer(vmode, xfb, COLOR_BLACK);
    
	// This function initialises the attached controllers
	//WPAD_Init();
	PAD_Init();

	printf("\n\n\n\nHello World!\n");
	if(	read32(0x0d800064) != 0xFFFFFFFF )
	{
		printf("AHB Access Disabled\n");
		exit(0);
	}
	
	printf("AHB Access Enabled\n");
	write16(0x0d8b420a, 0);
	s8 mini_loaded = 0;
	while(1) {

		// Call WPAD_ScanPads each loop, this reads the latest controller states
		WPAD_ScanPads();
		PAD_ScanPads();

		// WPAD_ButtonsDown tells us which buttons were pressed in this loop
		// this is a "one shot" state which will not fire again until the button has been released
		u32 pressed = 0; //WPAD_ButtonsDown(0);
		u32 gcPressed ATTRIBUTE_ALIGN(32) = PAD_ButtonsDown(0);
		u32 output ATTRIBUTE_ALIGN(32) = 0xB16B00B5;

		// We return to the launcher application via exit
		if ( pressed & WPAD_BUTTON_HOME || gcPressed & PAD_BUTTON_START) 
		{
			if(!mini_loaded)
				exit(0);
			
			ioctlv data[2] = {0};
			u64 titleID = 0x000100014C554C5ALL;
			data[0].data = &titleID;
			data[0].len = sizeof(u64);
			printf("opening ES : %d\n", __ES_Init());
			printf("ret of Launch : %d\n", IOS_IoctlvReboot((mini_loaded) ? 0x08 : 0x50, 0x08, 2, 0, data));
			while(1);
		}
		
		if ( pressed & WPAD_BUTTON_B || gcPressed & PAD_BUTTON_B)
		{
			printf("im alive!\n");
		}
		
		if ( pressed & WPAD_BUTTON_2 || gcPressed & PAD_BUTTON_X ) 
		{
			write16(0x0d8b420a, 0);
			//printf("msg : 0x%08X\n", read32(0xd8000E0));
			//printf("sram : 0x%08X\n", read32(SRAMADDR(0xFFFFE010)));
			u32* page_table = (u32*)0x93850000;
			printf("sram : 0x%08X\n", read32(SRAMADDR(0xFFFFE010)));
			printf("page table 0 : 0x%08X\n", page_table[0]);
			printf("page table 1 : 0x%08X\n", page_table[1]);
			printf("page table 2 : 0x%08X\n", page_table[2]);
			printf("page table 0x80 : 0x%08X\n", page_table[0x4E8/4]);
			//printf("page table entry 1 : 0x%08X\n", read32((u32)page_table));
			
			gprintf("dumping page table...\n");
			for(int i = 0; i < 0x1000; i++)
			{
				gprintf("%08X", page_table[i]);
			}
			gprintf("\ndone\n");
			
			gprintf("dumping other page info...\n");
			u32* other_table = (u32*)0x93854000;
			for(int i = 0x4000; i < 0x20000; i+= 4)
			{
				gprintf("%08X", other_table[i]);
			}
			gprintf("\ndone\n");
			//
		}
		
		if ( pressed & WPAD_BUTTON_1 || gcPressed & PAD_BUTTON_Y ) 
		{
			printf("poking MEM2\n");
			*(vu32*)0x90001234 = 0x9001CAFE;
			DCFlushRange((void*)0x90001234, 4);
			/*write32(0x91234567, 0x9001CAFE);
			DCFlushRange((void*)0x90001234, 4);   */
			printf("opening ES : %d\n", __ES_Init());
			printf("sending IOS_Ioctl : 0x%08X - 0x%08X\n",(u32)&pressed, (u32)&output);
			printf("ret of IOS_Ioctl : %d\n", IOS_Ioctl((mini_loaded) ? 0x08 : 0x50, 0x0F, &pressed, 4, &output, 4));
			printf("output : 0x%08X\n", output);
		}
		
		if ( pressed & WPAD_BUTTON_A || gcPressed & PAD_BUTTON_A ) 
		{
			if(mini_loaded)
				continue;
			
			//load mini
			printf("loading mini...\n");
			mini_loaded = 1;
			/*// ** Boot mini from mem code by giantpune ** //
			void *mini = memalign(32, armboot_bin_size);  
			if(!mini) 
				  return 0;    

			memcpy(mini, armboot_bin, armboot_bin_size);  
			DCFlushRange(mini, armboot_bin_size);               

			*(u32*)0xc150f000 = 0x424d454d;  
			asm volatile("eieio");  

			*(u32*)0xc150f004 = MEM_VIRTUAL_TO_PHYSICAL(mini);  
			asm volatile("eieio");

			tikview views[4] ATTRIBUTE_ALIGN(32);
			printf("Shutting down IOS subsystems.\n");
			__IOS_ShutdownSubsystems();
			printf("Loading IOS 254.\n");
			__ES_Init();
			u32 numviews;
			ES_GetNumTicketViews(IOS_TO_LOAD, &numviews);
			ES_GetTicketViews(IOS_TO_LOAD, views, numviews);
			ES_LaunchTitleBackground(IOS_TO_LOAD, &views[0]);
			free(mini);*/
			
			// ** boot mini without BootMii IOS code by Crediar ** //
			unsigned char ES_ImportBoot2[16] =
			{
				0x68, 0x4B, 0x2B, 0x06, 0xD1, 0x0C, 0x68, 0x8B, 0x2B, 0x00, 0xD1, 0x09, 0x68, 0xC8, 0x68, 0x42
			};

			printf("Shutting down IOS subsystems.\n");
			__IOS_ShutdownSubsystems();
			for(u32 i = 0x939F0000; i < 0x939FE000; i+=2 )
			{
				if( memcmp( (void*)(i), ES_ImportBoot2, sizeof(ES_ImportBoot2) ) == 0 )
				{
					DCInvalidateRange( (void*)i, 0x20 );
					
					*(vu32*)(i+0x00)	= 0x48034904;	// LDR R0, 0x10, LDR R1, 0x14
					*(vu32*)(i+0x04)	= 0x477846C0;	// BX PC, NOP
					*(vu32*)(i+0x08)	= 0xE6000870;	// SYSCALL
					*(vu32*)(i+0x0C)	= 0xE12FFF1E;	// BLR
					*(vu32*)(i+0x10)	= 0x10100000;	// offset
					*(vu32*)(i+0x14)	= 0x0025161F;	// version

					DCFlushRange( (void*)i, 0x20 );

					void *mini = (void*)0x90100000;
					memcpy(mini, armboot_bin, armboot_bin_size);
					DCFlushRange( mini, armboot_bin_size );
					
					s32 fd = IOS_Open( "/dev/es", 0 );
					
					u8 *buffer = (u8*)memalign( 32, 0x100 );
					memset( buffer, 0, 0x100 );
					
					printf("ES_ImportBoot():%d\n", IOS_IoctlvAsync( fd, 0x1F, 0, 0, (ioctlv*)buffer, NULL, NULL ) );
					__IPC_Reinitialize();
					printf("IPC reinit\n");
					break;
				}
			}
		}

		// Wait for the next frame
		VIDEO_WaitVSync();
	}

	return 0;
}
