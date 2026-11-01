#include "view.h"
#include "font.h"
#include "codex.h"

#include <stdio.h>
#include <string.h>

#define ENDING_FRAMES 420

/* ------------------------------------------------------------------ helpers */

static void text_scaled(Frame *f, int x, int y, const char *s, int pal, int sc)
{
    for (; *s; s++, x += FONT_W * sc) {
        const uint8_t *g = font_glyph((unsigned char)*s);
        for (int c = 0; c < 5; c++)
            for (int r = 0; r < 7; r++)
                if ((g[c] >> r) & 1) fr_rect(f, x + c * sc, y + r * sc, sc, sc, pal);
    }
}

static void text_scaled_center(Frame *f, int cx, int y, const char *s, int pal, int sc)
{
    int w = (int)strlen(s) * FONT_W * sc - sc;
    text_scaled(f, cx - w / 2, y, s, pal, sc);
}

/* Greedy word wrap into fixed-size line buffers. */
#define WRAP_MAX 8
#define WRAP_LEN 64
static int wrap(const char *text, int maxc, char out[WRAP_MAX][WRAP_LEN])
{
    int n = 0;
    const char *p = text;
    if (maxc >= WRAP_LEN) maxc = WRAP_LEN - 1;
    while (*p && n < WRAP_MAX) {
        while (*p == ' ') p++;
        int len = (int)strlen(p);
        if (len <= maxc) { memcpy(out[n], p, (size_t)len + 1); n++; break; }
        int cut = maxc;
        while (cut > 0 && p[cut] != ' ') cut--;
        if (cut == 0) cut = maxc;
        memcpy(out[n], p, (size_t)cut);
        out[n][cut] = '\0';
        n++;
        p += cut;
    }
    return n;
}

static void ring(Frame *f, int cx, int cy, int r, int pal, int phase)
{
    for (int a = 0; a < 360; a += 6) {
        if (((a / 6) + phase) & 1) continue;
        float rad = (float)a * 0.0174533f;
        fr_pixel(f, cx + (int)(cosf(rad) * (float)r), cy + (int)(sinf(rad) * (float)r), pal);
    }
}

static bool blink(const Game *g, int period) { return (g->frame / period) & 1; }

static void uppercase(char *s) { for (; *s; s++) if (*s >= 'a' && *s <= 'z') *s -= 32; }

static void status_bar(Frame *f, int x, int y, int w, int h, float frac, int col, int back)
{
    fr_rect(f, x, y, w, h, back);
    int fw = (int)((float)w * clampf(frac, 0, 1));
    if (fw > 0) fr_rect(f, x, y, fw, h, col);
}

/* ------------------------------------------------------------------- arena */

static const Sprite *sprite_for(int type)
{
    switch (type) {
    case E_JOURNALD:  return &SPR_JOURNALD;
    case E_LOGIND:    return &SPR_LOGIND;
    case E_NETWORKD:  return &SPR_NETWORKD;
    case E_TIMER:     return &SPR_TIMER;
    case E_UDEVD:     return &SPR_UDEVD;
    case E_RESOLVED:  return &SPR_RESOLVED;
    case E_DEPTARGET: return &SPR_DEPTARGET;
    case E_USB:       return &SPR_USB;
    case E_FLOPPY:    return &SPR_FLOPPY;
    case E_PRINTER:   return &SPR_PRINTER;
    case E_MARKER:    return &SPR_MARKER;
    default:          return &SPR_JOURNALD;
    }
}

static void draw_terminal_prop(Frame *f, int x, int y, bool lit, int frame)
{
    fr_rect(f, x, y, 12, 9, C_GREY);
    fr_rect(f, x + 1, y + 1, 10, 6, lit ? C_DKGREEN : C_BLACK);
    if (lit && ((frame / 30) & 1)) fr_rect(f, x + 2, y + 5, 3, 1, C_GREEN);
    if (!lit) { fr_pixel(f, x + 5, y + 3, C_GREY); fr_pixel(f, x + 6, y + 4, C_GREY); }
    fr_rect(f, x + 4, y + 7, 4, 2, C_GREY);
}

static void draw_arena_bg(const Game *g, Frame *f)
{
    for (int y = AY0 + 8; y < AY1; y += 16)
        for (int x = AX0 + 8; x < AX1; x += 16) fr_pixel(f, x, y, C_DKGREEN);
    fr_box(f, AX0 - 1, AY0 - 1, AX1 - AX0 + 2, AY1 - AY0 + 2, C_DKGREEN);
    int off = (g->wave >= 4) + (g->wave >= 9) + (g->wave >= 14);   /* old terminals go dark as the campaign goes on */
    if (g->state == ST_MENU || g->state == ST_SELECT) off = 0;
    int px[4] = { AX0 + 3, AX1 - 15, AX0 + 3, AX1 - 15 };
    int py[4] = { AY0 + 3, AY0 + 3, AY1 - 12, AY1 - 12 };
    for (int i = 0; i < 4; i++) draw_terminal_prop(f, px[i], py[i], i >= off, g->frame);
    /* background log flood while a journald is alive: dim, behind everything */
    bool journald = false;
    for (int i = 0; i < MAX_ENEMY; i++) if (g->en[i].on && g->en[i].type == E_JOURNALD) journald = true;
    if (journald && g->state == ST_PLAY) {
        static const char *const LOG[] = { "systemd[1]: Started unit.", "kernel: eth0 link up", "journald: rotated",
                                           "NOTICE: still running", "dbus: activating", "udev: new device" };
        for (int i = 0; i < 5; i++) {
            int y = AY0 + 18 + i * 34;
            int x = AX1 - ((g->frame * (1 + i % 2) / 2 + i * 70) % (AX1 - AX0 + 150));
            fr_text(f, x, y, LOG[i % 6], C_DKGREEN);
        }
    }
}

