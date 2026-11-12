/* ===========================================================================
   GTA VI: DOS CITY -- video.h
   Platform layer: 320x200x256, a 1 kHz timer, raw set-1 scancodes, PC speaker.
   Two backends behind one interface (real DOS / host for debugging).
   =========================================================================== */
#ifndef GTA_VIDEO_H
#define GTA_VIDEO_H

#include "assets.h"

#define SCREEN_W 320
#define SCREEN_H 200
#define VIEW_H   168            /* playfield; 168..199 is the HUD */
#define HUD_Y    VIEW_H
#define TICK_HZ  1000

extern unsigned char *g_backbuf;

void vid_init(void);
void vid_shutdown(void);
void vid_setpal(const unsigned char *rgb6);
void vid_present(void);
void vid_waitvsync(void);
void vid_fade(int level);        /* 0 = black, 16 = full */
void vid_retrace(void);          /* cheap frame limiter, safe to call often */

void timer_init(void);
void timer_done(void);
unsigned long timer_ticks(void);
#ifdef GTA_HOST
void timer_advance(unsigned long ms);
#endif

void kbd_init(void);
void kbd_done(void);
int  kbd_pop(void);              /* next press scancode, or 0 */
int  kbd_held(int sc);
void kbd_flush(void);

/* set-1 scancodes we actually use */
#define K_ESC    0x01
#define K_1      0x02
#define K_2      0x03
#define K_3      0x04
#define K_4      0x05
#define K_5      0x06
#define K_TAB    0x0F
#define K_Q      0x10
#define K_W      0x11
#define K_E      0x12
#define K_R      0x13
#define K_T      0x14
#define K_Y      0x15
#define K_U      0x16
#define K_I      0x17
#define K_O      0x18
#define K_P      0x19
#define K_ENTER  0x1C
#define K_CTRL   0x1D
#define K_A      0x1E
#define K_S      0x1F
#define K_D      0x20
#define K_F      0x21
#define K_G      0x22
#define K_H      0x23
#define K_J      0x24
#define K_K      0x25
#define K_L      0x26
#define K_Z      0x2C
#define K_X      0x2D
#define K_C      0x2E
#define K_V      0x2F
#define K_B      0x30
#define K_N      0x31
#define K_M      0x32
#define K_LSHIFT 0x2A
#define K_RSHIFT 0x36
#define K_SPACE  0x39
#define K_F1     0x3B
#define K_F2     0x3C
#define K_F3     0x3D
#define K_F10    0x44
#define K_UP     0x48
#define K_LEFT   0x4B
#define K_RIGHT  0x4D
#define K_DOWN   0x50

#define K_ANY_UP    (kbd_held(K_UP) || kbd_held(K_W))
#define K_ANY_DOWN  (kbd_held(K_DOWN) || kbd_held(K_S))
#define K_ANY_LEFT  (kbd_held(K_LEFT) || kbd_held(K_A))
#define K_ANY_RIGHT (kbd_held(K_RIGHT) || kbd_held(K_D))

#endif
