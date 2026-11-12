/* ===========================================================================
   GTA VI: DOS CITY -- g_mission.c
   The story: seven jobs, one city, and a man who keeps agreeing to things.

   A mission is a short list of objectives. Each objective is either "go
   somewhere", "collect N things", "kill N people", or "wreck that car".
   That is enough structure to tell a joke and enough freedom to let the
   player drive into the sea on the way.
   =========================================================================== */
#include <stdio.h>
#include <string.h>

#include "g.h"

static const Mission g_missions[N_MISSIONS] = {
  { S_M01_NAME, S_M01_D1, S_M01_D2, S_M01_D3, S_M01_D4,
    { { OB_GOTO,    POI_CLUB,  0, 0,  S_M01_O1 },
      { OB_COLLECT, POI_CLUB,  1, 0,  S_M01_O2 },
      { OB_GOTO,    POI_GARAGE, 0, 0, S_M01_O3 } },
    500 },

  { S_M02_NAME, S_M02_D1, S_M02_D2, S_M02_D3, S_M02_D4,
    { { OB_COLLECT, POI_PARK,   3, 90, S_M02_O1 },
      { OB_GOTO,    POI_CLUB,   0, 0,  S_M02_O2 } },
    800 },

  { S_M03_NAME, S_M03_D1, S_M03_D2, S_M03_D3, S_M03_D4,
    { { OB_GOTO,  POI_MALL, 0, 0, S_M03_O1 },
      { OB_WRECK, POI_MALL, 1, 0, S_M03_O2 } },
    1200 },

  { S_M04_NAME, S_M04_D1, S_M04_D2, S_M04_D3, S_M04_D4,
    { { OB_GOTO, POI_DOCKS, 0, 0, S_M04_O1 },
      { OB_KILL, POI_DOCKS, 4, 0, S_M04_O2 } },
    2000 },

  { S_M05_NAME, S_M05_D1, S_M05_D2, S_M05_D3, S_M05_D4,
    { { OB_GOTO,    POI_MALL,  0, 0, S_M05_O1 },
      { OB_COLLECT, POI_MALL,  1, 0, S_M05_O2 },
      { OB_ESCAPE,  POI_BEACH, 0, 0, S_M05_O3 } },
    5000 },

  { S_M06_NAME, S_M06_D1, S_M06_D2, S_M06_D3, S_M06_D4,
    { { OB_ESCAPE, POI_AIRPORT, 0, 0, S_M06_O1 } },
    10000 },

  { S_M07_NAME, S_M07_D1, S_M07_D2, S_M07_D3, S_M07_D4,
    { { OB_GOTO, POI_BEACH, 0, 0, S_M07_O1 } },
    0 },
};

/* dialogue state */
static const char *g_dlg[4];
int g_dialog_after;

static int rnd(int n)
{
  return (int)(gta_rand() % (unsigned int)n);
}

const Mission *mission_get(int idx)
{
  if (idx < 0 || idx >= N_MISSIONS)
    return 0;
  return &g_missions[idx];
}

/* --- markers -------------------------------------------------------------- */

static const Obj *cur_obj(void)
{
  const Mission *m;
  if (g_mission < 0 || g_mission >= N_MISSIONS || !g_mission_active)
    return 0;
  m = &g_missions[g_mission];
  if (g_obj >= 3 || m->obj[g_obj].text == 0)
    return 0;
  return &m->obj[g_obj];
}

int mission_marker_pos(int *x, int *y)
{
  const Obj *o = cur_obj();
  if (!o) {
    if (g_marker_on && g_mission >= 0 && g_mission < N_MISSIONS) {
      const Mission *m = &g_missions[g_mission];
      *x = g_poi[m->obj[0].poi].x;
      *y = g_poi[m->obj[0].poi].y;
      return 1;
    }
    return 0;
  }
  if (o->kind == OB_COLLECT) {
    /* point at the nearest mission pickup instead of a fixed place */
    int i, best = -1;
    long bestd = 0;
    for (i = 0; i < MAX_PICKUPS; i++) {
      long dx, dy, d;
      if (!g_pickups[i].active || !g_pickups[i].mission)
        continue;
      dx = g_pickups[i].x - FI(g_pl.x);
      dy = g_pickups[i].y - FI(g_pl.y);
      d = dx * dx + dy * dy;
      if (best < 0 || d < bestd) {
        best = i;
        bestd = d;
      }
    }
    if (best < 0)
      return 0;
    *x = g_pickups[best].x;
    *y = g_pickups[best].y;
    return 1;
  }
  if (o->kind == OB_WRECK) {
    /* the blip rides on the target car: it is a chase, not a place */
    int i;
    for (i = 0; i < MAX_CARS; i++) {
      if (g_cars[i].active && g_cars[i].target) {
        *x = FI(g_cars[i].x);
        *y = FI(g_cars[i].y);
        return 1;
      }
    }
    return 0;
  }
  *x = g_poi[o->poi].x;
  *y = g_poi[o->poi].y;
  return 1;
}

