#include "bootsplash.h"
#include <nds.h>
#include <maxmod9.h>
#include <ctime>

#include "tonccpy.h"
#include "gif.hpp"
#include "lodepng.h"

#include "sound.h"

extern bool useTwlCfg;

extern bool fadeType;
extern bool controlTopBright;
extern bool controlBottomBright;
extern int screenBrightness;

bool cartInserted;

void bootSplashDSi(void) {
	// u16 whiteCol = ((whiteCol>>10)&0x1f) | ((whiteCol)&((31-3*ms().blfLevel)<<5)) | (whiteCol&(31-6*ms().blfLevel))<<10 | BIT(15);
	// toncset16(BG_GFX, 0xFFFF, 256*256);
	// toncset16(BG_GFX_SUB, 0xFFFF, 256*256);

	cartInserted = (REG_SCFG_MC != 0x11);

	int language = (useTwlCfg ? *(u8*)0x02000406 : PersonalData->language);

	char currentDate[16];
	time_t Raw;
	time(&Raw);
	const struct tm *Time = localtime(&Raw);

	strftime(currentDate, sizeof(currentDate), "%m/%d", Time);

	char path[256];
	sprintf(path, "nitro:/video/splash/%s.gif", language == 6 ? "iquedsi" : (isDSiMode() ? "dsi" : "ds"));
	Gif splash(path, true, true);

	path[0] = '\0';
	sprintf(path, "nitro:/video/tttstc/%i.gif", language);
	Gif healthSafety(path, false, true);

	// Draw first frame, then wait until the top is done
	healthSafety.displayFrame();
	healthSafety.pause();

	timerStart(0, ClockDivider_1024, TIMER_FREQ_1024(100), Gif::timerHandler);

	if (cartInserted) {
		u16 *gfx[2];
		int yPos = 142;
		for (int i = 0; i < 2; i++) {
			gfx[i] = oamAllocateGfx(&oamMain, SpriteSize_64x32, SpriteColorFormat_Bmp);
			oamSet(&oamMain, i, 67 + (i * 64), yPos, 0, 15, SpriteSize_64x32, SpriteColorFormat_Bmp, gfx[i], 0, false, false, false, false, false);
		}

		std::vector<unsigned char> image;
		unsigned int width, height;
		lodepng::decode(image, width, height, "nitro:/graphics/nintendo.png");

		for (unsigned int i = 0, y = 0, x = 0;i < image.size() / 4; i++, x++) {
			/* if (image[(i * 4) + 3] > 0) {
				switch (ms().nintendoLogoColor) {
					default: // Gray (Original color)
						break;
					case 1: // Red (Current color)
						image[(i * 4) + 0] = 0xFF;
						image[(i * 4) + 1] = 0;
						image[(i * 4) + 2] = 0;
						break;
					case 2: // Blue (Past JAP color)
						image[(i * 4) + 0] = 0;
						image[(i * 4) + 1] = 0;
						image[(i * 4) + 2] = 0xFF;
						break;
					case 3: // Magneta (GBA color)
						image[(i * 4) + 0] = 0xFF;
						image[(i * 4) + 1] = 0;
						image[(i * 4) + 2] = 0xFF;
						break;
				}
			} */
			u16 color = image[i * 4] >> 3 | (image[(i * 4) + 1] >> 3) << 5 | (image[(i * 4) + 2] >> 3) << 10 | (image[(i * 4) + 3] > 0) << 15;

			if (x >= width) {
				x = 0;
				y++;
			}

			gfx[x >= 64][y * 64 + (x % 64)] = color;
		}

		oamUpdate(&oamMain);
	}

	{
		const u16 white = 0xFFFF;
		BG_PALETTE[0] = white;
		BG_PALETTE_SUB[0] = white;

		controlBottomBright = false;
		fadeType = false;
		screenBrightness = 0;
		swiWaitForVBlank();
		controlTopBright = false;
		screenBrightness = 25;
		swiWaitForVBlank();
	}

	controlBottomBright = true;
	fadeType = true;

	// If both will loop forever, show for 3s or until button press
	if (splash.loopForever() && healthSafety.loopForever()) {
		for (int i = 0; i < 60 * 3 && !keysDown(); i++) {
			swiWaitForVBlank();
			scanKeys();

			if (splash.currentFrame() == 16)
				snd().playDSiBoot();
		}
	} else {
		u16 pressed = 0;
		while (!(splash.finished() && healthSafety.finished()) && !(pressed & KEY_START)) {
			swiWaitForVBlank();
			scanKeys();
			pressed = keysDown();
			pressed &= ~KEY_LID;

			if (splash.waitingForInput()) {
				if (healthSafety.paused())
					healthSafety.unpause();
				if (pressed) {
					splash.resume();
					snd().playSelect();
				}
			}

			if (healthSafety.waitingForInput()) {
				if (pressed) {
					healthSafety.resume();
					snd().playSelect();
				}
			}

			if (splash.currentFrame() == 26)
				snd().playDSiBoot();
		}
	}

	// Fade out
	controlTopBright = true;
	controlBottomBright = true;
	fadeType = false;
	for (int i = 0; i < 25; i++) { swiWaitForVBlank(); }

	timerStop(0);
}

void bootSplashInit(void) {
	videoSetMode(MODE_5_2D);
	videoSetModeSub(MODE_5_2D);
	vramSetBankA(VRAM_A_MAIN_BG);
	vramSetBankB(VRAM_B_MAIN_SPRITE);
	vramSetBankC(VRAM_C_SUB_BG);

	bgInit(3, BgType_Bmp8, BgSize_B8_256x256, 0, 0);
	bgSetPriority(3, 3);

	bgInitSub(3, BgType_Bmp8, BgSize_B8_256x256, 0, 0);
	bgSetPriority(7, 3);

	oamInit(&oamMain, SpriteMapping_Bmp_1D_128, false);

	snd();
	bootSplashDSi();
}
