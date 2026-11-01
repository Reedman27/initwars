/* bot.h - a configurable, human-ish autoplayer used by the tests and the balance report.
 * It only sees the Game struct and produces Input, exactly like a keyboard would. */
#ifndef BOT_H
#define BOT_H

#include <math.h>
#include <string.h>
#include "game.h"

typedef struct {
    const char *name;
    int reaction;        /* frames between movement decisions (human latency) */
    float dodge_radius;  /* only reacts to bullets this close */
    float ignore;        /* chance per decision to ignore bullets entirely (inattention) */
    float special_use;   /* chance per ready-frame to press special when in trouble */
} BotSkill;

static const BotSkill BOT_NOVICE = { "novice", 16, 14.0f, 0.35f, 0.02f };
static const BotSkill BOT_AVERAGE = { "average", 10, 20.0f, 0.20f, 0.04f };
static const BotSkill BOT_EXPERT = { "expert", 4, 30.0f, 0.04f, 0.08f };

typedef struct { int hold; float mx, my; uint32_t rng; } Bot;

static inline float bot_rnd(Bot *b)
{
    uint32_t x = b->rng ? b->rng : 1u;
    x ^= x << 13; x ^= x >> 17; x ^= x << 5; b->rng = x;
    return (float)(x >> 8) / 16777216.0f;
}

static inline void bot_input(Bot *b, const BotSkill *sk, const Game *g, Input *in, int target_ch, int step)
{
    memset(in->held, 0, sizeof in->held);
    memset(in->pressed, 0, sizeof in->pressed);
    bool pulse = (step % 3) == 0;
    switch (g->state) {
    case ST_BOOT: in->pressed[K_ANY] = pulse; return;
    case ST_MENU: in->pressed[K_CONFIRM] = pulse; return;
    case ST_SELECT:
        if (g->ch_sel != target_ch) in->pressed[K_RIGHT] = pulse; else in->pressed[K_CONFIRM] = pulse;
        return;
    case ST_STORY: case ST_UPGRADE: case ST_RESULTS: in->pressed[K_CONFIRM] = pulse; in->pressed[K_ANY] = pulse; return;
    case ST_PANIC: case ST_ENDING: in->pressed[K_ANY] = pulse; in->pressed[K_CONFIRM] = pulse; return;
    case ST_PLAY: break;
    default: return;
    }

    if (b->hold <= 0) {
        b->hold = sk->reaction;
        float tx = g->p.x, ty = g->p.y, best = 1e9f;
        for (int i = 0; i < MAX_ENEMY; i++) {
            const Enemy *e = &g->en[i];
            if (!e->on || e->type == E_FAKE) continue;
            float d = (e->x - g->p.x) * (e->x - g->p.x) + (e->y - g->p.y) * (e->y - g->p.y);
            if (d < best) { best = d; tx = e->x; ty = e->y; }
        }
        float dx = tx - g->p.x, dy = ty - g->p.y, d = sqrtf(dx * dx + dy * dy) + 0.01f;
        float mx = 0, my = 0;
        if (d > 90) { mx = dx / d; my = dy / d; } else if (d < 60) { mx = -dx / d; my = -dy / d; }
        mx += -dy / d * 0.7f; my += dx / d * 0.7f;
        if (bot_rnd(b) >= sk->ignore) {
            for (int i = 0; i < MAX_EBUL; i++) {
                const EBullet *bl = &g->eb[i];
                if (!bl->on) continue;
                float bx = bl->x - g->p.x, by = bl->y - g->p.y, bd = bx * bx + by * by;
                if (bd < sk->dodge_radius * sk->dodge_radius) {
                    mx -= bx / (sqrtf(bd) + 1) * 2; my -= by / (sqrtf(bd) + 1) * 2;
                }
            }
        }
        if (g->p.x < AX0 + 25) mx += 1;
        if (g->p.x > AX1 - 25) mx -= 1;
        if (g->p.y < AY0 + 25) my += 1;
        if (g->p.y > AY1 - 25) my -= 1;
        b->mx = mx; b->my = my;
    }
    b->hold--;
    in->held[K_LEFT] = b->mx < -0.3f; in->held[K_RIGHT] = b->mx > 0.3f;
    in->held[K_UP] = b->my < -0.3f;   in->held[K_DOWN] = b->my > 0.3f;
    in->held[K_FIRE] = true;

    /* special: when ready and in trouble (shots close, low HP, or something touching distance) */
    if (g->p.special_t <= 0) {
        int near = 0;
        for (int i = 0; i < MAX_EBUL; i++) {
            const EBullet *bl = &g->eb[i];
            if (bl->on && (bl->x - g->p.x) * (bl->x - g->p.x) + (bl->y - g->p.y) * (bl->y - g->p.y) < 40 * 40) near++;
        }
        bool trouble = near >= 3 || (near >= 1 && g->p.hp <= 2);
        if (trouble && bot_rnd(b) < sk->special_use * 8) in->pressed[K_SPECIAL] = true;
        else if (bot_rnd(b) < sk->special_use * 0.02f) in->pressed[K_SPECIAL] = true;
    }
}

#endif
