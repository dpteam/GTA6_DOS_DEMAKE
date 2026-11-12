/* ===========================================================================
   GTA VI: DOS CITY -- sound.c

   The radio is a sequencer. Each 16th note is chopped into up to three
   sub-slots -- drum, lead, bass -- and the speaker switches between them
   faster than the ear can complain. That is exactly how 1-bit music worked
   before anyone had a sample to spare, and it is why a 1990 title tune sounds
   like a band and not like a beep.

   Timing comes from the 1 kHz PIT, so the music does not care about frame
   rate. SFX take the speaker away for a moment; the song keeps its place
   because the step index is derived from absolute time, not from a counter.
   =========================================================================== */
#include <dos.h>

#include "sound.h"
#include "video.h"

#ifdef GTA_HOST

void snd_init(void) {}
void snd_done(void) {}
void snd_tick(unsigned long now_ms) { (void)now_ms; }
void snd_play(int id) { (void)id; }
void snd_radio(int station) { (void)station; }
int  snd_station(void) { return -1; }
void snd_siren(int on) { (void)on; }
void snd_engine(int speed) { (void)speed; }
void snd_off(void) {}

#else

#define SPK_PORT 0x61
#define PIT_CMD  0x43
#define PIT_CH2  0x42

/* --- speaker primitives --------------------------------------------------- */

static unsigned char g_spk_state;

static void spk_gate(int on)
{
  if (on)
    g_spk_state |= 3;
  else
    g_spk_state &= (unsigned char)~3;
  outportb(SPK_PORT, g_spk_state);
}

static void pit2_div(unsigned int div)
{
  if (div < 8)
    div = 8;
  outportb(PIT_CMD, 0xB6);
  outportb(PIT_CH2, (unsigned char)(div & 0xFF));
  outportb(PIT_CH2, (unsigned char)((div >> 8) & 0xFF));
}

/* A linear congruential generator is the entire drum kit. */
static unsigned long g_noise = 0x1D872B41UL;

static unsigned int noise_div(unsigned int lo, unsigned int hi)
{
  g_noise = g_noise * 1103515245UL + 12345UL;
  return lo + (unsigned int)((g_noise >> 16) % (hi - lo + 1));
}

/* --- one sub-event -------------------------------------------------------- */

typedef struct {
  unsigned short div;
  unsigned char start;      /* ms offset inside the step */
  unsigned char dur;        /* ms */
  unsigned char wave;
} Ev;

#define MAX_EV 4

static Ev g_ev[MAX_EV];
static int g_ev_n;
static int g_ev_i;
static unsigned long g_ev_end;

/* --- sfx ------------------------------------------------------------------ */

typedef struct {
  unsigned char note;       /* MIDI note; 0xFF ends the list */
  unsigned short dur;
  unsigned char wave;
} SNote;

#define END {0xFF, 0, 0}

static const SNote fx_pistol[]  = { {96, 16, W_NOISE}, {72, 30, W_NOISE_LO}, END };
static const SNote fx_punch[]   = { {45, 28, W_SQUARE}, {38, 24, W_SQUARE}, END };
static const SNote fx_pickup[]  = { {84, 30, W_SQUARE}, {91, 50, W_SQUARE}, END };
static const SNote fx_money[]   = { {88, 28, W_SQUARE}, {92, 28, W_SQUARE},
                                    {96, 70, W_SQUARE}, END };
static const SNote fx_crash[]   = { {84, 40, W_NOISE}, {60, 70, W_NOISE_LO}, END };
static const SNote fx_ui[]      = { {76, 16, W_SQUARE}, END };
static const SNote fx_select[]  = { {84, 18, W_SQUARE}, {88, 34, W_SQUARE}, END };
static const SNote fx_enter[]   = { {60, 45, W_SQUARE}, {67, 45, W_SQUARE}, END };
static const SNote fx_exit[]    = { {67, 45, W_SQUARE}, {60, 45, W_SQUARE}, END };
static const SNote fx_horn[]    = { {69, 170, W_PULSE}, {72, 190, W_PULSE}, END };
static const SNote fx_mission[] = { {72, 80, W_SQUARE}, {76, 80, W_SQUARE},
                                    {79, 80, W_SQUARE}, {84, 90, W_SQUARE},
                                    {79, 90, W_SQUARE}, {84, 230, W_SQUARE}, END };
