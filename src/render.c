/* ===========================================================================
   GTA VI: DOS CITY -- render.c
   =========================================================================== */
#include <string.h>

#include "render.h"
#include "pal.h"

void r_clear(unsigned char c)
{
  memset(g_backbuf, c, SCREEN_W * SCREEN_H);
}

void r_pixel(int x, int y, unsigned char c)
{
  if ((unsigned)x < (unsigned)SCREEN_W && (unsigned)y < (unsigned)SCREEN_H)
    g_backbuf[y * SCREEN_W + x] = c;
}

void r_hline(int x0, int x1, int y, unsigned char c)
{
  unsigned char *p;
  /* Signed checks. `(unsigned)y < 0` is always false, so a negative y slips
     through and memsets memory *before* the framebuffer: that is a heap
     corruption, not a rendering bug, and it only shows up as a crash inside
     exit(). */
  if (y < 0 || y >= SCREEN_H)
    return;
  if (x0 < 0) x0 = 0;
  if (x1 >= SCREEN_W) x1 = SCREEN_W - 1;
  if (x0 > x1)
    return;
  p = g_backbuf + y * SCREEN_W + x0;
  memset(p, c, (size_t)(x1 - x0 + 1));
}

void r_vline(int x, int y0, int y1, unsigned char c)
{
  unsigned char *p;
  if (x < 0 || x >= SCREEN_W)
    return;
  if (y0 < 0) y0 = 0;
  if (y1 >= SCREEN_H) y1 = SCREEN_H - 1;
  if (y0 > y1)
    return;
  p = g_backbuf + y0 * SCREEN_W + x;
  while (y0++ <= y1) {
    *p = c;
    p += SCREEN_W;
  }
}

void r_rect(int x0, int y0, int x1, int y1, unsigned char c)
{
  int y;
  if (y0 < 0) y0 = 0;
  if (y1 >= SCREEN_H) y1 = SCREEN_H - 1;
  for (y = y0; y <= y1; y++)
    r_hline(x0, x1, y, c);
}

void r_frame(int x0, int y0, int x1, int y1, unsigned char c)
{
  r_hline(x0, x1, y0, c);
  r_hline(x0, x1, y1, c);
  r_vline(x0, y0, y1, c);
  r_vline(x1, y0, y1, c);
}

void r_line(int x0, int y0, int x1, int y1, unsigned char c)
{
  int dx = x1 - x0, dy = y1 - y0;
  int sx = dx < 0 ? -1 : 1, sy = dy < 0 ? -1 : 1;
  int err, e2;
  if (dx < 0) dx = -dx;
  if (dy < 0) dy = -dy;
  err = dx - dy;
  for (;;) {
    r_pixel(x0, y0, c);
    if (x0 == x1 && y0 == y1)
      break;
    e2 = err * 2;
    if (e2 > -dy) { err -= dy; x0 += sx; }
    if (e2 < dx)  { err += dx; y0 += sy; }
  }
}

/* --- sprites ------------------------------------------------------------- */

static void blit(const unsigned char *px, int w, int h, int x, int y,
                 int clipy0, int clipy1, int flip, int steps)
{
  int sy0 = 0, sy1 = h - 1;
  int dy, dx;
  if (y < clipy0) sy0 = clipy0 - y;
  if (y + h - 1 > clipy1) sy1 = clipy1 - y;
  if (sy0 > sy1)
    return;

  for (dy = sy0; dy <= sy1; dy++) {
    int py = y + dy;
    unsigned char *dst;
    const unsigned char *src;
    if ((unsigned)py >= SCREEN_H)
      continue;
    dst = g_backbuf + py * SCREEN_W;
    src = px + dy * w;
    for (dx = 0; dx < w; dx++) {
      unsigned char c = flip ? src[w - 1 - dx] : src[dx];
      int px_ = x + dx;
      if (!c || (unsigned)px_ >= SCREEN_W)
        continue;
      if (steps) {
        /* Step down the palette ramp instead of blending: no colour maths,
           and the result still looks like it belongs to the same world. */
        if (c == PAL_TRANSPARENT)
          continue;
        c = (unsigned char)(c > steps ? c - steps : 1);
      }
      dst[px_] = c;
    }
  }
}

