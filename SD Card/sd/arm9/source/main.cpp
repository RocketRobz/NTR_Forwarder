#include <nds.h>
#include <nds/fifocommon.h>
#include <stdio.h>
#include <fat.h>
#include "fat_ext.h"
#include <sys/stat.h>
#include <limits.h>
#include <nds/disc_io.h>

#include <string.h>
#include <unistd.h>

#include "fatHeader.h"
#include "ndsheaderbanner.h"
#include "nds_loader_arm9.h"
// #include "nitrofs.h"
#include "tonccpy.h"

#include "cheat.h"
#include "inifile.h"
#include "fileCopy.h"
#include "perGameSettings.h"

#include "donorMap.h"
#include "saveMap.h"
#include "ROMList.h"

using namespace std;

static int language = -1;
static int region = -2;
static bool cacheFatTable = false;

static bool bootstrapFile = false;
static bool widescreenLoaded = false;

static bool consoleInited = false;

int consoleModel = 0; // 0: Retail DSi

//---------------------------------------------------------------------------------
void stop (void) {
//---------------------------------------------------------------------------------
	while (1) {
		swiWaitForVBlank();
	}
}

char filePath[PATH_MAX];

//---------------------------------------------------------------------------------
void doPause() {
//---------------------------------------------------------------------------------
	iprintf("Press start...\n");
	while(1) {
		scanKeys();
		if(keysDown() & KEY_START)
			break;
		swiWaitForVBlank();
	}
	scanKeys();
}

bool extention(const std::string& filename, const char* ext) {
	if(strcasecmp(filename.c_str() + filename.size() - strlen(ext), ext)) {
		return false;
	} else {
		return true;
	}
}

/**
 * Remove trailing slashes from a pathname, if present.
 * @param path Pathname to modify.
 */
void RemoveTrailingSlashes(std::string& path)
{
	while (!path.empty() && path[path.size()-1] == '/') {
		path.resize(path.size()-1);
	}
}

/**
 * Set donor SDK version for a specific game.
 */
int SetDonorSDK(const char* filename) {
	FILE *f_nds_file = fopen(filename, "rb");

	char game_TID[5];
	fseek(f_nds_file, 0xC, SEEK_SET);
	fread(game_TID, 1, 4, f_nds_file);
	game_TID[4] = 0;
	game_TID[3] = 0;
	fclose(f_nds_file);
	
	for (auto i : donorMap) {
		if (i.first == 5 && game_TID[0] == 'V')
			return 5;

		if (i.second.find(game_TID) != i.second.cend())
			return i.first;
	}

	return 0;
}

/**
 * Fix AP for some games.
 */
std::string setApFix(const char *filename, const bool isRunFromSd) {
	const bool useTwlmPath = (access("/_nds/TWiLightMenu/extras/apfix.pck", F_OK) == 0);

	char game_TID[5];
	u16 headerCRC16 = 0;

	bool ipsFound = false;
	bool cheatVer = true;
	char ipsPath[256];
	snprintf(ipsPath, sizeof(ipsPath), "/_nds/%s/apfix/%s.ips", (useTwlmPath ? "TWiLightMenu/extras" : "ntr-forwarder"), filename);
	ipsFound = (access(ipsPath, F_OK) == 0);
	if (!ipsFound) {
		snprintf(ipsPath, sizeof(ipsPath), "/_nds/%s/apfix/%s.bin", (useTwlmPath ? "TWiLightMenu/extras" : "ntr-forwarder"), filename);
		ipsFound = (access(ipsPath, F_OK) == 0);
	} else {
		cheatVer = false;
	}

	if (!ipsFound) {
		FILE *f_nds_file = fopen(filename, "rb");

		fseek(f_nds_file, offsetof(sNDSHeaderExt, gameCode), SEEK_SET);
		fread(game_TID, 1, 4, f_nds_file);
		fseek(f_nds_file, offsetof(sNDSHeaderExt, headerCRC16), SEEK_SET);
		fread(&headerCRC16, sizeof(u16), 1, f_nds_file);
		fclose(f_nds_file);
		game_TID[4] = 0;

		snprintf(ipsPath, sizeof(ipsPath), "/_nds/%s/apfix/%s-%X.ips", (useTwlmPath ? "TWiLightMenu/extras" : "ntr-forwarder"), game_TID, headerCRC16);
		ipsFound = (access(ipsPath, F_OK) == 0);
		if (!ipsFound) {
			snprintf(ipsPath, sizeof(ipsPath), "/_nds/%s/apfix/%s-%X.bin", (useTwlmPath ? "TWiLightMenu/extras" : "ntr-forwarder"), game_TID, headerCRC16);
			ipsFound = (access(ipsPath, F_OK) == 0);
		} else {
			cheatVer = false;
		}
	}

	if (ipsFound) {
		return ipsPath;
	}

	FILE *file = fopen(useTwlmPath ? "/_nds/TWiLightMenu/extras/apfix.pck" : "/_nds/ntr-forwarder/apfix.pck", "rb");
	if (file) {
		char buf[5] = {0};
		fread(buf, 1, 4, file);
		if (strcmp(buf, ".PCK") != 0) // Invalid file
			return "";

		u32 fileCount;
		fread(&fileCount, 1, sizeof(fileCount), file);

		u32 offset = 0, size = 0;

		// Try binary search for the game
		int left = 0;
		int right = fileCount;

		while (left <= right) {
			int mid = left + ((right - left) / 2);
			fseek(file, 16 + mid * 16, SEEK_SET);
			fread(buf, 1, 4, file);
			int cmp = strcmp(buf,  game_TID);
			if (cmp == 0) { // TID matches, check CRC
				u16 crc;
				fread(&crc, 1, sizeof(crc), file);

				if (crc == headerCRC16) { // CRC matches
					fread(&offset, 1, sizeof(offset), file);
					fread(&size, 1, sizeof(size), file);
					cheatVer = fgetc(file) & 1;
					break;
				} else if (crc < headerCRC16) {
					left = mid + 1;
				} else {
					right = mid - 1;
				}
			} else if (cmp < 0) {
				left = mid + 1;
			} else {
				right = mid - 1;
			}
		}

		if (offset > 0 && size > 0) {
			fseek(file, offset, SEEK_SET);
			u8 *buffer = new u8[size];
			fread(buffer, 1, size, file);

			mkdir("/_nds/nds-bootstrap", 0777);
			snprintf(ipsPath, sizeof(ipsPath), "/_nds/nds-bootstrap/apFix%s", cheatVer ? "Cheat.bin" : ".ips");
			FILE *out = fopen(ipsPath, "wb");
			if(out) {
				fwrite(buffer, 1, size, out);
				fclose(out);
			}
			delete[] buffer;
			fclose(file);
			return ipsPath;
		}

		fclose(file);
	}

	return "";
}

