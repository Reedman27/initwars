/* input.h - abstract per-frame input state (filled by main.c, bot.c or tests) */
#ifndef INPUT_H
#define INPUT_H

#include <stdbool.h>

typedef enum {
    K_UP, K_DOWN, K_LEFT, K_RIGHT, K_FIRE, K_SPECIAL, K_CONFIRM, K_ESC,
    K_1, K_2, K_3, K_ANY, K_CRT, K_MUTE, K_VOL_DN, K_VOL_UP, K_COUNT
} Key;

typedef struct {
    bool held[K_COUNT];
    bool pressed[K_COUNT]; /* edge: set for exactly one game update */
} Input;

static inline bool in_confirm(const Input *i)
{
    return i->pressed[K_FIRE] || i->pressed[K_CONFIRM];
}

#endif