void mission_draw_marker(void)
{
  int mx, my, sx, sy, i;
  int pulse = (int)((g_frames >> 2) & 7);

  if (g_state != ST_PLAY)
    return;
  if (!mission_marker_pos(&mx, &my))
    return;
  world_to_screen(mx, my, &sx, &sy);
  if (sx < -40 || sx > SCREEN_W + 40 || sy < -40 || sy > VIEW_H + 40)
    return;

  /* a dotted ring that spins, because 1994 never met a static marker it
     liked */
  for (i = 0; i < 16; i++) {
    int a = (i + (int)(g_frames >> 3)) & 15;
    int dx = gta_cos(a);
    int dy = gta_sin(a);
    r_pixel(sx + dx * 14 / 16, sy + dy * 14 / 16, PAL_YELLOW6);
    r_pixel(sx + dx * 13 / 16, sy + dy * 13 / 16, PAL_YELLOW4);
  }
  /* the chevron above the marker: up when idle, down when you are close */
  {
    int h = 26 + pulse;
    unsigned char col = (unsigned char)(PAL_YELLOW6 - pulse / 3);
    r_hline(sx - 4, sx + 4, sy - h, col);
    r_hline(sx - 3, sx + 3, sy - h + 1, col);
    r_hline(sx - 2, sx + 2, sy - h + 2, col);
    r_hline(sx - 1, sx + 1, sy - h + 3, col);
    r_pixel(sx, sy - h + 4, col);
  }
}

/* --- spawning mission actors ---------------------------------------------- */

static void cleanup_mission_actors(void)
{
  int i;
  for (i = 0; i < MAX_PICKUPS; i++)
    if (g_pickups[i].mission)
      g_pickups[i].active = 0;
  for (i = 0; i < MAX_PEDS; i++)
    if (g_peds[i].active && (g_peds[i].hostile || g_peds[i].kind == PED_GANG))
      g_peds[i].active = 0;
  for (i = 0; i < MAX_CARS; i++)
    if (g_cars[i].target)
      g_cars[i].active = 0;
}

static void spawn_mission_pickup(int kind, int poi, int spread)
{
  int i = -1, k;
  for (k = 0; k < MAX_PICKUPS; k++)
    if (!g_pickups[k].active) { i = k; break; }
  if (i < 0)
    return;
  g_pickups[i].active = 1;
  g_pickups[i].kind = (unsigned char)kind;
  g_pickups[i].mission = 1;
  g_pickups[i].bob = (unsigned char)rnd(64);
  g_pickups[i].x = g_poi[poi].x + rnd(spread * 2 + 1) - spread;
  g_pickups[i].y = g_poi[poi].y + rnd(spread * 2 + 1) - spread;
}

static void spawn_gang(int poi, int count, int spread)
{
  int n;
  for (n = 0; n < count; n++) {
    int i = find_free_ped();
    Ped *p;
    int tries;
    if (i < 0)
      return;
    p = &g_peds[i];
    p->active = 1;
    p->kind = PED_GANG;
    p->state = 3;
    p->hostile = 1;
    p->health = 9;
    p->dir = 0;
    p->timer = 0;

    /* Place them in a ring around the objective, but never within arm's
       reach of where the player is standing: spawning four armed men on top
       of someone is not difficulty, it is an execution. */
    p->x = FX(g_poi[poi].x);
    p->y = FX(g_poi[poi].y);
    for (tries = 0; tries < 24; tries++) {
      int ox = rnd(spread * 2 + 1) - spread;
      int oy = rnd(spread * 2 + 1) - spread;
      int px = g_poi[poi].x + ox;
      int py = g_poi[poi].y + oy;
      int dx = px - FI(g_pl.x);
      int dy = py - FI(g_pl.y);
      if (dx * dx + dy * dy < 80 * 80)
        continue;
      if (solid_px(px, py) || water_px(px, py))
        continue;
      p->x = FX(px);
      p->y = FX(py);
      break;
    }
  }
}

