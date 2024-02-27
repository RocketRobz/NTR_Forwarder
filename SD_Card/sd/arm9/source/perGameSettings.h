#ifndef PER_GAME_SETTINGS_H
#define PER_GAME_SETTINGS_H

#include <nds/ndstypes.h>
#include <string>

#include "inifile.h"

class GameSettings {
	std::string iniPath, romPath;
	CIniFile ini;

public:
	GameSettings(const std::string &fileName, const std::string &filePath);

	int language = -2;
	int region = -3;
	int saveNo = 0;
	int dsiMode = -1;
	int boostCpu = -1;
	int boostVram = -1;
	int cardReadDMA = -1;
	int asyncCardRead = -1;
	int bootstrapFile = -1;
	int widescreen = -1;

	void save(void);

	bool isDonorRom(const u32 arm7size, const u32 a7mbk6, const u32 SDKVersion);

	void menu(FILE* f_nds_file, const std::string &fileName, const bool isHomebrew);

};

#endif // PER_GAME_SETTINGS_H
