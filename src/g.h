/* ===========================================================================
   GTA VI: DOS CITY -- g.h
   Shared internals. Every module sees this and nothing else.

   Coordinates: world pixels, 1536x1536. Anything that moves is stored in 8.8
   fixed point (FX / FI macros) because a 486 has no FPU worth speaking of and
   a car that snaps to whole pixels looks like it is being pushed.
   =========================================================================== */
#ifndef GTA_G_H
#define GTA_G_H

#include "video.h"
#include "render.h"
#include "sound.h"
#include "assets.h"
#include "tiles.h"
#include "city.h"
#include "music.h"
#include "strings.h"
#include "pal.h"

#define TILE     16
#define WORLD_W  CITY_PX_W
#define WORLD_H  CITY_PX_H
#define HUD_H    (SCREEN_H - VIEW_H)

#define FX(x)    ((int)((x) << 8))
#define FI(v)    ((int)((v) >> 8))

#define MAX_PEDS     96
#define MAX_CARS     128
#define MAX_BULLETS  40
#define MAX_PICKUPS  64
#define MAX_FX       24
#define MAX_DECALS   24

/* ped kinds map to sprite groups */
enum { PED_CIV_A, PED_CIV_B, PED_CIV_C, PED_COP, PED_LUCIA, PED_GANG,
       PED_KINDS };

/* car models, in the order genart.py emits them */
enum { CAR_SEDAN_RED, CAR_SEDAN_BLUE, CAR_SEDAN_WHITE, CAR_SPORTS,
       CAR_TAXI, CAR_VAN, CAR_COP, CAR_MODELS };

/* game states */
enum {
  ST_TITLE, ST_INTRO, ST_PLAY, ST_DIALOG, ST_PAUSE, ST_MAP,
  ST_MISSION_DONE, ST_WASTED, ST_BUSTED, ST_CREDITS, ST_CONTROLS,
  ST_ABOUT, ST_RADIO_MENU
};

/* objective kinds */
enum { OB_GOTO, OB_COLLECT, OB_KILL, OB_WRECK, OB_ESCAPE, OB_END };

/* --- entities ------------------------------------------------------------- */

typedef struct {
  unsigned char active;
  unsigned char kind;
  unsigned char dir;        /* 0 down, 1 up, 2 left, 3 right */
  unsigned char health;
  unsigned char state;      /* 0 idle, 1 walk, 2 flee, 3 chase, 4 dead */
  unsigned char anim;
  unsigned char timer;
  unsigned char hostile;
  int x, y;                 /* 8.8 */
  int vx, vy;
} Ped;

typedef struct {
  unsigned char active;
  unsigned char model;
  unsigned char angle;      /* 0..15, clockwise from east */
  unsigned char dir;        /* road direction for AI: 0 E, 1 S, 2 W, 3 N */
  unsigned char health;
  unsigned char ai;         /* 0 parked, 1 traffic, 2 chase */
  unsigned char is_cop;
  unsigned char target;     /* mission car flag */
  unsigned char honk;
  int x, y;                 /* 8.8 */
  int speed;                /* 8.8 px/frame */
  int tx, ty;               /* AI target, 8.8 */
} Car;

typedef struct {
  unsigned char active;
  unsigned char life;
  int x, y, vx, vy;
} Bullet;

typedef struct {
  unsigned char active;
  unsigned char kind;       /* 0 bag, 1 case, 2 health, 3 armour, 4 ammo */
  unsigned char bob;
  unsigned char mission;
  int x, y;
} Pickup;

typedef struct {
  unsigned char active;
  unsigned char kind;       /* 0 spark, 1 smoke, 2 blood, 3 explosion */
  unsigned char life;
  int x, y, vx, vy;
} Fx;

typedef struct {
  unsigned char active;
  unsigned char spr;
  unsigned char life;
  int x, y;
} Decal;

/* --- player --------------------------------------------------------------- */

typedef struct {
  int x, y;                 /* 8.8 */
  int vx, vy;
  unsigned char dir;
  unsigned char anim;
  unsigned char on_foot;
  int car;                  /* index into g_cars, -1 when on foot */
  unsigned char health;
  unsigned char armour;
  unsigned char ammo;
  unsigned char clip;       /* rounds left in the magazine */
  unsigned long money;
  unsigned char wanted;
  unsigned char wanted_timer;
  unsigned char fire_cd;
  unsigned char hurt_flash;
  unsigned char dead;
} Player;

/* --- missions ------------------------------------------------------------- */

typedef struct {
  unsigned char kind;
  unsigned char poi;
  unsigned char count;
  unsigned short timer;     /* seconds, 0 = no limit */
  const char *text;
} Obj;

typedef struct {
  const char *name;
  const char *d1, *d2, *d3, *d4;
  Obj obj[3];
  unsigned long reward;
} Mission;

#define N_MISSIONS 7

/* --- globals (defined in g_world.c) --------------------------------------- */