void r_sprite(int idx, int x, int y)
{
  const Sprite *s = &g_sprites[idx];
  blit(s->px, s->w, s->h, x, y, 0, SCREEN_H - 1, 0, 0);
}

void r_sprite_flip(int idx, int x, int y)
{
  const Sprite *s = &g_sprites[idx];
  blit(s->px, s->w, s->h, x, y, 0, SCREEN_H - 1, 1, 0);
}

void r_sprite_clip(int idx, int x, int y, int clipy0, int clipy1)
{
  const Sprite *s = &g_sprites[idx];
  blit(s->px, s->w, s->h, x, y, clipy0, clipy1, 0, 0);
}

void r_sprite_dim(int idx, int x, int y, int steps)
{
  const Sprite *s = &g_sprites[idx];
  blit(s->px, s->w, s->h, x, y, 0, SCREEN_H - 1, 0, steps);
}

void r_sprite_scale(int idx, int x, int y, int scale)
{
  const Sprite *s = &g_sprites[idx];
  int dx, dy;
  if (scale < 1)
    scale = 1;
  for (dy = 0; dy < s->h * scale; dy++) {
    int py = y + dy;
    unsigned char *dst;
    const unsigned char *src;
    if ((unsigned)py >= SCREEN_H)
      continue;
    dst = g_backbuf + py * SCREEN_W;
    src = s->px + (dy / scale) * s->w;
    for (dx = 0; dx < s->w * scale; dx++) {
      unsigned char c = src[dx / scale];
      int px_ = x + dx;
      if (c && (unsigned)px_ < SCREEN_W)
        dst[px_] = c;
    }
  }
}

void r_sprite_centered(int idx, int cx, int cy)
{
  const Sprite *s = &g_sprites[idx];
  r_sprite(idx, cx - s->w / 2, cy - s->h / 2);
}

/* --- tiles --------------------------------------------------------------- */

void r_tile(int tile, int x, int y)
{
  const unsigned char *px;
  int dy, dx;
  if ((unsigned)tile >= NTILES)
    return;
  px = g_tiles + (unsigned)tile * TILE_W * TILE_H;

  /* Fast path: an opaque tile that sits fully on screen is 16 memcpys. Most
     of the city is opaque, so this is where the frame time goes. */
  if (g_tile_opaque[tile] && x >= 0 && y >= 0 &&
      x + TILE_W <= SCREEN_W && y + TILE_H <= SCREEN_H) {
    unsigned char *dst = g_backbuf + y * SCREEN_W + x;
    for (dy = 0; dy < TILE_H; dy++) {
      memcpy(dst, px, TILE_W);
      px += TILE_W;
      dst += SCREEN_W;
    }
    return;
  }

  for (dy = 0; dy < TILE_H; dy++) {
    int py = y + dy;
    unsigned char *dst;
    const unsigned char *src;
    if ((unsigned)py >= SCREEN_H)
      continue;
    dst = g_backbuf + py * SCREEN_W;
    src = px + dy * TILE_W;
    for (dx = 0; dx < TILE_W; dx++) {
      unsigned char c = src[dx];
      int pxx = x + dx;
      if (c && (unsigned)pxx < SCREEN_W)
        dst[pxx] = c;
    }
  }
}

void r_tile_dim(int tile, int x, int y, int steps)
{
  const unsigned char *px;
  int dy, dx;
  if ((unsigned)tile >= NTILES)
    return;
  px = g_tiles + (unsigned)tile * TILE_W * TILE_H;

  if (x >= 0 && y >= 0 && x + TILE_W <= SCREEN_W && y + TILE_H <= SCREEN_H) {
    unsigned char *dst = g_backbuf + y * SCREEN_W + x;
    for (dy = 0; dy < TILE_H; dy++) {
      for (dx = 0; dx < TILE_W; dx++) {
        unsigned char c = px[dx];
        dst[dx] = (unsigned char)(c > steps ? c - steps : 1);
      }
      px += TILE_W;
      dst += SCREEN_W;
    }
    return;
  }

  for (dy = 0; dy < TILE_H; dy++) {
    int py = y + dy;
    unsigned char *dst;
    const unsigned char *src;
    if ((unsigned)py >= SCREEN_H)
      continue;
    dst = g_backbuf + py * SCREEN_W;
    src = px + dy * TILE_W;
    for (dx = 0; dx < TILE_W; dx++) {
      unsigned char c = src[dx];
      int pxx = x + dx;
      if (!c || (unsigned)pxx >= SCREEN_W)
        continue;
      dst[pxx] = (unsigned char)(c > steps ? c - steps : 1);
    }
  }
}

