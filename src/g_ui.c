/* ===========================================================================
   GTA VI: DOS CITY -- g_ui.c
   Everything the player reads: HUD, minimap, title, menus, the big map, the
   dialogue box, and the two screens you see when the city wins.

   The whole UI is bevelled boxes and one 8x16 font. That is not a limitation,
   it is a style, and I am sticking to it.
   =========================================================================== */
#include <stdio.h>
#include <string.h>

#include "g.h"

/* --- transient messages --------------------------------------------------- */

static char g_msg[64];
static long g_msg_timer;

void ui_message(const char *text, unsigned long ms)
{
  strncpy(g_msg, text, sizeof(g_msg) - 1);
  g_msg[sizeof(g_msg) - 1] = 0;
  g_msg_timer = (long)ms;
}

/* Called once per logic step. A message with no countdown is a permanent
   caption, and a permanent caption is a bug. */
void ui_tick(void)
{
  if (g_msg_timer > 0)
    g_msg_timer -= 16;
}

void ui_draw_messages(void)
{
  int w, x;
  if (g_msg_timer <= 0)
    return;
  w = r_text_width(g_msg) + 10;
  x = SCREEN_W / 2 - w / 2;
  if (x < 2) x = 2;
  if (x + w > SCREEN_W - 2) w = SCREEN_W - 2 - x;
  r_rect(x, VIEW_H - 26, x + w, VIEW_H - 10, PAL_VOID0);
  r_hline(x, x + w, VIEW_H - 26, PAL_VOID2);
  r_text_shadow(SCREEN_W / 2 - r_text_width(g_msg) / 2, VIEW_H - 25, g_msg,
                PAL_YELLOW6);
}

/* --- big gradient text ---------------------------------------------------- */

void ui_big_text(int x, int y, const char *s, int scale, int ramp_base,
                 int ramp_n)
{
  const char *p;
  int pass;

  for (pass = 0; pass < 2; pass++) {
    int cx = x + (pass ? 0 : scale * 2);
    int cy = y + (pass ? 0 : scale * 2);
    for (p = s; *p; p++) {
      const unsigned char *g = g_font + (unsigned char)*p * 16;
      int gy;
      for (gy = 0; gy < 16; gy++) {
        unsigned char bits = g[gy];
        int gx;
        if (!bits)
          continue;
        for (gx = 0; gx < 8; gx++) {
          int dx, dy;
          unsigned char col;
          if (!(bits & (0x80 >> gx)))
            continue;
          if (!pass) {
            col = PAL_VOID1;
          } else {
            int step = (ramp_n - 1) - (gy * ramp_n) / 16;
            if (step < 0) step = 0;
            col = (unsigned char)(ramp_base + step);
          }
          for (dy = 0; dy < scale; dy++)
            for (dx = 0; dx < scale; dx++)
              r_pixel(cx + gx * scale + dx, cy + gy * scale + dy, col);
        }
      }
      cx += 8 * scale;
    }
  }
}

/* --- HUD ------------------------------------------------------------------ */

static void bar(int x, int y, int w, int h, int value, int max,
                unsigned char fill_lo, unsigned char fill_hi)
{
  int i;
  r_box(x, y, x + w, y + h, PAL_VOID1, PAL_VOID3, PAL_VOID0);
  if (value < 0) value = 0;
  if (value > max) value = max;
  for (i = 0; i < (w - 2); i++) {
    int on = (i * max) / (w - 2) < value;
    unsigned char c = on ? (unsigned char)(fill_lo + (i * 3) / (w - 2))
                         : PAL_VOID2;
    r_vline(x + 1 + i, y + 1, y + h - 1, c);
  }
  (void)fill_hi;
}