static const SNote fx_fail[]    = { {67, 120, W_SQUARE}, {63, 120, W_SQUARE},
                                    {58, 260, W_SQUARE}, END };
static const SNote fx_wanted[]  = { {72, 70, W_PULSE}, {67, 70, W_PULSE},
                                    {72, 70, W_PULSE}, END };
static const SNote fx_hit[]     = { {52, 40, W_NOISE}, {44, 60, W_SQUARE}, END };
static const SNote fx_skid[]    = { {100, 22, W_NOISE}, {100, 22, W_NOISE},
                                    {100, 22, W_NOISE}, END };
static const SNote fx_reload[]  = { {72, 20, W_NOISE}, {60, 20, W_NOISE}, END };

static const SNote *g_sfx;
static int g_sfx_i;
static unsigned long g_sfx_end;

/* --- state ---------------------------------------------------------------- */

static int g_song = -1;
static unsigned long g_song_start;
static unsigned long g_step_seen = (unsigned long)-1;
static int g_siren;
static int g_engine;
static unsigned int g_phase;

/* --- scheduling ----------------------------------------------------------- */

/* Holds (255) keep the previous pitch; scanning back is cheap and keeps the
   pattern table in the form it was written in. */
static unsigned char note_at(const unsigned char *track, int step, int steps)
{
  int i = step;
  int guard = 0;
  while (i >= 0 && guard++ < steps) {
    if (track[i] != 255)
      return track[i];
    i--;
  }
  return 0;
}

static void push(Ev *e, int *n, unsigned char note, unsigned char start,
                 unsigned char dur, unsigned char wave)
{
  if (*n >= MAX_EV || dur < 8)
    return;
  e[*n].div = g_note_div[note & 127];
  e[*n].start = start;
  e[*n].dur = dur;
  e[*n].wave = wave;
  (*n)++;
}

static void schedule_step(unsigned long step_index)
{
  const Song *s = &g_songs[g_song];
  int steps = s->steps;
  int step = (int)(step_index % (unsigned long)steps);
  int step_ms = 60000 / (s->bpm * 4);
  unsigned char drum = (unsigned char)s->drums[step & 15];
  unsigned char lead = note_at(s->lead, step, steps);
  unsigned char bass = note_at(s->bass, step, steps);
  int used = 0;
  int n = 0;
  int split;

  if (step_ms < 40)
    step_ms = 40;

  if (drum == 'k') {
    push(g_ev, &n, 28, 0, 26, W_SQUARE);
    used = 26;
  } else if (drum == 's') {
    push(g_ev, &n, 96, 0, 30, W_NOISE);
    used = 30;
  } else if (drum == 'h') {
    push(g_ev, &n, 108, 0, 12, W_NOISE);
    used = 12;
  }

  split = step_ms * 58 / 100;
  if (split < used + 16)
    split = used + 16;

  if (lead && bass) {
    push(g_ev, &n, lead, (unsigned char)used, (unsigned char)(split - used),
         s->wave);
    push(g_ev, &n, bass, (unsigned char)split,
         (unsigned char)(step_ms - split), (unsigned char)(s->wave == W_SAW ?
                                                          W_SQUARE : W_SAW));
  } else if (lead) {
    push(g_ev, &n, lead, (unsigned char)used, (unsigned char)(step_ms - used),
         s->wave);
  } else if (bass) {
    push(g_ev, &n, bass, (unsigned char)used, (unsigned char)(step_ms - used),
         (unsigned char)(s->wave == W_SAW ? W_SQUARE : W_SAW));
  }

  g_ev_n = n;
  g_ev_i = 0;
  g_ev_end = 0;
}

/* --- output --------------------------------------------------------------- */

static void apply(unsigned short div, unsigned char wave)
{
  switch (wave) {
    case W_PULSE:
      /* A 1-bit speaker cannot do duty cycle, so it detunes instead: the
         beating is heard as a rough edge, which is what a pulse sounds like. */
      pit2_div((g_phase & 1) ? div : (unsigned short)(div + div / 96 + 1));
      break;
    case W_SAW:
      pit2_div((g_phase & 1) ? div : (unsigned short)(div * 2));
      break;
    case W_TRI:
      pit2_div((g_phase & 1) ? div : (unsigned short)(div / 2));
      break;
    case W_NOISE:
      pit2_div(noise_div(8, 46));
      break;
    case W_NOISE_LO:
      pit2_div(noise_div(420, 980));
      break;
    default:
      pit2_div(div);
      break;
  }
  spk_gate(1);
}