static void draw_enemy(const Game *g, Frame *f, const Enemy *e)
{
    int x = (int)e->x, y = (int)e->y;
    if (e->type == E_BOSS) {
        int bx = x - 24, by = y - 16;
        bool inv = g->boss_inv > 0 || (g->boss_phase >= 4 && !g->boss_exposed);
        int body = e->flash ? C_WHITE : C_DKRED;
        if (e->warp > 0 && (g->frame & 1)) return;
        for (int i = 0; i < 6; i++) fr_rect(f, bx + 3 + i * 8, by - 3, 4, 3, C_GREY);   /* gear teeth */
        fr_rect(f, bx, by, 48, 32, body);
        fr_box(f, bx, by, 48, 32, (e->state == 2 && blink(g, 4)) ? C_WHITE : (inv ? C_CYAN : C_RED));
        int dx = g->p.x > e->x ? 1 : (g->p.x < e->x ? -1 : 0);
        fr_rect(f, bx + 8, by + 6, 10, 7, C_AMBER);  fr_rect(f, bx + 30, by + 6, 10, 7, C_AMBER);
        fr_rect(f, bx + 11 + dx, by + 8, 4, 4, C_BLACK); fr_rect(f, bx + 33 + dx, by + 8, 4, 4, C_BLACK);
        fr_rect(f, bx + 6, by + 3, 14, 2, C_BLACK);  fr_rect(f, bx + 28, by + 3, 14, 2, C_BLACK);  /* angry brows */
        fr_text_center(f, x, by + 21, "systemd", C_WHITE);
        return;
    }
    int w = ENEMIES[e->type].w, h = ENEMIES[e->type].h;
    if (e->type == E_CRON) {
        int col = e->flash ? C_WHITE : C_CYAN;
        fr_box(f, x - 7, y - 7, 14, 14, col);
        fr_rect(f, x - 1, y - 5, 2, 5, C_WHITE);
        fr_rect(f, x, y - 1, 4, 2, C_AMBER);
        fr_pixel(f, x - 4, y - 4, C_AMBER); fr_pixel(f, x + 4, y + 4, C_AMBER);
        if (e->state == 1 && blink(g, 4)) fr_box(f, x - 9, y - 9, 18, 18, C_WHITE);
        status_bar(f, x - 7, y + 9, 14, 2, e->hp / e->maxhp, C_RED, C_DKRED);
        return;
    }
    if (e->type == E_OOMD) {
        int col = e->flash ? C_WHITE : C_RED;
        fr_rect(f, x - 7, y - 6, 14, 12, C_DKRED);
        fr_box(f, x - 7, y - 6, 14, 12, col);
        fr_rect(f, x - 4, y - 3, 3, 3, C_AMBER); fr_rect(f, x + 2, y - 3, 3, 3, C_AMBER);
        fr_rect(f, x - 4, y + 3, 8, 2, C_WHITE);
        if (e->sub == 1 && blink(g, 3)) fr_box(f, x - 10, y - 9, 20, 18, C_AMBER);
        status_bar(f, x - 8, y + 9, 16, 2, e->hp / e->maxhp, C_RED, C_DKRED);
        return;
    }
    if (e->type == E_FAKE) {
        if ((g->frame + (int)e->x) % 29 < 3) return;           /* flicker: a tell */
        const Sprite *s = sprite_for(e->sub);
        fr_sprite(f, s, x - s->w / 2, y - s->h / 2, C_GREEN, T_DIM, false);
        return;
    }
    const Sprite *s = sprite_for(e->type);
    Tint t = T_NORMAL;
    if (e->warp > 0) t = T_GHOST;
    else if (e->flash > 0) t = T_FLASH;
    else if (e->state == 1 && blink(g, 3)) t = T_FLASH;
    int sx = x - s->w / 2, sy = y - s->h / 2;
    if (e->type == E_MARKER && e->warp == 0 && blink(g, 8) == false && e->t > 40) t = T_DIM;
    fr_sprite(f, s, sx, sy, C_GREEN, t, false);
    (void)w; (void)h;
    if (e->restart && !e->restarted) {                           /* Restart=always badge */
        fr_rect(f, sx - 1, sy - 5, 7, 6, C_DKAMBER);
        fr_text(f, sx, sy - 5, "R", C_AMBER);
    }
    if (e->link >= 0 && e->warp == 0) {
        const Enemy *b = &g->en[e->link];
        fr_hline_dotted(f, x < (int)b->x ? x : (int)b->x, x < (int)b->x ? (int)b->x : x, y, C_AMBER, g->frame / 2);
        fr_vline_dotted(f, (int)b->x, y < (int)b->y ? y : (int)b->y, y < (int)b->y ? (int)b->y : y, C_AMBER, g->frame / 2);
        fr_box(f, sx - 1, sy - 1, s->w + 2, s->h + 2, C_DKAMBER);
    }
    if (e->type == E_TIMER && e->state == 2) {                    /* visible countdown */
        char d[16];
        snprintf(d, sizeof d, "%d", (int)(e->t2 / 30.0f) + 1);
        fr_text_center(f, x, sy - 10, d, blink(g, 5) ? C_RED : C_WHITE);
    }
    if (e->type == E_MARKER && e->warp == 0) {
        char d[16];
        snprintf(d, sizeof d, "%d", (int)(e->t / 60.0f) + 1);
        fr_text_center(f, x, sy - 9, d, C_AMBER);
    }
    if (e->type == E_DEPTARGET || e->hp < e->maxhp) {
        if (e->type != E_MARKER && e->type != E_FAKE)
            status_bar(f, sx, sy + s->h + 2, s->w, 2, e->hp / e->maxhp, C_RED, C_DKRED);
    }
}

static void draw_player(const Game *g, Frame *f)
{
    const Player *p = &g->p;
    int x = (int)p->x, y = (int)p->y;
    const Sprite *s = &SPR_PLAYER[p->ch];
    bool weak = p->ch == CH_SYSVINIT && g->sysv_weak;
    Tint t = weak ? T_WORN : T_NORMAL;
    if (p->hurt_flash > 0) t = T_FLASH;
    bool hide = p->inv > 0 && p->dash_t == 0 && p->hurt_flash == 0 && ((g->frame / 3) & 1);
    int bob = 0;
    if (p->idle_t > 0 && !weak) bob = (g->frame / 20) & 1;
    if (weak) bob = (g->frame / 40) & 1;                          /* slower, heavier idle */
    if (!hide) fr_sprite(f, s, x - 6, y - 6 + bob, CHARS[p->ch].color, t, p->fx < -0.1f);
    if (weak && p->idle_t > 120 && ((p->idle_t / 90) & 1))        /* peeks at the outdated boot manual */
        fr_sprite(f, &SPR_MANUAL, x + 4, y + 1, C_WHITE, T_NORMAL, false);
    if (p->shield_t > 0 && (p->shield_t > 40 || blink(g, 3))) ring(f, x, y, 9, C_WHITE, g->frame / 4);
    if (p->stasis_t > 0) ring(f, x, y, (int)STASIS_RADIUS, C_CYAN, g->frame / 6);
    fr_pixel(f, x, y, hide ? C_BLACK : C_WHITE);                  /* hitbox marker */
}

static void draw_play_world(const Game *g, Frame *f)
{
    draw_arena_bg(g, f);
    for (int i = 0; i < MAX_LINES; i++) {
        const NetLine *n = &g->lines[i];
        if (!n->on) continue;
        if (n->age < n->tele) {
            if (n->vertical) fr_vline_dotted(f, (int)n->x0, (int)n->y0, (int)n->y1, C_CYAN, 0);
            else fr_hline_dotted(f, (int)n->x0, (int)n->x1, (int)n->y0, C_CYAN, 0);
        } else if (n->vertical) {
            fr_rect(f, (int)n->x0 - 1, (int)n->y0, 2, (int)(n->y1 - n->y0), C_CYAN);
            fr_vline_dotted(f, (int)n->x0 + 1, (int)n->y0, (int)n->y1, C_WHITE, g->frame / 2);
        } else {
            fr_rect(f, (int)n->x0, (int)n->y0 - 1, (int)(n->x1 - n->x0), 2, C_CYAN);
            fr_hline_dotted(f, (int)n->x0, (int)n->x1, (int)n->y0 + 1, C_WHITE, g->frame / 2);
        }
    }
    for (int i = 0; i < MAX_BARRIER; i++) {
        const Barrier *b = &g->bars[i];
        if (!b->on) continue;
        if (b->age < b->tele) {
            if (blink(g, 4)) fr_box(f, (int)b->r.x, (int)b->r.y, (int)b->r.w, (int)b->r.h, C_AMBER);
        } else {
            fr_rect(f, (int)b->r.x, (int)b->r.y, (int)b->r.w, (int)b->r.h, C_AMBER);
            fr_box(f, (int)b->r.x, (int)b->r.y, (int)b->r.w, (int)b->r.h, C_DKAMBER);
        }
    }
    for (int i = 0; i < MAX_PICK; i++) {
        const Pickup *k = &g->picks[i];
        if (!k->on || (k->life < 120 && blink(g, 4))) continue;
        fr_sprite(f, &SPR_FLOPPY, (int)k->x - 4, (int)k->y - 4, C_BGREEN, T_NORMAL, false);
    }
    for (int i = 0; i < MAX_ENEMY; i++) if (g->en[i].on) draw_enemy(g, f, &g->en[i]);
    for (int i = 0; i < MAX_EBUL; i++) {
        const EBullet *b = &g->eb[i];
        if (!b->on) continue;
        if (b->kind) {
            fr_rect(f, (int)b->x - 2, (int)b->y - 2, 5, 5, C_RED);
            fr_rect(f, (int)b->x - 1, (int)b->y - 1, 3, 3, C_WHITE);
        } else {
            fr_rect(f, (int)b->x - 1, (int)b->y - 1, 3, 3, C_AMBER);
        }
    }
    /* aim reticle on whoever the auto-aim will pick */
    const Enemy *best = NULL;
    float bd = 1e9f;
    for (int i = 0; i < MAX_ENEMY; i++) {
        const Enemy *e = &g->en[i];
        if (!e->on || e->warp > 0 || e->type == E_FAKE) continue;
        float dx = e->x - g->p.x, dy = e->y - g->p.y, d = dx * dx + dy * dy;
        if (d < bd) { bd = d; best = e; }
    }
    if (best && g->state == ST_PLAY) {
        int rx = (int)best->x, ry = (int)best->y, r = best->type == E_BOSS ? 20 : 9;
        fr_pixel(f, rx - r, ry - r, C_GREEN); fr_pixel(f, rx + r, ry - r, C_GREEN);
        fr_pixel(f, rx - r, ry + r, C_GREEN); fr_pixel(f, rx + r, ry + r, C_GREEN);
    }
    draw_player(g, f);
    for (int i = 0; i < MAX_PBUL; i++) {
        const PBullet *b = &g->pb[i];
        if (b->on) fr_rect(f, (int)b->x - 1, (int)b->y - 1, 3, 3, C_BGREEN);
    }
    for (int i = 0; i < MAX_PART; i++) {
        const Particle *p = &g->parts[i];
        if (p->on) fr_pixel(f, (int)p->x, (int)p->y, p->color);
    }
    for (int i = 0; i < MAX_POPUP; i++) {
        const Popup *p = &g->pops[i];
        if (p->on) fr_text_center(f, (int)p->x, (int)p->y, p->text, p->color);
    }
}