void ui_draw_minimap(void)
{
  int mx = SCREEN_W - 68, my = 4;
  int px = FI(g_pl.x) >> 4;
  int py = FI(g_pl.y) >> 4;
  int tx, ty;

  r_box(mx - 2, my - 2, mx + 63, my + 63, PAL_VOID1, PAL_CONCRETE5,
        PAL_VOID0);

  for (ty = 0; ty < 64; ty++) {
    for (tx = 0; tx < 64; tx++) {
      int wx = px - 32 + tx;
      int wy = py - 32 + ty;
      unsigned char c;
      int t = map_tile(wx, wy);
      if ((unsigned)wx >= CITY_W || (unsigned)wy >= CITY_H) {
        c = PAL_VOID1;
      } else if (g_tile_flags[t] & TF_WATER) {
        c = PAL_WATER5;
      } else if (g_tile_flags[t] & TF_ROOF) {
        c = PAL_TEAL4;
      } else if (t == T_GRASS || t == T_GRASS2) {
        c = PAL_GREEN5;
      } else if (t == T_SAND) {
        c = PAL_SAND5;
      } else if (t == T_ASPHALT || t == T_ASPHALT_H || t == T_ASPHALT_V ||
                 t == T_CROSSWALK || t == T_MANHOLE) {
        c = PAL_ASPHALT6;
      } else {
        c = PAL_CONCRETE6;
      }
      r_pixel(mx + tx, my + ty, c);
    }
  }

  /* blips */
  {
    int i;
    int mxs, mys;
    if (mission_marker_pos(&mxs, &mys)) {
      int bx = mx + 32 + (mxs >> 4) - px;
      int by = my + 32 + (mys >> 4) - py;
      if (bx > mx && bx < mx + 63 && by > my && by < my + 63) {
        if ((g_frames >> 3) & 1) {
          r_pixel(bx, by, PAL_YELLOW7);
          r_pixel(bx + 1, by, PAL_YELLOW5);
          r_pixel(bx, by + 1, PAL_YELLOW5);
        }
      }
    }
    for (i = 0; i < MAX_CARS; i++) {
      int bx, by;
      if (!g_cars[i].active || !g_cars[i].is_cop)
        continue;
      bx = mx + 32 + (FI(g_cars[i].x) >> 4) - px;
      by = my + 32 + (FI(g_cars[i].y) >> 4) - py;
      if (bx > mx && bx < mx + 63 && by > my && by < my + 63)
        r_pixel(bx, by, PAL_BLUE6);
    }
  }

  /* the player: a blinking white dot, because a 1px dot is easy to lose */
  r_pixel(mx + 32, my + 32, PAL_WHITE7);
  if ((g_frames >> 2) & 1) {
    r_pixel(mx + 31, my + 32, PAL_WHITE5);
    r_pixel(mx + 33, my + 32, PAL_WHITE5);
    r_pixel(mx + 32, my + 31, PAL_WHITE5);
    r_pixel(mx + 32, my + 33, PAL_WHITE5);
  }

  /* north marker */
  r_text(mx + 28, my + 54, "N", PAL_CONCRETE4);
}

void ui_draw_hud(void)
{
  int y = VIEW_H;
  char buf[32];
  int i;

  r_rect(0, y, SCREEN_W - 1, SCREEN_H - 1, PAL_VOID1);
  r_hline(0, SCREEN_W - 1, y, PAL_VOID3);
  r_hline(0, SCREEN_W - 1, y + 1, PAL_VOID2);

  /* row 1: health, armour, money, weapon, wanted */
  bar(4, y + 4, 56, 9, g_pl.health, 100, PAL_RED3, PAL_RED6);
  bar(64, y + 4, 40, 9, g_pl.armour, 100, PAL_BLUE3, PAL_BLUE6);

  sprintf(buf, "$%07lu", g_pl.money % 10000000UL);
  r_text(112, y, buf, PAL_GOLD6);

  r_text(184, y, "9MM", PAL_CONCRETE3);
  sprintf(buf, "%d", g_pl.ammo);
  r_text(216, y, buf, g_pl.ammo ? PAL_WHITE6 : PAL_RED5);

  for (i = 0; i < 5; i++)
    r_sprite(SPR_PICK_BASE + (i < g_pl.wanted ? 5 : 6), 240 + i * 13, y + 1);

  /* row 2: radio and clock */
  if (g_radio >= 0) {
    const Song *s = &g_songs[g_radio];
    r_text(4, y + 16, S_HUD_RADIO, PAL_CONCRETE3);
    r_text(52, y + 16, s->title, PAL_NEONCYAN5);
  } else {
    r_text(4, y + 16, S_HUD_RADIO_OFF, PAL_CONCRETE3);
  }

  sprintf(buf, "%02lu:%02lu", (g_clock / 60) % 24, g_clock % 60);
  r_text_right(SCREEN_W - 6, y + 16, buf, PAL_CONCRETE4);

  if (g_show_fps) {
    sprintf(buf, "%lu", g_frames);
    r_text(4, 4, buf, PAL_GREEN6);
  }
}

