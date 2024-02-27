#include "perGameSettings.h"
#include "cheat.h"
#include "ndsheaderbanner.h"

#include <array>
#include <nds.h>
#include <dirent.h>

extern int consoleModel;

constexpr std::array<const char *, 3> offOnLabels = {"Default", "Off", "On"};
constexpr std::array<const char *, 10> languageLabels = {"Default", "System", "Japanese", "English", "French", "German", "Italian", "Spanish", "Chinese", "Korean"};
constexpr std::array<const char *, 9> regionLabels = {"Default", "Per-game", "System", "Japan", "USA", "Europe", "Australia", "China", "Korea"};
constexpr std::array<const char *, 4> runInLabels = {"Default", "DS Mode", "Auto", "DSi Mode"};
constexpr std::array<const char *, 3> cpuLabels = {"Default", "67 MHz (NTR)", "133 MHz (TWL)"};
constexpr std::array<const char *, 3> vramLabels = {"Default", "DS Mode", "DSi Mode"};
constexpr std::array<const char *, 4> expandLabels = {"Default", "No", "Yes", "Yes+512 KB"};
constexpr std::array<const char *, 3> bootstrapLabels = {"Default", "Release", "Nightly"};
constexpr std::array<const char *, 4> widescreenLabels = {"Default", "Off", "On", "Forced"};

GameSettings::GameSettings(const std::string &fileName, const std::string &filePath) : iniPath("/_nds/ntr-forwarder/gamesettings/" + fileName + ".ini"), romPath(filePath), ini(iniPath) {
	language = ini.GetInt("GAMESETTINGS", "LANGUAGE", language);
	region = ini.GetInt("GAMESETTINGS", "REGION", region);
	saveNo = ini.GetInt("GAMESETTINGS", "SAVE_NUMBER", saveNo);
	dsiMode = ini.GetInt("GAMESETTINGS", "DSI_MODE", dsiMode);
	boostCpu = ini.GetInt("GAMESETTINGS", "BOOST_CPU", boostCpu);
	boostVram = ini.GetInt("GAMESETTINGS", "BOOST_VRAM", boostVram);
	cardReadDMA = ini.GetInt("GAMESETTINGS", "CARD_READ_DMA", cardReadDMA);
	asyncCardRead = ini.GetInt("GAMESETTINGS", "ASYNC_CARD_READ", asyncCardRead);
	bootstrapFile = ini.GetInt("GAMESETTINGS", "BOOTSTRAP_FILE", bootstrapFile);
	widescreen = ini.GetInt("GAMESETTINGS", "WIDESCREEN", widescreen);
}

void GameSettings::save() {
	ini.SetInt("GAMESETTINGS", "LANGUAGE", language);
	ini.SetInt("GAMESETTINGS", "REGION", region);
	ini.SetInt("GAMESETTINGS", "SAVE_NUMBER", saveNo);
	ini.SetInt("GAMESETTINGS", "DSI_MODE", dsiMode);
	ini.SetInt("GAMESETTINGS", "BOOST_CPU", boostCpu);
	ini.SetInt("GAMESETTINGS", "BOOST_VRAM", boostVram);
	ini.SetInt("GAMESETTINGS", "CARD_READ_DMA", cardReadDMA);
	ini.SetInt("GAMESETTINGS", "ASYNC_CARD_READ", asyncCardRead);
	ini.SetInt("GAMESETTINGS", "BOOTSTRAP_FILE", bootstrapFile);
	ini.SetInt("GAMESETTINGS", "WIDESCREEN", widescreen);

	// Ensure the folder exists
	if(access("/_nds/ntr-forwarder/gamesettings", F_OK) != 0)
		mkdir("/_nds/ntr-forwarder/gamesettings", 0777);

	ini.SaveIniFile(iniPath);
}

bool GameSettings::isDonorRom(const u32 arm7size, const u32 a7mbk6, const u32 SDKVersion) {
	if (a7mbk6 == 0x080037C0 || SDKVersion == 0) {
		return false;
	}

	return ((SDKVersion > 0x2000000 && SDKVersion < 0x3000000
		&& (arm7size == 0x2619C // SDK2.0
		 || arm7size == 0x262A0
		 || arm7size == 0x26A60
		 || arm7size == 0x27218
		 || arm7size == 0x27224
		 || arm7size == 0x2724C
		 || arm7size == 0x27280))
	|| (!isDSiMode() && SDKVersion > 0x5000000	// SDK5 (NTR)
	 && (arm7size==0x23708
	  || arm7size==0x2378C
	  || arm7size==0x237F0
	  || arm7size==0x26370
	  || arm7size==0x2642C
	  || arm7size==0x26488
	  || arm7size==0x26BEC
	  || arm7size==0x27618
	  || arm7size==0x2762C
	  || arm7size==0x29CEC))
	|| (!isDSiMode() && SDKVersion > 0x5000000	// SDK5 (TWL)
	 && (arm7size==0x22B40
	  || arm7size==0x22BCC
	  || arm7size==0x28F84
	  || arm7size==0x2909C
	  || arm7size==0x2914C
	  || arm7size==0x29164
	  || arm7size==0x29EE8
	  || arm7size==0x2A2EC
	  || arm7size==0x2A318
	  || arm7size==0x2AF18
	  || arm7size==0x2B184
	  || arm7size==0x2B24C
	  || arm7size==0x2C5B4)));
}