static void draw_hud(const Game *g, Frame *f)
{
    const Player *p = &g->p;
    const CharDef *c = &CHARS[p->ch];
    fr_rect(f, 0, 0, LW, 23, C_BLACK);
    fr_rect(f, 0, 22, LW, 1, C_DKGREEN);
    char b[64];
    bool weak = p->ch == CH_SYSVINIT && g->sysv_weak;
    fr_text(f, 6, 3, c->name, weak ? color_dark(c->color) : c->color);
    if (weak) fr_text(f, 6 + text_width(c->name) + 6, 3, "LEGACY", C_AMBER);
    for (int i = 0; i < p->maxhp; i++) {
        int x = 6 + i * 8;
        bool low_blink = p->hp == 1 && i == 0 && blink(g, 8);
        fr_rect(f, x, 13, 6, 6, i < p->hp ? (low_blink ? C_WHITE : C_RED) : C_DKRED);
        if (i >= p->hp) fr_rect(f, x + 1, 14, 4, 4, C_BLACK);
    }
    if (g->boss_rush) snprintf(b, sizeof b, "BOSS %d/8", g->wave);
    else if (g->endless) snprintf(b, sizeof b, "WAVE %d / INF", g->wave);
    else snprintf(b, sizeof b, "WAVE %d/%d", g->wave, NUM_WAVES);
    fr_text_center(f, 160, 3, b, C_BGREEN);
    snprintf(b, sizeof b, "SCORE %06d", g->score);
    fr_text_right(f, LW - 6, 3, b, C_AMBER);

    /* special: label above a thin recharge bar, so the text is always readable */
    int cd = game_special_cd(g);
    float ready = p->special_t <= 0 ? 1.0f : 1.0f - (float)p->special_t / (float)cd;
    int bx = 160 - 38;
    status_bar(f, bx, 20, 76, 2, ready, p->special_t <= 0 ? C_CYAN : C_GREY, C_DKGREEN);
    if (p->special_t <= 0) {
        snprintf(b, sizeof b, "SHIFT %s", c->special_name);
        fr_text_center(f, 160, 11, b, blink(g, 20) ? C_WHITE : C_CYAN);
    } else {
        snprintf(b, sizeof b, "%s %d%%", c->special_name, (int)(ready * 100));
        fr_text_center(f, 160, 11, b, C_GREY);
    }
    /* boss bar or wave name */
    const Enemy *boss = game_boss(g);
    if (boss) {
        status_bar(f, LW - 6 - 56, 13, 56, 6, boss->hp / boss->maxhp, C_RED, C_DKRED);
        fr_text(f, LW - 6 - 56 - 46, 13, "SYSTEMD", C_RED);
    } else {
        if (g->boss_rush) snprintf(b, sizeof b, "BOSS RUSH");
        else if (g->endless) snprintf(b, sizeof b, g->boss_wave ? "BOSS ROUND" : "ENDLESS MODE");
        else { strncpy(b, WAVE_NAMES[g->wave < 1 ? 0 : (g->wave > NUM_WAVES ? NUM_WAVES - 1 : g->wave - 1)], sizeof b - 1); b[sizeof b - 1] = '\0'; }
        fr_text_right(f, LW - 6, 13, b, C_GREY);
    }
    /* fire cooldown pip */
    float fr = p->fire_t > 0 ? 1.0f - (float)p->fire_t / (float)game_player_fire_cd(g) : 1.0f;
    status_bar(f, 6, 20, 40, 1, fr, C_GREEN, C_BLACK);
}

static void draw_status_lines(const Game *g, Frame *f)
{
    fr_rect(f, 0, 213, LW, 27, C_BLACK);
    fr_rect(f, 0, 213, LW, 1, C_DKGREEN);
    char b[80];
    snprintf(b, sizeof b, "  %s", g->status[1]);
    uppercase(b);
    fr_text(f, 2, 218, b, C_GREY);
    snprintf(b, sizeof b, "> %s", g->status[0]);
    uppercase(b);
    int col = g->status_col[0];
    if (g->status_t > 360) col = C_GREEN;
    fr_text(f, 2, 228, b, col);
    if (blink(g, 30) && 2 + text_width(b) + 8 <= LW) fr_rect(f, 2 + text_width(b) + 3, 228, 5, 7, C_GREEN);
}

static void draw_play(const Game *g, Frame *f)
{
    Frame tmp;
    Frame *t = f;
    int ox = 0, oy = 0;
    if (g->shake > 0 && !g->no_shake) {
        ox = (g->frame % 3) - 1;
        oy = ((g->frame / 2) % 3) - 1;
        t = &tmp;
    }
    fr_clear(t, C_BLACK);
    draw_play_world(g, t);
    if (t != f) {   /* cheap screen shake: shift the whole composed frame */
        for (int y = 0; y < LH; y++)
            for (int x = 0; x < LW; x++) {
                int sx = x - ox, sy = y - oy;
                f->px[y * LW + x] = (sx >= 0 && sx < LW && sy >= 0 && sy < LH) ? tmp.px[sy * LW + sx] : 0;
            }
    }
    draw_hud(g, f);
    draw_status_lines(g, f);

    /* feedback overlays: red border when hit, white border on big events, expanding rings for specials */
    if (g->hurt_flash_t > 0 && (g->frame & 2)) fr_box(f, AX0 - 1, AY0 - 1, AX1 - AX0 + 2, AY1 - AY0 + 2, C_RED), fr_box(f, AX0, AY0, AX1 - AX0, AY1 - AY0, C_RED);
    if (g->flash_t > 0) fr_box(f, AX0 - 1, AY0 - 1, AX1 - AX0 + 2, AY1 - AY0 + 2, C_WHITE), fr_box(f, AX0, AY0, AX1 - AX0, AY1 - AY0, C_WHITE);
    for (int i = 0; i < 2; i++) {
        if (!g->rings[i].on) continue;
        int r = g->rings[i].max_r * g->rings[i].t / 16;
        ring(f, (int)g->rings[i].x, (int)g->rings[i].y, r, g->rings[i].color, 0);
        ring(f, (int)g->rings[i].x, (int)g->rings[i].y, r > 3 ? r - 3 : r, g->rings[i].color, 1);
    }
    if (g->state == ST_PLAY && !g->endless && g->wave == 1 && g->run_frames < 900 && g->banner_t <= 0) {
        fr_rect(f, 28, 174, 264, 30, C_BLACK);
        fr_box(f, 28, 174, 264, 30, C_DKGREEN);
        fr_text_center(f, 160, 179, "WASD / ARROWS: MOVE      ESC: PAUSE", C_GREEN);
        fr_text_center(f, 160, 191, "SPACE: FIRE (AUTO-AIM)   SHIFT: SPECIAL", C_GREEN);
    }
    if (g->banner_t > 0) {
        fr_rect(f, 40, 48, 240, 50, C_BLACK);
        fr_box(f, 40, 48, 240, 50, g->banner_t > 20 || blink(g, 3) ? C_AMBER : C_DKAMBER);
        text_scaled_center(f, 160, 54, g->banner_text[0], C_AMBER, 2);
        fr_text_center(f, 160, 78, g->banner_text[1], C_LTGREY);
    }

    if (g->phase == PH_INTRO && g->state == ST_PLAY && g->banner_t <= 0) {
        char b[24];
        if (g->boss_wave) snprintf(b, sizeof b, "PID 1");
        else snprintf(b, sizeof b, "WAVE %d", g->wave);
        if (g->phase_t < 80 || blink(g, 4)) {
            text_scaled_center(f, 160, 78, b, C_AMBER, 3);
            fr_text_center(f, 160, 106, g->endless ? (g->boss_wave ? "BOSS ROUND" : "ENDLESS MODE") : WAVE_NAMES[g->wave - 1], C_GREEN);
        }
    } else if (g->phase == PH_CLEAR) {
        text_scaled_center(f, 160, 84, "WAVE CLEARED", C_BGREEN, 2);
    }
    const Enemy *boss = game_boss(g);
    if (boss && g->boss_phase >= 4 && !g->boss_exposed) {
        char b[40];
        snprintf(b, sizeof b, "SURVIVE %d", g->boss_survive / 60 + 1);
        status_bar(f, 100, 190, 120, 4, (float)g->boss_survive / (float)BOSS_SURVIVE_FRAMES, C_CYAN, C_DKGREEN);
        fr_text_center(f, 160, 198, b, blink(g, 10) ? C_CYAN : C_WHITE);
    } else if (boss && g->boss_exposed && g->boss_phase >= 4 && blink(g, 12)) {
        fr_text_center(f, 160, 198, "EXPOSED!", C_BGREEN);
    }
}

