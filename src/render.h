/* render.h - small software rasteriser for the 320x240 logical screen.
 * Everything is drawn into a Frame (ARGB8888); main.c uploads the Frame to an
 * SDL texture and scales it with nearest-neighbour filtering. Keeping the
 * rasteriser free of SDL lets the headless tests render and dump frames. */
#ifndef RENDER_H
#define RENDER_H

#include "common.h"
#include "sprites.h"

typedef struct {
    uint32_t px[LW * LH];
} Frame;

typedef enum { T_NORMAL, T_FLASH, T_DIM, T_GHOST, T_WORN } Tint;

uint32_t color_argb(int pal);
int      color_dark(int pal);

void fr_clear(Frame *f, int pal);
void fr_pixel(Frame *f, int x, int y, int pal);
void fr_rect(Frame *f, int x, int y, int w, int h, int pal);
void fr_box(Frame *f, int x, int y, int w, int h, int pal);   /* 1px outline */
void fr_line(Frame *f, int x0, int y0, int x1, int y1, int pal);
void fr_hline_dotted(Frame *f, int x0, int x1, int y, int pal, int phase);
void fr_vline_dotted(Frame *f, int x, int y0, int y1, int pal, int phase);

int  text_width(const char *s);
void fr_text(Frame *f, int x, int y, const char *s, int pal);
void fr_text_center(Frame *f, int cx, int y, const char *s, int pal);
void fr_text_right(Frame *f, int rx, int y, const char *s, int pal);

void fr_sprite(Frame *f, const Sprite *s, int x, int y, int main_pal, Tint tint, bool flip);
void fr_sprite_scaled(Frame *f, const Sprite *s, int x, int y, int main_pal, Tint tint, int scale);

/* Whole-frame effects. */
void fr_dim(Frame *f, int num, int den);   /* multiply brightness by num/den */
void fr_scanlines(Frame *f);               /* optional CRT look: darken odd rows */
bool fr_save_ppm(const Frame *f, const char *path);

#endif
