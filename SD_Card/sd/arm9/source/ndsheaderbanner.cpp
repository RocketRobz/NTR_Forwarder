#include <nds.h>
#include <stdio.h>
#include <malloc.h>
#include <unistd.h>

#include "ndsheaderbanner.h"
#include "module_params.h"

static u32 arm9Sig[3][4];
sNDSHeaderExt ndsHeader;
sNDSHeaderExt ndsHeaderBinaryCheck;

bool checkDsiBinaries(FILE* ndsFile) {
	u8 unitCode = 0;

	fseek(ndsFile, 0x12, SEEK_SET);
	fread(&unitCode, 1, 1, ndsFile);

	if (unitCode == 0 || unitCode == 3) {
		return true;
	}

	fseek(ndsFile, 0, SEEK_SET);
	fread(&ndsHeaderBinaryCheck, 1, sizeof(ndsHeaderBinaryCheck), ndsFile);

	if (ndsHeaderBinaryCheck.arm9iromOffset < 0x8000 || ndsHeaderBinaryCheck.arm9iromOffset >= 0x20000000
	 || ndsHeaderBinaryCheck.arm7iromOffset < 0x8000 || ndsHeaderBinaryCheck.arm7iromOffset >= 0x20000000) {
		return false;
	}

	for (int i = 0; i < 3; i++) {
		arm9Sig[i][0] = 0;
		arm9Sig[i][1] = 0;
		arm9Sig[i][2] = 0;
		arm9Sig[i][3] = 0;
	}

	fseek(ndsFile, 0x8000, SEEK_SET);
	fread(arm9Sig[0], sizeof(u32), 4, ndsFile);
	fseek(ndsFile, ndsHeaderBinaryCheck.arm9iromOffset, SEEK_SET);
	fread(arm9Sig[1], sizeof(u32), 4, ndsFile);
	fseek(ndsFile, ndsHeaderBinaryCheck.arm7iromOffset, SEEK_SET);
	fread(arm9Sig[2], sizeof(u32), 4, ndsFile);
	for (int i = 1; i < 3; i++) {
		if (arm9Sig[i][0] == arm9Sig[0][0]
		 && arm9Sig[i][1] == arm9Sig[0][1]
		 && arm9Sig[i][2] == arm9Sig[0][2]
		 && arm9Sig[i][3] == arm9Sig[0][3]) {
			return false;
		}
		if (arm9Sig[i][0] == 0
		 && arm9Sig[i][1] == 0
		 && arm9Sig[i][2] == 0
		 && arm9Sig[i][3] == 0) {
			return false;
		}
		if (arm9Sig[i][0] == 0xFFFFFFFF
		 && arm9Sig[i][1] == 0xFFFFFFFF
		 && arm9Sig[i][2] == 0xFFFFFFFF
		 && arm9Sig[i][3] == 0xFFFFFFFF) {
			return false;
		}
	}

	return true;
}

/**
 * Get SDK version from an NDS file.
 * @param ndsFile NDS file.
 * @param filename NDS ROM filename.
 * @return 0 on success; non-zero on error.
 */
u32 getSDKVersion(FILE* ndsFile) {
	if (ndsHeader.arm7destination >= 0x037F8000)
		return 0;
	return getModuleParams(&ndsHeader, ndsFile)->sdk_version;
}

int checkIfHomebrew(FILE* ndsFile, const bool isRunFromSd) {
	fseek(ndsFile, 0, SEEK_SET);
	fread(&ndsHeader, 1, sizeof(ndsHeader), ndsFile);

	fseek(ndsFile, ndsHeader.arm9romOffset + ((strncmp(ndsHeader.gameCode, "BIG", 3) == 0) ? 0x02000800 : ndsHeader.arm9executeAddress) - ndsHeader.arm9destination, SEEK_SET);
	// "Battle/Combat of Giants: Mutant Insects" (TID: BIG) has code that is run before the actual SDK boot code
	fread(arm9Sig[0], sizeof(u32), 4, ndsFile);
	if ((arm9Sig[0][0] == 0xE3A0C301 || (arm9Sig[0][0] >= 0xEA000000 && arm9Sig[0][0] < 0xEC000000 /* If title contains cracktro or extra splash */))
	  && arm9Sig[0][1] == 0xE58CC208) {
		// Title seems to be developed with Nintendo SDK, verify
		if ((arm9Sig[0][2] >= 0xEB000000 && arm9Sig[0][2] < 0xEC000000) // SDK 2 & TWL SDK 5
		 && (arm9Sig[0][3] >= 0xE3A00000 && arm9Sig[0][3] < 0xE3A01000)) {
			return 0;
		} else
		if (arm9Sig[0][2] == 0xE1DC00B6 // SDK 3-5
		 && arm9Sig[0][3] == 0xE3500000) {
			return 0;
		} else
		if (arm9Sig[0][2] == 0xEAFFFFFF // SDK 4 (HM DS Cute)
		 && arm9Sig[0][3] == 0xE1DC00B6) {
			return 0;
		}
	} else if (strncmp(ndsHeader.gameCode, "HNA", 3) == 0) {
		return 0;
	}

	if (arm9Sig[0][0] == 0xE3A00301
	 && arm9Sig[0][1] == 0xE5800208
	 && arm9Sig[0][2] == 0xE3A00013
	 && arm9Sig[0][3] == 0xE129F000) {
		if (!isRunFromSd) {
			return 2;
		}
		if (ndsHeader.arm7executeAddress >= 0x037F0000 && ndsHeader.arm7destination >= 0x037F0000) {
			int res = 1; // Have nds-bootstrap load it

			u32 searchOffset = 0;
			fseek(ndsFile, ndsHeader.arm7romOffset + 0xC8, SEEK_SET);
			fread(&searchOffset, sizeof(u32), 1, ndsFile);
			if (searchOffset >= 0x037F0000 && searchOffset < 0x03810000) {
				searchOffset -= ndsHeader.arm7destination;
				searchOffset += ndsHeader.arm7romOffset;
				searchOffset += 0x200;
				const u32 searchOffsetEnd = ndsHeader.arm7romOffset + ndsHeader.arm7binarySize;

				// Check if homebrew accesses the SD command register
				fseek(ndsFile, searchOffset, SEEK_SET);
				u32 sdReg = 0;
				for (int i = 0; i < 0x800; i += 4) {
					fread(&sdReg, sizeof(u32), 1, ndsFile);
					if (sdReg == 0x04004800) {
						res = 2; // Homebrew supports reading from SD without a DLDI driver, so do not use nds-bootstrap
						break;
					}
					searchOffset += 4;
					if (searchOffset >= searchOffsetEnd) break;
				}
			} else {
				res = 2; // Homebrew supports reading from SD without a DLDI driver, so do not use nds-bootstrap
			}

			return res;
		}
	} else if ((ndsHeader.unitCode == 0) && ((memcmp(ndsHeader.gameTitle, "NMP4BOOT", 8) == 0)
	 || (ndsHeader.arm7executeAddress >= 0x037F0000 && ndsHeader.arm7destination >= 0x037F0000))) {
		return isRunFromSd ? 1 : 2; // Homebrew is old (requires a DLDI driver to read from SD)
	}

	return 2;
}