/* --- title ---------------------------------------------------------------- */

static void sky_and_city(void)
{
  int i, x;
  unsigned char sky_base = PAL_PURPLE_BASE;
  int sky_n = PAL_PURPLE_N;

  r_gradient_v(0, 92, sky_base, sky_n, 1, 7);
  /* a sun that has clearly given up on the day */
  for (i = 0; i < 20; i++)
    r_hline(250 - i, 250 + i, 60 - i / 2, PAL_ORANGE6);
  for (i = 0; i < 16; i++)
    r_hline(250 - i, 250 + i, 64 - i / 2, PAL_ORANGE7);

  /* skyline: deterministic, so the title screen is the same city every time */
  x = -6;
  while (x < SCREEN_W) {
    int w = 10 + (int)(gta_rand() % 16);
    int h = 12 + (int)(gta_rand() % 40);
    int top = 92 - h;
    r_rect(x, top, x + w - 1, 91, PAL_VOID1);
    /* windows */
    {
      int wy, wx;
      for (wy = top + 3; wy < 89; wy += 4)
        for (wx = x + 2; wx < x + w - 1; wx += 3)
          if ((gta_rand() % 100) < 22)
            r_pixel(wx, wy, PAL_YELLOW4);
    }
    x += w + 2;
  }

  /* water */
  r_rect(0, 92, SCREEN_W - 1, 103, PAL_WATER2);
  for (i = 0; i < 40; i++) {
    int wy = 94 + (int)(gta_rand() % 9);
    int wx = (int)(gta_rand() % SCREEN_W);
    r_hline(wx, wx + 3 + (int)(gta_rand() % 6), wy, PAL_WATER5);
  }

  /* palms, in silhouette, because this is Florida */
  for (i = 0; i < 3; i++) {
    int px = 20 + i * 100 + (int)(gta_rand() % 40);
    int py = 92;
    int k;
    for (k = 0; k < 7; k++) {
      int a = k * 2;
      int dx = gta_cos(a), dy = gta_sin(a);
      int t;
      for (t = 2; t < 14; t++)
        r_pixel(px + dx * t / 16, py - 6 + dy * t / 20, PAL_DARKGREEN3);
    }
    r_vline(px, py - 6, py, PAL_VOID1);
  }
}

static const char *menu_items[] = {
  S_MENU_NEW, S_MENU_CONT, S_MENU_RADIO, S_MENU_CTRL, S_MENU_ABOUT, S_MENU_EXIT
};
#define MENU_N 6

void ui_draw_title(void)
{
  int i;
  int y;

  r_clear(PAL_VOID1);
  sky_and_city();

  /* logo */
  ui_big_text(SCREEN_W / 2 - (6 * 8 * 3) / 2, 2, S_GAME_TITLE, 3,
              PAL_GOLD_BASE, PAL_GOLD_N);
  ui_big_text(SCREEN_W / 2 - (8 * 8 * 2) / 2, 54, S_GAME_SUB, 2,
              PAL_NEONCYAN_BASE, PAL_NEONCYAN_N);
  r_text_center(70, S_GAME_TAG, PAL_CONCRETE4);

  /* menu */
  r_box_double(60, 86, SCREEN_W - 61, 182, PAL_VOID1, PAL_CONCRETE5,
               PAL_VOID0);
  y = 88;
  for (i = 0; i < MENU_N; i++) {
    if (i == g_menu_sel) {
      r_rect(66, y - 1, SCREEN_W - 67, y + 14, PAL_NAVY4);
      r_text(72, y, ">", PAL_YELLOW6);
      r_text(84, y, menu_items[i], PAL_WHITE7);
    } else {
      r_text(84, y, menu_items[i], PAL_CONCRETE4);
    }
    y += 15;
  }

  r_text_center(184, S_TITLE_HINT, PAL_CONCRETE3);
}

/* --- intro ---------------------------------------------------------------- */

