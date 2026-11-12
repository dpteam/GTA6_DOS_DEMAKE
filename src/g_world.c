/* ===========================================================================
   GTA VI: DOS CITY -- g_world.c
   Globals, the RNG, map queries, spawning, the camera, and the renderer.

   The city is drawn in three passes because buildings need to be *above* the
   street: first every ground tile, then each roof with a short dark extrusion
   below it, then a light edge on the north and west sides. That single trick
   is the entire difference between "a tilemap" and "a city seen from a
   helicopter".
   =========================================================================== */
#include <string.h>

#include "g.h"

/* --- globals -------------------------------------------------------------- */

Ped   g_peds[MAX_PEDS];
Car   g_cars[MAX_CARS];
Bullet g_bullets[MAX_BULLETS];
Pickup g_pickups[MAX_PICKUPS];
Fx    g_fx[MAX_FX];
Decal g_decals[MAX_DECALS];
Player g_pl;

int g_state = ST_TITLE;
int g_cam_x, g_cam_y;
unsigned long g_clock = 19 * 60;
unsigned long g_frames;
int g_radio = 0;
int g_mission = -1;
int g_obj;
int g_obj_count;
long g_obj_timer = -1;
int g_mission_active;
int g_mission_failed;
unsigned long g_reward_pending;
int g_marker_poi = POI_CLUB;
int g_marker_on;
int g_last_mission = -1;
int g_dialog_i, g_dialog_n, g_dialog_chars;
int g_menu_sel;
int g_radio_sel;
int g_autopilot;
int g_show_fps;
int g_last_death;
int g_fade = 16;
int g_debug;
int g_quit;

/* --- rng ------------------------------------------------------------------ */

static unsigned long g_rng = 0x5EED1234UL;

unsigned int gta_rand(void)
{
  g_rng ^= g_rng << 13;
  g_rng ^= g_rng >> 17;
  g_rng ^= g_rng << 5;
  return (unsigned int)(g_rng & 0x7FFFFFFFUL);
}

void gta_srand(unsigned int seed)
{
  g_rng = seed | 1;
}

static int rnd(int n)
{
  return (int)(gta_rand() % (unsigned int)n);
}

/* --- map ------------------------------------------------------------------ */

int map_tile(int tx, int ty)
{
  if ((unsigned)tx >= (unsigned)CITY_W || (unsigned)ty >= (unsigned)CITY_H)
    return T_VOID;
  return g_map[ty * CITY_W + tx];
}

int tile_flags(int tx, int ty)
{
  int t = map_tile(tx, ty);
  if ((unsigned)t >= NTILES)
    return 0;
  return g_tile_flags[t];
}

int solid_px(int x, int y)
{
  int t = map_tile(x >> 4, y >> 4);
  if ((unsigned)t >= NTILES)
    return 1;
  return (g_tile_flags[t] & TF_SOLID) != 0;
}

int water_px(int x, int y)
{
  int t = map_tile(x >> 4, y >> 4);
  if ((unsigned)t >= NTILES)
    return 0;
  return (g_tile_flags[t] & TF_WATER) != 0;
}

int in_view(int x, int y, int pad)
{
  return x >= g_cam_x - pad && x < g_cam_x + SCREEN_W + pad &&
         y >= g_cam_y - pad && y < g_cam_y + VIEW_H + pad;
}

/* angle 0 = east, increasing clockwise (screen y points down) */
int gta_cos(int a)
{
  static const signed char C[16] = {
    16, 15, 11, 6, 0, -6, -11, -15, -16, -15, -11, -6, 0, 6, 11, 15
  };
  return C[a & 15];
}

int gta_sin(int a)
{
  static const signed char S[16] = {
    0, 6, 11, 15, 16, 15, 11, 6, 0, -6, -11, -15, -16, -15, -11, -6
  };
  return S[a & 15];
}

/* --- entity slots --------------------------------------------------------- */

int find_free_car(void)
{
  int i;
  for (i = 0; i < MAX_CARS; i++)
    if (!g_cars[i].active)
      return i;
  return -1;
}

int find_free_ped(void)
{
  int i;
  for (i = 0; i < MAX_PEDS; i++)
    if (!g_peds[i].active)
      return i;
  return -1;
}

int car_at(int x, int y, int radius)
{
  int i, best = -1;
  long bestd = (long)radius * radius;
  for (i = 0; i < MAX_CARS; i++) {
    long dx, dy, d;
    if (!g_cars[i].active || g_cars[i].is_cop)
      continue;
    dx = FI(g_cars[i].x) - x;
    dy = FI(g_cars[i].y) - y;
    d = dx * dx + dy * dy;
    if (d < bestd) {
      bestd = d;
      best = i;
    }
  }
  return best;
}