void GameSettings::menu(FILE* f_nds_file, const std::string &fileName, const bool isHomebrew) {
	consoleDemoInit();

	extern sNDSHeaderExt ndsHeader;
	const u32 arm7size = ndsHeader.arm7binarySize;

	u32 SDKVersion = 0;
	// u8 sdkSubVer = 0;
	// char sdkSubVerChar[8] = {0};
	if (memcmp(ndsHeader.gameCode, "HND", 3) == 0 || memcmp(ndsHeader.gameCode, "HNE", 3) == 0 || (ndsHeader.a7mbk6 != 0x080037C0 && !isHomebrew)) {
		SDKVersion = getSDKVersion(f_nds_file);
		// tonccpy(&sdkSubVer, (u8*)&SDKVersion+2, 1);
		// sprintf(sdkSubVerChar, "%d", sdkSubVer);
	}

	u16 held;
	int cursorPosition = 0;
	int numOptions = consoleModel == 2 ? 9 : 8;
	if (!isDSiMode()) {
		numOptions = 3;
	}
	const bool showSetDonorRom = isDonorRom(arm7size, ndsHeader.a7mbk6, SDKVersion);
	if (showSetDonorRom) {
		numOptions++;
	}
	while(1) {
		consoleClear();
		iprintf("ntr-forwarder\n\n");
		iprintf("  Language: %s\n", languageLabels[language + 2]);
		iprintf("  Region: %s\n", regionLabels[region + 3]);
		iprintf("  Save Number: %d\n", saveNo);
		if (isDSiMode()) {
			iprintf("  Run in: %s\n", runInLabels[dsiMode + 1]);
			iprintf("  ARM9 CPU Speed: %s\n", cpuLabels[boostCpu + 1]);
			iprintf("  VRAM Mode: %s\n", vramLabels[boostVram + 1]);
			iprintf("  Card Read DMA: %s\n", offOnLabels[cardReadDMA + 1]);
			iprintf("  Async Card Read: %s\n", offOnLabels[asyncCardRead + 1]);
		}
		iprintf("  Bootstrap File: %s\n", bootstrapLabels[bootstrapFile + 1]);
		if(consoleModel == 2)
			iprintf("  Widescreen: %s\n", widescreenLabels[widescreen + 1]);
		if (showSetDonorRom)
			iprintf("  Set as Donor ROM\n");
		iprintf("\n<B> cancel, <START> save,\n<X> cheats\n");

		// Print cursor
		iprintf("\x1b[%d;0H>", 2 + cursorPosition);

		do {
			scanKeys();
			held = keysDownRepeat();

			swiWaitForVBlank();
		} while(!held);

		if(held & KEY_UP) {
			cursorPosition--;
			if(cursorPosition < 0)
				cursorPosition = numOptions;
		} else if(held & KEY_DOWN) {
			cursorPosition++;
			if(cursorPosition > numOptions)
				cursorPosition = 0;
		} else if(held & (KEY_LEFT | KEY_A)) {
			if (isDSiMode()) {
				switch(cursorPosition) {
					case 0:
						language--;
						if(language < -2) language = 7;
						break;
					case 1:
						region--;
						if(region < -3) region = 5;
						break;
					case 2:
						saveNo--;
						if(saveNo < 0) saveNo = 9;
						break;
					case 3:
						dsiMode--;
						if(dsiMode < -1) dsiMode = 2;
						break;
					case 4:
						boostCpu--;
						if(boostCpu < -1) boostCpu = 1;
						break;
					case 5:
						boostVram--;
						if(boostVram < -1) boostVram = 1;
						break;
					case 6:
						cardReadDMA--;
						if(cardReadDMA < -1) cardReadDMA = 1;
						break;
					case 7:
						asyncCardRead--;
						if(asyncCardRead < -1) asyncCardRead = 1;
						break;
					case 8:
						bootstrapFile--;
						if(bootstrapFile < -1) bootstrapFile = 1;
						break;
					case 9:
						widescreen--;
						if(widescreen < -1) widescreen = 2;
						break;
				}
			} else {
				switch(cursorPosition) {
					case 0:
						language--;
						if(language < -2) language = 7;
						break;
					case 1:
						region--;
						if(region < -3) region = 5;
						break;
					case 2:
						saveNo--;
						if(saveNo < 0) saveNo = 9;
						break;
					case 3:
						bootstrapFile--;
						if(bootstrapFile < -1) bootstrapFile = 1;
						break;
				}
			}
		} else if(held & KEY_RIGHT) {
			if (isDSiMode()) {
				switch(cursorPosition) {
					case 0:
						language++;
						if(language > 7) language = -2;
						break;
					case 1:
						region++;
						if(region > 5) region = -3;
						break;
					case 2:
						saveNo++;
						if(saveNo > 9) saveNo = 0;
						break;
					case 3:
						dsiMode++;
						if(dsiMode > 2) dsiMode = -1;
						break;
					case 4:
						boostCpu++;
						if(boostCpu > 1) boostCpu = -1;
						break;
					case 5:
						boostVram++;
						if(boostVram > 1) boostVram = -1;
						break;
					case 6:
						cardReadDMA++;
						if(cardReadDMA > 1) cardReadDMA = -1;
						break;
					case 7:
						asyncCardRead++;
						if(asyncCardRead > 1) asyncCardRead = -1;
						break;
					case 8:
						bootstrapFile++;
						if(bootstrapFile > 1) bootstrapFile = -1;
						break;
					case 9:
						widescreen++;
						if(widescreen > 2) widescreen = -1;
						break;
				}
			} else {
				switch(cursorPosition) {
					case 0:
						language++;
						if(language > 7) language = -2;
						break;
					case 1:
						region++;
						if(region > 5) region = -3;
						break;
					case 2:
						saveNo++;
						if(saveNo > 9) saveNo = 0;
						break;
					case 3:
						bootstrapFile++;
						if(bootstrapFile > 1) bootstrapFile = -1;
						break;
				}
			}
		} else if(held & KEY_B) {
			return;
		} else if(held & KEY_X) {
			CheatCodelist codelist;
			codelist.selectCheats(romPath);
		} else if(held & KEY_START) {
			save();
			return;
		}

		if(showSetDonorRom && (held & KEY_A) && cursorPosition == numOptions) {
			consoleClear();

			const char* pathDefine = "DONORTWL_NDS_PATH"; // SDK5.x (TWL)
			if (arm7size == 0x29EE8) {
				pathDefine = "DONORTWL0_NDS_PATH"; // SDK5.0 (TWL)
			} else if (SDKVersion > 0x5000000
					 && (arm7size==0x23708
					  || arm7size==0x2378C
					  || arm7size==0x237F0
					  || arm7size==0x26370
					  || arm7size==0x2642C
					  || arm7size==0x26488
					  || arm7size==0x27618
					  || arm7size==0x2762C
					  || arm7size==0x29CEC)) {
				pathDefine = "DONOR5_NDS_PATH"; // SDK5.x (NTR)
			} else if (SDKVersion > 0x5000000
					 && (arm7size==0x26BEC)) {
				pathDefine = "DONOR5_NDS_PATH_ALT"; // SDK5.x (NTR)
			} else if (SDKVersion > 0x2000000 && SDKVersion < 0x3000000
					&& (arm7size == 0x2619C
					 || arm7size == 0x262A0
					 || arm7size == 0x26A60
					 || arm7size == 0x27218
					 || arm7size == 0x27224
					 || arm7size == 0x2724C
					 || arm7size == 0x27280)) {
				pathDefine = "DONOR20_NDS_PATH"; // SDK2.0
			}

			extern std::string romfolder;
			extern void RemoveTrailingSlashes(std::string& path);

			std::string romFolderNoSlash = romfolder;
			RemoveTrailingSlashes(romFolderNoSlash);
			const char *bootstrapIniPath = "/_nds/nds-bootstrap.ini";
			CIniFile bootstrapini(bootstrapIniPath);
			bootstrapini.SetString("NDS-BOOTSTRAP", pathDefine, romFolderNoSlash+"/"+fileName);
			bootstrapini.SaveIniFile(bootstrapIniPath);

			iprintf("Done!\n\n");
			iprintf("<A> continue\n");

			for (int i = 0; i < 25; i++) {
				swiWaitForVBlank();
			}
			do {
				scanKeys();
				held = keysDownRepeat();

				swiWaitForVBlank();
			} while(!(held & KEY_A));
		}
	}
}