void setAutoload(const char *resetTid) {
	u8 *autoloadParams = (u8 *)0x02000300;

	toncset32(autoloadParams, 0x434E4C54, 1); // 'TLNC'
	((vu8 *)autoloadParams)[4] = 0x01;
	((vu8 *)autoloadParams)[5] = 0x18; // Length of data

	// Old TID, can be 0
	toncset(autoloadParams + 8, 0, 8);

	// New TID
	tonccpy(autoloadParams + 16, resetTid, 4);
	toncset32(autoloadParams + 20, 0x00030000 | resetTid[4], 1);

	toncset32(autoloadParams + 24, 0x00000017, 1); // Flags
	toncset32(autoloadParams + 28, 0x00000000, 1);

	// CRC16
	u16 crc16 = swiCRC16(0xFFFF, autoloadParams + 8, 0x18);
	tonccpy(autoloadParams + 6, &crc16, 2);
}

/**
 * Enable widescreen for some games.
 */
void SetWidescreen(const char *filename, bool isHomebrew, const char *resetTid, bool force) {
	const bool useTwlmPath = (access("sd:/_nds/TWiLightMenu/extras/widescreen.pck", F_OK) == 0);

	const char* wideCheatDataPath = "sd:/_nds/nds-bootstrap/wideCheatData.bin";
	remove(wideCheatDataPath);

	if (isHomebrew) {
		if (access("sd:/luma/sysmodules/TwlBg.cxi", F_OK) == 0) {
			rename("sd:/luma/sysmodules/TwlBg.cxi", "sd:/_nds/ntr-forwarder/TwlBg.cxi.bak");
		}
		if (rename("sd:/_nds/TWiLightMenu/TwlBg/Widescreen.cxi", "sd:/luma/sysmodules/TwlBg.cxi") == 0
		 || rename("sd:/_nds/ntr-forwarder/Widescreen.cxi", "sd:/luma/sysmodules/TwlBg.cxi") == 0) {
			CIniFile ntrforwarderini("sd:/_nds/ntr_forwarder.ini");
			ntrforwarderini.SetInt("NTR-FORWARDER", "WIDESCREEN_LOADED", true);
			ntrforwarderini.SaveIniFile("sd:/_nds/ntr_forwarder.ini");

			setAutoload(resetTid);
			DC_FlushAll();
			fifoSendValue32(FIFO_USER_01, 1);
			stop();
		}
		return;
	}

	char tid[5];
	u16 crc16;

	FILE *f_nds_file = fopen(filename, "rb");
	fseek(f_nds_file, offsetof(sNDSHeaderExt, gameCode), SEEK_SET);
	fread(tid, 1, 4, f_nds_file);
	fseek(f_nds_file, offsetof(sNDSHeaderExt, headerCRC16), SEEK_SET);
	fread(&crc16, sizeof(u16), 1, f_nds_file);
	fclose(f_nds_file);
	tid[4] = 0;

	bool wideCheatFound = false;
	char wideBinPath[256];
	snprintf(wideBinPath, sizeof(wideBinPath), "sd:/_nds/%s/widescreen/%s.bin", (useTwlmPath ? "TWiLightMenu/extras" : "ntr-forwarder"), filename);
	wideCheatFound = (access(wideBinPath, F_OK) == 0);

	if (!wideCheatFound) {
		snprintf(wideBinPath, sizeof(wideBinPath), "sd:/_nds/%s/widescreen/%s-%X.bin", (useTwlmPath ? "TWiLightMenu/extras" : "ntr-forwarder"), tid, crc16);
		wideCheatFound = (access(wideBinPath, F_OK) == 0);
	}

	mkdir("sd:/_nds", 0777);
	mkdir("sd:/_nds/nds-bootstrap", 0777);

	if (wideCheatFound) {
		if (fcopy(wideBinPath, wideCheatDataPath) != 0) {
			remove(wideCheatDataPath);
			consoleDemoInit();
			iprintf("Failed to copy widescreen\ncode for the game.");
			for (int i = 0; i < 60 * 3; i++) {
				swiWaitForVBlank(); // Wait 3 seconds
			}
			return;
		}
	} else {
		FILE *file = fopen(useTwlmPath ? "sd:/_nds/TWiLightMenu/extras/widescreen.pck" : "sd:/_nds/ntr-forwarder/widescreen.pck", "rb");
		if (file) {
			char buf[5] = {0};
			fread(buf, 1, 4, file);
			if (strcmp(buf, ".PCK") != 0) // Invalid file
				return;

			u32 fileCount;
			fread(&fileCount, 1, sizeof(fileCount), file);

			u32 offset = 0, size = 0;

			// Try binary search for the game
			int left = 0;
			int right = fileCount;

			while (left <= right) {
				int mid = left + ((right - left) / 2);
				fseek(file, 16 + mid * 16, SEEK_SET);
				fread(buf, 1, 4, file);
				int cmp = strcmp(buf, tid);
				if (cmp == 0) { // TID matches, check CRC
					u16 crc;
					fread(&crc, 1, sizeof(crc), file);

					if (crc == crc16) { // CRC matches
						fread(&offset, 1, sizeof(offset), file);
						fread(&size, 1, sizeof(size), file);
						wideCheatFound = true;
						break;
					} else if (crc < crc16) {
						left = mid + 1;
					} else {
						right = mid - 1;
					}
				} else if (cmp < 0) {
					left = mid + 1;
				} else {
					right = mid - 1;
				}
			}

			if (offset > 0) {
				fseek(file, offset, SEEK_SET);
				u8 *buffer = new u8[size];
				fread(buffer, 1, size, file);

				FILE *out = fopen("sd:/_nds/nds-bootstrap/wideCheatData.bin", "wb");
				if(out) {
					fwrite(buffer, 1, size, out);
					fclose(out);
				}
				delete[] buffer;
			}

			fclose(file);
		}
	}
	if ((wideCheatFound || force) && (access("sd:/_nds/TWiLightMenu/TwlBg/Widescreen.cxi", F_OK) == 0 || access("sd:/_nds/ntr-forwarder/Widescreen.cxi", F_OK) == 0)) {
		if (access("sd:/luma/sysmodules/TwlBg.cxi", F_OK) == 0) {
			rename("sd:/luma/sysmodules/TwlBg.cxi", (useTwlmPath ? "sd:/_nds/TWiLightMenu/TwlBg/TwlBg.cxi.bak" : "sd:/_nds/ntr-forwarder/TwlBg.cxi.bak"));
		}
		if (rename("sd:/_nds/TWiLightMenu/TwlBg/Widescreen.cxi", "sd:/luma/sysmodules/TwlBg.cxi") == 0
		 || rename("sd:/_nds/ntr-forwarder/Widescreen.cxi", "sd:/luma/sysmodules/TwlBg.cxi") == 0) {
			CIniFile ntrforwarderini("sd:/_nds/ntr_forwarder.ini");
			ntrforwarderini.SetInt("NTR-FORWARDER", "WIDESCREEN_LOADED", true);
			ntrforwarderini.SaveIniFile("sd:/_nds/ntr_forwarder.ini");

			setAutoload(resetTid);
			DC_FlushAll();
			fifoSendValue32(FIFO_USER_01, 1);
			stop();
		}
	}
}