/* ------------------------------------------------------------- other views */

static void draw_lineup(const Game *g, Frame *f, int y, int scale, int highlight)
{
    int step = 14 * scale + 10;
    int x0 = 160 - (step * (CH_COUNT - 1)) / 2 - 6 * scale;
    for (int i = 0; i < CH_COUNT; i++) {
        int bob = ((g->frame / 24 + i) & 1) ? scale : 0;
        Tint t = (highlight >= 0 && highlight != i) ? T_DIM : T_NORMAL;
        fr_sprite_scaled(f, &SPR_PLAYER[i], x0 + i * step, y - bob, CHARS[i].color, t, scale);
    }
}

/* "VOLUME" label plus ten cells; used by the menus and the small hotkey readout. */
static void draw_volume_cells(const Game *g, Frame *f, int x, int y, int col)
{
    for (int i = 0; i < VOL_MAX; i++) {
        bool on = i < g->volume;
        int c = g->mute ? (on ? C_GREY : C_DKGREEN) : (on ? col : C_DKGREEN);
        fr_rect(f, x + i * 6, y, 5, 7, c);
    }
}

static void draw_volume_row(const Game *g, Frame *f, int y, bool sel)
{
    const char *label = g->mute ? "MUTED" : "VOLUME";
    int lw = text_width("VOLUME");
    int cx = lw + 16;                                   /* where the cells start, relative to the label */
    int x0 = 160 - (cx + VOL_MAX * 6) / 2;
    fr_text(f, x0, y, label, sel ? C_BGREEN : C_GREEN);
    draw_volume_cells(g, f, x0 + cx, y, sel ? C_AMBER : C_GREEN);
    if (sel) {
        fr_text(f, x0 - 12, y, ">", C_AMBER);
        fr_text(f, x0 + cx - 9, y, "<", C_AMBER);
        fr_text(f, x0 + cx + VOL_MAX * 6 + 3, y, ">", C_AMBER);
    }
}

static void draw_volume_readout(const Game *g, Frame *f)
{
    int w = 94, h = 13, x = LW - w - 4, y = 26;
    fr_rect(f, x, y, w, h, C_BLACK);
    fr_box(f, x, y, w, h, C_DKGREEN);
    fr_text(f, x + 4, y + 3, g->mute ? "MUTE" : "VOL", C_GREY);
    draw_volume_cells(g, f, x + 30, y + 3, C_AMBER);
}

static void draw_boot(const Game *g, Frame *f)
{
    fr_clear(f, C_BLACK);
    int n = g->state_t / 6;
    if (n > BOOT_LINE_COUNT) n = BOOT_LINE_COUNT;
    for (int i = 0; i < n; i++) {
        char b[80];
        snprintf(b, sizeof b, "%s", BOOT_LINES[i]);
        fr_text(f, 8, 12 + i * 10, b, i == 8 ? C_RED : (i == 0 ? C_AMBER : C_GREEN));
    }
    if (blink(g, 20)) fr_rect(f, 8, 12 + n * 10, 5, 7, C_GREEN);
    fr_text_center(f, 160, 226, "PRESS ANY KEY TO SKIP", C_GREY);
}

static void draw_menu(const Game *g, Frame *f)
{
    fr_clear(f, C_BLACK);
    draw_arena_bg(g, f);
    fr_rect(f, 24, 18, 272, 66, C_BLACK);
    text_scaled_center(f, 160, 22, "INIT WARS", C_AMBER, 4);
    text_scaled_center(f, 160, 56, "THE LAST PID", C_GREEN, 2);
    fr_text_center(f, 160, 74, "A SMALL UNIX ARCADE GAME", C_GREY);
    fr_text_right(f, 314, 6, "V" GAME_VERSION, C_GREY);   /* so you can tell which build you are running */
    static const char *const names[MENU_COUNT] = { "CAMPAIGN", "ENDLESS MODE", "BOSS RUSH", "HIGH SCORES", "ACHIEVEMENTS", "RUN STATS", "CODEX", "SETTINGS", "", "", "CREDITS", "QUIT" };
    for (int i = 0; i < MENU_COUNT; i++) {
        char b[40];
        if (i == MENU_CRT) snprintf(b, sizeof b, "CRT SCANLINES: %s", g->crt ? "ON" : "OFF");
        else snprintf(b, sizeof b, "%s", names[i]);
        int y = 86 + (i * 17) / 2;      /* 12 rows: alternate 8 and 9 px steps */
        bool sel = i == g->menu_sel;
        if (i == MENU_VOLUME) { draw_volume_row(g, f, y, sel); continue; }
        fr_text_center(f, 160, y, b, sel ? C_BGREEN : C_GREEN);
        if (sel && blink(g, 30)) {
            fr_text(f, 160 - text_width(b) / 2 - 12, y, ">", C_AMBER);
        }
    }
    draw_lineup(g, f, 191, 2, -1);
    fr_text_center(f, 160, 216, "WASD/ARROWS MOVE  SPACE FIRE  SHIFT SPECIAL", C_GREY);
    fr_text_center(f, 160, 226, "ENTER SELECT  ESC PAUSE  F2 CRT  M MUTE  +/- VOLUME", C_GREY);
}

static void draw_stat_row(Frame *f, int y, const char *label, float frac, int col)
{
    fr_text(f, 30, y, label, C_GREY);
    status_bar(f, 80, y + 1, 120, 5, frac, col, C_DKGREEN);
}

static void draw_select(const Game *g, Frame *f)
{
    fr_clear(f, C_BLACK);
    text_scaled_center(f, 160, 8, "SELECT INIT SYSTEM", C_AMBER, 2);
    int step = 58, x0 = 160 - step * 2;
    for (int i = 0; i < CH_COUNT; i++) {
        int cx = x0 + i * step;
        bool sel = i == g->ch_sel;
        int bob = sel && blink(g, 15) ? 2 : 0;
        if (sel) fr_box(f, cx - 20, 36, 40, 44, C_AMBER);
        fr_sprite_scaled(f, &SPR_PLAYER[i], cx - 18, 40 - bob, CHARS[i].color, sel ? T_NORMAL : T_DIM, 3);
    }
    const CharDef *c = &CHARS[g->ch_sel];
    text_scaled_center(f, 160, 90, c->name, c->color, 2);
    fr_text_center(f, 160, 110, c->tagline, C_LTGREY);
    float dps = c->damage * 60.0f / (float)c->fire_cd;
    draw_stat_row(f, 126, "HEALTH", (float)c->max_hp / 5.0f, C_RED);
    draw_stat_row(f, 136, "SPEED", c->speed / 2.0f, C_GREEN);
    draw_stat_row(f, 146, "DAMAGE", dps / 10.0f, C_AMBER);
    draw_stat_row(f, 156, "HITBOX", (float)c->half_hit / 4.0f, C_CYAN);
    char b[80];
    snprintf(b, sizeof b, "SPECIAL: %s", c->special_name);
    fr_text_center(f, 160, 172, b, C_CYAN);
    fr_text_center(f, 160, 182, c->special_desc, C_GREY);
    if (g->ch_sel == CH_SYSVINIT)
        fr_text_center(f, 160, 192, "LEGACY VETERAN: HIS STORY CHANGES HIM", C_DKAMBER);
    static const char *const modes[SCORE_MODE_COUNT] = { "CAMPAIGN", "ENDLESS MODE", "BOSS RUSH" };
    char mode_line[64];
    snprintf(mode_line, sizeof mode_line, "MODE: %s", modes[g->select_mode]);
    fr_text_center(f, 160, 202, mode_line, C_AMBER);
    if (g->boss_rush) fr_text_center(f, 160, 212, "EIGHT ESCALATING SYSTEMD FIGHTS", C_DKAMBER);
    else if (g->endless) fr_text_center(f, 160, 212, "SURVIVE THE INCREASING THREAT", C_DKAMBER);
    else fr_text_center(f, 160, 212, "24 WAVES, STORY AND FINAL BOSS", C_DKAMBER);
    fr_text_center(f, 160, 222, "LEFT/RIGHT CHARACTER   UP/DOWN MODE", C_GREEN);
    fr_text_center(f, 160, 232, "ENTER START   ESC BACK", C_GREEN);
}

