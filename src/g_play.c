/* ===========================================================================
   GTA VI: DOS CITY -- g_play.c
   Player, cars, pedestrians, cops, bullets, pickups, and the consequences.

   Everything moves in 8.8 fixed point. Cars turn in 16 steps because that is
   how many sprites the generator drew, and nobody has ever needed a 17th
   heading at 320x200.
   =========================================================================== */
#include <string.h>

#include "g.h"

/* angle 0 = east, increasing clockwise (screen y points down) */
static const signed char COS16[16] = {
  16, 15, 11, 6, 0, -6, -11, -15, -16, -15, -11, -6, 0, 6, 11, 15
};
static const signed char SIN16[16] = {
  0, 6, 11, 15, 16, 15, 11, 6, 0, -6, -11, -15, -16, -15, -11, -6
};

/* input, filled by game.c every frame */
unsigned char g_key_left, g_key_right, g_key_up, g_key_down;
unsigned char g_key_fire, g_key_run, g_fire_pressed;

static int rnd(int n)
{
  return (int)(gta_rand() % (unsigned int)n);
}

static int clamp(int v, int lo, int hi)
{
  return v < lo ? lo : (v > hi ? hi : v);
}

/* --- collision ------------------------------------------------------------ */

/* A body is a small box around the feet. Checking the four corners of the
   box rather than the centre means you cannot walk into a wall corner. */
static int blocked(int x, int y, int hw, int hh)
{
  return solid_px(x - hw, y - hh) || solid_px(x + hw, y - hh) ||
         solid_px(x - hw, y + hh) || solid_px(x + hw, y + hh) ||
         solid_px(x, y);
}

/* Exposed so the autopilot can plan around the same body the player uses. */
int body_blocked(int x, int y)
{
  return blocked(x, y, 4, 3);
}

static int wet(int x, int y)
{
  return water_px(x, y);
}

/* --- player --------------------------------------------------------------- */

void hurt_player(int amount)
{
  if (g_pl.dead)
    return;
  if (g_pl.armour) {
    int absorbed = amount / 2;
    if (absorbed > g_pl.armour)
      absorbed = g_pl.armour;
    g_pl.armour = (unsigned char)(g_pl.armour - absorbed);
    amount -= absorbed;
  }
  if (amount <= 0)
    return;
  if ((int)g_pl.health <= amount) {
    g_pl.health = 0;
    g_pl.dead = 1;
  } else {
    g_pl.health = (unsigned char)(g_pl.health - amount);
  }
  g_pl.hurt_flash = 12;
  snd_play(SFX_HIT);
}

void add_wanted(int amount)
{
  int before = g_pl.wanted;
  g_pl.wanted = (unsigned char)clamp(g_pl.wanted + amount, 0, 5);
  g_pl.wanted_timer = 0;
  if (g_pl.wanted > before)
    snd_play(SFX_WANTED);
}

void player_shoot(void)
{
  int i, dx, dy, sx, sy;
  if (g_pl.fire_cd)
    return;
  if (g_pl.ammo == 0) {
    snd_play(SFX_UI);
    ui_message(S_NO_AMMO, 1200);
    return;
  }
  g_pl.ammo--;
  g_pl.fire_cd = 9;
  for (i = 0; i < MAX_BULLETS; i++) {
    if (g_bullets[i].active)
      continue;
    switch (g_pl.dir) {
      case 0: dx = 0; dy = 1; break;
      case 1: dx = 0; dy = -1; break;
      case 2: dx = -1; dy = 0; break;
      default: dx = 1; dy = 0; break;
    }
    g_bullets[i].active = 1;
    g_bullets[i].life = 34;
    g_bullets[i].x = g_pl.x;
    g_bullets[i].y = g_pl.y;
    g_bullets[i].vx = dx * FX(7);
    g_bullets[i].vy = dy * FX(7);
    sx = FI(g_pl.x) + dx * 6;
    sy = FI(g_pl.y) + dy * 6;
    add_fx(0, FX(sx), FX(sy), 0, 0);
    break;
  }
  snd_play(SFX_PISTOL);
  /* shooting in public is rude, and this city has opinions about rude */
  if (g_pl.wanted == 0 && rnd(3) == 0)
    add_wanted(1);
}