void UnsetWidescreen() {
	bool useTwlmPath = (access("sd:/_nds/TWiLightMenu/extras/widescreen.pck", F_OK) == 0);

	CIniFile ntrforwarderini("sd:/_nds/ntr_forwarder.ini");
	ntrforwarderini.SetInt("NTR-FORWARDER", "WIDESCREEN_LOADED", false);
	ntrforwarderini.SaveIniFile("sd:/_nds/ntr_forwarder.ini");

	// Revert back to 4:3 for next boot
	if(useTwlmPath)
		mkdir("sd:/_nds/TWiLightMenu/TwlBg", 0777);
	if (rename("sd:/luma/sysmodules/TwlBg.cxi", (useTwlmPath ? "sd:/_nds/TWiLightMenu/TwlBg/Widescreen.cxi" : "sd:/_nds/ntr-forwarder/Widescreen.cxi")) != 0) {
		consoleDemoInit();
		iprintf("Failed to rename TwlBg.cxi\n");
		iprintf("back to Widescreen.cxi\n");
		for (int i = 0; i < 60*3; i++) swiWaitForVBlank();
	}
	if (access(useTwlmPath ? "sd:/_nds/TWiLightMenu/TwlBg/TwlBg.cxi.bak" : "sd:/_nds/ntr-forwarder/TwlBg.cxi.bak", F_OK) == 0) {
		rename(useTwlmPath ? "sd:/_nds/TWiLightMenu/TwlBg/TwlBg.cxi.bak" : "sd:/_nds/ntr-forwarder/TwlBg.cxi.bak", "sd:/luma/sysmodules/TwlBg.cxi");
	}
}