static int story_speaker(const StoryLine *ln)
{
    if (!strcmp(ln->who, "SysVinit")) return CH_SYSVINIT;
    if (!strcmp(ln->who, "runit")) return CH_RUNIT;
    if (!strcmp(ln->who, "s6")) return CH_S6;
    if (!strcmp(ln->who, "OpenRC")) return CH_OPENRC;
    if (!strcmp(ln->who, "BusyBox")) return CH_BUSYBOX;
    return -1; /* Dinit has its own terminal/graph mark, not a player sprite. */
}

static bool story_is_dinit(const StoryLine *ln)
{
    return strcmp(ln->who, "Dinit") == 0;
}

/* Guests who are not playable have a small terminal mark instead of a sprite: Dinit (cyan D), Upstart (amber U). */
static bool story_guest(const StoryLine *ln, char *letter, int *col)
{
    if (story_is_dinit(ln))              { *letter = 'D'; *col = C_CYAN; return true; }
    if (!strcmp(ln->who, "Upstart"))     { *letter = 'U'; *col = C_AMBER; return true; }
    return false;
}

static void draw_guest_mark(Frame *f, int x, int y, int scale, char letter, int col)
{
    int w = 22 * scale, h = 22 * scale;
    char t[2] = { letter, '\0' };
    fr_rect(f, x, y, w, h, C_BLACK);
    fr_box(f, x, y, w, h, col);
    fr_text_center(f, x + w / 2, y + 5 * scale, t, C_WHITE);
    fr_line(f, x + 5 * scale, y + 17 * scale, x + 17 * scale, y + 17 * scale, col);
}

static void draw_dinit_graph(Frame *f)
{
    /* A tiny dependency graph: prerequisites feed the supervised service. */
    fr_box(f, 117, 92, 34, 15, C_CYAN);
    fr_box(f, 169, 92, 34, 15, C_CYAN);
    fr_box(f, 143, 116, 34, 14, C_BGREEN);
    fr_text_center(f, 134, 96, "NET", C_WHITE);
    fr_text_center(f, 186, 96, "LOG", C_WHITE);
    fr_text_center(f, 160, 119, "BOOT", C_WHITE);
    fr_line(f, 134, 107, 151, 116, C_CYAN);
    fr_line(f, 186, 107, 169, 116, C_CYAN);
    fr_text_center(f, 160, 83, "DINIT // DEPENDENCIES", C_CYAN);
}

/* Typed-out dialogue inside a box whose top-left is (bx, by); lines start at text_y. */
static void draw_story_text(const Game *g, Frame *f, const StoryLine *ln, int text_y)
{
    char lines[WRAP_MAX][WRAP_LEN];
    int n = wrap(ln->text, 41, lines);
    int typed = game_scene_chars(g);
    for (int i = 0; i < n && typed > 0; i++) {
        char part[WRAP_LEN];
        int len = (int)strlen(lines[i]);
        int take = typed < len ? typed : len;
        memcpy(part, lines[i], (size_t)take);
        part[take] = '\0';
        fr_text(f, 52, text_y + i * 10, part, ln->color);
        typed -= len + 1;
    }
}

/* A scene that interrupts the fight: the frozen arena stays visible, dimmed, with the dialogue below it. */
static void draw_story_inplay(const Game *g, Frame *f)
{
    const Scene *sc = story_scene((SceneId)g->scene);
    const StoryLine *ln = &sc->lines[g->scene_line];
    int speaker = story_speaker(ln);
    draw_play(g, f);
    fr_dim(f, 2, 5);
    draw_player(g, f);                                          /* the player stays at full brightness */
    int pr = (int)g->p.x, py = (int)g->p.y;
    ring(f, pr, py, 12 + ((g->frame / 8) & 1), C_AMBER, 0);
    static const char NOTICE[] = "MIGRATION NOTICE: ";
    if (g->scene == SC_REDHAT && !strncmp(ln->text, NOTICE, sizeof NOTICE - 1)) {   /* each notice is drawn as a pop-up terminal */
        int ny = py > 120 ? 108 : 78;
        fr_rect(f, 52, ny, 216, 30, C_BLACK);
        fr_box(f, 52, ny, 216, 30, blink(g, 12) ? C_AMBER : C_DKAMBER);
        fr_text_center(f, 160, ny + 6, "! MIGRATION NOTICE !", C_AMBER);
        fr_text_center(f, 160, ny + 18, ln->text + sizeof NOTICE - 1, C_LTGREY);
    }
    /* the dialogue sits on whichever half of the arena the player is not in, so it never hides them */
    int by = py > 120 ? 28 : 142;
    fr_rect(f, 8, by, 304, 68, C_BLACK);
    fr_box(f, 8, by, 304, 68, sc->quiet ? C_GREY : C_GREEN);
    fr_text(f, 14, by + 4, sc->name, C_AMBER);
    char who[40];
    snprintf(who, sizeof who, "%s", ln->who[0] ? ln->who : "SYSTEM LOG");
    uppercase(who);
    fr_text(f, 52, by + 14, who, ln->color);
    if (speaker >= 0) fr_sprite_scaled(f, &SPR_PLAYER[speaker], 14, by + 16, CHARS[speaker].color,
                                       (speaker == CH_SYSVINIT && g->sysv_weak) ? T_WORN : T_NORMAL, 2);
    else { char gl; int gc; if (story_guest(ln, &gl, &gc)) draw_guest_mark(f, 14, by + 16, 1, gl, gc); }
    draw_story_text(g, f, ln, by + 24);
    if (g->state_t < 40) {
        fr_text(f, 14, by + 57, "GET READY...", C_GREY);
    } else {
        fr_text(f, 14, by + 57, "[ESC] BACK TO THE FIGHT", C_GREY);
        if (game_scene_chars(g) >= (int)strlen(ln->text) && blink(g, 20)) fr_text_right(f, 304, by + 57, "[SPACE]", C_GREY);
    }
}

static void draw_story(const Game *g, Frame *f)
{
    if (g->scene_inplay) { draw_story_inplay(g, f); return; }
    const Scene *sc = story_scene((SceneId)g->scene);
    const StoryLine *ln = &sc->lines[g->scene_line];
    fr_clear(f, C_BLACK);
    draw_arena_bg(g, f);
    int speaker = story_speaker(ln);

    draw_lineup(g, f, story_is_dinit(ln) ? 52 : 72, 2, speaker);
    if (!strcmp(ln->who, "systemd") || (g->scene == SC_BOSS && g->scene_line >= 1)) {
        fr_rect(f, 120, 100, 80, 24, C_DKRED);
        fr_box(f, 120, 100, 80, 24, C_RED);
        fr_text_center(f, 160, 108, "systemd", C_WHITE);
    } else if (story_is_dinit(ln)) {
        draw_dinit_graph(f);
    }
    if (sc->quiet) fr_dim(f, 3, 7);                              /* subdued palette */

    fr_rect(f, 8, 134, 304, 76, C_BLACK);
    fr_box(f, 8, 134, 304, 76, sc->quiet ? C_GREY : C_GREEN);
    fr_text(f, 14, 138, sc->name, C_AMBER);
    char who[40];
    snprintf(who, sizeof who, "%s", ln->who[0] ? ln->who : "SYSTEM LOG");
    uppercase(who);
    fr_text(f, 52, 150, who, ln->color);
    if (speaker >= 0) fr_sprite_scaled(f, &SPR_PLAYER[speaker], 14, 150, CHARS[speaker].color,
                                       (speaker == CH_SYSVINIT && g->sysv_weak) ? T_WORN : T_NORMAL, 2);
    else { char gl; int gc; if (story_guest(ln, &gl, &gc)) draw_guest_mark(f, 14, 150, 1, gl, gc); }
    draw_story_text(g, f, ln, 162);
    if (game_scene_chars(g) >= (int)strlen(ln->text) && blink(g, 20))
        fr_text_right(f, 304, 198, "[SPACE]", C_GREY);
    fr_text(f, 14, 198, "[ESC] SKIP SCENE", C_GREY);
}