void ui_draw_intro(void)
{
  int i;
  r_clear(PAL_VOID1);
  r_gradient_v(0, 140, PAL_PURPLE_BASE, PAL_PURPLE_N, 2, 6);
  ui_big_text(SCREEN_W / 2 - (6 * 8 * 2) / 2, 12, S_GAME_TITLE, 2,
              PAL_GOLD_BASE, PAL_GOLD_N);
  r_text_center(46, S_GAME_SUB, PAL_NEONCYAN5);
  for (i = 0; i < 8; i++) {
    const char *line = 0;
    switch (i) {
      case 0: line = S_INTRO_1; break;
      case 1: line = S_INTRO_2; break;
      case 2: line = S_INTRO_3; break;
      case 3: line = S_INTRO_4; break;
      case 4: line = S_INTRO_5; break;
      case 5: line = S_INTRO_6; break;
      case 6: line = S_INTRO_7; break;
      default: line = S_INTRO_8; break;
    }
    r_text_center(74 + i * 12, line, PAL_WHITE5);
  }
  if ((g_frames >> 4) & 1)
    r_text_center(184, S_INTRO_HINT, PAL_YELLOW6);
}

/* --- pause ---------------------------------------------------------------- */

static const char *pause_items[] = {
  S_PAUSE_RESUME, S_PAUSE_MAP, S_PAUSE_RADIO, S_PAUSE_CTRL, S_PAUSE_SAVE,
  S_PAUSE_QUIT
};
#define PAUSE_N 6

void ui_draw_pause(void)
{
  int i;
  int x0 = 64, y0 = 24, x1 = SCREEN_W - 65, y1 = 164;

  r_dither_rect(0, 0, SCREEN_W - 1, VIEW_H - 1, PAL_VOID1, PAL_VOID0);
  r_box_double(x0, y0, x1, y1, PAL_VOID1, PAL_CONCRETE5, PAL_VOID0);
  r_text_center(y0 + 6, S_PAUSE_TITLE, PAL_GOLD5);
  for (i = 0; i < PAUSE_N; i++) {
    int y = y0 + 28 + i * 18;
    if (i == g_menu_sel) {
      r_rect(x0 + 6, y - 1, x1 - 6, y + 15, PAL_NAVY4);
      r_text(x0 + 10, y, ">", PAL_YELLOW6);
      r_text(x0 + 24, y, pause_items[i], PAL_WHITE7);
    } else {
      r_text(x0 + 24, y, pause_items[i], PAL_CONCRETE4);
    }
  }
}

/* --- map ------------------------------------------------------------------ */

void ui_draw_map(void)
{
  int scale = 2;
  int ox = (SCREEN_W - CITY_W * scale) / 2;
  int oy = 22;
  int tx, ty;

  r_clear(PAL_VOID1);
  r_text_center(2, S_MAP_TITLE, PAL_GOLD5);

  r_box(ox - 2, oy - 2, ox + CITY_W * scale + 1, oy + CITY_H * scale + 1,
        PAL_VOID1, PAL_CONCRETE4, PAL_VOID0);

  for (ty = 0; ty < CITY_H; ty++) {
    for (tx = 0; tx < CITY_W; tx++) {
      int t = map_tile(tx, ty);
      unsigned char c;
      if (g_tile_flags[t] & TF_WATER)
        c = PAL_WATER5;
      else if (g_tile_flags[t] & TF_ROOF)
        c = PAL_TEAL3;
      else if (t == T_GRASS || t == T_GRASS2)
        c = PAL_GREEN5;
      else if (t == T_SAND)
        c = PAL_SAND5;
      else if (t == T_ASPHALT || t == T_ASPHALT_H || t == T_ASPHALT_V ||
               t == T_CROSSWALK || t == T_MANHOLE)
        c = PAL_ASPHALT5;
      else
        c = PAL_CONCRETE5;
      r_rect(ox + tx * scale, oy + ty * scale,
             ox + tx * scale + scale - 1, oy + ty * scale + scale - 1, c);
    }
  }

  /* blips */
  {
    int mx, my, i;
    if (mission_marker_pos(&mx, &my)) {
      if ((g_frames >> 3) & 1) {
        r_rect(ox + (mx >> 4) * scale - 2, oy + (my >> 4) * scale - 2,
               ox + (mx >> 4) * scale + 2, oy + (my >> 4) * scale + 2,
               PAL_YELLOW7);
      }
    }
    for (i = 0; i < MAX_CARS; i++) {
      if (g_cars[i].active && g_cars[i].is_cop)
        r_rect(ox + (FI(g_cars[i].x) >> 4) * scale,
               oy + (FI(g_cars[i].y) >> 4) * scale,
               ox + (FI(g_cars[i].x) >> 4) * scale + 1,
               oy + (FI(g_cars[i].y) >> 4) * scale + 1, PAL_BLUE6);
    }
  }

  /* the player: a white crosshair so it survives the noise */
  {
    int px = ox + (FI(g_pl.x) >> 4) * scale;
    int py = oy + (FI(g_pl.y) >> 4) * scale;
    r_hline(px - 3, px + 3, py, PAL_WHITE7);
    r_vline(px, py - 3, py + 3, PAL_WHITE7);
    r_pixel(px, py, PAL_RED7);
  }

  r_text_center(196, S_MAP_HINT, PAL_CONCRETE4);
}

