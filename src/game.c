/* ===========================================================================
   GTA VI: DOS CITY -- game.c
   The state machine, the input mapping, and the autopilot that proves the game
   can be finished without a human (or, on a bad day, without a crash).
   =========================================================================== */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "g.h"

static int g_return_state = ST_TITLE;
static int g_prev_fire;

/* --- input ---------------------------------------------------------------- */

static void read_input(void)
{
  g_key_left = (unsigned char)(K_ANY_LEFT);
  g_key_right = (unsigned char)(K_ANY_RIGHT);
  g_key_up = (unsigned char)(K_ANY_UP);
  g_key_down = (unsigned char)(K_ANY_DOWN);
  g_key_fire = (unsigned char)(kbd_held(K_SPACE));
  g_key_run = (unsigned char)(kbd_held(K_LSHIFT) || kbd_held(K_RSHIFT));
  g_fire_pressed = (unsigned char)(g_key_fire && !g_prev_fire);
  g_prev_fire = g_key_fire;
}

/* --- autopilot ------------------------------------------------------------ */

/* The autopilot does not "steer toward the marker": that walks into the first
   building between here and there and then vibrates. It runs a BFS over the
   tile grid and follows the waypoints, which is both simpler and the only way
   the story test proves anything. 96x96 tiles, so the whole search is 9216
   cells and runs a few times a second at worst. */

#define AP_W CITY_W
#define AP_H CITY_H
/* The whole path, not a 4-tile window: keeping only the last few cells of a
   backwards walk gives you the cells nearest the *goal*, which makes the
   autopilot beeline into the first wall in between. 9216 shorts is 18 KB and
   this is a 16 MB machine. */
#define AP_MAX (AP_W * AP_H)

static short ap_px[AP_MAX], ap_py[AP_MAX];
static int ap_n, ap_i;
static unsigned long ap_last_path;

static short ap_prev[AP_W * AP_H];
static unsigned char ap_seen[AP_W * AP_H];
static short ap_queue[AP_W * AP_H];

static int ap_passable(int tx, int ty)
{
  if ((unsigned)tx >= (unsigned)CITY_W || (unsigned)ty >= (unsigned)CITY_H)
    return 0;
  return !solid_px(tx * 16 + 8, ty * 16 + 8) && !water_px(tx * 16 + 8, ty * 16 + 8);
}

static int ap_find_path(int sx, int sy, int gx, int gy)
{
  int head = 0, tail = 0;
  int i;

  if (!ap_passable(gx, gy))
    return 0;
  if ((unsigned)sx >= (unsigned)AP_W || (unsigned)sy >= (unsigned)AP_H)
    return 0;

  for (i = 0; i < AP_W * AP_H; i++) {
    ap_seen[i] = 0;
    ap_prev[i] = -1;
  }

  ap_queue[tail++] = (short)(sy * AP_W + sx);
  ap_seen[sy * AP_W + sx] = 1;

  while (head < tail) {
    int cur = ap_queue[head++];
    int cx = cur % AP_W, cy = cur / AP_W;
    int d;
    if (cx == gx && cy == gy)
      break;
    for (d = 0; d < 4; d++) {
      int nx = cx + (d == 0) - (d == 1);
      int ny = cy + (d == 2) - (d == 3);
      int ni;
      if (!ap_passable(nx, ny))
        continue;
      ni = ny * AP_W + nx;
      if (ap_seen[ni])
        continue;
      ap_seen[ni] = 1;
      ap_prev[ni] = (short)cur;
      ap_queue[tail++] = (short)ni;
    }
  }

  i = gy * AP_W + gx;
  if (!ap_seen[i])
    return 0;

  /* The backward walk gives goal -> start; store it and reverse in place.
     Doing this with two 18 KB local arrays is a good way to meet the stack
     limit on a machine from 1994, so it reverses the same buffer. */
  {
    int k = 0, a, b;
    while (i >= 0 && k < AP_MAX) {
      ap_px[k] = (short)(i % AP_W);
      ap_py[k] = (short)(i / AP_W);
      k++;
      i = ap_prev[i];
    }
    ap_n = k;
    for (a = 0, b = k - 1; a < b; a++, b--) {
      short tx = ap_px[a], ty = ap_py[a];
      ap_px[a] = ap_px[b];
      ap_py[a] = ap_py[b];
      ap_px[b] = tx;
      ap_py[b] = ty;
    }
  }
  ap_i = 1;                 /* skip the tile we are standing on */
  if (ap_i >= ap_n)
    ap_i = ap_n - 1;
  return ap_n > 0;
}