std::string ReplaceAll(std::string str, const std::string& from, const std::string& to) {
    size_t start_pos = 0;
    while((start_pos = str.find(from, start_pos)) != std::string::npos) {
        str.replace(start_pos, from.length(), to);
        start_pos += to.length(); // Handles case where 'to' is a substring of 'from'
    }
    return str;
}

// From NTM
// https://github.com/Epicpkmn11/NTM/blob/db69aca1b49733da51f64ee857ac9b861b1c468c/arm9/src/sav.c#L7-L93
bool createDSiWareSave(const char *path, int size) {
	const u16 sectorSize = 0x200;

	if (!path || size < sectorSize)
		return false;

	//fit maximum sectors for the size
	const u16 maxSectors = size / sectorSize;
	u16 sectorCount = 1;
	u16 secPerTrk = 1;
	u16 numHeads = 1;
	u16 sectorCountNext = 0;
	while (sectorCountNext <= maxSectors) {
		sectorCountNext = secPerTrk * (numHeads + 1) * (numHeads + 1);
		if (sectorCountNext <= maxSectors) {
			numHeads++;
			sectorCount = sectorCountNext;

			secPerTrk++;
			sectorCountNext = secPerTrk * numHeads * numHeads;
			if (sectorCountNext <= maxSectors) {
				sectorCount = sectorCountNext;
			}
		}
	}
	sectorCountNext = (secPerTrk + 1) * numHeads * numHeads;
	if (sectorCountNext <= maxSectors) {
		secPerTrk++;
		sectorCount = sectorCountNext;
	}

	u8 secPerCluster = (sectorCount > (8 << 10)) ? 8 : (sectorCount > (1 << 10) ? 4 : 1);

	u16 rootEntryCount = size < 0x8C000 ? 0x20 : 0x200;

	#define ALIGN_TO_MULTIPLE(v, a) (((v) % (a)) ? ((v) + (a) - ((v) % (a))) : (v))
	u16 totalClusters = ALIGN_TO_MULTIPLE(sectorCount, secPerCluster) / secPerCluster;
	u32 fatBytes = (ALIGN_TO_MULTIPLE(totalClusters, 2) / 2) * 3; // 2 sectors -> 3 byte
	u16 fatSize = ALIGN_TO_MULTIPLE(fatBytes, sectorSize) / sectorSize;


	FATHeader h;
	toncset(&h, 0, sizeof(FATHeader));

	h.BS_JmpBoot[0] = 0xE9;
	h.BS_JmpBoot[1] = 0;
	h.BS_JmpBoot[2] = 0;

	tonccpy(h.BS_OEMName, "MSWIN4.1", 8);

	h.BPB_BytesPerSec = sectorSize;
	h.BPB_SecPerClus = secPerCluster;
	h.BPB_RsvdSecCnt = 0x0001;
	h.BPB_NumFATs = 0x02;
	h.BPB_RootEntCnt = rootEntryCount;
	h.BPB_TotSec16 = sectorCount;
	h.BPB_Media = 0xF8; // "hard drive"
	h.BPB_FATSz16 = fatSize;
	h.BPB_SecPerTrk = secPerTrk;
	h.BPB_NumHeads = numHeads;
	h.BS_DrvNum = 0x05;
	h.BS_BootSig = 0x29;
	h.BS_VolID = 0x12345678;
	tonccpy(h.BS_VolLab, "VOLUMELABEL", 11);
	tonccpy(h.BS_FilSysType,"FAT12   ", 8);
	h.BS_BootSign = 0xAA55;

	FILE *file = fopen(path, "wb");
	if (file) {
		fwrite(&h, sizeof(FATHeader), 1, file); // Write header
		int i = 0;
		while (1) {
			i += 0x8000;
			if (i > size) i = size;
			fseek(file, i - 1, SEEK_SET); // Pad rest of the file
			fputc('\0', file);
			if (i == size) break;
		}
		fclose(file);
		return true;
	}

	return false;
}


std::string ndsPath;
std::string romfolder;
std::string filename;
std::string savename;
std::string romFolderNoSlash;
std::string savepath;