/* --- dialogue ------------------------------------------------------------- */

/* Wrapped text with a character budget, so the typewriter can cut a line in
   the middle of a word without the layout jumping around. */
static void text_wrap_type(int x, int y, int maxw, const char *s,
                           unsigned char c, int lh, int budget)
{
  char line[128];
  int n = 0, cx = 0, used = 0;
  const char *p = s;

  while (*p) {
    const char *word = p;
    int wlen = 0;
    while (*p && *p != ' ') { p++; wlen++; }
    if (cx && (cx + 1 + wlen) * 8 > maxw) {
      line[n] = 0;
      r_text(x, y, line, c);
      y += lh;
      n = 0;
      cx = 0;
    }
    if (n && cx) {
      line[n++] = ' ';
      cx++;
      used++;
    }
    if (used >= budget) {
      line[n] = 0;
      r_text(x, y, line, c);
      return;
    }
    memcpy(line + n, word, (size_t)wlen);
    n += wlen;
    cx += wlen;
    used += wlen;
    if (*p == ' ')
      p++;
  }
  line[n] = 0;
  r_text(x, y, line, c);
}

void dialog_draw(void)
{
  int y0 = VIEW_H - 72;
  int x0 = 8, x1 = SCREEN_W - 9;
  const char *line = dialog_line(g_dialog_i);
  int portrait = SPR_HUMAN_BASE + PED_LUCIA * HUMAN_DIRS;
  int budget;

  if (!line)
    return;

  r_box_double(x0, y0, x1, VIEW_H - 8, PAL_VOID1, PAL_CONCRETE5, PAL_VOID0);

  /* portrait: the same 12x16 sprite as the world, at 2x, because a dialogue
     box deserves at least one face */
  r_rect(x0 + 6, y0 + 8, x0 + 6 + 23, y0 + 8 + 31, PAL_NAVY3);
  r_sprite_scale(portrait, x0 + 6, y0 + 8, 2);

  budget = g_dialog_chars / 8;
  text_wrap_type(x0 + 36, y0 + 10, x1 - x0 - 44, line, PAL_WHITE6, 12,
                 budget);

  if ((g_frames >> 4) & 1)
    r_text_right(x1 - 8, VIEW_H - 24, S_PRESS_ENTER, PAL_YELLOW5);
}

/* --- wasted / busted ------------------------------------------------------ */

/* Centred text that wraps instead of running off both edges. */
static void text_center_wrap(int y, int maxw, const char *s, unsigned char c,
                             int lh)
{
  char line[128];
  int n = 0, cx = 0;
  const char *p = s;

  while (*p) {
    const char *word = p;
    int wlen = 0;
    while (*p && *p != ' ') { p++; wlen++; }
    if (cx && (cx + 1 + wlen) * 8 > maxw) {
      line[n] = 0;
      r_text_center(y, line, c);
      y += lh;
      n = 0;
      cx = 0;
    }
    if (n && cx) {
      line[n++] = ' ';
      cx++;
    }
    memcpy(line + n, word, (size_t)wlen);
    n += wlen;
    cx += wlen;
    if (*p == ' ')
      p++;
  }
  line[n] = 0;
  r_text_center(y, line, c);
}