/* --- spawning ------------------------------------------------------------- */

void spawn_peds(void)
{
  int i;
  for (i = 0; i < MAX_PEDS; i++)
    g_peds[i].active = 0;
  for (i = 0; i < N_PED_SPAWNS && i < MAX_PEDS; i++) {
    Ped *p = &g_peds[i];
    p->active = 1;
    p->kind = (unsigned char)(PED_CIV_A + (i % 3));
    p->x = FX(g_ped_spawns[i].x);
    p->y = FX(g_ped_spawns[i].y);
    p->dir = (unsigned char)rnd(4);
    p->health = 10;
    p->state = 1;
    p->timer = (unsigned char)(20 + rnd(60));
    p->anim = (unsigned char)rnd(4);
  }
}

void spawn_cars(void)
{
  int i;
  for (i = 0; i < MAX_CARS; i++)
    g_cars[i].active = 0;
  for (i = 0; i < N_CAR_SPAWNS && i < MAX_CARS; i++) {
    Car *c = &g_cars[i];
    c->active = 1;
    c->model = g_car_spawns[i].model;
    if (c->model >= CAR_COP)
      c->model = CAR_SEDAN_BLUE;
    c->angle = g_car_spawns[i].angle & 15;
    c->x = FX(g_car_spawns[i].x);
    c->y = FX(g_car_spawns[i].y);
    c->health = 60;
    c->ai = (g_car_spawns[i].angle == 192 && (i % 7) == 0) ? 0 : 1;
    c->speed = FX(1) / 8;
    c->tx = c->x;
    c->ty = c->y;
  }
}

void spawn_pickups(void)
{
  int i;
  for (i = 0; i < MAX_PICKUPS; i++)
    g_pickups[i].active = 0;
  for (i = 0; i < N_PICKUPS && i < MAX_PICKUPS; i++) {
    Pickup *p = &g_pickups[i];
    p->active = 1;
    p->kind = g_pickup_spawns[i].kind;
    p->x = g_pickup_spawns[i].x;
    p->y = g_pickup_spawns[i].y;
    p->bob = (unsigned char)rnd(64);
  }
}

void world_init(void)
{
  memset(g_peds, 0, sizeof(g_peds));
  memset(g_cars, 0, sizeof(g_cars));
  memset(g_bullets, 0, sizeof(g_bullets));
  memset(g_pickups, 0, sizeof(g_pickups));
  memset(g_fx, 0, sizeof(g_fx));
  memset(g_decals, 0, sizeof(g_decals));
  spawn_peds();
  spawn_cars();
  spawn_pickups();
  g_clock = 19 * 60;
  g_frames = 0;
}

/* --- camera --------------------------------------------------------------- */

void world_to_screen(int wx, int wy, int *sx, int *sy)
{
  *sx = wx - g_cam_x;
  *sy = wy - g_cam_y;
}

void cam_update(void)
{
  int cx = FI(g_pl.x) - SCREEN_W / 2;
  int cy = FI(g_pl.y) - VIEW_H / 2;

  /* Lead the camera slightly in the direction of travel: 8 pixels, no more.
     Any more and the player spends the game looking at the edge of the
     screen instead of at the road. */
  cx += FI(g_pl.vx) * 6;
  cy += FI(g_pl.vy) * 6;

  if (cx < 0) cx = 0;
  if (cy < 0) cy = 0;
  if (cx > WORLD_W - SCREEN_W) cx = WORLD_W - SCREEN_W;
  if (cy > WORLD_H - VIEW_H) cy = WORLD_H - VIEW_H;
  g_cam_x = cx;
  g_cam_y = cy;
}

/* --- fx / decals ---------------------------------------------------------- */

void add_fx(int kind, int x, int y, int vx, int vy)
{
  int i;
  for (i = 0; i < MAX_FX; i++) {
    if (!g_fx[i].active) {
      g_fx[i].active = 1;
      g_fx[i].kind = (unsigned char)kind;
      g_fx[i].life = (unsigned char)(kind == 3 ? 14 : 8);
      g_fx[i].x = x;
      g_fx[i].y = y;
      g_fx[i].vx = vx;
      g_fx[i].vy = vy;
      return;
    }
  }
}

void add_decal(int spr, int x, int y)
{
  int i;
  for (i = 0; i < MAX_DECALS; i++) {
    if (!g_decals[i].active) {
      g_decals[i].active = 1;
      g_decals[i].spr = (unsigned char)spr;
      g_decals[i].x = x;
      g_decals[i].y = y;
      g_decals[i].life = 200;
      return;
    }
  }
}

