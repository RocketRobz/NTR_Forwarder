/*-----------------------------------------------------------------
 Copyright (C) 2005 - 2013
	Michael "Chishm" Chisholm
	Dave "WinterMute" Murphy
	Claudio "sverx"

 This program is free software; you can redistribute it and/or
 modify it under the terms of the GNU General Public License
 as published by the Free Software Foundation; either version 2
 of the License, or (at your option) any later version.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with this program; if not, write to the Free Software
 Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.

------------------------------------------------------------------*/
#include <nds.h>
#include <nds/fifocommon.h>
#include <stdio.h>
#include <fat.h>
#include <sys/stat.h>
#include <limits.h>
#include <nds/disc_io.h>

#include <string.h>
#include <unistd.h>

#include "nds_loader_arm9.h"

using namespace std;

//---------------------------------------------------------------------------------
void stop (void) {
//---------------------------------------------------------------------------------
	while (1) {
		swiWaitForVBlank();
	}
}

//---------------------------------------------------------------------------------
int main(int argc, char **argv) {
//---------------------------------------------------------------------------------
	if (fatInitDefault()) {
		const char *ndsPath = "fat:/<<<Start NDS Path                                                                                                                                                                                                                           End NDS Path>>>";

		const char *argarray[2] = {
			"fat:/_nds/ntr-forwarder/sdcard.nds",
			ndsPath
		};
		int err = runNdsFile(argarray[0], sizeof(argarray) / sizeof(argarray[0]), argarray);

		consoleDemoInit();
		iprintf("Start failed. Error %i\n", err);
		if (err == 1) {
			iprintf("/_nds/ntr-forwarder/sdcard.nds\n");
			iprintf("not found.\n");
			iprintf("\n");
			iprintf("Please get this file from\n");
			iprintf("the SD forwarder pack, in\n");
			iprintf("\"for SD card root/_nds/\n");
			iprintf("ntr-forwarder\"");
		}
	} else {
		consoleDemoInit();
		iprintf("fatinitDefault failed!");
	}

	while (1) {
		swiWaitForVBlank();
	}

	return 0;
}