//---------------------------------------------------------------------------------
int main(int argc, char **argv) {
//---------------------------------------------------------------------------------

	defaultExceptionHandler();

	keysSetRepeat(25, 5);

	if (isDSiMode()) {
		// Cut slot1 power to save battery
		disableSlot1();

		*(vu32*)0x0DFFFE0C = 0x4652544E; // Check for Debug RAM
		if (*(vu32*)0x0DFFFE0C == 0x4652544E) {
			consoleModel = fifoGetValue32(FIFO_USER_05) == 0xD2 ? 1 : 2; // 1: Panda DSi, 2: 3DS/2DS
		}
	}

	if (fatInitDefault()) {
		if (argc < 2 || access(argv[1], F_OK) != 0) {
			consoleDemoInit();
			if (argc < 2) {
				iprintf("sdcard.nds should not be loaded\ndirectly.\n");
				iprintf("Load a forwarder instead.\n");
			} else {
				iprintf("Not found:\n%s\n\n", argv[1]);
				iprintf("Please recreate the forwarder\n");
				iprintf("with the correct ROM path.\n");
			}
		} else {
		// nitroFSInit(argv[0]);
		const bool isRunFromSd = (strncmp(argv[0], "sd", 2) == 0);

		CIniFile ntrforwarderini( isRunFromSd ? "sd:/_nds/ntr_forwarder.ini" : "fat:/_nds/ntr_forwarder.ini" );

		bootstrapFile = ntrforwarderini.GetInt("NTR-FORWARDER", "BOOTSTRAP_FILE", 0);
		widescreenLoaded = ntrforwarderini.GetInt("NTR-FORWARDER", "WIDESCREEN_LOADED", false);

		ndsPath = (std::string)argv[1];
		/*consoleDemoInit();
		printf(argv[1]);
		printf("\n");
		printf("Press START\n");
		while (1) {
			scanKeys();
			if (keysDown() & KEY_START) break;
			swiWaitForVBlank();
		}*/

		romfolder = ndsPath;
		while (!romfolder.empty() && romfolder[romfolder.size()-1] != '/') {
			romfolder.resize(romfolder.size()-1);
		}
		chdir(romfolder.c_str());

		filename = ndsPath;
		const size_t last_slash_idx = filename.find_last_of("/");
		if (std::string::npos != last_slash_idx)
		{
			filename.erase(0, last_slash_idx + 1);
		}

		FILE *f_nds_file = fopen(filename.c_str(), "rb");
		bool dsiBinariesFound = (!isDSiMode()) || checkDsiBinaries(f_nds_file);
		int isHomebrew = checkIfHomebrew(f_nds_file, isRunFromSd);
		if (isHomebrew == 0) {
			mkdir ("saves", 0777);
		}

		extern sNDSHeaderExt ndsHeader;

		const bool isDSiWare = (ndsHeader.unitCode != 0 && (ndsHeader.accessControl & BIT(4))); // Check if it's a DSiWare game

		swiWaitForVBlank();
		GameSettings gameSettings(filename, ndsPath);
		scanKeys();
		if(keysHeld() & KEY_Y) {
			gameSettings.menu(f_nds_file, filename, (bool)isHomebrew);
		}

		fclose(f_nds_file);

		if (isHomebrew == 2) {
			const char *argarray[] = {argv[1]};
			int err = runNdsFile(argarray[0], sizeof(argarray) / sizeof(argarray[0]), argarray);
			if (!consoleInited) {
				consoleDemoInit();
				consoleInited = true;
			}
			consoleDemoInit();
			iprintf("Start failed. Error %i\n", err);
			if (err == 1) iprintf ("ROM not found.\n");
		} else {
			mkdir("/_nds/nds-bootstrap", 0777);

			if (isRunFromSd) {
				if (argc >= 3) {
					u32 autoloadParams[2] = {0};
					memcpy(autoloadParams, argv[2], 4);
					autoloadParams[1] = 0x00030000 | argv[2][4];

					FILE *srBackendFile = fopen("sd:/_nds/nds-bootstrap/srBackendId.bin", "wb");
					fwrite(autoloadParams, sizeof(u32), 2, srBackendFile);
					fclose(srBackendFile);
				} else {
					FILE *headerFile = fopen("sd:/_nds/ntr-forwarder/header.bin", "rb");
					if (headerFile) {
						FILE *srBackendFile = fopen("sd:/_nds/nds-bootstrap/srBackendId.bin", "wb");
						fread(__DSiHeader, 1, 0x1000, headerFile);
						fwrite((char*)__DSiHeader+0x230, sizeof(u32), 2, srBackendFile);
						fclose(srBackendFile);
						fclose(headerFile);
					} else if (access("sd:/_nds/nds-bootstrap/srBackendId.bin", F_OK) == 0) {
						remove("sd:/_nds/nds-bootstrap/srBackendId.bin");
					}
				}
			}

			// Delete cheat data
			remove("/_nds/nds-bootstrap/cheatData.bin");
			if(!widescreenLoaded)
				remove("/_nds/nds-bootstrap/wideCheatData.bin");

			const char *typeToReplace = ".nds";
			if (extention(filename, ".dsi")) {
				typeToReplace = ".dsi";
			} else if (extention(filename, ".ids")) {
				typeToReplace = ".ids";
			} else if (extention(filename, ".srl")) {
				typeToReplace = ".srl";
			} else if (extention(filename, ".app")) {
				typeToReplace = ".app";
			}

			char savExtension[16] = ".sav";
			if(gameSettings.saveNo > 0)
				snprintf(savExtension, sizeof(savExtension), ".sav%d", gameSettings.saveNo);
			savename = ReplaceAll(filename, typeToReplace, savExtension);
			romFolderNoSlash = romfolder;
			RemoveTrailingSlashes(romFolderNoSlash);
			savepath = romFolderNoSlash+"/saves/"+savename;

			std::string dsiWareSrlPath = ndsPath;
			std::string dsiWarePubPath = ReplaceAll(savepath, ".sav", ".pub");
			std::string dsiWarePrvPath = ReplaceAll(savepath, ".sav", ".prv");

			if (isDSiWare) {
				if (isRunFromSd) {
					if ((getFileSize(dsiWarePubPath.c_str()) == 0) && (ndsHeader.pubSavSize > 0)) {
						consoleDemoInit();
						iprintf("Creating public save file...\n\n");
						createDSiWareSave(dsiWarePubPath.c_str(), ndsHeader.pubSavSize);
						iprintf("Public save file created!\n");

						for (int i = 0; i < 30; i++) {
							swiWaitForVBlank();
						}
					}

					if ((getFileSize(dsiWarePrvPath.c_str()) == 0) && (ndsHeader.prvSavSize > 0)) {
						consoleDemoInit();
						iprintf("Creating private save file...\n\n");
						createDSiWareSave(dsiWarePrvPath.c_str(), ndsHeader.prvSavSize);
						iprintf("Private save file created!\n");

						for (int i = 0; i < 30; i++) {
							swiWaitForVBlank();
						}
					}
				} else {
					const u32 savesize = ((ndsHeader.pubSavSize > 0) ? ndsHeader.pubSavSize : ndsHeader.prvSavSize);
					if ((getFileSize(savepath.c_str()) == 0) && (savesize > 0)) {
						consoleDemoInit();
						iprintf("Creating save file...\n\n");

						FILE *pFile = fopen(savepath.c_str(), "wb");
						if (pFile) {
							fseek(pFile, savesize - 1, SEEK_SET);
							fputc('\0', pFile);
							fclose(pFile);
						}
						iprintf("Done!\n");

						for (int i = 0; i < 30; i++) {
							swiWaitForVBlank();
						}
					}
				}
			} else if (isHomebrew == 0) {
				u32 orgsavesize = getFileSize(savepath.c_str());
				u32 savesize = 524288; // 512KB (default size for most games)

				u32 gameTidHex = 0;
				tonccpy(&gameTidHex, &ndsHeader.gameCode, 4);

				for (int i = 0; i < (int)sizeof(ROMList)/12; i++) {
					ROMListEntry* curentry = &ROMList[i];
					if (gameTidHex == curentry->GameCode) {
						if (curentry->SaveMemType != 0xFFFFFFFF) savesize = sramlen[curentry->SaveMemType];
						break;
					}
				}

				if ((orgsavesize == 0 && savesize > 0) || (orgsavesize < savesize)) {
					consoleDemoInit();
					iprintf((orgsavesize == 0) ? "Creat" : "Expand");
					iprintf("ing save file...\n\n");

					FILE *pFile = fopen(savepath.c_str(), orgsavesize > 0 ? "r+" : "wb");
					if (pFile) {
						fseek(pFile, savesize - 1, SEEK_SET);
						fputc('\0', pFile);
						fclose(pFile);
					}
					iprintf("Done!\n");

					for (int i = 0; i < 30; i++) {
						swiWaitForVBlank();
					}
				}
			}

			// Set widescreen
			if(isRunFromSd && consoleModel == 2 && gameSettings.widescreen >= 1) {
				if(widescreenLoaded) {
					UnsetWidescreen();
				} else if(argc >= 3) {
					SetWidescreen(ndsPath.c_str(), isHomebrew, argv[2], gameSettings.widescreen == 2);
				} else {
					consoleDemoInit();
					iprintf("If using a 3DS-mode forwarder,\n");
					iprintf("reinstall bootstrap.cia to use\n");
					iprintf("widescreen.\n\n");
					iprintf("If using a DSi-mode forwarder\n");
					iprintf("then remake the forwarder.\n");
					goto error;
				}
			}

			// load cheats
			CheatCodelist codelist;
			u32 gameCode, crc32;

			bool cheatsEnabled = true;
			const char* cheatDataBin = "/_nds/nds-bootstrap/cheatData.bin";
			mkdir("/_nds/nds-bootstrap", 0777);
			if(codelist.romData(ndsPath, gameCode, crc32)) {
				long cheatOffset; size_t cheatSize;
				// First try ntr-forwarder folder
				FILE* dat=fopen("/_nds/ntr-forwarder/usrcheat.dat","rb");
				// If that fails, try TWiLight's file
				if(!dat)
					dat=fopen("/_nds/TWiLightMenu/extras/usrcheat.dat","rb");

				if (dat) {
					if (codelist.searchCheatData(dat, gameCode, crc32, cheatOffset, cheatSize)) {
						codelist.parse(ndsPath);
						codelist.writeCheatsToFile(cheatDataBin);
						FILE* cheatData=fopen(cheatDataBin,"rb");
						if (cheatData) {
							u32 check[2];
							fread(check, 1, 8, cheatData);
							fclose(cheatData);
							if (check[1] == 0xCF000000 || getFileSize(cheatDataBin) > 0x1C00) {
								cheatsEnabled = false;
							}
						}
					} else {
						cheatsEnabled = false;
					}
					fclose(dat);
				} else {
					cheatsEnabled = false;
				}
			} else {
				cheatsEnabled = false;
			}
			if (!cheatsEnabled) {
				remove(cheatDataBin);
			}

			const char* esrbSplashPath = "/_nds/nds-bootstrap/esrb.bin";
			if (access(esrbSplashPath, F_OK) == 0) {
				// Remove ESRB splash screen generated by TWiLight Menu++
				remove(esrbSplashPath);
			}

			const char* bootstrapIniPath = "/_nds/nds-bootstrap.ini";
			CIniFile bootstrapini( bootstrapIniPath );

			int donorSdkVer = 0;
			bool dsModeForced = false;

			if (isHomebrew == 0 && ndsHeader.unitCode == 2 && !dsiBinariesFound && gameSettings.dsiMode != 0) {
				consoleDemoInit();
				iprintf ("The DSi binaries are missing.\n");
				iprintf ("Please obtain a clean ROM\n");
				iprintf ("to replace the current one.\n");
				iprintf ("\n");
				iprintf ("Press A to proceed to run in\n");
				iprintf ("DS mode.\n");
				while (1) {
					scanKeys();
					if (keysDown() & KEY_A) {
						dsModeForced = true;
						break;
					}
					swiWaitForVBlank();
				}
			}

			if (isHomebrew == 0) {
				donorSdkVer = SetDonorSDK(ndsPath.c_str());
			}

			char sfnSrl[62];
			char sfnPub[62];
			char sfnPrv[62];
			if (isRunFromSd && isDSiWare) {
				fatGetAliasPath("sd:/", dsiWareSrlPath.c_str(), sfnSrl);
				fatGetAliasPath("sd:/", dsiWarePubPath.c_str(), sfnPub);
				fatGetAliasPath("sd:/", dsiWarePrvPath.c_str(), sfnPrv);
			}

			// Fix weird bug where some settings would get cleared
			cacheFatTable = bootstrapini.GetInt("NDS-BOOTSTRAP", "CACHE_FAT_TABLE", cacheFatTable);

			int requiresDonorRom = 0;
			bool dsDebugRam = false;
			if (!isDSiMode()) {
				const u32 wordBak = *(vu32*)0x02800000;
				const u32 wordBak2 = *(vu32*)0x02C00000;
				*(vu32*)(0x02800000) = 0x314D454D;
				*(vu32*)(0x02C00000) = 0x324D454D;
				dsDebugRam = ((*(vu32*)(0x02800000) == 0x314D454D) && (*(vu32*)(0x02C00000) == 0x324D454D));
				*(vu32*)(0x02800000) = wordBak;
				*(vu32*)(0x02C00000) = wordBak2;
			}

			if (ndsHeader.a7mbk6 == 0x080037C0 && !isDSiMode()
			&& ((dsDebugRam ? (memcmp(ndsHeader.gameCode, "DME", 3) == 0 || memcmp(ndsHeader.gameCode, "DMD", 3) == 0 || memcmp(ndsHeader.gameCode, "DMP", 3) == 0 || memcmp(ndsHeader.gameCode, "DHS", 3) == 0) : (memcmp(ndsHeader.gameCode, "DMP", 3) == 0 || memcmp(ndsHeader.gameCode, "DHS", 3) == 0))
			|| (ndsHeader.gameCode[0] != 'D' && memcmp(ndsHeader.gameCode, "KCX", 3) != 0 && memcmp(ndsHeader.gameCode, "KAV", 3) != 0 && memcmp(ndsHeader.gameCode, "KNK", 3) != 0 && memcmp(ndsHeader.gameCode, "KE3", 3) != 0))) {
				requiresDonorRom = 51; // SDK5 ROM required
			} else if (memcmp(ndsHeader.gameCode, "AYI", 3) == 0 && ndsHeader.arm7binarySize == 0x25F70) {
				requiresDonorRom = 20; // SDK2.0 ROM required for Yoshi Touch & Go (Europe)
			}
			if (requiresDonorRom > 0) {
				const char* pathDefine = "DONORTWL_NDS_PATH"; // SDK5.x (TWL)
				if (requiresDonorRom == 20) {
					pathDefine = "DONOR20_NDS_PATH"; // SDK2.0
				}
				std::string donorRomPath;
				donorRomPath = bootstrapini.GetString("NDS-BOOTSTRAP", pathDefine, "");
				bool donorRomFound = ((!isDSiMode() && requiresDonorRom != 20 && access("fat:/_nds/nds-bootstrap/b4dsTwlDonor.bin", F_OK) == 0)
									|| (donorRomPath != "" && access(donorRomPath.c_str(), F_OK) == 0));
				if (!donorRomFound && requiresDonorRom != 20) {
					pathDefine = "DONORTWL0_NDS_PATH"; // SDK5.0 (TWL)
					donorRomPath = bootstrapini.GetString("NDS-BOOTSTRAP", pathDefine, "");
					donorRomFound = (donorRomPath != "" && access(donorRomPath.c_str(), F_OK) == 0);
				}
				if (!donorRomFound && !isDSiMode() && requiresDonorRom != 20 && memcmp(ndsHeader.gameCode, "KUB", 3) != 0) {
					pathDefine = "DONOR5_NDS_PATH"; // SDK5.x (NTR)
					donorRomPath = bootstrapini.GetString("NDS-BOOTSTRAP", pathDefine, "");
					donorRomFound = (donorRomPath != "" && access(donorRomPath.c_str(), F_OK) == 0);
					if (!donorRomFound) {
						pathDefine = "DONOR5_NDS_PATH_ALT"; // SDK5.x (NTR)
						donorRomPath = bootstrapini.GetString("NDS-BOOTSTRAP", pathDefine, "");
						donorRomFound = (donorRomPath != "" && access(donorRomPath.c_str(), F_OK) == 0);
					}
				}
				if (!donorRomFound) {
					consoleDemoInit();
					iprintf("A donor ROM is required to\n");
					iprintf("launch this title!\n");
					iprintf("\n");
					iprintf("Please create a forwarder for\n");
					iprintf("an SDK");
					if (requiresDonorRom == 51) {
						iprintf("5");
					} else {
						iprintf("2.0");
					}
					iprintf(" title, launch it,\n");
					iprintf("hold the Y button to open the\n");
					iprintf("per-game settings, then select\n");
					iprintf("\"Set as Donor ROM\"\n");
					goto error;
				}
			}

			// Write
			bootstrapini.SetString("NDS-BOOTSTRAP", "NDS_PATH", ndsPath);
			if (isRunFromSd && isDSiWare) {
				bootstrapini.SetString("NDS-BOOTSTRAP", "APP_PATH", sfnSrl);
				bootstrapini.SetString("NDS-BOOTSTRAP", "SAV_PATH", sfnPub);
				bootstrapini.SetString("NDS-BOOTSTRAP", "PRV_PATH", sfnPrv);
			} else {
				bootstrapini.SetString("NDS-BOOTSTRAP", "SAV_PATH", savepath);
			}
			if (isHomebrew == 0) {
				bootstrapini.SetString("NDS-BOOTSTRAP", "AP_FIX_PATH", isDSiWare ? "" : setApFix(filename.c_str(), isRunFromSd));
			}
			bootstrapini.SetString("NDS-BOOTSTRAP", "HOMEBREW_ARG", "");
			bootstrapini.SetInt("NDS-BOOTSTRAP", "BOOST_CPU", gameSettings.boostCpu);
			bootstrapini.SetInt("NDS-BOOTSTRAP", "BOOST_VRAM", gameSettings.boostVram == -1 ? false : gameSettings.boostVram);
			bootstrapini.SetInt("NDS-BOOTSTRAP", "CARD_READ_DMA", gameSettings.cardReadDMA);
			bootstrapini.SetInt("NDS-BOOTSTRAP", "ASYNC_CARD_READ", gameSettings.asyncCardRead);
			bootstrapini.SetInt("NDS-BOOTSTRAP", "DSI_MODE", dsModeForced ? 0 : (gameSettings.dsiMode == -1 ? true : gameSettings.dsiMode));
			//bootstrapini.SetInt("NDS-BOOTSTRAP", "CACHE_FAT_TABLE", cacheFatTable);
			bootstrapini.SetInt("NDS-BOOTSTRAP", "DONOR_SDK_VER", donorSdkVer);
			bootstrapini.SetInt("NDS-BOOTSTRAP", "PATCH_MPU_REGION", 0);
			bootstrapini.SetInt("NDS-BOOTSTRAP", "PATCH_MPU_SIZE", 0);
			bootstrapini.SetInt("NDS-BOOTSTRAP", "CONSOLE_MODEL", consoleModel);
			bootstrapini.SetInt("NDS-BOOTSTRAP", "LANGUAGE", gameSettings.language == -2 ? language : gameSettings.language);
			bootstrapini.SetInt("NDS-BOOTSTRAP", "REGION", gameSettings.region == -3 ? region : gameSettings.region);
			bootstrapini.SaveIniFile( bootstrapIniPath );

			if (isHomebrew == 1) {
				const char *argarray[] = {bootstrapFile ? "sd:/_nds/nds-bootstrap-hb-nightly.nds" : "sd:/_nds/nds-bootstrap-hb-release.nds"};
				int err = runNdsFile(argarray[0], sizeof(argarray) / sizeof(argarray[0]), argarray);
				if (!consoleInited) {
					consoleDemoInit();
					consoleInited = true;
				}
				consoleDemoInit();
				iprintf("Start failed. Error %i\n", err);
				if (err == 1) iprintf ("nds-bootstrap (hb) not found.\n");
			} else {
				const char *argarray[] = {isRunFromSd ? (bootstrapFile ? "sd:/_nds/nds-bootstrap-nightly.nds" : "sd:/_nds/nds-bootstrap-release.nds") : (bootstrapFile ? "fat:/_nds/nds-bootstrap-nightly.nds" : "fat:/_nds/nds-bootstrap-release.nds")};
				int err = runNdsFile(argarray[0], sizeof(argarray) / sizeof(argarray[0]), argarray);
				if (!consoleInited) {
					consoleDemoInit();
					consoleInited = true;
				}
				consoleDemoInit();
				iprintf("Start failed. Error %i\n", err);
				if (err == 1) iprintf ("nds-bootstrap not found.\n");
			}
		}
		}
	} else {
		// Subscreen as a console
		videoSetModeSub(MODE_0_2D);
		vramSetBankH(VRAM_H_SUB_BG);
		consoleInit(NULL, 0, BgType_Text4bpp, BgSize_T_256x256, 15, 0, false, true);	

		iprintf ("fatinitDefault failed!\n");
	}

error:
	if (!isDSiMode()) {
		while (1) {
			swiWaitForVBlank();
		}
	}
	iprintf ("\n");		
	iprintf ("Press B to return to\n");
	iprintf (consoleModel >= 2 ? "HOME Menu.\n" : "DSi Menu.\n");

	while (1) {
		scanKeys();
		if (keysDown() & KEY_B) fifoSendValue32(FIFO_USER_01, 1);	// Tell ARM7 to reboot into 3DS HOME Menu (power-off/sleep mode screen skipped)
		swiWaitForVBlank();
	}

	return 0;
}