static int ap_fire_prev;
static int ap_car_stuck;

/* Point the gun at the nearest thing the current objective wants dead. */
static int ap_want_fire(int *out_dx, int *out_dy)
{
  const Obj *o;
  int i, best = -1;
  long bestd = 0;
  int px = FI(g_pl.x), py = FI(g_pl.y);

  if (!g_mission_active || g_mission < 0 || g_mission >= N_MISSIONS)
    return 0;
  if (g_obj < 0 || g_obj >= 3)
    return 0;
  {
    const Mission *m = mission_get(g_mission);
    if (!m)
      return 0;
    o = &m->obj[g_obj];
  }
  if (!o->text)
    return 0;

  if (o->kind == OB_WRECK) {
    for (i = 0; i < MAX_CARS; i++) {
      long dx, dy, d;
      if (!g_cars[i].active || !g_cars[i].target)
        continue;
      dx = FI(g_cars[i].x) - px;
      dy = FI(g_cars[i].y) - py;
      d = dx * dx + dy * dy;
      if (best < 0 || d < bestd) { best = i; bestd = d; }
    }
    if (best >= 0 && bestd < 150L * 150L) {
      *out_dx = FI(g_cars[best].x) - px;
      *out_dy = FI(g_cars[best].y) - py;
      return 1;
    }
  } else if (o->kind == OB_KILL) {
    for (i = 0; i < MAX_PEDS; i++) {
      long dx, dy, d;
      if (!g_peds[i].active || g_peds[i].state == 4 || !g_peds[i].hostile)
        continue;
      dx = FI(g_peds[i].x) - px;
      dy = FI(g_peds[i].y) - py;
      d = dx * dx + dy * dy;
      if (best < 0 || d < bestd) { best = i; bestd = d; }
    }
    if (best >= 0 && bestd < 130L * 130L) {
      *out_dx = FI(g_peds[best].x) - px;
      *out_dy = FI(g_peds[best].y) - py;
      return 1;
    }
  }
  return 0;
}