static void spawn_target_car(int poi)
{
  int i = find_free_car();
  Car *c;
  if (i < 0)
    return;
  c = &g_cars[i];
  c->active = 1;
  c->model = CAR_SPORTS;
  c->target = 1;
  /* Parked. A target that drives away at the same speed you walk is not a
     chase, it is a treadmill; parked, it is a decision: shoot it or ram it. */
  c->ai = 0;
  c->dir = 0;
  c->angle = 6;
  c->health = 40;
  c->speed = 0;
  c->x = FX(g_poi[poi].x);
  c->y = FX(g_poi[poi].y);
  c->tx = c->x;
  c->ty = c->y;
}

/* --- dialogue ------------------------------------------------------------- */

void dialog_start(const char *a, const char *b, const char *c, const char *d)
{
  g_dlg[0] = a;
  g_dlg[1] = b;
  g_dlg[2] = c;
  g_dlg[3] = d;
  g_dialog_n = 0;
  while (g_dialog_n < 4 && g_dlg[g_dialog_n])
    g_dialog_n++;
  g_dialog_i = 0;
  g_dialog_chars = 0;
  g_state = ST_DIALOG;
}

int dialog_active(void)
{
  return g_state == ST_DIALOG;
}

void dialog_tick(void)
{
  if (g_dialog_i >= g_dialog_n)
    return;
  g_dialog_chars += 2;
}

/* returns 1 when the whole dialogue is over */
int dialog_key(int sc)
{
  const char *line;
  if (sc != K_ENTER && sc != K_SPACE)
    return 0;
  line = g_dlg[g_dialog_i];
  if (line && g_dialog_chars < (int)strlen(line) * 8) {
    g_dialog_chars = (int)strlen(line) * 8 + 1;   /* skip the typing */
    return 0;
  }
  g_dialog_i++;
  g_dialog_chars = 0;
  snd_play(SFX_UI);
  if (g_dialog_i >= g_dialog_n) {
    g_dialog_i = 0;
    g_dialog_n = 0;
    return 1;
  }
  return 0;
}

const char *dialog_line(int i)
{
  if (i < 0 || i >= g_dialog_n)
    return 0;
  return g_dlg[i];
}

/* --- mission flow --------------------------------------------------------- */

void mission_reset(void)
{
  /* The first job is always on the board; "no mission" is g_mission >=
     N_MISSIONS, not -1, because -1 is a perfectly good array index and one
     day it will be treated as one. */
  g_mission = 0;
  g_obj = 0;
  g_obj_count = 0;
  g_obj_timer = -1;
  g_mission_active = 0;
  g_mission_failed = 0;
  g_marker_on = 1;
  g_reward_pending = 0;
  cleanup_mission_actors();
}

void mission_start(int idx)
{
  const Mission *m;
  if (idx < 0 || idx >= N_MISSIONS)
    return;
  m = &g_missions[idx];
  g_mission = idx;
  g_obj = 0;
  g_obj_count = 0;
  g_mission_active = 1;
  g_mission_failed = 0;
  g_marker_on = 0;
  cleanup_mission_actors();

  if (m->obj[0].timer)
    g_obj_timer = (long)m->obj[0].timer * 1000L;
  else
    g_obj_timer = -1;

  /* spawn whatever this mission needs to exist in the world */
  switch (idx) {
    case 0:
      break;
    case 1:
      spawn_mission_pickup(1, POI_PARK, 40);
      spawn_mission_pickup(1, POI_TOWER, 40);
      spawn_mission_pickup(1, POI_STADIUM, 40);
      break;
    case 2:
      spawn_target_car(POI_MALL);
      break;
    case 3:
      break;
    case 4:
      break;
    case 5:
      g_pl.wanted = 3;
      g_pl.wanted_timer = 0;
      break;
    default:
      break;
  }
  snd_play(SFX_MISSION);
  /* the objective strip appears on its own; a toast here would just repeat it */
}

static void set_obj_timer(void)
{
  const Obj *o = cur_obj();
  if (o && o->timer)
    g_obj_timer = (long)o->timer * 1000L;
  else
    g_obj_timer = -1;
}