void enter_car(int idx)
{
  Car *c;
  if (idx < 0 || idx >= MAX_CARS)
    return;
  c = &g_cars[idx];
  if (!c->active)
    return;
  if (c->ai == 1 && !c->is_cop) {
    add_wanted(1);
    ui_message(S_WANTED_UP, 1400);
  }
  c->ai = 0;
  g_pl.car = idx;
  g_pl.on_foot = 0;
  g_pl.x = c->x;
  g_pl.y = c->y;
  snd_play(SFX_CAR_ENTER);
}

void exit_car(void)
{
  int idx = g_pl.car;
  Car *c;
  int x, y;
  if (idx < 0)
    return;
  c = &g_cars[idx];
  x = FI(c->x);
  y = FI(c->y);
  c->speed = 0;
  g_pl.car = -1;
  g_pl.on_foot = 1;
  /* step out to whichever side is free; if both are walls, stay in the car
     rather than teleporting into a building */
  if (!blocked(x - 14, y, 4, 4)) {
    x -= 14;
  } else if (!blocked(x + 14, y, 4, 4)) {
    x += 14;
  } else {
    g_pl.car = idx;
    g_pl.on_foot = 0;
    return;
  }
  g_pl.x = FX(x);
  g_pl.y = FX(y);
  g_pl.vx = g_pl.vy = 0;
  snd_play(SFX_CAR_EXIT);
}

static void player_on_foot(void)
{
  int speed = g_key_run ? FX(3) : FX(2) - FX(1) / 2;
  int dx = 0, dy = 0;
  int nx, ny;

  if (g_key_left) dx = -1;
  if (g_key_right) dx = 1;
  if (g_key_up) dy = -1;
  if (g_key_down) dy = 1;

  if (dx && dy) {
    speed = speed * 3 / 4;      /* diagonals are not a free lunch */
  }

  /* facing: the axis with the newest input wins, otherwise keep it */
  if (dx < 0) g_pl.dir = 2;
  else if (dx > 0) g_pl.dir = 3;
  else if (dy < 0) g_pl.dir = 1;
  else if (dy > 0) g_pl.dir = 0;

  g_pl.vx = dx * speed;
  g_pl.vy = dy * speed;

  if (dx || dy)
    g_pl.anim++;

  nx = FI(g_pl.x) + FI(g_pl.vx);
  if (!blocked(nx, FI(g_pl.y), 4, 3))
    g_pl.x += g_pl.vx;
  else
    g_pl.vx = 0;

  ny = FI(g_pl.y) + FI(g_pl.vy);
  if (!blocked(FI(g_pl.x), ny, 4, 3))
    g_pl.y += g_pl.vy;
  else
    g_pl.vy = 0;

  if (wet(FI(g_pl.x), FI(g_pl.y))) {
    g_pl.health = 0;
    g_pl.dead = 1;
    g_last_death = 0;
  }

  /* Holding fire repeats, limited by fire_cd. A pistol that requires nine
     separate taps to kill a car is a pistol nobody uses. */
  if (g_key_fire)
    player_shoot();
}

static void player_in_car(void)
{
  Car *c = &g_cars[g_pl.car];
  int accel = FX(1) / 12;
  int maxfwd = FX(3) - FX(1) / 4;
  int maxrev = -FX(1);
  int nx, ny;

  if (g_key_up)
    c->speed += accel;
  else if (g_key_down)
    c->speed -= accel;
  else
    c->speed = c->speed * 15 / 16;      /* coasting */

  if (g_key_fire)
    c->speed = c->speed * 7 / 8;        /* handbrake */

  if (c->speed > maxfwd) c->speed = maxfwd;
  if (c->speed < maxrev) c->speed = maxrev;

  /* Steering authority scales with speed: a parked car does not rotate. */
  if (g_key_left || g_key_right) {
    int sp = c->speed < 0 ? -c->speed : c->speed;
    if (sp > FX(1) / 4) {
      int turn = sp > FX(2) ? 3 : 2;
      if (g_key_left)
        c->angle = (unsigned char)((c->angle + 16 - turn) & 15);
      else
        c->angle = (unsigned char)((c->angle + turn) & 15);
      if (sp > FX(2) && (g_frames & 15) == 0)
        snd_play(SFX_SKID);
    }
  }

  nx = FI(c->x) + (c->speed * COS16[c->angle]) / 16;
  ny = FI(c->y) + (c->speed * SIN16[c->angle]) / 16;

  if (!blocked(nx, FI(c->y), 7, 5)) {
    c->x = FX(nx);
  } else {
    /* a wall is a wall: stop, damage, and hope the player learns */
    if (c->speed > FX(1) || c->speed < -FX(1)) {
      damage_car(g_pl.car, 8, 0);
      snd_play(SFX_CRASH);
      add_fx(1, c->x, c->y, 0, 0);
      hurt_player(2);
    }
    c->speed = -c->speed / 3;
  }
  if (!blocked(FI(c->x), ny, 7, 5)) {
    c->y = FX(ny);
  } else {
    if (c->speed > FX(1) || c->speed < -FX(1)) {
      damage_car(g_pl.car, 8, 0);
      snd_play(SFX_CRASH);
      add_fx(1, c->x, c->y, 0, 0);
      hurt_player(2);
    }
    c->speed = -c->speed / 3;
  }

  if (wet(FI(c->x), FI(c->y))) {
    g_pl.health = 0;
    g_pl.dead = 1;
    g_last_death = 0;
  }

  g_pl.x = c->x;
  g_pl.y = c->y;
  g_pl.vx = (c->speed * COS16[c->angle]) / 16;
  g_pl.vy = (c->speed * SIN16[c->angle]) / 16;
}

