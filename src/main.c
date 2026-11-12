/* ===========================================================================
   GTA VI: DOS CITY -- main.c

   Boot, the fixed-step frame loop, and the self-test that renders the game
   without a human at the keyboard.

   The loop runs at 60 logic steps per second regardless of machine speed, so
   a 486 and a Ryzen play the same game. The accumulator is capped at 120 ms:
   a stall must not turn into a 40-frame teleport through a wall.

   Command line:
     GTA6DOS.EXE            play
     GTA6DOS.EXE -test [n]  autopilot for n frames, dump PPM screenshots
   =========================================================================== */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "g.h"

#define STEP_MS 16            /* ~60 Hz */
#define FRAME_MS 14           /* render cap: ~70 fps, one retrace */

static int g_selftest;
static int g_selftest_frames;
static unsigned long g_renders;
static int g_norender;

#ifndef GTA_HOST
static int g_shot_n;
#endif

#ifndef GTA_HOST
/* Screenshot in PPM. No library, no compression, no arguing. */
static void selftest_dump(const char *path)
{
  FILE *f = fopen(path, "wb");
  int i;
  if (!f)
    return;
  fprintf(f, "P6\n%d %d\n255\n", SCREEN_W, SCREEN_H);
  for (i = 0; i < SCREEN_W * SCREEN_H; i++) {
    unsigned char c = g_backbuf[i];
    unsigned char rgb[3];
    rgb[0] = (unsigned char)(g_pal[c * 3 + 0] << 2);
    rgb[1] = (unsigned char)(g_pal[c * 3 + 1] << 2);
    rgb[2] = (unsigned char)(g_pal[c * 3 + 2] << 2);
    fwrite(rgb, 1, 3, f);
  }
  fclose(f);
}
#endif