/* --- render list ---------------------------------------------------------- */

#define MAX_RITEMS 160

typedef struct {
  int y;
  unsigned char type;      /* 0 ped, 1 car, 2 player, 3 prop, 4 pickup, 5 fx */
  unsigned char idx;
} RItem;

static RItem g_ritems[MAX_RITEMS];
static int g_ritem_n;

static void ritem_add(int y, int type, int idx)
{
  int i;
  if (g_ritem_n >= MAX_RITEMS)
    return;
  /* insertion sort: the list is short and mostly ordered already */
  i = g_ritem_n++;
  while (i > 0 && g_ritems[i - 1].y > y) {
    g_ritems[i] = g_ritems[i - 1];
    i--;
  }
  g_ritems[i].y = y;
  g_ritems[i].type = (unsigned char)type;
  g_ritems[i].idx = (unsigned char)idx;
}

/* --- world render --------------------------------------------------------- */

void render_world(void)
{
  int tx, ty;
  int tx0 = g_cam_x >> 4;
  int ty0 = g_cam_y >> 4;
  int tx1 = (g_cam_x + SCREEN_W) >> 4;
  int ty1 = (g_cam_y + VIEW_H) >> 4;
  int sx, sy;

  if (tx0 < 0) tx0 = 0;
  if (ty0 < 0) ty0 = 0;
  if (tx1 >= CITY_W) tx1 = CITY_W - 1;
  if (ty1 >= CITY_H) ty1 = CITY_H - 1;

  /* pass 1: ground */
  for (ty = ty0; ty <= ty1; ty++) {
    sy = ty * 16 - g_cam_y;
    for (tx = tx0; tx <= tx1; tx++) {
      int t = g_map[ty * CITY_W + tx];
      if ((unsigned)t < NTILES && (g_tile_flags[t] & TF_ROOF))
        continue;
      r_tile(t, tx * 16 - g_cam_x, sy);
    }
  }

  /* pass 2: buildings, top row first so a nearer block covers a farther wall */
  for (ty = ty0; ty <= ty1; ty++) {
    sy = ty * 16 - g_cam_y;
    for (tx = tx0; tx <= tx1; tx++) {
      int t = g_map[ty * CITY_W + tx];
      int below, left, right, up;
      sx = tx * 16 - g_cam_x;
      if ((unsigned)t >= NTILES || !(g_tile_flags[t] & TF_ROOF))
        continue;
      below = tile_flags(tx, ty + 1) & TF_ROOF;
      left = tile_flags(tx - 1, ty) & TF_ROOF;
      right = tile_flags(tx + 1, ty) & TF_ROOF;
      up = tile_flags(tx, ty - 1) & TF_ROOF;

      /* the wall: the same texture, six pixels lower and much darker */
      if (!below) {
        int i;
        for (i = 0; i < 6; i++)
          r_tile_dim(t, sx, sy + 6, 5);
      }
      r_tile(t, sx, sy);
      if (!up)
        r_hline(sx, sx + 15, sy, PAL_CONCRETE7);
      if (!left)
        r_vline(sx, sy, sy + 15, PAL_CONCRETE6);
      if (!right)
        r_vline(sx + 15, sy, sy + 15, PAL_CONCRETE2);
      if (!below) {
        r_hline(sx, sx + 15, sy + 15, PAL_VOID1);
        r_hline(sx, sx + 15, sy + 21, PAL_VOID1);
      }
    }
  }
}

/* --- entity render -------------------------------------------------------- */