static void draw_upgrade(const Game *g, Frame *f)
{
    fr_clear(f, C_BLACK);
    draw_hud(g, f);
    char b[60];
    if (g->boss_rush_loadout > 0) snprintf(b, sizeof b, "BOSS RUSH LOADOUT");
    else snprintf(b, sizeof b, "WAVE %d CLEARED", g->wave);
    text_scaled_center(f, 160, 28, b, C_BGREEN, 2);
    if (g->boss_rush_loadout > 0) {
        char remaining[40];
        snprintf(remaining, sizeof remaining, "%d STARTING PICKS LEFT", g->boss_rush_loadout);
        fr_text_center(f, 160, 46, remaining, C_AMBER);
    } else fr_text_center(f, 160, 46, "CHOOSE A PATCH", C_AMBER);
    for (int i = 0; i < 3; i++) {
        int x = 14 + i * 100, y = 58;
        bool sel = i == g->upg_sel;
        fr_rect(f, x, y, 92, 90, C_BLACK);
        fr_box(f, x, y, 92, 90, sel ? C_AMBER : C_DKGREEN);
        snprintf(b, sizeof b, "[%d]", i + 1);
        fr_text(f, x + 6, y + 6, b, sel ? C_AMBER : C_GREY);
        char lines[WRAP_MAX][WRAP_LEN];
        int n = wrap(UPG_NAME[g->upg_opt[i]], 14, lines);
        for (int k = 0; k < n; k++) fr_text(f, x + 6, y + 22 + k * 10, lines[k], sel ? C_BGREEN : C_GREEN);
        n = wrap(UPG_DESC[g->upg_opt[i]], 14, lines);
        for (int k = 0; k < n; k++) fr_text(f, x + 6, y + 50 + k * 9, lines[k], C_LTGREY);
    }
    fr_text_center(f, 160, 170, "LEFT/RIGHT OR 1-3, THEN ENTER", C_GREEN);
    if (g->boss_rush_loadout > 0) fr_text_center(f, 160, 182, "BUILD YOUR STARTING CONFIGURATION", C_CYAN);
    fr_text_center(f, 160, 190, "SCORE", C_GREY);
    snprintf(b, sizeof b, "%06d", g->score);
    fr_text_center(f, 160, 200, b, C_AMBER);
}

static void draw_results(const Game *g, Frame *f)
{
    fr_clear(f, C_BLACK);
    text_scaled_center(f, 160, 12, g->victory ? "MACHINE RUNNING" : "SYSTEM FAILURE", g->victory ? C_BGREEN : C_RED, 2);
    char b[80];
    int y = 52;
    snprintf(b, sizeof b, "INIT SYSTEM   %s", CHARS[g->p.ch].name);
    fr_text_center(f, 160, y, b, CHARS[g->p.ch].color); y += 14;
    snprintf(b, sizeof b, "FINAL SCORE   %06d", g->score);
    fr_text_center(f, 160, y, b, C_AMBER); y += 14;
    if (g->boss_rush) snprintf(b, sizeof b, "BOSSES DEFEATED %d / 8", g->wave);
    else if (g->endless) snprintf(b, sizeof b, "WAVE REACHED  %d / INFINITE", g->wave);
    else snprintf(b, sizeof b, "WAVE REACHED  %d / %d", g->wave, NUM_WAVES);
    fr_text_center(f, 160, y, b, C_GREEN); y += 14;
    snprintf(b, sizeof b, "UNITS STOPPED %d", g->kills);
    fr_text_center(f, 160, y, b, C_GREEN); y += 14;
    int secs = g->run_frames / FPS;
    snprintf(b, sizeof b, "UPTIME        %d:%02d", secs / 60, secs % 60);
    fr_text_center(f, 160, y, b, C_GREEN); y += 20;
    if (g->last_rank >= 0) {
        snprintf(b, sizeof b, "NEW HIGH SCORE  RANK #%d", g->last_rank + 1);
        fr_text_center(f, 160, y, b, blink(g, 15) ? C_AMBER : C_WHITE);
    } else {
        fr_text_center(f, 160, y, "NO NEW HIGH SCORE", C_GREY);
    }
    if (g->save_failed) fr_text_center(f, 160, 176, "(COULD NOT SAVE HIGH SCORES - CHECK FILE PERMISSIONS)", C_DKAMBER);
    fr_text_center(f, 160, 200, g->victory ? "[ENTER] CONTINUE" : "[ENTER] TRY AGAIN   [ESC] MENU", C_GREEN);
}

static void draw_panic(const Game *g, Frame *f)
{
    fr_clear(f, C_BLACK);
    int n = g->state_t / 12;
    if (n > PANIC_LINE_COUNT) n = PANIC_LINE_COUNT;
    for (int i = 0; i < n; i++)
        fr_text(f, 6, 10 + i * 12, PANIC_LINES[i], i == 0 ? C_WHITE : C_LTGREY);
    if (g->state_t > PANIC_LINE_COUNT * 12 + 60 && blink(g, 25)) fr_text_center(f, 160, 222, "PRESS ANY KEY", C_GREY);
}

static void draw_ending(const Game *g, Frame *f)
{
    fr_clear(f, C_BLACK);
    fr_box(f, 20, 20, 280, 170, C_GREY);
    fr_box(f, 22, 22, 276, 166, C_DKGREEN);
    struct { int start; const char *text; int col; } L[] = {
        { 30,  "INIT: Reached runlevel 3.", C_GREEN },
        { 130, "Dinit: PID 1 accepted. Services stay free.", C_CYAN },
        { 250, "SysVinit: exit 0. Runlevels archived.", C_AMBER },
    };
    for (int i = 0; i < 3; i++) {
        int chars = (g->state_t - L[i].start) / 2;
        if (chars <= 0) continue;
        char b[64];
        int len = (int)strlen(L[i].text);
        if (chars > len) chars = len;
        memcpy(b, L[i].text, (size_t)chars);
        b[chars] = '\0';
        fr_text(f, 32, 40 + i * 24, b, L[i].col);
    }
    fr_sprite_scaled(f, &SPR_PLAYER[CH_SYSVINIT], 240, 130, C_LTGREY, T_WORN, 3);
    fr_text_center(f, 160, 172, "LEGACY IMAGE / EXIT STATUS 0", C_GREY);   /* inside the 188 px box edge */
    if (g->state_t >= ENDING_FRAMES - 60 && blink(g, 30))
        fr_text_center(f, 160, 206, "PRESS ANY KEY TO BOOT AGAIN", C_GREEN);
}

static void draw_credits(const Game *g, Frame *f)
{
    fr_clear(f, C_BLACK);
    int base = LH - g->state_t / 2;
    for (int i = 0; i < CREDITS_LINE_COUNT; i++) {
        int y = base + i * 14;
        if (y < -8 || y > LH) continue;
        fr_text_center(f, 160, y, CREDITS_LINES[i], i == 0 ? C_AMBER : C_GREEN);
    }
    int hy = base + CREDITS_LINE_COUNT * 14 + 60;               /* nearly invisible: dark green on black */
    if (hy > -8 && hy < LH) fr_text_center(f, 160, hy, HIDDEN_MESSAGE, C_DKGREEN);
    fr_text_center(f, 160, 230, "[ESC] BACK", C_GREY);
}