static void autopilot_input(void)
{
  int tx = FI(g_pl.x) >> 4;
  int ty = FI(g_pl.y) >> 4;
  int mx, my, gx, gy;
  int wx, wy, dx, dy;

  if (!mission_marker_pos(&mx, &my)) {
    mx = g_poi[POI_CLUB].x;
    my = g_poi[POI_CLUB].y;
  }
  gx = mx >> 4;
  gy = my >> 4;

  /* Re-path when the waypoint is reached, or every 30 frames as a safety net. */
  if (ap_n == 0 || (g_frames - ap_last_path) > 30 ||
      (ap_i >= 0 && ap_i < ap_n && tx == ap_px[ap_i] && ty == ap_py[ap_i])) {
    ap_last_path = g_frames;
    if (!ap_find_path(tx, ty, gx, gy)) {
      ap_n = 0;
      /* no path: fall back to walking straight at it and hoping */
      g_key_left = (unsigned char)(mx - FI(g_pl.x) < -6);
      g_key_right = (unsigned char)(mx - FI(g_pl.x) > 6);
      g_key_up = (unsigned char)(my - FI(g_pl.y) < -6);
      g_key_down = (unsigned char)(my - FI(g_pl.y) > 6);
      return;
    }
    while (ap_i + 1 < ap_n && tx == ap_px[ap_i] && ty == ap_py[ap_i])
      ap_i++;
  }

  wx = ap_px[ap_i] * 16 + 8;
  wy = ap_py[ap_i] * 16 + 8;
  dx = wx - FI(g_pl.x);
  dy = wy - FI(g_pl.y);

  /* snap to the waypoint when close, then advance along the list */
  if (dx > -7 && dx < 7 && dy > -7 && dy < 7 && ap_i + 1 < ap_n) {
    ap_i++;
    wx = ap_px[ap_i] * 16 + 8;
    wy = ap_py[ap_i] * 16 + 8;
    dx = wx - FI(g_pl.x);
    dy = wy - FI(g_pl.y);
  }

  if (g_pl.on_foot) {
    int ax, ay;
    int wx_ok, wy_ok;
    int px = FI(g_pl.x), py = FI(g_pl.y);

    /* combat: if the objective wants something dead, face it, shoot it, and
       keep closing the distance. Standing still to shoot is how you lose a
       chase against a car that is driving away. */
    if (ap_want_fire(&ax, &ay)) {
      if (ax > -8 && ax < 8) {
        g_pl.dir = (unsigned char)(ay < 0 ? 1 : 0);
      } else {
        g_pl.dir = (unsigned char)(ax < 0 ? 2 : 3);
      }
      g_key_fire = 1;
      g_fire_pressed = 1;
      ap_fire_prev = 1;

      g_key_left = (unsigned char)(ax < -14);
      g_key_right = (unsigned char)(ax > 14);
      g_key_up = (unsigned char)(ay < -14);
      g_key_down = (unsigned char)(ay > 14);
      return;
    }
    ap_fire_prev = 0;
    g_key_fire = 0;
    g_fire_pressed = 0;

    /* Plan around the actual body, not the tile centre: a 9-pixel-wide
       pedestrian can be stopped by a corner the BFS thinks is open. */
    wx_ok = (dx < -2 && !body_blocked(px - 3, py)) ||
            (dx > 2 && !body_blocked(px + 3, py));
    wy_ok = (dy < -2 && !body_blocked(px, py - 3)) ||
            (dy > 2 && !body_blocked(px, py + 3));

    g_key_left = (unsigned char)(dx < -2 && wx_ok);
    g_key_right = (unsigned char)(dx > 2 && wx_ok);
    g_key_up = (unsigned char)(dy < -2 && wy_ok);
    g_key_down = (unsigned char)(dy > 2 && wy_ok);

    /* both blocked: force a step on whichever axis has room, even if it is
       away from the waypoint. Standing still forever is worse than detouring. */
    if (!g_key_left && !g_key_right && !g_key_up && !g_key_down) {
      if (!body_blocked(px - 3, py)) g_key_left = 1;
      else if (!body_blocked(px + 3, py)) g_key_right = 1;
      else if (!body_blocked(px, py - 3)) g_key_up = 1;
      else if (!body_blocked(px, py + 3)) g_key_down = 1;
      ap_last_path = 0;             /* force a fresh path next frame */
    }
  } else {
    Car *c = &g_cars[g_pl.car];
    int want = c->angle, bestdot = -32768, a;

    /* A car is 28 pixels wide and the alleys are not. If it has been sitting
       at zero speed for a second, get out and walk: the objective is still
       the objective. */
    {
      int sp = c->speed < 0 ? -c->speed : c->speed;
      if (sp < FX(1) / 8)
        ap_car_stuck++;
      else
        ap_car_stuck = 0;
      if (ap_car_stuck > 60) {
        ap_car_stuck = 0;
        exit_car();
        return;
      }
    }

    ap_fire_prev = 0;
    g_key_fire = 0;
    g_fire_pressed = 0;

    for (a = 0; a < 16; a++) {
      int dot = gta_cos(a) * dx + gta_sin(a) * dy;
      if (dot > bestdot) {
        bestdot = dot;
        want = a;
      }
    }
    if (want != c->angle) {
      int diff = (want - c->angle + 16) & 15;
      g_key_left = (unsigned char)(diff > 8);
      g_key_right = (unsigned char)(diff <= 8);
    } else {
      g_key_left = 0;
      g_key_right = 0;
    }
    g_key_up = 1;
    g_key_down = 0;

    /* a car cannot fit down a sidewalk, so get out near the target */
    if (dx * dx + dy * dy < 26 * 26)
      exit_car();
  }
}