void render_entities(void)
{
  int i;
  int sx, sy;

  g_ritem_n = 0;

  for (i = 0; i < N_PROPS; i++) {
    const Prop *p = &g_props[i];
    if (in_view(p->x, p->y, 40))
      ritem_add(p->y + 20, 3, i);
  }
  for (i = 0; i < MAX_PICKUPS; i++) {
    if (g_pickups[i].active && in_view(g_pickups[i].x, g_pickups[i].y, 24))
      ritem_add(g_pickups[i].y + 8, 4, i);
  }
  for (i = 0; i < MAX_PEDS; i++) {
    if (g_peds[i].active && in_view(FI(g_peds[i].x), FI(g_peds[i].y), 24))
      ritem_add(FI(g_peds[i].y) + 6, 0, i);
  }
  for (i = 0; i < MAX_CARS; i++) {
    if (g_cars[i].active && in_view(FI(g_cars[i].x), FI(g_cars[i].y), 40))
      ritem_add(FI(g_cars[i].y) + 8, 1, i);
  }
  if (g_pl.on_foot)
    ritem_add(FI(g_pl.y) + 6, 2, 0);

  for (i = 0; i < MAX_DECALS; i++) {
    if (g_decals[i].active && in_view(g_decals[i].x, g_decals[i].y, 24)) {
      world_to_screen(g_decals[i].x, g_decals[i].y, &sx, &sy);
      r_sprite(SPR_DECAL_BASE + g_decals[i].spr, sx - 8, sy - 4);
    }
  }

  for (i = 0; i < g_ritem_n; i++) {
    RItem *it = &g_ritems[i];
    switch (it->type) {
      case 0: {
        Ped *p = &g_peds[it->idx];
        int spr;
        if (p->state == 4)
          break;
        spr = SPR_HUMAN_BASE + p->kind * HUMAN_DIRS + (p->dir & 3);
        world_to_screen(FI(p->x), FI(p->y), &sx, &sy);
        if (p->dir == 2)
          r_sprite_flip(spr, sx - 6, sy - 8);
        else
          r_sprite(spr, sx - 6, sy - 8);
        break;
      }
      case 1: {
        Car *c = &g_cars[it->idx];
        int spr = SPR_CAR_BASE + c->model * CAR_DIRS + (c->angle & 15);
        const Sprite *sp = &g_sprites[spr];
        world_to_screen(FI(c->x), FI(c->y), &sx, &sy);
        r_sprite(spr, sx - sp->w / 2, sy - sp->h / 2);
        break;
      }
      case 2: {
        int spr = SPR_HUMAN_BASE + g_pl.dir;
        if (g_pl.hurt_flash & 1)
          r_sprite_dim(spr, FI(g_pl.x) - g_cam_x - 6, FI(g_pl.y) - g_cam_y - 8, 3);
        else if (g_pl.dir == 2)
          r_sprite_flip(spr, FI(g_pl.x) - g_cam_x - 6, FI(g_pl.y) - g_cam_y - 8);
        else
          r_sprite(spr, FI(g_pl.x) - g_cam_x - 6, FI(g_pl.y) - g_cam_y - 8);
        break;
      }
      case 3: {
        const Prop *p = &g_props[it->idx];
        const Sprite *sp = &g_sprites[p->spr];
        world_to_screen(p->x, p->y, &sx, &sy);
        r_sprite(p->spr, sx - sp->w / 2, sy - sp->h / 2);
        break;
      }
      case 4: {
        Pickup *p = &g_pickups[it->idx];
        int bob = ((p->bob >> 3) & 1);
        world_to_screen(p->x, p->y, &sx, &sy);
        r_sprite(SPR_PICK_BASE + p->kind, sx - 5, sy - 5 - bob);
        break;
      }
      default:
        break;
    }
  }

  /* bullets and particles live above everything: they are 2 pixels, nobody
     will complain about the sorting */
  for (i = 0; i < MAX_BULLETS; i++) {
    if (!g_bullets[i].active)
      continue;
    world_to_screen(FI(g_bullets[i].x), FI(g_bullets[i].y), &sx, &sy);
    r_pixel(sx, sy, PAL_YELLOW6);
    r_pixel(sx + 1, sy, PAL_WHITE7);
  }

  for (i = 0; i < MAX_FX; i++) {
    Fx *f = &g_fx[i];
    int r;
    if (!f->active)
      continue;
    world_to_screen(FI(f->x), FI(f->y), &sx, &sy);
    if (f->kind == 3) {
      r = 10 - f->life / 2;
      if (r < 1) r = 1;
      r_hline(sx - r, sx + r, sy, PAL_ORANGE7);
      r_hline(sx - r + 2, sx + r - 2, sy - 1, PAL_YELLOW7);
      r_hline(sx - r + 2, sx + r - 2, sy + 1, PAL_FIRE2);
      r_hline(sx - r + 4, sx + r - 4, sy - 2, PAL_WHITE6);
      r_hline(sx - r + 4, sx + r - 4, sy + 2, PAL_RED4);
    } else if (f->kind == 2) {
      r_pixel(sx, sy, PAL_BLOOD4);
      r_pixel(sx + 1, sy, PAL_BLOOD5);
    } else if (f->kind == 1) {
      r_pixel(sx, sy, PAL_CONCRETE4);
      r_pixel(sx, sy - 1, PAL_CONCRETE2);
    } else {
      r_pixel(sx, sy, PAL_YELLOW7);
      r_pixel(sx + 1, sy + 1, PAL_ORANGE6);
    }
  }
}
