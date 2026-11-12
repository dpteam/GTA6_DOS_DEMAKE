/* ===========================================================================
   GTA VI: DOS CITY -- video.c

   Two backends behind one tiny interface:

     DOS   : real VGA mode 13h at A000:0000, PIT at 1 kHz, IRQ1 keyboard.
     HOST  : same game, framebuffer goes to PPM files, input comes from a
             script file. This is how the game gets debugged without booting
             DOSBox every 40 seconds.

   Build with -DGTA_HOST for the host backend.
   =========================================================================== */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "video.h"

unsigned char *g_backbuf;

/* ========================================================================= */
#ifdef GTA_HOST

#include <time.h>

static unsigned char *g_pal_copy;
static unsigned long g_frame;
static int g_script[8192][2];
static int g_script_n;
static int g_script_pos;
static int g_dump_every = 1;
static int g_dump_from;
static unsigned char g_host_held[128];

static void host_load_script(void)
{
  const char *path = getenv("GTA_SCRIPT");
  FILE *f;
  int fr, sc;
  if (!path)
    return;
  f = fopen(path, "r");
  if (!f)
    return;
  while (g_script_n < 8192 && fscanf(f, "%d %x", &fr, &sc) == 2) {
    g_script[g_script_n][0] = fr;
    g_script[g_script_n][1] = sc;
    g_script_n++;
  }
  fclose(f);
}

void vid_init(void)
{
  const char *s;
  g_backbuf = (unsigned char *)calloc(SCREEN_W * SCREEN_H, 1);
  g_pal_copy = (unsigned char *)calloc(768, 1);
  memcpy(g_pal_copy, g_pal, 768);
  s = getenv("GTA_DUMP_EVERY");
  if (s)
    g_dump_every = atoi(s);
  s = getenv("GTA_DUMP_FROM");
  if (s)
    g_dump_from = atoi(s);
  host_load_script();
  printf("GTA6DOS host build: %dx%d, dump_every=%d\n", SCREEN_W, SCREEN_H,
         g_dump_every);
}

void vid_shutdown(void)
{
  printf("frames rendered: %lu\n", g_frame);
}

void vid_setpal(const unsigned char *rgb6)
{
  memcpy(g_pal_copy, rgb6, 768);
}

static void write_ppm(const char *path)
{
  FILE *f = fopen(path, "wb");
  int i;
  if (!f)
    return;
  fprintf(f, "P6\n%d %d\n255\n", SCREEN_W, SCREEN_H);
  for (i = 0; i < SCREEN_W * SCREEN_H; i++) {
    unsigned char c = g_backbuf[i];
    unsigned char v[3];
    v[0] = (unsigned char)(g_pal_copy[c * 3 + 0] << 2);
    v[1] = (unsigned char)(g_pal_copy[c * 3 + 1] << 2);
    v[2] = (unsigned char)(g_pal_copy[c * 3 + 2] << 2);
    fwrite(v, 1, 3, f);
  }
  fclose(f);
}

void vid_present(void)
{
  char name[80];
  if (g_dump_every > 0 && g_frame >= (unsigned long)g_dump_from &&
      (g_frame % (unsigned long)g_dump_every) == 0) {
    sprintf(name, "build/frames/f%05lu.ppm", g_frame);
    write_ppm(name);
  }
  g_frame++;
}

void vid_waitvsync(void) {}
void vid_retrace(void) {}

void vid_fade(int level)
{
  unsigned char tmp[768];
  int i;
  for (i = 0; i < 768; i++)
    tmp[i] = (unsigned char)((g_pal[i] * level) / 16);
  vid_setpal(tmp);
}

void timer_init(void) {}
void timer_done(void) {}

static unsigned long g_host_ms;

void timer_advance(unsigned long ms)
{
  g_host_ms += ms;
}

unsigned long timer_ticks(void)
{
  /* Time advances with logic steps, not with rendered frames. Coupling them
     means "don't render" also means "don't simulate", and then the loop spins
     forever with a stopped clock. */
  return g_host_ms;
}

void kbd_init(void) {}
void kbd_done(void) {}

int kbd_pop(void)
{
  /* The frame limit lives in main(), not here: tying it to the *render*
     counter means a run with rendering disabled never terminates, which is
     exactly the kind of bug you find at 2 AM with a hung terminal. */
  while (g_script_pos < g_script_n &&
         g_script[g_script_pos][0] <= (int)g_frame) {
    int v = g_script[g_script_pos++][1];
    if (v & 0x80) {
      g_host_held[v & 0x7F] = 0;
    } else {
      g_host_held[v & 0x7F] = 1;
      return v & 0x7F;
    }
  }
  return 0;
}

int kbd_held(int sc)
{
  if (sc < 0 || sc > 127)
    return 0;
  return g_host_held[sc];
}

void kbd_flush(void) {}

/* ========================================================================= */
#else  /* real DOS */
#include <dos.h>
#include <stdlib.h>
#include <sys/nearptr.h>
#include <go32.h>
#include <dpmi.h>

#define VGA_INSTAT1  0x3DA
#define VGA_DAC_WR   0x3C8
#define VGA_DAC_DATA 0x3C9
#define PIT_CH0      0x40
#define PIT_CMD      0x43
#define PIT_HZ       1193182UL