/* --- state ---------------------------------------------------------------- */

void game_set_state(int st)
{
  g_state = st;
}

int game_state(void)
{
  return g_state;
}

int game_ap_debug_n(void) { return ap_n; }
int game_ap_debug_i(void) { return ap_i; }
int game_ap_debug_wx(void) { return (ap_n && ap_i < ap_n) ? ap_px[ap_i] : -1; }
int game_ap_debug_wy(void) { return (ap_n && ap_i < ap_n) ? ap_py[ap_i] : -1; }

static void player_reset(void)
{
  memset(&g_pl, 0, sizeof(g_pl));
  g_pl.x = FX(g_poi[POI_SPAWN].x);
  g_pl.y = FX(g_poi[POI_SPAWN].y);
  g_pl.on_foot = 1;
  g_pl.car = -1;
  g_pl.health = 100;
  g_pl.ammo = 60;
  g_pl.money = 0;
  g_pl.dir = 0;
}

void game_new(void)
{
  world_init();
  mission_reset();
  player_reset();
  g_menu_sel = 0;
  g_obj_timer = -1;
  g_marker_poi = POI_CLUB;
  g_marker_on = 1;
  g_state = ST_INTRO;
  cam_update();
  snd_radio(0);
}

void game_init(void)
{
  gta_srand(0xC0FFEE);
  player_reset();
  world_init();
  mission_reset();
  g_state = ST_TITLE;
  g_menu_sel = 0;
  g_radio = N_SONGS - 1;          /* the main theme */
  g_radio_sel = g_radio;
  snd_radio(g_radio);
  g_marker_poi = POI_CLUB;
  g_marker_on = 1;
  cam_update();
}

/* --- tick ----------------------------------------------------------------- */

static void tick_play(void)
{
  read_input();
  if (g_autopilot)
    autopilot_input();

  update_player();
  update_traffic();
  update_peds();
  update_cops();
  update_bullets();
  update_pickups();
  update_fx();
  update_traffic_collisions();
  update_wanted();
  mission_tick();
  cam_update();
  ui_tick();

  snd_engine(g_pl.on_foot ? 0 : player_speed_px());

  g_clock += (g_frames % 120) == 0;
  if (g_clock > 24 * 60)
    g_clock = 0;

  if (g_pl.dead) {
    g_state = ST_WASTED;
    g_last_death = 0;
    snd_play(SFX_FAIL);
    snd_siren(0);
  }
}

void game_tick(void)
{
  g_frames++;

#ifdef GTA_HOST
  if (g_debug)
    fprintf(stderr, "tick %lu state=%d mission=%d obj=%d act=%d\n",
            g_frames, g_state, g_mission, g_obj, g_mission_active);
#endif

  /* The autopilot has to click through dialogue too, otherwise the story
     test stops at the first conversation and reports a clean run of nothing. */
  if (g_autopilot && g_state == ST_DIALOG && (g_frames % 6) == 0)
    game_key(K_ENTER);

  switch (g_state) {
    case ST_PLAY:
      tick_play();
      break;
    case ST_DIALOG:
      dialog_tick();
      break;
    case ST_MISSION_DONE:
      if (g_autopilot && (g_frames % 30) == 0)
        game_key(K_ENTER);
      break;
    case ST_WASTED:
    case ST_BUSTED:
      if (g_autopilot && (g_frames % 40) == 0)
        game_key(K_ENTER);
      break;
    case ST_CREDITS:
      /* The story test has nothing left to prove once the credits roll. */
      if (g_autopilot && (g_frames % 30) == 0)
        g_quit = 1;
      break;
    case ST_TITLE:
      if ((g_frames & 255) == 0)
        snd_radio(N_SONGS - 1);
      break;
    default:
      break;
  }
}

/* --- render --------------------------------------------------------------- */