void ui_draw_wasted(int busted)
{
  static const char *lines[] = { S_WASTED_1, S_WASTED_2, S_WASTED_3, S_WASTED_4 };
  static const char *blines[] = { S_BUSTED_1, S_BUSTED_2, S_BUSTED_3 };
  const char *line;
  int i = (int)((g_frames / 60) % 4);
  int j = (int)((g_frames / 60) % 3);

  r_dither_rect(0, 0, SCREEN_W - 1, VIEW_H - 1, PAL_VOID1, PAL_VOID0);
  r_rect(0, 48, SCREEN_W - 1, 104, PAL_VOID0);

  if (busted) {
    ui_big_text(SCREEN_W / 2 - (10 * 8 * 2) / 2, 54, S_BUSTED, 2,
                PAL_BLUE_BASE, PAL_BLUE_N);
    line = blines[j];
  } else {
    ui_big_text(SCREEN_W / 2 - (10 * 8 * 2) / 2, 54, S_WASTED, 2,
                PAL_RED_BASE, PAL_RED_N);
    line = lines[i];
  }
  text_center_wrap(88, SCREEN_W - 32, line, PAL_CONCRETE4, 12);
  if ((g_frames >> 4) & 1)
    r_text_center(120, S_DEATH_HINT, PAL_WHITE5);
}

/* --- mission complete ----------------------------------------------------- */

void ui_draw_mission_done(void)
{
  const Mission *m = mission_get(g_last_mission);
  char buf[40];
  int y0 = 50;

  r_dither_rect(0, 0, SCREEN_W - 1, VIEW_H - 1, PAL_VOID1, PAL_VOID0);
  r_box_double(40, y0, SCREEN_W - 41, y0 + 76, PAL_VOID1, PAL_CONCRETE5,
               PAL_VOID0);

  if (g_mission_failed) {
    r_text_center(y0 + 8, S_MISSION_FAIL, PAL_RED6);
  } else {
    r_text_center(y0 + 8, S_MISSION_DONE, PAL_GREEN6);
    if (m) {
      r_text_center(y0 + 26, m->name, PAL_WHITE6);
      sprintf(buf, "%s %lu", S_MISSION_REWARD, g_reward_pending);
      r_text_center(y0 + 42, buf, PAL_GOLD6);
    }
  }
  if ((g_frames >> 4) & 1)
    r_text_center(y0 + 60, S_MISSION_NEXT, PAL_CONCRETE4);
}

/* --- controls / about / radio --------------------------------------------- */

void ui_draw_controls(void)
{
  static const char *lines[] = {
    S_CTRL_1, S_CTRL_2, S_CTRL_3, S_CTRL_4, S_CTRL_5, S_CTRL_6, S_CTRL_7
  };
  int i;
  r_clear(PAL_VOID1);
  r_text_center(6, S_CTRL_TITLE, PAL_GOLD5);
  for (i = 0; i < 7; i++)
    r_text(16, 26 + i * 16, lines[i], PAL_CONCRETE5);
  r_text_wrap(16, 142, SCREEN_W - 32, S_CTRL_NOTE, PAL_TEAL4, 12);
  if ((g_frames >> 4) & 1)
    r_text_center(180, S_CTRL_8, PAL_WHITE5);
}

void ui_draw_about(void)
{
  static const char *lines[] = {
    S_ABOUT_1, S_ABOUT_2, S_ABOUT_3, S_ABOUT_4, S_ABOUT_5, S_ABOUT_6,
    S_ABOUT_7, S_ABOUT_8, S_ABOUT_9
  };
  int i;
  r_clear(PAL_VOID1);
  r_text_center(6, S_ABOUT_TITLE, PAL_GOLD5);
  for (i = 0; i < 9; i++)
    r_text_center(30 + i * 14, lines[i], PAL_CONCRETE4);
  if ((g_frames >> 4) & 1)
    r_text_center(180, S_ABOUT_HINT, PAL_WHITE5);
}

void ui_draw_radio_menu(void)
{
  int i;
  int n = N_SONGS + 1;                 /* + the "off" entry */
  int list_h = n * 15;
  int top = 26;

  r_clear(PAL_VOID1);
  r_text_center(6, S_RADIO_TITLE, PAL_GOLD5);

  for (i = 0; i < n; i++) {
    int sel = i - 1;                   /* -1 is "off" */
    int y = top + i * 15;
    const char *name = (sel < 0) ? S_RADIO_OFF : g_songs[sel].title;
    if (sel == g_radio_sel) {
      r_rect(12, y - 1, SCREEN_W - 13, y + 13, PAL_NAVY4);
      r_text(16, y, ">", PAL_YELLOW6);
      r_text(28, y, name, PAL_WHITE7);
    } else {
      r_text(28, y, name, PAL_CONCRETE4);
    }
  }

  /* subtitle of the highlighted station, wrapped to the box */
  {
    int y = top + list_h + 8;
    const char *sub;
    if (g_radio_sel >= 0 && g_radio_sel < N_SONGS)
      sub = g_songs[g_radio_sel].subtitle;
    else
      sub = S_RADIO_HINT;
    if (sub && *sub)
      r_text_wrap(16, y, SCREEN_W - 32, sub, PAL_TEAL4, 12);
    else
      r_text(16, y, S_RADIO_NOW, PAL_TEAL4);
  }

  r_text_center(VIEW_H - 12, S_RADIO_HINT, PAL_CONCRETE3);
}

