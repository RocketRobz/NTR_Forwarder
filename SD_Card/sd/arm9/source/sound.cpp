#include "sound.h"

#include "string.h"
#include "tonccpy.h"
#include <algorithm>
#include <sys/stat.h>

#include "soundbank.h"

char soundBank[0x4C000] = {0};

SoundControl::SoundControl()
{
	// Get date
	extern bool useTwlCfg;
	int birthMonth = (useTwlCfg ? *(u8*)0x02000446 : PersonalData->birthMonth);
	int birthDay = (useTwlCfg ? *(u8*)0x02000447 : PersonalData->birthDay);
	char soundBankPath[32], currentDate[16], birthDate[16];
	time_t Raw;
	time(&Raw);
	const struct tm *Time = localtime(&Raw);

	strftime(currentDate, sizeof(currentDate), "%m/%d", Time);
	sprintf(birthDate, "%02d/%02d", birthMonth, birthDay);

	sprintf(soundBankPath, "nitro:/soundbank%s.bin", (strcmp(currentDate, birthDate) == 0) ? "_bday" : "");

	// Load sound bank into memory
	FILE* soundBankF = fopen(soundBankPath, "rb");
	fread(soundBank, 1, sizeof(soundBank), soundBankF);
	fclose(soundBankF);

	mmInitDefaultMem((mm_addr)soundBank);

	mmLoadEffect( isDSiMode() ? SFX_DSIBOOT : SFX_DSBOOT );
	mmLoadEffect( SFX_SELECT );

	if (isDSiMode()) {
		snd_dsiboot = {
			{ SFX_DSIBOOT } ,			// id
			(int)(1.0f * (1<<10)),	// rate
			0,		// handle
			255,	// volume
			128,	// panning
		};
	} else {
		snd_dsiboot = {
			{ SFX_DSBOOT } ,			// id
			(int)(1.0f * (1<<10)),	// rate
			0,		// handle
			255,	// volume
			128,	// panning
		};
	}

	snd_select = {
		{ SFX_SELECT } ,			// id
		(int)(1.0f * (1<<10)),	// rate
		0,		// handle
		255,	// volume
		128,	// panning
	};
}

mm_sfxhand SoundControl::playDSiBoot() { return mmEffectEx(&snd_dsiboot); }
mm_sfxhand SoundControl::playSelect() { return mmEffectEx(&snd_select); }