static void draw_world_all(void)
{
  render_world();
  mission_draw_marker();
  render_entities();
}

void game_render(void)
{
  switch (g_state) {
    case ST_TITLE:
      ui_draw_title();
      break;
    case ST_INTRO:
      ui_draw_intro();
      break;
    case ST_PLAY:
      draw_world_all();
      ui_draw_minimap();
      mission_banner_draw();
      ui_draw_messages();
      ui_draw_hud();
      break;
    case ST_DIALOG:
      draw_world_all();
      ui_draw_minimap();
      ui_draw_hud();
      dialog_draw();
      break;
    case ST_PAUSE:
      draw_world_all();
      ui_draw_minimap();
      ui_draw_hud();
      ui_draw_pause();
      break;
    case ST_MAP:
      ui_draw_map();
      break;
    case ST_WASTED:
      draw_world_all();
      ui_draw_wasted(0);
      break;
    case ST_BUSTED:
      draw_world_all();
      ui_draw_wasted(1);
      break;
    case ST_MISSION_DONE:
      draw_world_all();
      ui_draw_hud();
      ui_draw_mission_done();
      break;
    case ST_CONTROLS:
      ui_draw_controls();
      break;
    case ST_ABOUT:
      ui_draw_about();
      break;
    case ST_RADIO_MENU:
      ui_draw_radio_menu();
      break;
    case ST_CREDITS:
      ui_draw_credits();
      break;
    default:
      ui_draw_title();
      break;
  }
}

/* --- keys ----------------------------------------------------------------- */

static void cycle_radio(void)
{
  g_radio++;
  if (g_radio >= N_SONGS)
    g_radio = -1;
  snd_radio(g_radio);
  g_radio_sel = g_radio;
  if (g_radio >= 0)
    ui_message(g_songs[g_radio].title, 1800);
  else
    ui_message(S_HUD_RADIO_OFF, 1200);
}

static void key_title(int sc)
{
  if (sc == K_UP) {
    g_menu_sel = (g_menu_sel + 5) % 6;
    snd_play(SFX_UI);
  } else if (sc == K_DOWN) {
    g_menu_sel = (g_menu_sel + 1) % 6;
    snd_play(SFX_UI);
  } else if (sc == K_ENTER) {
    snd_play(SFX_SELECT);
    switch (g_menu_sel) {
      case 0: game_new(); break;
      case 1:
        if (ui_load()) {
          g_state = ST_PLAY;
          ui_message(S_LOAD_OK, 1500);
        } else {
          ui_message(S_LOAD_FAIL, 1800);
        }
        break;
      case 2: g_radio_sel = g_radio; g_return_state = ST_TITLE;
              g_state = ST_RADIO_MENU; break;
      case 3: g_return_state = ST_TITLE; g_state = ST_CONTROLS; break;
      case 4: g_return_state = ST_TITLE; g_state = ST_ABOUT; break;
      default: g_quit = 1; break;
    }
  }
}

static void key_pause(int sc)
{
  if (sc == K_UP) {
    g_menu_sel = (g_menu_sel + 5) % 6;
    snd_play(SFX_UI);
  } else if (sc == K_DOWN) {
    g_menu_sel = (g_menu_sel + 1) % 6;
    snd_play(SFX_UI);
  } else if (sc == K_ESC) {
    g_state = ST_PLAY;
  } else if (sc == K_ENTER) {
    snd_play(SFX_SELECT);
    switch (g_menu_sel) {
      case 0: g_state = ST_PLAY; break;
      case 1: g_return_state = ST_PAUSE; g_state = ST_MAP; break;
      case 2: g_radio_sel = g_radio; g_return_state = ST_PAUSE;
              g_state = ST_RADIO_MENU; break;
      case 3: g_return_state = ST_PAUSE; g_state = ST_CONTROLS; break;
      case 4:
        if (ui_save())
          ui_message(S_SAVE_OK, 1800);
        else
          ui_message(S_SAVE_FAIL, 2000);
        break;
      default: g_state = ST_TITLE; snd_radio(N_SONGS - 1); break;
    }
  }
}