void snd_init(void)
{
  spk_gate(0);
  g_song = -1;
  g_sfx = 0;
}

void snd_done(void)
{
  spk_gate(0);
}

void snd_off(void)
{
  spk_gate(0);
  g_song = -1;
  g_sfx = 0;
  g_siren = 0;
  g_engine = 0;
}

void snd_radio(int station)
{
  if (station < 0 || station >= N_SONGS)
    station = -1;
  if (station == g_song)
    return;
  g_song = station;
  g_song_start = timer_ticks();
  g_step_seen = (unsigned long)-1;
}

int snd_station(void)
{
  return g_song;
}

void snd_siren(int on)
{
  g_siren = on;
}

void snd_engine(int speed)
{
  g_engine = speed;
}

void snd_play(int id)
{
  switch (id) {
    case SFX_PISTOL:  g_sfx = fx_pistol;  break;
    case SFX_PUNCH:   g_sfx = fx_punch;   break;
    case SFX_PICKUP:  g_sfx = fx_pickup;  break;
    case SFX_MONEY:   g_sfx = fx_money;   break;
    case SFX_CRASH:   g_sfx = fx_crash;   break;
    case SFX_UI:      g_sfx = fx_ui;      break;
    case SFX_SELECT:  g_sfx = fx_select;  break;
    case SFX_CAR_ENTER: g_sfx = fx_enter; break;
    case SFX_CAR_EXIT:  g_sfx = fx_exit;  break;
    case SFX_HORN:    g_sfx = fx_horn;    break;
    case SFX_MISSION: g_sfx = fx_mission; break;
    case SFX_FAIL:    g_sfx = fx_fail;    break;
    case SFX_WANTED:  g_sfx = fx_wanted;  break;
    case SFX_HIT:     g_sfx = fx_hit;     break;
    case SFX_SKID:    g_sfx = fx_skid;    break;
    case SFX_RELOAD:  g_sfx = fx_reload;  break;
    default: return;
  }
  g_sfx_i = 0;
  g_sfx_end = 0;
}

void snd_tick(unsigned long now_ms)
{
  unsigned long t;

  g_phase++;

  /* 1. SFX own the speaker while they last. */
  if (g_sfx) {
    if (now_ms < g_sfx_end) {
      apply(g_sfx[g_sfx_i].note == 0xFF ? 0 : g_note_div[g_sfx[g_sfx_i].note],
            g_sfx[g_sfx_i].wave);
      return;
    }
    g_sfx_i++;
    if (g_sfx[g_sfx_i].note == 0xFF) {
      g_sfx = 0;
    } else {
      apply(g_note_div[g_sfx[g_sfx_i].note], g_sfx[g_sfx_i].wave);
      g_sfx_end = now_ms + g_sfx[g_sfx_i].dur;
      return;
    }
  }

  /* 2. Siren, if the cops care. */
  if (g_siren) {
    pit2_div(((now_ms / 260) & 1) ? 900 : 620);
    spk_gate(1);
    return;
  }

  /* 3. The radio. */
  if (g_song >= 0) {
    const Song *s = &g_songs[g_song];
    unsigned long step_ms = (unsigned long)(60000 / (s->bpm * 4));
    unsigned long step_index;
    unsigned long step_start;
    if (step_ms < 40)
      step_ms = 40;
    step_index = (now_ms - g_song_start) / step_ms;
    step_start = g_song_start + step_index * step_ms;

    if (step_index != g_step_seen) {
      g_step_seen = step_index;
      schedule_step(step_index);
    }
    t = now_ms - step_start;
    while (g_ev_i < g_ev_n && t >= (unsigned long)g_ev[g_ev_i].start +
           g_ev[g_ev_i].dur) {
      g_ev_i++;
    }
    if (g_ev_i < g_ev_n && t >= (unsigned long)g_ev[g_ev_i].start) {
      apply(g_ev[g_ev_i].div, g_ev[g_ev_i].wave);
      return;
    }
    spk_gate(0);
    return;
  }

  /* 4. No radio: the engine gets a voice. */
  if (g_engine > 0) {
    unsigned int div = (unsigned int)(2600 - g_engine * 7);
    if (div < 700)
      div = 700;
    pit2_div((g_phase & 2) ? div : (unsigned short)(div + 40));
    spk_gate(1);
    return;
  }

  spk_gate(0);
}

#endif