int player_speed_px(void)
{
  int v;
  if (g_pl.on_foot)
    return 0;
  v = g_cars[g_pl.car].speed;
  if (v < 0)
    v = -v;
  v = FI(v) * 40;
  return clamp(v, 0, 255);
}

void update_player(void)
{
  if (g_pl.dead)
    return;
  if (g_pl.fire_cd)
    g_pl.fire_cd--;
  if (g_pl.hurt_flash)
    g_pl.hurt_flash--;

  if (g_pl.on_foot)
    player_on_foot();
  else
    player_in_car();

  if (FI(g_pl.x) < 8) g_pl.x = FX(8);
  if (FI(g_pl.y) < 8) g_pl.y = FX(8);
  if (FI(g_pl.x) > WORLD_W - 8) g_pl.x = FX(WORLD_W - 8);
  if (FI(g_pl.y) > WORLD_H - 8) g_pl.y = FX(WORLD_H - 8);
}

/* --- cars ----------------------------------------------------------------- */

void damage_car(int idx, int amount, int by_player)
{
  Car *c = &g_cars[idx];
  if (!c->active)
    return;
  if (c->health <= amount) {
    c->health = 0;
    add_fx(3, c->x, c->y, 0, 0);
    add_fx(3, c->x + FX(8), c->y - FX(6), 0, 0);
    snd_play(SFX_CRASH);
    if (c->is_cop) {
      if (by_player)
        add_wanted(2);
    } else if (c->target) {
      /* mission car: leaving it in the street is the point */
      c->active = 0;
      if (g_pl.car == idx) {
        g_pl.car = -1;
        g_pl.on_foot = 1;
        hurt_player(20);
      }
      mission_event(MEV_WRECK, FI(c->x), FI(c->y));
      return;
    }
    if (g_pl.car == idx) {
      /* your own car exploded with you in it. Rude, but traditional. */
      g_pl.car = -1;
      g_pl.on_foot = 1;
      hurt_player(45);
    }
    c->active = 0;
  } else {
    c->health = (unsigned char)(c->health - amount);
    if (by_player && c->is_cop)
      add_wanted(1);
  }
}

static void ai_set_target(Car *c)
{
  int ix = FI(c->x) / 256;
  int iy = FI(c->y) / 256;
  int nx = ix, ny = iy;
  int tries = 0;

  for (;;) {
    switch (c->dir) {
      case 0: nx = ix + 1; break;
      case 1: ny = iy + 1; break;
      case 2: nx = ix - 1; break;
      default: ny = iy - 1; break;
    }
    if (nx >= 0 && nx < 6 && ny >= 0 && ny < 6)
      break;
    /* dead end at the edge of the map: turn around and pretend it was a
       decision */
    c->dir = (unsigned char)((c->dir + 2) & 3);
    if (++tries > 4)
      break;
  }

  c->tx = FX(nx * 256 + 32 + ((c->dir == 1) ? -16 : (c->dir == 3 ? 16 : 0)));
  c->ty = FX(ny * 256 + 32 + ((c->dir == 0) ? 16 : (c->dir == 2) ? -16 : 0));
}