static void draw_scores(const Game *g, Frame *f)
{
    static const char *const mode_names[SCORE_MODE_COUNT] = { "CAMPAIGN", "ENDLESS MODE", "BOSS RUSH" };
    fr_clear(f, C_BLACK);
    text_scaled_center(f, 160, 12, "HIGH SCORES", C_AMBER, 2);
    char heading[64];
    snprintf(heading, sizeof heading, "< %s >", mode_names[g->scores_mode]);
    fr_text_center(f, 160, 32, heading, C_CYAN);
    int shown = 0;
    for (int i = 0; i < g->scores.n; i++) {
        const ScoreEntry *e = &g->scores.e[i];
        if (e->mode != g->scores_mode) continue;
        char b[80];
        snprintf(b, sizeof b, "%d. %-12s %06d  W%-2d %s", shown + 1, CHARS[e->ch].name, e->score, e->wave, e->win ? "WIN" : "");
        fr_text(f, 30, 48 + shown * 16, b, shown == 0 ? C_AMBER : C_GREEN);
        shown++;
    }
    if (shown == 0) fr_text_center(f, 160, 112, "NO SCORES YET. BOOT SOMETHING.", C_GREY);
    fr_text_center(f, 160, 210, "LEFT/RIGHT: CHANGE MODE", C_CYAN);
    fr_text_center(f, 160, 224, "ANY KEY: BACK", C_GREY);
}

static void draw_achievements(const Game *g, Frame *f)
{
    fr_clear(f, C_BLACK);
    text_scaled_center(f, 160, 8, "ACHIEVEMENTS", C_AMBER, 2);
    char progress[40];
    snprintf(progress, sizeof progress, "%d / %d UNLOCKED", achievements_count(g->achievements), ACH_COUNT);
    fr_text_center(f, 160, 30, progress, C_CYAN);
    for (int i = 0; i < ACH_COUNT; i++) {
        const AchievementInfo *a = achievement_info((AchievementId)i);
        bool unlocked = (g->achievements & (UINT32_C(1) << i)) != 0;
        int y = 48 + i * 20;
        fr_text(f, 18, y, unlocked ? "[OK]" : "[--]", unlocked ? C_BGREEN : C_DKGREEN);
        fr_text(f, 54, y, a->name, unlocked ? C_AMBER : C_GREY);
        fr_text(f, 54, y + 9, a->description, unlocked ? C_GREEN : C_DKGREEN);
    }
    fr_text_center(f, 160, 226, "ANY KEY: BACK", C_GREY);
}

static void draw_stats(const Game *g, Frame *f)
{
    static const char *const names[SCORE_MODE_COUNT] = { "CAMPAIGN", "ENDLESS MODE", "BOSS RUSH" };
    const ModeStats *m = &g->profile.mode[g->stats_mode];
    fr_clear(f, C_BLACK);
    text_scaled_center(f, 160, 10, "RUN STATISTICS", C_AMBER, 2);
    char b[72];
    snprintf(b, sizeof b, "< %s >", names[g->stats_mode]);
    fr_text_center(f, 160, 34, b, C_CYAN);
    snprintf(b, sizeof b, "RUNS STARTED       %llu", (unsigned long long)m->runs);
    fr_text(f, 34, 62, b, C_GREEN);
    snprintf(b, sizeof b, "VICTORIES           %llu", (unsigned long long)m->wins);
    fr_text(f, 34, 82, b, C_BGREEN);
    snprintf(b, sizeof b, "UNITS STOPPED       %llu", (unsigned long long)m->kills);
    fr_text(f, 34, 102, b, C_GREEN);
    snprintf(b, sizeof b, "TOTAL UPTIME       %llu:%02llu",
             (unsigned long long)(m->frames / (uint64_t)(FPS * 60)),
             (unsigned long long)((m->frames / FPS) % 60));
    fr_text(f, 34, 122, b, C_GREEN);
    snprintf(b, sizeof b, "BEST SCORE       %06d", m->best_score);
    fr_text(f, 34, 150, b, C_AMBER);
    snprintf(b, sizeof b, "FARTHEST WAVE        %d", m->best_wave);
    fr_text(f, 34, 170, b, C_GREEN);
    if (m->runs) {
        unsigned long long pct = (unsigned long long)((long double)m->wins * 100.0L / (long double)m->runs);
        snprintf(b, sizeof b, "SUCCESS RATE         %llu%%", pct);
    } else snprintf(b, sizeof b, "SUCCESS RATE          --");
    fr_text(f, 34, 190, b, C_CYAN);
    if (g->profile_save_failed) fr_text_center(f, 160, 208, "PROFILE SAVE FAILED", C_RED);
    fr_text_center(f, 160, 226, "LEFT/RIGHT: CHANGE MODE   ANY KEY: BACK", C_GREY);
}

static void draw_pause(const Game *g, Frame *f)
{
    fr_dim(f, 1, 3);
    fr_rect(f, 50, 38, 220, 148, C_BLACK);
    fr_box(f, 50, 38, 220, 148, C_GREEN);
    fr_text_center(f, 160, 44, "PAUSED", C_AMBER);
    fr_text_center(f, 160, 136, "WASD/ARROWS MOVE   SPACE FIRE", C_GREY);
    fr_text_center(f, 160, 146, "SHIFT/X SPECIAL    ENTER CONFIRM", C_GREY);
    fr_text_center(f, 160, 156, "F2 CRT  M MUTE  +/- VOL  F FULL", C_GREY);
    fr_text_center(f, 160, 168, "UP/DOWN PICK  L/R VOLUME  ESC BACK", C_GREEN);
    static const char *const items[PAUSE_COUNT] = { "RESUME", "", "CODEX", "SETTINGS", "RESTART RUN", "QUIT TO MENU" };
    for (int i = 0; i < PAUSE_COUNT; i++) {
        int y = 60 + i * 11;
        if (i == PAUSE_VOLUME) { draw_volume_row(g, f, y, i == g->pause_sel); continue; }
        fr_text_center(f, 160, y, items[i], i == g->pause_sel ? C_BGREEN : C_GREEN);
        if (i == g->pause_sel) fr_text(f, 160 - text_width(items[i]) / 2 - 10, y, ">", C_AMBER);
    }
}

static void draw_settings_value(Frame *f, int rx, int y, bool on, bool sel)
{
    const char *t = on ? "ON" : "OFF";
    int col = on ? (sel ? C_AMBER : C_BGREEN) : C_GREY;
    fr_text_right(f, rx, y, t, col);
}

static void draw_settings(const Game *g, Frame *f)
{
    if (g->settings_from == ST_PAUSE) {
        draw_play(g, f);
        fr_dim(f, 1, 3);
    } else {
        fr_clear(f, C_BLACK);
        draw_arena_bg(g, f);
    }
    const int px = 30, py = 26, pw = 260, ph = 190;
    fr_rect(f, px, py, pw, ph, C_BLACK);
    fr_box(f, px, py, pw, ph, C_GREEN);
    text_scaled_center(f, 160, py + 8, "SETTINGS", C_AMBER, 2);
    static const char *const labels[SET_COUNT] = {
        "MASTER VOLUME", "MUSIC", "MUTE ALL SOUND", "CRT SCANLINES", "SCREEN SHAKE", "FULLSCREEN", "BACK"
    };
    const int lx = px + 22, rx = px + pw - 22;
    for (int i = 0; i < SET_COUNT; i++) {
        int y = py + 34 + i * 15;
        bool sel = i == g->settings_sel;
        fr_text(f, lx, y, labels[i], sel ? C_BGREEN : C_GREEN);
        if (sel) fr_text(f, lx - 12, y, ">", C_AMBER);
        switch (i) {
        case SET_VOLUME: {
            int cx = rx - VOL_MAX * 6;
            draw_volume_cells(g, f, cx, y, sel ? C_AMBER : C_GREEN);
            if (sel) { fr_text(f, cx - 9, y, "<", C_AMBER); fr_text(f, rx + 3, y, ">", C_AMBER); }
            break;
        }
        case SET_MUSIC:      draw_settings_value(f, rx, y, !g->no_music, sel); break;
        case SET_MUTE:       draw_settings_value(f, rx, y, g->mute, sel); break;
        case SET_CRT:        draw_settings_value(f, rx, y, g->crt, sel); break;
        case SET_SHAKE:      draw_settings_value(f, rx, y, !g->no_shake, sel); break;
        case SET_FULLSCREEN: draw_settings_value(f, rx, y, g->fullscreen, sel); break;
        default: break;
        }
    }
    fr_text_center(f, 160, py + ph - 24, "UP/DOWN PICK  L/R CHANGE  ESC BACK", C_GREEN);
    fr_text_center(f, 160, py + ph - 14, g->settings_from == ST_PAUSE ? "YOUR RUN IS STILL PAUSED" : "F2 CRT  M MUTE  F FULLSCREEN", C_GREY);
}


/* ------------------------------------------------------------------- codex */