void ui_draw_credits(void)
{
  static const char *lines[] = {
    S_CREDITS_1, "", S_CREDITS_2, S_CREDITS_3, "", S_CREDITS_4,
    S_CREDITS_5, "", S_CREDITS_6
  };
  int n = 9;
  int i;
  int total = n * 14;
  /* Start with the first line already on screen, then scroll up slowly. A
     credits roll that begins with 3 seconds of empty gradient looks broken. */
  int scroll = (int)((g_frames / 3) % (unsigned long)(total + SCREEN_H));

  r_clear(PAL_VOID1);
  r_gradient_v(0, SCREEN_H, PAL_PURPLE_BASE, PAL_PURPLE_N, 0, 5);
  for (i = 0; i < n; i++) {
    int y = 20 + i * 14 - scroll;
    if (y < -16 || y > SCREEN_H)
      continue;
    if (i == 0)
      ui_big_text(SCREEN_W / 2 - (9 * 8) / 2, y, lines[i], 1, PAL_GOLD_BASE,
                  PAL_GOLD_N);
    else
      r_text_center(y, lines[i], PAL_CONCRETE5);
  }
  r_text_center(SCREEN_H - 14, S_ABOUT_HINT, PAL_CONCRETE3);
}

/* --- save / load ---------------------------------------------------------- */

#define SAVE_MAGIC 0x47544136UL
#define SAVE_VER   2

typedef struct {
  unsigned long magic;
  unsigned short ver;
  unsigned short radio;
  long money;
  unsigned char health, armour, ammo, wanted, on_foot;
  unsigned char mission, obj, marker_on;
  unsigned char dir;
  int x, y;
  unsigned long clock;
} SaveBlob;

int ui_save(void)
{
  FILE *f;
  SaveBlob b;
  memset(&b, 0, sizeof(b));
  b.magic = SAVE_MAGIC;
  b.ver = SAVE_VER;
  b.radio = (unsigned short)(g_radio + 1);
  b.money = (long)g_pl.money;
  b.health = g_pl.health;
  b.armour = g_pl.armour;
  b.ammo = g_pl.ammo;
  b.wanted = g_pl.wanted;
  b.on_foot = g_pl.on_foot;
  b.mission = (unsigned char)(g_mission + 1);
  b.obj = (unsigned char)g_obj;
  b.marker_on = (unsigned char)g_marker_on;
  b.dir = g_pl.dir;
  b.x = g_pl.x;
  b.y = g_pl.y;
  b.clock = g_clock;

  f = fopen("GTA6.SAV", "wb");
  if (!f)
    return 0;
  if (fwrite(&b, sizeof(b), 1, f) != 1) {
    fclose(f);
    return 0;
  }
  fclose(f);
  return 1;
}

int ui_load(void)
{
  FILE *f;
  SaveBlob b;
  f = fopen("GTA6.SAV", "rb");
  if (!f)
    return 0;
  if (fread(&b, sizeof(b), 1, f) != 1 || b.magic != SAVE_MAGIC ||
      b.ver != SAVE_VER) {
    fclose(f);
    return 0;
  }
  fclose(f);

  world_init();
  mission_reset();
  g_radio = (int)b.radio - 1;
  snd_radio(g_radio);
  g_pl.money = (unsigned long)b.money;
  g_pl.health = b.health;
  g_pl.armour = b.armour;
  g_pl.ammo = b.ammo;
  g_pl.wanted = b.wanted;
  g_pl.on_foot = b.on_foot ? 1 : 0;
  g_pl.car = -1;
  g_pl.dir = b.dir;
  g_pl.x = b.x;
  g_pl.y = b.y;
  g_clock = b.clock;
  g_mission = (int)b.mission - 1;
  g_obj = b.obj;
  g_marker_on = b.marker_on;
  g_mission_active = 0;
  if (g_mission < 0) {
    g_mission = -1;
    g_marker_on = 1;
  }
  return 1;
}
