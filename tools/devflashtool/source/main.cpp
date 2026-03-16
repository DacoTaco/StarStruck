#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <unistd.h>
#include <string.h>
#include <string>
#include <malloc.h>
#include <time.h>

#include <fat.h>
#include <gccore.h>
#include <wiiuse/wpad.h>
#include <ogc/ipc.h>
#include <ogc/machine/processor.h>
#include <ogc/machine/asm.h>
#include <sdcard/wiisd_io.h>
#include <ogc/mutex.h>

#include "gecko.h"
#include "IOS.hpp"

static void *xfb = NULL;
static GXRModeObj *vmode = NULL;

typedef struct
{
	u32 NandSizeBitShift;
	u32 BlockSizeBitShift;
	u32 PageSizeBitShift;
	u32 EccSizeBitShift;
	u32 HMACSizeShift;
	u16 PageCopyMask;
	u16 SupportPageCopy;
	u16 EccDataCheckByteOffset;
	u8 Padding[2];
} NandSizeInformation;

// Small helper for derived NAND values
typedef struct
{
	u32 PageSize;         // bytes per page
	u32 EccSize;          // bytes per ecc
	u32 ChunkSize;        // page + ecc
	u32 PagesPerBlock;   // pages in a block
} NandStats;

static const int IOCTL_GET_STATS = 1;
static s32 flashHandle = -1;

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
	if (vmode->viTVMode & VI_NON_INTERLACE)
		VIDEO_WaitVSync();

	// Initialize the console
	CON_Init(xfb, (vmode->viWidth + vmode->viXOrigin - 640) / 2,
	         (vmode->viHeight + vmode->viYOrigin - 480) / 2, 640, 480,
	         640 * VI_DISPLAY_PIX_SZ);
	CheckForGecko();
	VIDEO_ClearFrameBuffer(vmode, xfb, COLOR_BLACK);

	if (read32(0x0d800064) != 0xFFFFFFFF)
	{
		printf("AHB Access Disabled\n");
		exit(0);
	}

	printf("AHB Access Enabled\n");
	write16(0x0d8b420a, 0);

	// This function initialises the attached controllers
	//WPAD_Init();
	PAD_Init();

	if (!fatMountSimple("sd", &__io_wiisd))
	{
		printf("fatMountSimple failure: terminating\n");
		return 0;
	}

	PatchIOS({ AhbProtPatcher, NandAccessPatcher });
	PatchIOSKernel({ DebugRedirectionPatch });

	flashHandle = IOS_Open("/dev/flash", IPC_OPEN_READ);
	if (flashHandle < 0)
	{
		//patch IOS and retry via /dev/fs
		printf("\npatching /dev/fs open...");
		if (PatchIOS({ OpenFSAsFlash }) > 0)
		{
			//the patch should have made /dev/fs work like /dev/flash on open
			flashHandle = IOS_Open("/dev/fs", IPC_OPEN_READ);
			if (flashHandle >= 0)
				PatchIOS({ OpenFSAsFS }); //restore the original open check for /dev/fs, so it doesn't mess with stuff
		}

		if (flashHandle < 0)
		{
			printf("failed to open /dev/flash!\n");
			return 0;
		}
	}

	//Get NAND Stats
	NandSizeInformation info;
	int rc = IOS_Ioctl(flashHandle, IOCTL_GET_STATS, NULL, 0, &info, sizeof(info));
	if (rc < 0)
	{
		printf("Failed to fetch nand stats\n");
		return 0;
	}

	u32 pageSize = 1u << (info.PageSizeBitShift & 0xFF);
	u32 eccSize = 1u << (info.EccSizeBitShift & 0xFF);
	NandStats stats = { .PageSize = pageSize,
		                .EccSize = eccSize,
		                .ChunkSize = pageSize + eccSize,
		                .PagesPerBlock =
		                    1u << ((info.BlockSizeBitShift - info.PageSizeBitShift) & 0xFF) };

	gprintf("NAND stats: page=%u ecc=%u chunk=%u pages per block=%u\n",
	        stats.PageSize, stats.EccSize, stats.ChunkSize, stats.PagesPerBlock);

	while (1)
	{
		// Call WPAD_ScanPads each loop, this reads the latest controller states
		WPAD_ScanPads();
		PAD_ScanPads();

		// WPAD_ButtonsDown tells us which buttons were pressed in this loop
		// this is a "one shot" state which will not fire again until the button has been released
		u32 pressed = 0; //WPAD_ButtonsDown(0);
		u32 gcPressed ATTRIBUTE_ALIGN(32) = PAD_ButtonsDown(0);

		// We return to the launcher application via exit
		if (pressed & WPAD_BUTTON_HOME || gcPressed & PAD_BUTTON_START)
		{
			exit(0);
		}

		if (pressed & WPAD_BUTTON_B || gcPressed & PAD_BUTTON_B)
		{
			printf("im alive!\n");
		}

		if (pressed & WPAD_BUTTON_2 || gcPressed & PAD_BUTTON_X)
		{
			return 0;
		}

		if (pressed & WPAD_BUTTON_A || gcPressed & PAD_BUTTON_A)
		{
			//save nand dump
			gprintf("starting dump...\n");

			void *buf = NULL;
			FILE *outputFile = NULL;
			try
			{
				//reset the handler's file positition
				IOS_Seek(flashHandle, 0, SEEK_SET);

				outputFile = fopen("sd:/nand_ios.bin", "wb");
				if (!outputFile)
					throw std::string("Failed to open output file for writing");

				void *buf = memalign(32, stats.ChunkSize);
				if (!buf)
					throw std::string("Failed to allocate aligned buffer for NAND read");

				u64 totalWritten = 0;
				u64 iterations = 0;
				for (s32 readData = 1; readData > 0;)
				{
					readData = IOS_Read(flashHandle, buf, stats.ChunkSize);
					iterations++;

					if (readData == -4) // EINVAL -> end-of-flash
					{
						gprintf("End of flash reached (rc=%d)\n", readData);
						break;
					}

					if (readData == -11 || readData == -12)
					{
						// bad DMAed page: assume buffer contains chunkSize bytes, write them
						size_t wrote = fwrite(buf, 1, stats.ChunkSize, outputFile);
						if (wrote != stats.ChunkSize)
							throw std::string("Failed to write bad page to file");

						totalWritten += wrote;
						// advance past the bad page
						IOS_Seek(flashHandle, 1, SEEK_CUR);
						gprintf("Wrote bad page, total=%llu bytes\n",
						        (unsigned long long)totalWritten);
						continue;
					}

					if (readData < 0)
						throw new std::string("IOS_Read failed with error code " +
						                      std::to_string(readData));

					if (readData == 0)
						break;

					// positive bytes read
					size_t toWrite = (size_t)readData;
					size_t wrote = fwrite(buf, 1, toWrite, outputFile);
					if (wrote != toWrite)
						throw std::string("Failed to write output page to file");

					totalWritten += wrote;

					// progress every 128 iterations
					if ((iterations & 127) == 0)
					{
						double sizeInMegabyte = (double)totalWritten / (1024.0 * 1024.0);
						gprintf("Dump progress: %.2f MB\n", sizeInMegabyte);
					}
				}

				gprintf("Dump complete, total bytes=%llu\n", totalWritten);

			} catch (const std::string &ex)
			{
				gprintf("IOS Flash Exception -> %s\n", ex.c_str());
			} catch (char const *ex)
			{
				gprintf("IOS Flash Exception -> %s\n", ex);
			} catch (...)
			{
				gprintf("IOS Flash Exception was thrown\n");
			}

			if (buf)
			{
				free(buf);
				buf = NULL;
			}

			if (outputFile)
			{
				fclose(outputFile);
				outputFile = NULL;
			}
		}

		// Wait for the next frame
		VIDEO_WaitVSync();
	}

	return 0;
}
