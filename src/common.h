/* common.h - shared constants, palette and small helpers */
#ifndef COMMON_H
#define COMMON_H

#include <math.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define GAME_TITLE "INIT WARS: The Last PID"
#define GAME_VERSION "1.6.0"

/* Logical resolution; everything is drawn at this size and scaled up. */
#define LW 320
#define LH 240
#define FPS 60

/* Playfield (between the HUD at the top and the status lines at the bottom) */
#define AX0 8
#define AY0 24
#define AX1 312
#define AY1 212

typedef struct { float x, y, w, h; } Rectf;
typedef struct { uint8_t r, g, b; } Color;

enum {
    C_BLACK, C_DKGREEN, C_GREEN, C_BGREEN, C_AMBER, C_DKAMBER,
    C_GREY, C_LTGREY, C_RED, C_DKRED, C_WHITE, C_CYAN, C_NCOLORS
};
extern const Color PALETTE[C_NCOLORS];

static inline float clampf(float v, float lo, float hi)
{
    return v < lo ? lo : (v > hi ? hi : v);
}

static inline bool rect_overlap(const Rectf *a, const Rectf *b)
{
    return a->x < b->x + b->w && a->x + a->w > b->x &&
           a->y < b->y + b->h && a->y + a->h > b->y;
}

/* Small deterministic xorshift RNG. State lives in the Game struct. */
uint32_t rng_u32(uint32_t *s);
int rng_range(uint32_t *s, int lo, int hi);   /* [lo, hi) */
float rng_float(uint32_t *s);                 /* [0, 1) */

#endif