static void ai_pick_dir(Car *c)
{
  int r = rnd(100);
  if (r < 62) {
    /* straight */
  } else if (r < 81) {
    c->dir = (unsigned char)((c->dir + 1) & 3);
  } else {
    c->dir = (unsigned char)((c->dir + 3) & 3);
  }
}

void update_traffic(void)
{
  int i;
  for (i = 0; i < MAX_CARS; i++) {
    Car *c = &g_cars[i];
    int dx, dy, dist, step;
    if (!c->active || c->ai == 0 || i == g_pl.car)
      continue;

    dx = FI(c->tx) - FI(c->x);
    dy = FI(c->ty) - FI(c->y);
    dist = (dx < 0 ? -dx : dx) + (dy < 0 ? -dy : dy);

    if (dist < 6) {
      if (c->ai == 1) {
        ai_pick_dir(c);
        ai_set_target(c);
      } else {
        /* a chasing cop retargets onto the player every time it arrives */
        c->tx = g_pl.x;
        c->ty = g_pl.y;
      }
      continue;
    }

    step = c->ai == 2 ? FX(3) - FX(1) / 2 : FX(1) + FX(1) / 3;
    /* A mission car you cannot catch is not a chase, it is a screensaver.
       It drives slower and stops at corners like it is lost, which is also
       the plot. */
    if (c->target)
      step = FX(1) - FX(1) / 3;
    if (c->speed > 0 && c->speed < step)
      step = c->speed;

    if (dist > step) {
      c->x += FX(dx * step / dist);
      c->y += FX(dy * step / dist);
    } else {
      c->x = c->tx;
      c->y = c->ty;
    }

    /* sprite heading from the dominant axis of travel */
    if ((dx < 0 ? -dx : dx) > (dy < 0 ? -dy : dy))
      c->angle = (unsigned char)(dx > 0 ? 0 : 8);
    else
      c->angle = (unsigned char)(dy > 0 ? 4 : 12);

    /* keep the AI on the road even if the map generator is having a day */
    if (solid_px(FI(c->x), FI(c->y))) {
      c->tx = FX(FI(c->x) - dx);
      c->ty = FX(FI(c->y) - dy);
      c->speed = 0;
    }
  }
}

/* --- pedestrians ---------------------------------------------------------- */

void kill_ped(int idx, int by_player)
{
  Ped *p = &g_peds[idx];
  if (p->state == 4)
    return;
  p->state = 4;
  p->health = 0;
  add_decal(0, FI(p->x), FI(p->y));
  add_decal(1, FI(p->x) + rnd(8) - 4, FI(p->y) + rnd(6) - 3);
  add_fx(2, p->x, p->y, 0, 0);
  if (by_player) {
    if (p->kind == PED_COP)
      add_wanted(2);
    else
      add_wanted(1);
    if (p->kind == PED_GANG)
      mission_event(MEV_KILL_PED, FI(p->x), FI(p->y));
  }
  snd_play(SFX_HIT);
}