static unsigned char *VGA;
static unsigned char g_pal_cur[768];
static unsigned long tick_count;
static volatile int key_head, key_tail;
static unsigned char key_queue[64];
static unsigned char key_held[128];
static _go32_dpmi_seginfo old_timer, old_kbd;
static _go32_dpmi_seginfo timer_seg, kbd_seg;

static void timer_isr(void)
{
  tick_count++;
  outportb(0x20, 0x20);
}

static void kbd_isr(void)
{
  unsigned char sc = inportb(0x60);
  int next;
  if (sc & 0x80) {
    key_held[sc & 0x7F] = 0;
  } else {
    key_held[sc] = 1;
    next = (key_head + 1) & 63;
    if (next != key_tail) {
      key_queue[key_head] = sc;
      key_head = next;
    }
  }
  outportb(0x20, 0x20);
}

void vid_init(void)
{
  union REGS r;
  r.x.ax = 0x0013;                 /* mode 13h, 320x200x256 */
  int86(0x10, &r, &r);

  __djgpp_nearptr_enable();
  VGA = (unsigned char *)(0xA0000 + __djgpp_conventional_base);

  g_backbuf = (unsigned char *)malloc(SCREEN_W * SCREEN_H);
  if (!g_backbuf)
    g_backbuf = (unsigned char *)0xB8000;   /* text memory: at least visible */
  memset(g_backbuf, 0, SCREEN_W * SCREEN_H);

  vid_setpal(g_pal);
  memset(VGA, 0, 64000);
}

void vid_shutdown(void)
{
  union REGS r;
  memset(VGA, 0, 64000);
  r.x.ax = 0x0003;
  int86(0x10, &r, &r);
  __djgpp_nearptr_disable();
}

void vid_setpal(const unsigned char *rgb6)
{
  int i;
  memcpy(g_pal_cur, rgb6, 768);
  outportb(VGA_DAC_WR, 0);
  for (i = 0; i < 768; i++)
    outportb(VGA_DAC_DATA, g_pal_cur[i]);
}

void vid_fade(int level)
{
  int i;
  outportb(VGA_DAC_WR, 0);
  for (i = 0; i < 768; i++)
    outportb(VGA_DAC_DATA, (unsigned char)((g_pal_cur[i] * level) / 16));
}

void vid_waitvsync(void)
{
  while (inportb(VGA_INSTAT1) & 8)
    ;
  while (!(inportb(VGA_INSTAT1) & 8))
    ;
}

void vid_retrace(void)
{
  /* Wait for the vertical retrace to *start* so the blit does not tear
     halfway down the screen. Two loops, same as a million demos. */
  while (!(inportb(VGA_INSTAT1) & 8))
    ;
}

void vid_present(void)
{
  memcpy(VGA, g_backbuf, 64000);
}

void timer_init(void)
{
  unsigned long div = PIT_HZ / TICK_HZ;

  /* IRQ0 through the DPMI wrapper: the CPU is in real mode when the IRQ
     fires, so a plain __interrupt function would be a protected-mode address
     in a place that expects a far real-mode one. */
  _go32_dpmi_lock_data(&tick_count, sizeof(tick_count));
  _go32_dpmi_lock_code(timer_isr, 4096);

  _go32_dpmi_get_protected_mode_interrupt_vector(8, &old_timer);
  timer_seg.pm_offset = (int)timer_isr;
  timer_seg.pm_selector = _go32_my_cs();
  _go32_dpmi_allocate_iret_wrapper(&timer_seg);
  _go32_dpmi_set_protected_mode_interrupt_vector(8, &timer_seg);

  outportb(PIT_CMD, 0x36);
  outportb(PIT_CH0, (unsigned char)(div & 0xFF));
  outportb(PIT_CH0, (unsigned char)((div >> 8) & 0xFF));
}

void timer_done(void)
{
  _go32_dpmi_set_protected_mode_interrupt_vector(8, &old_timer);
  _go32_dpmi_free_iret_wrapper(&timer_seg);
  outportb(PIT_CMD, 0x36);
  outportb(PIT_CH0, 0);
  outportb(PIT_CH0, 0);
}

unsigned long timer_ticks(void)
{
  return tick_count;
}

void kbd_init(void)
{
  _go32_dpmi_lock_data(key_queue, sizeof(key_queue));
  _go32_dpmi_lock_data(key_held, sizeof(key_held));
  _go32_dpmi_lock_code(kbd_isr, 4096);

  _go32_dpmi_get_protected_mode_interrupt_vector(9, &old_kbd);
  kbd_seg.pm_offset = (int)kbd_isr;
  kbd_seg.pm_selector = _go32_my_cs();
  _go32_dpmi_allocate_iret_wrapper(&kbd_seg);
  _go32_dpmi_set_protected_mode_interrupt_vector(9, &kbd_seg);
}

void kbd_done(void)
{
  _go32_dpmi_set_protected_mode_interrupt_vector(9, &old_kbd);
  _go32_dpmi_free_iret_wrapper(&kbd_seg);
}

int kbd_pop(void)
{
  int sc;
  if (key_tail == key_head)
    return 0;
  sc = key_queue[key_tail];
  key_tail = (key_tail + 1) & 63;
  return sc;
}

int kbd_held(int sc)
{
  if (sc < 0 || sc > 127)
    return 0;
  return key_held[sc];
}

void kbd_flush(void)
{
  key_tail = key_head;
}

#endif
