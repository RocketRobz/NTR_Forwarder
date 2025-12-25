#pragma once
#ifndef __TWILIGHTMENU_SOUND__
#define __TWILIGHTMENU_SOUND__
#include <nds.h>
#include <mm_types.h>
#include <maxmod9.h>
#include "singleton.h"
#include <cstdio>

/*
 * Handles playing sound effects and the streaming background music control.
 * See streamingaudio.c for a technical overview of how streaming works.
 */
class SoundControl {
    public:
        SoundControl();
        mm_sfxhand playDSiBoot();
        mm_sfxhand playSelect();

    private:
        mm_sound_effect snd_dsiboot;
        mm_sound_effect snd_select;
};

typedef singleton<SoundControl> soundCtl_s;
inline SoundControl &snd() { return soundCtl_s::instance(); }

#endif