void update_peds(void)
{
  int i;
  for (i = 0; i < MAX_PEDS; i++) {
    Ped *p = &g_peds[i];
    int dx, dy, sp;
    if (!p->active || p->state == 4)
      continue;

    dx = FI(g_pl.x) - FI(p->x);
    dy = FI(g_pl.y) - FI(p->y);

    if (p->kind == PED_COP && g_pl.wanted && g_pl.on_foot) {
      p->state = 3;
    } else if (p->hostile && !g_pl.dead) {
      p->state = 3;
    } else if (p->state == 3 && !p->hostile && p->kind != PED_COP) {
      p->state = 1;
    }

    if (p->state == 3) {
      /* chase: home in on the player */
      int ax = dx < 0 ? -dx : dx;
      int ay = dy < 0 ? -dy : dy;
      if (ax > ay)
        p->dir = (unsigned char)(dx > 0 ? 3 : 2);
      else
        p->dir = (unsigned char)(dy > 0 ? 0 : 1);
      sp = FX(1) + FX(1) / 2;
      if (ax + ay < 12) {
        /* in range: hit or arrest */
        if (p->kind == PED_COP && g_pl.on_foot) {
          if (p->timer == 0) {
            g_state = ST_BUSTED;
            snd_play(SFX_FAIL);
            p->timer = 90;
          }
        } else if (p->timer == 0) {
          hurt_player(4);
          p->timer = 55;
        }
      } else if (p->kind == PED_GANG && (g_frames & 63) == 0 && ax + ay < 140) {        /* gang members shoot; cops prefer to arrest but will also shoot */
        int b;
        for (b = 0; b < MAX_BULLETS; b++) {
          if (g_bullets[b].active)
            continue;
          g_bullets[b].active = 1;
          g_bullets[b].life = 30;
          g_bullets[b].x = p->x;
          g_bullets[b].y = p->y;
          g_bullets[b].vx = (dx > 0 ? 1 : -1) * FX(4);
          g_bullets[b].vy = (dy > 0 ? 1 : -1) * FX(4);
          snd_play(SFX_PISTOL);
          break;
        }
      }
    } else if (p->state == 2) {
      /* flee: away from the player, fast */
      int ax = dx < 0 ? -dx : dx;
      int ay = dy < 0 ? -dy : dy;
      if (ax > ay)
        p->dir = (unsigned char)(dx > 0 ? 2 : 3);
      else
        p->dir = (unsigned char)(dy > 0 ? 1 : 0);
      sp = FX(2) + FX(1) / 2;
      if (p->timer == 0)
        p->state = 1;
    } else {
      sp = FX(1) / 2 + FX(1) / 4;
      if (p->timer == 0) {
        p->dir = (unsigned char)rnd(4);
        p->timer = (unsigned char)(40 + rnd(120));
      }
      /* pedestrians hop out of the way of a speeding car */
      if (!g_pl.on_foot && (g_frames & 3) == 0) {
        int ax = dx < 0 ? -dx : dx;
        int ay = dy < 0 ? -dy : dy;
        if (ax + ay < 40 && player_speed_px() > 90) {
          p->state = 2;
          p->timer = 90;
        }
      }
    }

    if (p->timer)
      p->timer--;

    {
      int nx, ny;
      switch (p->dir) {
        case 0: nx = 0; ny = sp; break;
        case 1: nx = 0; ny = -sp; break;
        case 2: nx = -sp; ny = 0; break;
        default: nx = sp; ny = 0; break;
      }
      if (!blocked(FI(p->x) + FI(nx), FI(p->y), 3, 2))
        p->x += nx;
      else
        p->dir = (unsigned char)((p->dir + 2) & 3);
      if (!blocked(FI(p->x), FI(p->y) + FI(ny), 3, 2))
        p->y += ny;
      else
        p->dir = (unsigned char)((p->dir + 1) & 3);
      if (wet(FI(p->x), FI(p->y)))
        p->dir = (unsigned char)((p->dir + 2) & 3);
      p->anim++;
    }
  }
}

/* --- cops ----------------------------------------------------------------- */

void spawn_cop_car(void)
{
  int i = find_free_car();
  int ix, iy, tries;
  Car *c;
  if (i < 0)
    return;
  c = &g_cars[i];
  c->active = 1;
  c->model = CAR_COP;
  c->is_cop = 1;
  c->ai = 2;
  c->health = 80;
  c->speed = FX(1);
  /* spawn at an intersection far enough away that it does not appear out of
     thin air in front of the player */
  ix = FI(g_pl.x) / 256;
  iy = FI(g_pl.y) / 256;
  for (tries = 0; tries < 12; tries++) {
    int nx = clamp(ix + rnd(5) - 2, 0, 5);
    int ny = clamp(iy + rnd(5) - 2, 0, 5);
    int px = nx * 256 + 32, py = ny * 256 + 32;
    int dx = px - FI(g_pl.x), dy = py - FI(g_pl.y);
    if (dx * dx + dy * dy > 200 * 200 || tries == 11) {
      c->x = FX(px);
      c->y = FX(py);
      break;
    }
  }
  c->tx = g_pl.x;
  c->ty = g_pl.y;
  c->dir = 0;
}

void spawn_cop_ped(void)
{
  int i = find_free_ped();
  Ped *p;
  int ang;
  if (i < 0)
    return;
  p = &g_peds[i];
  p->active = 1;
  p->kind = PED_COP;
  p->state = 3;
  p->health = 12;
  p->dir = 0;
  p->hostile = 1;
  ang = rnd(4);
  p->x = g_pl.x + FX(ang == 0 ? 120 : (ang == 2 ? -120 : 0));
  p->y = g_pl.y + FX(ang == 1 ? 120 : (ang == 3 ? -120 : 0));
}