void mission_advance(void)
{
  const Mission *m;
  const Obj *o;

  if (!g_mission_active || g_mission < 0)
    return;
  m = &g_missions[g_mission];
  g_obj++;
  g_obj_count = 0;

  o = cur_obj();
  if (!o) {
    /* done: pay up, unlock the next job */
    g_last_mission = g_mission;
    g_mission_active = 0;
    g_reward_pending = m->reward;
    g_pl.money += m->reward;
    g_mission = g_mission + 1;
    g_marker_on = (g_mission < N_MISSIONS);
    g_obj_timer = -1;
    g_mission_failed = 0;
    g_state = ST_MISSION_DONE;
    snd_play(SFX_MISSION);
    cleanup_mission_actors();
    return;
  }
  set_obj_timer();
  snd_play(SFX_PICKUP);
  /* No ui_message here: the objective strip at the top of the screen already
     shows exactly this text, and printing it twice looks like a bug. */

  /* objective-specific world setup */
  if (o->kind == OB_KILL && g_obj == 1 && g_mission == 3)
    spawn_gang(POI_DOCKS, 4, 70);
  if (o->kind == OB_COLLECT && g_mission == 0)
    spawn_mission_pickup(1, POI_CLUB, 8);
  if (o->kind == OB_COLLECT && g_mission == 4)
    spawn_mission_pickup(1, POI_MALL, 12);
}

void mission_fail(void)
{
  if (!g_mission_active)
    return;
  g_mission_active = 0;
  g_mission_failed = 1;
  g_last_mission = g_mission;
  g_obj_timer = -1;
  g_state = ST_MISSION_DONE;
  snd_play(SFX_FAIL);
  cleanup_mission_actors();
  /* the job stays on the board: you get to try again */
  g_marker_on = 1;
}

void mission_event(int type, int x, int y)
{
  const Obj *o = cur_obj();
  (void)x;
  (void)y;
  if (!o)
    return;
  if (type == MEV_KILL_PED && o->kind == OB_KILL) {
    if (g_obj_count < 255)
      g_obj_count++;
    if (g_obj_count >= o->count)
      mission_advance();
    else
      ui_message(o->text, 1500);
  } else if (type == MEV_WRECK && o->kind == OB_WRECK) {
    mission_advance();
  }
}

void mission_tick(void)
{
  int mx, my;
  int dx, dy;
  const Obj *o;

  if (g_state != ST_PLAY)
    return;

  /* Standing on the marker with no job running starts the job. This has to be
     checked before the "no current objective" early-out below, because with
     no job running there is no current objective. */
  if (!g_mission_active && g_marker_on && g_mission >= 0 &&
      g_mission < N_MISSIONS) {
    const Mission *m = &g_missions[g_mission];
    dx = FI(g_pl.x) - g_poi[m->obj[0].poi].x;
    dy = FI(g_pl.y) - g_poi[m->obj[0].poi].y;
    if (dx > -20 && dx < 20 && dy > -20 && dy < 20) {
      g_dialog_after = DA_START_MISSION;
      dialog_start(m->d1, m->d2, m->d3, m->d4);
      return;
    }
  }

  o = cur_obj();

  if (g_obj_timer > 0) {
    g_obj_timer -= 16;
    if (g_obj_timer <= 0) {
      g_obj_timer = 0;
      ui_message(S_MISSION_TIMEOUT, 2000);
      mission_fail();
      return;
    }
  }

  if (!o)
    return;

  /* reaching the marker */
  if (o->kind == OB_GOTO || o->kind == OB_ESCAPE) {
    if (mission_marker_pos(&mx, &my)) {
      dx = FI(g_pl.x) - mx;
      dy = FI(g_pl.y) - my;
      if (dx > -22 && dx < 22 && dy > -22 && dy < 22)
        mission_advance();
    }
  }
}

/* --- mission banner ------------------------------------------------------- */

void mission_banner_draw(void)
{
  const Obj *o = cur_obj();
  int w, x;
  if (!o)
    return;

  /* Objective text needs its own strip: drawn straight onto the city it
     collides with lane paint and crosswalks and becomes unreadable. */
  w = r_text_width(o->text) + 8;
  x = SCREEN_W / 2 - w / 2;
  if (x < 2) x = 2;
  if (x + w > SCREEN_W - 2) w = SCREEN_W - 2 - x;
  r_rect(x, 2, x + w, 18, PAL_VOID0);
  r_hline(x, x + w, 2, PAL_VOID2);
  r_hline(x, x + w, 18, PAL_VOID0);
  r_text_shadow(SCREEN_W / 2 - r_text_width(o->text) / 2, 3, o->text,
                PAL_YELLOW6);

  if (g_obj_timer > 0) {
    char buf[16];
    int secs = (int)(g_obj_timer / 1000);
    sprintf(buf, "%02d:%02d", secs / 60, secs % 60);
    r_rect(SCREEN_W / 2 - 22, 20, SCREEN_W / 2 + 22, 34, PAL_VOID0);
    r_text_shadow(SCREEN_W / 2 - 20, 21, buf,
                  (unsigned char)(secs < 10 ? PAL_RED7 : PAL_WHITE6));
  }
}