static void key_radio_menu(int sc)
{
  if (sc == K_UP) {
    g_radio_sel--;
    if (g_radio_sel < -1)
      g_radio_sel = N_SONGS - 1;
    snd_play(SFX_UI);
  } else if (sc == K_DOWN) {
    g_radio_sel++;
    if (g_radio_sel >= N_SONGS)
      g_radio_sel = -1;
    snd_play(SFX_UI);
  } else if (sc == K_ENTER) {
    g_radio = g_radio_sel;
    snd_radio(g_radio);
    snd_play(SFX_SELECT);
    g_state = g_return_state;
  } else if (sc == K_ESC) {
    g_state = g_return_state;
  }
}

void game_key(int sc)
{
  switch (g_state) {
    case ST_TITLE:
      key_title(sc);
      break;
    case ST_INTRO:
      if (sc == K_ENTER || sc == K_SPACE) {
        g_state = ST_PLAY;
        snd_play(SFX_SELECT);
      } else if (sc == K_ESC) {
        g_state = ST_TITLE;
      }
      break;
    case ST_PLAY:
      if (sc == K_ESC) {
        g_menu_sel = 0;
        g_state = ST_PAUSE;
      } else if (sc == K_M) {
        g_return_state = ST_PLAY;
        g_state = ST_MAP;
      } else if (sc == K_TAB) {
        cycle_radio();
      } else if (sc == K_ENTER || sc == K_F) {
        if (g_pl.on_foot) {
          int c = car_at(FI(g_pl.x), FI(g_pl.y), 22);
          if (c >= 0)
            enter_car(c);
          else
            ui_message(S_NOT_IN_CAR, 1000);
        } else {
          exit_car();
        }
      } else if (sc == K_H) {
        if (!g_pl.on_foot) {
          snd_play(SFX_HORN);
          /* honking makes pedestrians move, which is the entire point */
          {
            int i;
            for (i = 0; i < MAX_PEDS; i++) {
              Ped *p = &g_peds[i];
              int dx, dy;
              if (!p->active || p->state == 4)
                continue;
              dx = FI(p->x) - FI(g_pl.x);
              dy = FI(p->y) - FI(g_pl.y);
              if (dx > -70 && dx < 70 && dy > -70 && dy < 70) {
                p->state = 2;
                p->timer = 70;
              }
            }
          }
        }
      } else if (sc == K_F1) {
        g_return_state = ST_PLAY;
        g_state = ST_CONTROLS;
      }
      break;
    case ST_DIALOG:
      if (dialog_key(sc)) {
        int after = g_dialog_after;
        g_dialog_after = DA_NONE;
        g_state = ST_PLAY;
        if (after == DA_START_MISSION) {
          mission_start(g_mission);
          g_state = ST_PLAY;
        } else if (after == DA_EPILOGUE) {
          g_state = ST_CREDITS;
        }
      }
      break;
    case ST_PAUSE:
      key_pause(sc);
      break;
    case ST_MAP:
      if (sc == K_ESC || sc == K_M)
        g_state = g_return_state;
      break;
    case ST_RADIO_MENU:
      key_radio_menu(sc);
      break;
    case ST_CONTROLS:
    case ST_ABOUT:
      if (sc == K_ENTER || sc == K_ESC)
        g_state = g_return_state;
      break;
    case ST_WASTED:
    case ST_BUSTED:
      if (sc == K_ENTER || sc == K_SPACE) {
        respawn_player(g_state == ST_BUSTED);
        if (g_mission_active)
          mission_fail();
        g_state = ST_PLAY;
      }
      break;
    case ST_MISSION_DONE:
      if (sc == K_ENTER || sc == K_SPACE) {
        if (g_mission >= N_MISSIONS)
          g_state = ST_CREDITS;
        else
          g_state = ST_PLAY;
      }
      break;
    case ST_CREDITS:
      if (sc == K_ENTER || sc == K_ESC)
        g_state = ST_TITLE;
      break;
    default:
      break;
  }
}