void update_cops(void)
{
  static int spawn_cd;
  int cop_cars = 0, cop_peds = 0;
  int i;

  if (g_pl.wanted == 0) {
    snd_siren(0);
    spawn_cd = 0;
    return;
  }

  for (i = 0; i < MAX_CARS; i++)
    if (g_cars[i].active && g_cars[i].is_cop)
      cop_cars++;
  for (i = 0; i < MAX_PEDS; i++)
    if (g_peds[i].active && g_peds[i].kind == PED_COP && g_peds[i].state != 4)
      cop_peds++;

  snd_siren(g_pl.wanted > 1);

  if (spawn_cd)
    spawn_cd--;
  if (spawn_cd == 0 && cop_cars + cop_peds < g_pl.wanted) {
    if (g_pl.on_foot && rnd(2))
      spawn_cop_ped();
    else
      spawn_cop_car();
    spawn_cd = 120 - g_pl.wanted * 12;
  }

  /* a cop car touching your car costs paint and health */
  for (i = 0; i < MAX_CARS; i++) {
    Car *c = &g_cars[i];
    if (!c->active || !c->is_cop)
      continue;
    if (!g_pl.on_foot) {
      Car *p = &g_cars[g_pl.car];
      int dx = FI(c->x) - FI(p->x);
      int dy = FI(c->y) - FI(p->y);
      if (dx > -18 && dx < 18 && dy > -18 && dy < 18) {
        if ((g_frames & 31) == 0) {
          hurt_player(4);
          damage_car(g_pl.car, 4, 0);
          snd_play(SFX_CRASH);
          add_fx(1, c->x, c->y, 0, 0);
        }
      }
    } else {
      int dx = FI(c->x) - FI(g_pl.x);
      int dy = FI(c->y) - FI(g_pl.y);
      if (dx > -16 && dx < 16 && dy > -16 && dy < 16 && (g_frames & 63) == 0) {
        hurt_player(8);
      }
    }
  }
}

void update_wanted(void)
{
  if (g_pl.wanted == 0)
    return;
  g_pl.wanted_timer++;
  if (g_pl.wanted_timer > 900) {       /* 15 seconds of good behaviour */
    g_pl.wanted_timer = 0;
    g_pl.wanted--;
  }
}

/* --- bullets -------------------------------------------------------------- */

void update_bullets(void)
{
  int i, j;
  for (i = 0; i < MAX_BULLETS; i++) {
    Bullet *b = &g_bullets[i];
    if (!b->active)
      continue;
    b->x += b->vx;
    b->y += b->vy;
    if (--b->life == 0) {
      b->active = 0;
      continue;
    }
    if (solid_px(FI(b->x), FI(b->y))) {
      b->active = 0;
      add_fx(0, b->x, b->y, 0, 0);
      continue;
    }
    /* peds */
    for (j = 0; j < MAX_PEDS; j++) {
      Ped *p = &g_peds[j];
      int dx, dy;
      if (!p->active || p->state == 4)
        continue;
      dx = FI(b->x) - FI(p->x);
      dy = FI(b->y) - FI(p->y);
      if (dx > -7 && dx < 7 && dy > -9 && dy < 7) {
        b->active = 0;
        if (p->health <= 3) {
          kill_ped(j, 1);
        } else {
          p->health -= 3;
          add_fx(2, p->x, p->y, 0, 0);
          if (p->kind != PED_COP)
            p->state = 2;
          p->timer = 120;
        }
        break;
      }
    }
    if (!b->active)
      continue;
    /* cars */
    for (j = 0; j < MAX_CARS; j++) {
      Car *c = &g_cars[j];
      int dx, dy;
      if (!c->active || j == g_pl.car)
        continue;
      dx = FI(b->x) - FI(c->x);
      dy = FI(b->y) - FI(c->y);
      if (dx > -16 && dx < 16 && dy > -10 && dy < 10) {
        b->active = 0;
        damage_car(j, 6, 1);
        add_fx(0, b->x, b->y, 0, 0);
        break;
      }
    }
  }
}

/* --- pickups -------------------------------------------------------------- */