#ifdef GTA_HOST
int main(void)
#else
int main(int argc, char **argv)
#endif
{
  unsigned long last, now;
  unsigned long acc = 0;
  int sc;
  int running = 1;

  vid_init();
  timer_init();
  kbd_init();
  snd_init();

  game_init();

#ifdef GTA_HOST
  {
    const char *ap = getenv("GTA_AUTOPILOT");
    const char *tour = getenv("GTA_TOUR");
    const char *tr = getenv("GTA_TRACE");
    const char *nr = getenv("GTA_NORENDER");
    if (tr && *tr == '1')
      g_debug = 1;
    if (nr && *nr == '1')
      g_norender = 1;
    if (ap && *ap == '1') {
      g_autopilot = 1;
      game_new();
      g_state = ST_PLAY;
    }
    if (tour && *tour == '1') {      /* Cycle through every screen and dump one frame each. Reviewing a menu
         by booting DOSBox and pressing keys is how you ship a typo. */
      static const int states[] = {
        ST_TITLE, ST_INTRO, ST_PLAY, ST_PAUSE, ST_MAP, ST_RADIO_MENU,
        ST_CONTROLS, ST_ABOUT, ST_WASTED, ST_BUSTED, ST_MISSION_DONE,
        ST_CREDITS
      };
      int n = (int)(sizeof(states) / sizeof(states[0]));
      int i;
      g_autopilot = 1;
      for (i = 0; i < n; i++) {
        int f;
        game_new();
        g_state = states[i];
        if (states[i] == ST_MISSION_DONE) {
          g_mission = 1;
          g_last_mission = 0;
          g_mission_failed = 0;
          g_reward_pending = 500;
        }
        for (f = 0; f < 20; f++) {
          game_tick();
          game_render();
          g_renders++;
          vid_present();     /* dumps on the 20th, i.e. once per screen */
        }
      }
      vid_shutdown();
      return 0;
    }
  }
#else
  if (argc > 1 && argv[1][0] == '-' && argv[1][1] == 't') {
    g_selftest = 1;
    g_selftest_frames = (argc > 2) ? atoi(argv[2]) : 600;
    g_autopilot = 1;
    g_state = ST_PLAY;
  }
#endif

  last = timer_ticks();
  {
#ifdef GTA_HOST
    /* The host has no real clock: it synthesises one so that the loop below
       is driven by logic steps. Without this, "render disabled" also means
       "time stopped" and the process spins forever. */
#endif
#ifndef GTA_HOST
    unsigned long next_frame = last;
#endif

    while (running) {
#ifdef GTA_HOST
      timer_advance(STEP_MS);
#endif
      now = timer_ticks();
      acc += now - last;
      last = now;
      if (acc > 120)
        acc = 120;

      while (acc >= STEP_MS) {
        acc -= STEP_MS;
        while ((sc = kbd_pop()) != 0) {
          if (sc == K_ESC && g_state == ST_TITLE && !g_selftest)
            running = 0;
          else
            game_key(sc);
        }
        if (g_quit)
          running = 0;
        game_tick();
        snd_tick(timer_ticks());

#ifdef GTA_HOST
        {
          static long max_frames = -1;
          if (max_frames < 0) {
            const char *lim = getenv("GTA_MAX_FRAMES");
            max_frames = lim ? atol(lim) : 0;
          }
          if (max_frames > 0 && (long)g_frames >= max_frames)
            running = 0;
        }
#endif

        if (g_selftest && --g_selftest_frames <= 0)
          running = 0;
      }

#ifdef GTA_HOST
      if (g_autopilot && (g_frames % 300) == 0) {
        printf("[f%lu] state=%d mission=%d obj=%d active=%d money=%lu "
               "hp=%d wanted=%d tile=%d,%d foot=%d car=%d speed=%d apn=%d "
               "api=%d\n",
               g_frames, g_state, g_mission, g_obj, g_mission_active,
               g_pl.money, g_pl.health, g_pl.wanted,
               FI(g_pl.x) >> 4, FI(g_pl.y) >> 4, g_pl.on_foot, g_pl.car,
               g_pl.on_foot ? 0 : g_cars[g_pl.car].speed,
               game_ap_debug_n(), game_ap_debug_i());
        printf("      px=%d py=%d wp=%d,%d keys=%d%d%d%d\n",
               FI(g_pl.x), FI(g_pl.y), game_ap_debug_wx(),
               game_ap_debug_wy(), g_key_up, g_key_down, g_key_left,
               g_key_right);
        fflush(stdout);
      }
#endif
      if (!g_norender) {
        game_render();
        g_renders++;
        vid_retrace();
        vid_present();
      }
#ifndef GTA_HOST
      if (g_selftest && (g_renders % 20) == 0) {
        char path[32];
        sprintf(path, "SHOT%03d.PPM", g_shot_n++);
        selftest_dump(path);
      }

      /* Frame cap that does not trust the video card. Waiting on the retrace
         bit alone is how a game ends up redrawing 4000 times a second on a
         card whose status register has opinions. This is driven by the same
         PIT the game logic uses, so it cannot disagree with it. */
      next_frame += FRAME_MS;
      if (timer_ticks() - next_frame > 100)
        next_frame = timer_ticks();       /* we fell behind: resync */
      while ((long)(timer_ticks() - next_frame) < 0)
        ;
#endif
    }
  }

  snd_off();
  snd_done();
  kbd_done();
  timer_done();
  vid_shutdown();

#ifdef GTA_HOST
  if (g_autopilot) {
    printf("\n=== autopilot summary ===\n");
    printf("frames=%lu renders=%lu\n", g_frames, g_renders);
    printf("missions completed: %d/%d\n", g_mission > N_MISSIONS ? N_MISSIONS
           : g_mission, N_MISSIONS);
    printf("money=%lu state=%d\n", g_pl.money, g_state);
    printf("story %s\n", (g_state == ST_CREDITS || g_mission >= N_MISSIONS)
           ? "COMPLETE" : "INCOMPLETE");
  }
#endif
#ifndef GTA_HOST
  {
    /* A run log, because "it felt fast" is not a bug report. */
    FILE *lf = fopen("RUNLOG.TXT", "w");
    if (lf) {
      fprintf(lf, "frames=%lu renders=%lu ticks=%lu\n", g_frames, g_renders,
              timer_ticks());
      if (timer_ticks()) {
        fprintf(lf, "logic/s=%.1f render/s=%.1f\n",
                g_frames * 1000.0 / (double)timer_ticks(),
                g_renders * 1000.0 / (double)timer_ticks());
      }
      fprintf(lf, "state=%d mission=%d money=%lu health=%d\n",
              g_state, g_mission, g_pl.money, g_pl.health);
      fprintf(lf, "player tile=%d,%d\n", FI(g_pl.x) >> 4, FI(g_pl.y) >> 4);
      fclose(lf);
    }
  }
#endif
  return 0;
}
