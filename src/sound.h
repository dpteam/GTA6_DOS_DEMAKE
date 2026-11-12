/* ===========================================================================
   GTA VI: DOS CITY -- sound.h

   One PC speaker. One bit of output. Everything you hear is scheduled
   arithmetic: a square wave, a 25% pulse faked by detuning, a noise burst made
   from a linear congruential generator, and drums that are just very short
   notes. No Sound Blaster, no samples, no mercy.
   =========================================================================== */
#ifndef GTA_SOUND_H
#define GTA_SOUND_H

#include "music.h"

enum {
  SFX_NONE = 0,
  SFX_PISTOL,
  SFX_PUNCH,
  SFX_PICKUP,
  SFX_MONEY,
  SFX_CRASH,
  SFX_UI,
  SFX_SELECT,
  SFX_CAR_ENTER,
  SFX_CAR_EXIT,
  SFX_HORN,
  SFX_MISSION,
  SFX_FAIL,
  SFX_WANTED,
  SFX_HIT,
  SFX_SKID,
  SFX_RELOAD,
  SFX_COUNT
};

void snd_init(void);
void snd_done(void);
void snd_tick(unsigned long now_ms);   /* call every logic step (60 Hz) */

void snd_play(int id);
void snd_radio(int station);           /* -1 = off, else song index */
int  snd_station(void);
void snd_siren(int on);
void snd_engine(int speed);            /* 0..255, only heard when radio is off */
void snd_off(void);

#endif