/* --- text ---------------------------------------------------------------- */

int r_text_width(const char *s)
{
  return (int)strlen(s) * 8;
}

int r_text(int x, int y, const char *s, unsigned char c)
{
  int cx = x;
  while (*s) {
    const unsigned char *g = g_font + (unsigned char)*s * 16;
    int gy;
    for (gy = 0; gy < 16; gy++) {
      unsigned char bits = g[gy];
      int gx;
      if (!bits)
        continue;
      for (gx = 0; gx < 8; gx++)
        if (bits & (0x80 >> gx))
          r_pixel(cx + gx, y + gy, c);
    }
    cx += 8;
    s++;
  }
  return cx;
}

int r_text_shadow(int x, int y, const char *s, unsigned char c)
{
  r_text(x + 1, y + 1, s, PAL_SHADOW);
  return r_text(x, y, s, c);
}

int r_text_center(int y, const char *s, unsigned char c)
{
  int x = (SCREEN_W - r_text_width(s)) / 2;
  if (x < 0)
    x = 0;
  return r_text(x, y, s, c);
}

int r_text_right(int x1, int y, const char *s, unsigned char c)
{
  return r_text(x1 - r_text_width(s), y, s, c);
}

int r_text_len(const char *s, int maxw)
{
  int n = 0;
  while (s[n] && (n + 1) * 8 <= maxw)
    n++;
  return n;
}

void r_text_wrap(int x, int y, int maxw, const char *s, unsigned char c, int lh)
{
  char line[160];
  int n = 0;
  int cx = 0;
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
    }
    memcpy(line + n, word, (size_t)wlen);
    n += wlen;
    cx += wlen;
    if (*p == ' ')
      p++;
  }
  line[n] = 0;
  r_text(x, y, line, c);
}

/* --- backgrounds --------------------------------------------------------- */

/* Vertical gradient inside one palette ramp. Interpolating palette *indices*
   across ramps walks through every ramp in between and paints a rainbow, so a
   gradient always names a ramp and two steps inside it. */
void r_gradient_v(int y0, int y1, int base, int n, int from, int to)
{
  int y;
  if (y1 <= y0)
    return;
  for (y = y0; y <= y1; y++) {
    int step = from + (to - from) * (y - y0) / (y1 - y0);
    if (step < 0) step = 0;
    if (step >= n) step = n - 1;
    r_hline(0, SCREEN_W - 1, y, (unsigned char)(base + step));
  }
}

void r_vignette(void)
{
  int y;
  for (y = 0; y < SCREEN_H; y++) {
    int d = (y < SCREEN_H / 2) ? y : (SCREEN_H - 1 - y);
    if (d < 2)
      r_hline(0, SCREEN_W - 1, y, PAL_VOID1);
  }
}

void r_box(int x0, int y0, int x1, int y1, unsigned char fill,
           unsigned char light, unsigned char dark)
{
  r_rect(x0, y0, x1, y1, fill);
  r_hline(x0, x1, y0, light);
  r_vline(x0, y0, y1, light);
  r_hline(x0, x1, y1, dark);
  r_vline(x1, y0, y1, dark);
}

void r_box_double(int x0, int y0, int x1, int y1, unsigned char fill,
                  unsigned char light, unsigned char dark)
{
  r_box(x0, y0, x1, y1, fill, light, dark);
  r_box(x0 + 1, y0 + 1, x1 - 1, y1 - 1, fill, dark, light);
}

void r_dither_rect(int x0, int y0, int x1, int y1, unsigned char a,
                   unsigned char b)
{
  int x, y;
  for (y = y0; y <= y1; y++)
    for (x = x0; x <= x1; x++)
      r_pixel(x, y, ((x ^ y) & 1) ? a : b);
}
