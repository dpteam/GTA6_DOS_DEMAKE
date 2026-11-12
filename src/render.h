/* ===========================================================================
   GTA VI: DOS CITY -- render.h
   A software renderer for one indexed 320x200 buffer. No blitter state, no
   clipping stack, no 4 MB of code: pixels, sprites, text, and a bevelled box.
   =========================================================================== */
#ifndef GTA_RENDER_H
#define GTA_RENDER_H

#include "video.h"
#include "tiles.h"

extern const unsigned char g_font[256 * 16];

void r_clear(unsigned char c);
void r_pixel(int x, int y, unsigned char c);
void r_rect(int x0, int y0, int x1, int y1, unsigned char c);
void r_frame(int x0, int y0, int x1, int y1, unsigned char c);
void r_hline(int x0, int x1, int y, unsigned char c);
void r_vline(int x, int y0, int y1, unsigned char c);
void r_line(int x0, int y0, int x1, int y1, unsigned char c);

void r_sprite(int idx, int x, int y);
void r_sprite_flip(int idx, int x, int y);
void r_sprite_clip(int idx, int x, int y, int clipy0, int clipy1);
void r_sprite_scale(int idx, int x, int y, int scale);
void r_sprite_dim(int idx, int x, int y, int steps);
void r_sprite_centered(int idx, int cx, int cy);

void r_tile(int tile, int x, int y);
void r_tile_dim(int tile, int x, int y, int steps);

int  r_text(int x, int y, const char *s, unsigned char c);
int  r_text_shadow(int x, int y, const char *s, unsigned char c);
int  r_text_center(int y, const char *s, unsigned char c);
int  r_text_right(int x1, int y, const char *s, unsigned char c);
int  r_text_width(const char *s);
void r_text_wrap(int x, int y, int maxw, const char *s, unsigned char c, int lh);
int  r_text_len(const char *s, int maxw);   /* chars that fit in maxw px */

void r_gradient_v(int y0, int y1, int base, int n, int from, int to);
void r_vignette(void);

/* A panel with a light top-left edge and a dark bottom-right one. Everything
   in this game's UI is one of these, which is why it looks like 1994. */
void r_box(int x0, int y0, int x1, int y1, unsigned char fill,
           unsigned char light, unsigned char dark);
void r_box_double(int x0, int y0, int x1, int y1, unsigned char fill,
                  unsigned char light, unsigned char dark);

void r_dither_rect(int x0, int y0, int x1, int y1, unsigned char a,
                   unsigned char b);

#endif