extern Ped   g_peds[MAX_PEDS];
extern Car   g_cars[MAX_CARS];
extern Bullet g_bullets[MAX_BULLETS];
extern Pickup g_pickups[MAX_PICKUPS];
extern Fx    g_fx[MAX_FX];
extern Decal g_decals[MAX_DECALS];
extern Player g_pl;

extern int g_state;
extern int g_cam_x, g_cam_y;
extern unsigned long g_clock;        /* game minutes since midnight */
extern unsigned long g_frames;
extern int g_radio;
extern int g_mission;
extern int g_obj;
extern int g_obj_count;
extern long g_obj_timer;             /* ms remaining, -1 = none */
extern int g_mission_active;
extern int g_mission_failed;
extern unsigned long g_reward_pending;
extern int g_marker_poi;             /* POI of the current mission marker */
extern int g_marker_on;
extern int g_last_mission;
extern int g_dialog_i;
extern int g_dialog_n;
extern int g_dialog_chars;
extern int g_dialog_after;
extern int g_menu_sel;
extern int g_radio_sel;
extern int g_autopilot;
extern int g_show_fps;
extern int g_last_death;
extern int g_fade;
extern int g_debug;
extern int g_quit;

/* --- g_world.c ------------------------------------------------------------ */

unsigned int  gta_rand(void);
void          gta_srand(unsigned int seed);
int           gta_cos(int a);          /* a in 0..15, returns -16..16 */
int           gta_sin(int a);
int           map_tile(int tx, int ty);
int           tile_flags(int tx, int ty);
int           solid_px(int x, int y);
int           water_px(int x, int y);
int           in_view(int x, int y, int pad);
void          world_init(void);
void          cam_update(void);
void          world_to_screen(int wx, int wy, int *sx, int *sy);
void          render_world(void);
void          render_entities(void);
void          spawn_peds(void);
void          spawn_cars(void);
void          spawn_pickups(void);
void          add_fx(int kind, int x, int y, int vx, int vy);
void          add_decal(int spr, int x, int y);
int           find_free_car(void);
int           find_free_ped(void);
int           car_at(int x, int y, int radius);

/* --- g_play.c ------------------------------------------------------------- */

extern unsigned char g_key_left, g_key_right, g_key_up, g_key_down;
extern unsigned char g_key_fire, g_key_run, g_fire_pressed;

void update_player(void);
int  body_blocked(int x, int y);
void update_traffic(void);
void update_traffic_collisions(void);
void update_peds(void);
void update_cops(void);
void update_bullets(void);
void update_pickups(void);
void update_fx(void);
void update_wanted(void);
void player_shoot(void);
void enter_car(int idx);
void exit_car(void);
void hurt_player(int amount);
void add_wanted(int amount);
void damage_car(int idx, int amount, int by_player);
void spawn_cop_car(void);
void spawn_cop_ped(void);
void kill_ped(int idx, int by_player);
void respawn_player(int wasted);
int  player_speed_px(void);

/* --- g_mission.c ---------------------------------------------------------- */

/* things that can complete an objective */
enum { MEV_KILL_PED = 1, MEV_WRECK, MEV_COLLECT };
/* what to do when a dialogue box closes */
enum { DA_NONE, DA_START_MISSION, DA_INTRO, DA_EPILOGUE };

void mission_reset(void);
void mission_start(int idx);
void mission_tick(void);
void mission_advance(void);
void mission_fail(void);
void mission_event(int type, int x, int y);
void mission_draw_marker(void);
int  mission_marker_pos(int *x, int *y);
const Mission *mission_get(int idx);
void dialog_start(const char *a, const char *b, const char *c, const char *d);
void dialog_tick(void);
int  dialog_key(int sc);
int  dialog_active(void);
const char *dialog_line(int i);
void dialog_draw(void);
void mission_banner_draw(void);

/* --- g_ui.c --------------------------------------------------------------- */

void ui_draw_hud(void);
void ui_draw_minimap(void);
void ui_draw_title(void);
void ui_draw_menu(void);
void ui_draw_intro(void);
void ui_draw_pause(void);
void ui_draw_map(void);
void ui_draw_wasted(int busted);
void ui_draw_mission_done(void);
void ui_draw_controls(void);
void ui_draw_about(void);
void ui_draw_radio_menu(void);
void ui_draw_credits(void);
void ui_big_text(int x, int y, const char *s, int scale, int top, int bot);
int  ui_save(void);
int  ui_load(void);
void ui_message(const char *text, unsigned long ms);
void ui_tick(void);
void ui_draw_messages(void);

/* --- game.c --------------------------------------------------------------- */

void game_init(void);
void game_tick(void);
void game_render(void);
void game_key(int sc);
int  game_state(void);
void game_set_state(int st);
void game_new(void);
int  game_ap_debug_n(void);
int  game_ap_debug_i(void);
int  game_ap_debug_wx(void);
int  game_ap_debug_wy(void);

#endif