static void codex_icon(const Game *g, Frame *f, const CodexEntry *e, int cx, int cy)
{
    switch (e->kind) {
    case CK_ENEMY: {
        Enemy tmp;
        memset(&tmp, 0, sizeof tmp);
        tmp.on = true; tmp.type = (uint8_t)e->ref; tmp.x = (float)cx; tmp.y = (float)cy;
        tmp.hp = tmp.maxhp = 1.0f; tmp.link = -1; tmp.sub = E_JOURNALD;
        draw_enemy(g, f, &tmp);
        break;
    }
    case CK_HERO:
        fr_sprite_scaled(f, &SPR_PLAYER[e->ref], cx - 14, cy - 14, CHARS[e->ref].color, T_NORMAL, 2);
        break;
    case CK_GUEST:
        draw_guest_mark(f, cx - 11, cy - 11, 1, e->ref == 0 ? 'D' : 'U', e->ref == 0 ? C_CYAN : C_AMBER);
        break;
    case CK_PATCH:
        fr_box(f, cx - 12, cy - 12, 24, 24, C_AMBER);
        fr_text_center(f, cx, cy - 3, "+1", C_AMBER);
        break;
    default:
        fr_box(f, cx - 12, cy - 12, 24, 24, C_CYAN);
        fr_text_center(f, cx, cy - 3, "?", C_CYAN);
        break;
    }
}

static void draw_codex_wrapped(Frame *f, int x, int y, const char *text, int cols, int pal, int max_lines)
{
    char lines[WRAP_MAX][WRAP_LEN];
    int n = wrap(text, cols, lines);
    for (int i = 0; i < n && i < max_lines; i++) fr_text(f, x, y + i * 9, lines[i], pal);
}

static void draw_codex(const Game *g, Frame *f)
{
    fr_clear(f, C_BLACK);
    text_scaled_center(f, 160, 5, "CODEX", C_AMBER, 2);

    /* tab strip */
    int total = 0;
    for (int t = 0; t < CX_TAB_COUNT; t++) total += text_width(codex_tab_name(t)) + (t ? 14 : 0);
    int tx = 160 - total / 2;
    for (int t = 0; t < CX_TAB_COUNT; t++) {
        const char *nm = codex_tab_name(t);
        bool on = t == g->codex_tab;
        fr_text(f, tx, 24, nm, on ? C_AMBER : C_GREY);
        if (on) fr_hline_dotted(f, tx, tx + text_width(nm) - 1, 33, C_AMBER, 0);
        tx += text_width(nm) + 14;
    }

    const int n = codex_entry_count(g->codex_tab);
    const int sel = g->codex_sel < n ? g->codex_sel : 0;

    /* list, scrolling so the selection is always visible */
    const int rows = 15, ly = 40;
    int first = sel >= rows ? sel - rows + 1 : 0;
    fr_box(f, 4, 37, 118, rows * 10 + 6, C_DKGREEN);
    for (int i = 0; i < rows && first + i < n; i++) {
        const CodexEntry *e = codex_entry(g->codex_tab, first + i);
        bool on = first + i == sel;
        if (on) fr_text(f, 7, ly + i * 10 + 3, ">", C_AMBER);
        fr_text(f, 15, ly + i * 10 + 3, codex_name(e), on ? C_BGREEN : C_GREEN);
    }
    if (first > 0) fr_text_right(f, 119, 39, "^", C_GREY);
    if (first + rows < n) fr_text_right(f, 119, 37 + rows * 10 + 3, "v", C_GREY);

    /* detail pane: icon top-right, name and a few short stat lines top-left, then the description, tip at the bottom */
    const CodexEntry *e = codex_entry(g->codex_tab, sel);
    const int px = 124, py = 37, pw = 190, ph = rows * 10 + 6;
    fr_box(f, px, py, pw, ph, C_GREEN);
    if (!e) return;
    if (e->kind != CK_RULE) codex_icon(g, f, e, px + pw - 36, py + 26);

    char b[96];
    int y = py + 6;
    fr_text(f, px + 8, y, codex_name(e), C_BGREEN);
    y += 12;
    switch (e->kind) {
    case CK_ENEMY: {
        const EnemyDef *d = &ENEMIES[e->ref];
        snprintf(b, sizeof b, "HP %d  SCORE %d", (int)d->hp, d->score);
        fr_text(f, px + 8, y, b, C_AMBER); y += 9;
        fr_text(f, px + 8, y, d->harmful ? "TOUCHING IT HURTS" : "HARMLESS TO TOUCH", d->harmful ? C_RED : C_GREY); y += 9;
        fr_text(f, px + 8, y, d->counts ? "MUST BE CLEARED" : "OPTIONAL TARGET", C_GREY);
        y = py + 50;
        break;
    }
    case CK_HERO: {
        const CharDef *c = &CHARS[e->ref];
        snprintf(b, sizeof b, "HP %d  SPEED %.1f", c->max_hp, (double)c->speed);
        fr_text(f, px + 8, y, b, C_AMBER); y += 9;
        snprintf(b, sizeof b, "DMG %.1f  SHOTS %.1f/S", (double)c->damage, (double)(60.0f / (float)c->fire_cd));
        fr_text(f, px + 8, y, b, C_AMBER); y += 9;
        snprintf(b, sizeof b, "RECHARGE %d SEC", c->special_cd / FPS);
        fr_text(f, px + 8, y, b, C_AMBER); y += 12;
        snprintf(b, sizeof b, "SPECIAL: %s", c->special_name);
        fr_text(f, px + 8, y, b, C_CYAN); y += 9;
        draw_codex_wrapped(f, px + 8, y, c->special_desc, 29, C_LTGREY, 2);
        y += 23;
        break;
    }
    case CK_GUEST:
        fr_text(f, px + 8, y, "STORY GUEST", C_AMBER); y += 9;
        fr_text(f, px + 8, y, "NOT PLAYABLE", C_GREY);
        y = py + 50;
        break;
    case CK_PATCH:
        draw_codex_wrapped(f, px + 8, y, UPG_DESC[e->ref], 20, C_AMBER, 2); y += 18;
        snprintf(b, sizeof b, "STACKS UP TO %d", e->ref == UPG_SPECIAL ? 3 : 5);
        fr_text(f, px + 8, y, b, C_GREY); y += 16;
        break;
    default:
        y += 4;
        break;
    }
    draw_codex_wrapped(f, px + 8, y, e->text, 29, C_GREEN, 7);
    if (e->tip[0]) {
        int ty = py + ph - 31;
        fr_text(f, px + 8, ty, "TIP", C_AMBER);
        draw_codex_wrapped(f, px + 8, ty + 9, e->tip, 29, C_LTGREY, 2);
    }

    snprintf(b, sizeof b, "%d/%d", sel + 1, n);
    fr_text_right(f, 314, 24, b, C_GREY);
    fr_text_center(f, 160, 222, "UP/DOWN ENTRY   LEFT/RIGHT TAB   ESC BACK", C_GREEN);
}

void view_draw(const Game *g, Frame *f)
{
    switch (g->state) {
    case ST_BOOT:    draw_boot(g, f); break;
    case ST_MENU:    draw_menu(g, f); break;
    case ST_SELECT:  draw_select(g, f); break;
    case ST_PLAY:    draw_play(g, f); break;
    case ST_PAUSE:   draw_play(g, f); draw_pause(g, f); break;
    case ST_UPGRADE: draw_upgrade(g, f); break;
    case ST_STORY:   draw_story(g, f); break;
    case ST_PANIC:   draw_panic(g, f); break;
    case ST_RESULTS: draw_results(g, f); break;
    case ST_ENDING:  draw_ending(g, f); break;
    case ST_CREDITS: draw_credits(g, f); break;
    case ST_SCORES:  draw_scores(g, f); break;
    case ST_ACHIEVEMENTS: draw_achievements(g, f); break;
    case ST_STATS: draw_stats(g, f); break;
    case ST_SETTINGS: draw_settings(g, f); break;
    case ST_CODEX:   draw_codex(g, f); break;
    }
    bool vol_row = (g->state == ST_PAUSE && g->pause_sel == PAUSE_VOLUME) || (g->state == ST_MENU && g->menu_sel == MENU_VOLUME) || g->state == ST_SETTINGS;
    if (g->vol_show_t > 0 && !vol_row && g->state != ST_BOOT) draw_volume_readout(g, f);
    if (g->crt) fr_scanlines(f);
}