void update_pickups(void)
{
  int i;
  for (i = 0; i < MAX_PICKUPS; i++) {
    Pickup *p = &g_pickups[i];
    int dx, dy;
    if (!p->active)
      continue;
    p->bob++;
    dx = FI(g_pl.x) - p->x;
    dy = FI(g_pl.y) - p->y;
    if (dx > -12 && dx < 12 && dy > -12 && dy < 12) {
      switch (p->kind) {
        case 0:
          g_pl.money += 50 + rnd(150);
          snd_play(SFX_MONEY);
          break;
        case 1:
          g_pl.money += 300;
          snd_play(SFX_MONEY);
          break;
        case 2:
          g_pl.health = (unsigned char)clamp(g_pl.health + 30, 0, 100);
          snd_play(SFX_PICKUP);
          ui_message(S_PICKUP_HEALTH, 1000);
          break;
        case 3:
          g_pl.armour = (unsigned char)clamp(g_pl.armour + 40, 0, 100);
          snd_play(SFX_PICKUP);
          ui_message(S_PICKUP_ARMOR, 1000);
          break;
        default:
          g_pl.ammo = (unsigned char)clamp(g_pl.ammo + 20, 0, 255);
          snd_play(SFX_PICKUP);
          ui_message(S_PICKUP_AMMO, 1000);
          break;
      }
      p->active = 0;
      if (p->mission)
        mission_advance();
    }
  }
}

void update_fx(void)
{
  int i;
  for (i = 0; i < MAX_FX; i++) {
    Fx *f = &g_fx[i];
    if (!f->active)
      continue;
    f->x += f->vx;
    f->y += f->vy;
    f->vy += FX(1) / 8;
    if (--f->life == 0)
      f->active = 0;
  }
  for (i = 0; i < MAX_DECALS; i++) {
    if (g_decals[i].active && --g_decals[i].life == 0)
      g_decals[i].active = 0;
  }
}

/* --- car versus pedestrian ------------------------------------------------ */

static void cars_hit_peds(void)
{
  int i, j;
  for (i = 0; i < MAX_CARS; i++) {
    Car *c = &g_cars[i];
    int sp;
    if (!c->active)
      continue;
    sp = c->speed < 0 ? -c->speed : c->speed;
    if (sp < FX(1))
      continue;
    if (!in_view(FI(c->x), FI(c->y), 32))
      continue;
    for (j = 0; j < MAX_PEDS; j++) {
      Ped *p = &g_peds[j];
      int dx, dy;
      if (!p->active || p->state == 4)
        continue;
      dx = FI(c->x) - FI(p->x);
      dy = FI(c->y) - FI(p->y);
      if (dx > -14 && dx < 14 && dy > -10 && dy < 10) {
        kill_ped(j, i == g_pl.car);
        if (i == g_pl.car) {
          c->speed = c->speed * 3 / 4;
          add_fx(2, p->x, p->y, 0, 0);
        }
      }
    }
  }
}

static void cars_hit_cars(void)
{
  int i;
  Car *p;
  if (g_pl.on_foot)
    return;
  p = &g_cars[g_pl.car];
  for (i = 0; i < MAX_CARS; i++) {
    Car *c = &g_cars[i];
    int dx, dy;
    if (!c->active || i == g_pl.car)
      continue;
    dx = FI(c->x) - FI(p->x);
    dy = FI(c->y) - FI(p->y);
    if (dx > -20 && dx < 20 && dy > -14 && dy < 14) {
      int sp = p->speed < 0 ? -p->speed : p->speed;
      if (sp > FX(1)) {
        damage_car(i, 12, 1);
        p->speed = p->speed * 2 / 3;
        snd_play(SFX_CRASH);
        add_fx(1, c->x, c->y, 0, 0);
      }
    }
  }
}

void update_traffic_collisions(void)
{
  cars_hit_peds();
  cars_hit_cars();
}

/* --- respawn -------------------------------------------------------------- */

void respawn_player(int wasted)
{
  int i;
  (void)wasted;
  g_pl.dead = 0;
  g_pl.health = 100;
  g_pl.armour = 0;
  g_pl.ammo = 12;
  g_pl.wanted = 0;
  g_pl.wanted_timer = 0;
  g_pl.on_foot = 1;
  g_pl.car = -1;
  g_pl.x = FX(g_poi[POI_SPAWN].x);
  g_pl.y = FX(g_poi[POI_SPAWN].y);
  g_pl.vx = g_pl.vy = 0;
  for (i = 0; i < MAX_BULLETS; i++)
    g_bullets[i].active = 0;
  for (i = 0; i < MAX_CARS; i++)
    if (g_cars[i].is_cop)
      g_cars[i].active = 0;
  for (i = 0; i < MAX_PEDS; i++)
    if (g_peds[i].kind == PED_COP && g_peds[i].state != 4)
      g_peds[i].active = 0;
  snd_siren(0);
}
