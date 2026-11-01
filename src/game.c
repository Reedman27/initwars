#include "game.h"
#include "codex.h"

#include <stdio.h>
#include <string.h>
#include <strings.h>

#define BOSS_W 48
#define BOSS_H 32
#define ARENA_CX ((AX0 + AX1) / 2)

/* ------------------------------------------------------------------ helpers */

static float rndf(Game *g) { return rng_float(&g->rng); }

static void sfx(Game *g, Sfx id, float pitch)
{
    if (g->sfx_n >= MAX_SFXQ) return;
    g->sfxq[g->sfx_n].id = (uint8_t)id;
    g->sfxq[g->sfx_n].pitch100 = (uint8_t)clampf(pitch * 100.0f, 10, 250);
    g->sfx_n++;
}

bool game_pop_sfx(Game *g, Sfx *id, float *pitch)
{
    if (g->sfx_n <= 0) return false;
    *id = (Sfx)g->sfxq[0].id;
    *pitch = g->sfxq[0].pitch100 / 100.0f;
    for (int i = 1; i < g->sfx_n; i++) g->sfxq[i - 1] = g->sfxq[i];
    g->sfx_n--;
    return true;
}

static void say(Game *g, const char *text, int col)
{
    memcpy(g->status[1], g->status[0], sizeof g->status[0]);
    g->status_col[1] = g->status_col[0];
    snprintf(g->status[0], sizeof g->status[0], "%s", text);
    g->status_col[0] = (uint8_t)col;
    g->status_t = 0;
}

static void set_banner(Game *g, const char *l1, const char *l2, int frames)
{
    snprintf(g->banner_text[0], sizeof g->banner_text[0], "%s", l1);
    snprintf(g->banner_text[1], sizeof g->banner_text[1], "%s", l2);
    g->banner_t = frames;
}

static void add_ring(Game *g, float x, float y, int max_r, int col)
{
    for (int i = 0; i < 2; i++)
        if (!g->rings[i].on) { g->rings[i].on = true; g->rings[i].x = x; g->rings[i].y = y; g->rings[i].t = 0; g->rings[i].max_r = max_r; g->rings[i].color = (uint8_t)col; return; }
}

static void set_state(Game *g, GameState s)
{
    g->prev_state = g->state;
    g->state = s;
    g->state_t = 0;
}

static float dist2(float ax, float ay, float bx, float by)
{
    float dx = ax - bx, dy = ay - by;
    return dx * dx + dy * dy;
}

static Rectf player_rect(const Game *g)
{
    float h = (float)CHARS[g->p.ch].half_hit;
    Rectf r = { g->p.x - h, g->p.y - h, 2 * h, 2 * h };
    return r;
}

static void enemy_size(const Enemy *e, float *w, float *h)
{
    if (e->type == E_BOSS) { *w = BOSS_W; *h = BOSS_H; return; }
    *w = (float)ENEMIES[e->type].w;
    *h = (float)ENEMIES[e->type].h;
}

static Rectf enemy_rect(const Enemy *e)
{
    float w, h;
    enemy_size(e, &w, &h);
    Rectf r = { e->x - w / 2, e->y - h / 2, w, h };
    return r;
}

static Rectf enemy_hurt_rect(const Enemy *e)   /* slightly forgiving contact box */
{
    Rectf r = enemy_rect(e);
    r.x += 2; r.y += 2; r.w -= 4; r.h -= 4;
    return r;
}

/* ---------------------------------------------------------------- particles */

static void emit(Game *g, float x, float y, int n, int col)
{
    for (int i = 0; i < MAX_PART && n > 0; i++) {
        if (g->parts[i].on) continue;
        float a = rndf(g) * 6.2831853f, s = 0.4f + rndf(g) * 1.4f;
        g->parts[i] = (Particle){ true, x, y, cosf(a) * s, sinf(a) * s, 14 + (int)(rndf(g) * 12), (uint8_t)col };
        n--;
    }
}

static void popup(Game *g, float x, float y, const char *txt, int col)
{
    for (int i = 0; i < MAX_POPUP; i++) {
        if (g->pops[i].on) continue;
        g->pops[i].on = true; g->pops[i].x = x; g->pops[i].y = y;
        g->pops[i].life = 40; g->pops[i].color = (uint8_t)col;
        snprintf(g->pops[i].text, sizeof g->pops[i].text, "%s", txt);
        return;
    }
}

/* ------------------------------------------------------------ player stats */

int game_player_fire_cd(const Game *g)
{
    float cd = (float)CHARS[g->p.ch].fire_cd;
    for (int i = 0; i < g->p.up[UPG_FIRE]; i++) cd *= 0.88f;
    if (g->p.ch == CH_SYSVINIT && g->sysv_weak) cd *= SYSV_WEAK_FIRE_MULT;
    return cd < 3 ? 3 : (int)(cd + 0.5f);
}

float game_player_damage(const Game *g)
{
    float d = CHARS[g->p.ch].damage * (1.0f + 0.2f * (float)g->p.up[UPG_DMG]);
    if (g->p.ch == CH_SYSVINIT && g->sysv_weak) d *= SYSV_WEAK_DMG_MULT;
    return d;
}

int game_special_cd(const Game *g)
{
    float cd = (float)CHARS[g->p.ch].special_cd;
    if ((g->endless || g->boss_rush) && g->p.ch == CH_BUSYBOX) cd *= 1.5f;
    for (int i = 0; i < g->p.up[UPG_SPECIAL]; i++) cd *= 0.8f;
    return (int)cd;
}

static float player_speed(const Game *g)
{
    float s = CHARS[g->p.ch].speed * (1.0f + 0.08f * (float)g->p.up[UPG_SPEED]);
    if (g->p.slow_t > 0) s *= NETLINE_SLOW;
    return s;
}

const Enemy *game_boss(const Game *g)
{
    for (int i = 0; i < MAX_ENEMY; i++)
        if (g->en[i].on && g->en[i].type == E_BOSS) return &g->en[i];
    return NULL;
}

bool game_boss_alive(const Game *g) { return game_boss(g) != NULL; }

int game_enemies_alive(const Game *g)
{
    int n = 0;
    for (int i = 0; i < MAX_ENEMY; i++)
        if (g->en[i].on && ENEMIES[g->en[i].type].counts) n++;
    return n;
}

int game_minion_count(const Game *g)
{
    int n = 0;
    for (int i = 0; i < MAX_ENEMY; i++) {
        const Enemy *e = &g->en[i];
        if (e->on && (e->type == E_USB || e->type == E_FLOPPY || e->type == E_PRINTER)) n++;
    }
    return n;
}

static int count_type(const Game *g, int type, bool summoned_only)
{
    int n = 0;
    for (int i = 0; i < MAX_ENEMY; i++) {
        const Enemy *e = &g->en[i];
        if (e->on && e->type == type && (!summoned_only || e->summoned)) n++;
    }
    return n;
}

int game_scene_chars(const Game *g)
{
    const Scene *sc = story_scene((SceneId)g->scene);
    const StoryLine *ln = &sc->lines[g->scene_line];
    int c = (g->scene_t - ln->pause) / 2;
    return c < 0 ? 0 : c;
}

/* -------------------------------------------------------------- bullets etc */

static void fire_ebullet(Game *g, float x, float y, float ang, float speed, int kind, int life)
{
    if (g->endless || g->boss_rush) speed *= g->enemy_tempo;
    for (int i = 0; i < MAX_EBUL; i++) {
        if (g->eb[i].on) continue;
        g->eb[i] = (EBullet){ true, x, y, cosf(ang) * speed, sinf(ang) * speed, life, (uint8_t)kind, (uint8_t)g->cur_src };
        return;
    }
}

static float aim_at_player(const Game *g, float x, float y)
{
    return atan2f(g->p.y - y, g->p.x - x);
}

static Enemy *spawn_enemy(Game *g, int type, float x, float y, bool restart, bool warp)
{
    for (int i = 0; i < MAX_ENEMY; i++) {
        Enemy *e = &g->en[i];
        if (e->on) continue;
        memset(e, 0, sizeof *e);
        e->on = true; e->type = (uint8_t)type; e->x = x; e->y = y;
        float hp_scale = 1.0f;
        if ((g->endless || g->boss_rush) && type != E_BOSS && g->wave > 1)
            hp_scale = clampf(1.0f + 0.035f * (float)(g->wave - 1), 1.0f, 1.75f);
        else if (!g->endless && !g->boss_rush && type != E_BOSS && g->wave > CAMPAIGN_HP_START)
            hp_scale = 1.0f + CAMPAIGN_HP_STEP * (float)(g->wave - CAMPAIGN_HP_START);
        e->hp = e->maxhp = ENEMIES[type].hp * hp_scale;
        e->link = -1;
        e->restart = restart;
        e->warp = warp ? SPAWN_WARP : 0;
        e->t = 50.0f + rndf(g) * 70.0f;
        e->tx = x; e->ty = y;
        e->ts = 1.0f;
        return e;
    }
    return NULL;
}

static void clear_hazards(Game *g)
{
    memset(g->eb, 0, sizeof g->eb);
    memset(g->lines, 0, sizeof g->lines);
    memset(g->bars, 0, sizeof g->bars);
}

/* ------------------------------------------------------------ damage rules */

static bool boss_invulnerable(const Game *g)
{
    return g->boss_inv > 0 || (g->boss_phase >= 4 && !g->boss_exposed);
}

static void kill_enemy(Game *g, Enemy *e, bool silent);

static void damage_enemy(Game *g, Enemy *e, float dmg)
{
    if (!e->on || e->warp > 0) return;
    if (e->type == E_FAKE) {   /* fake targets vanish when hit and are worth nothing */
        emit(g, e->x, e->y, 6, C_GREY);
        sfx(g, SFX_HIT, 1.6f);
        e->on = false;
        return;
    }
    if (e->type == E_BOSS) {
        if (boss_invulnerable(g)) { emit(g, e->x, e->y, 1, C_GREY); return; }
        if (count_type(g, E_DEPTARGET, true) > 0) dmg *= BOSS_LINK_MULT;
    } else if (e->link >= 0) {
        dmg *= LINK_DAMAGE_MULT;
    }
    e->hp -= dmg;
    e->flash = 4;
    sfx(g, SFX_HIT, 1.0f);
    if (e->hp <= 0) kill_enemy(g, e, false);
}

static void kill_enemy(Game *g, Enemy *e, bool silent)
{
    const EnemyDef *d = &ENEMIES[e->type];
    float x = e->x, y = e->y;
    int type = e->type;
    bool was_restart = e->restart && !e->restarted;
    bool restarted = e->restarted;
    e->on = false;
    if (silent) { emit(g, x, y, 5, C_GREY); return; }

    emit(g, x, y, type == E_BOSS ? 40 : 10, type == E_BOSS ? C_RED : C_BGREEN);
    sfx(g, SFX_KILL, type == E_BOSS ? 0.6f : 1.0f);
    if (d->score > 0) {
        char t[12];
        snprintf(t, sizeof t, "+%d", d->score);
        popup(g, x, y - 6, t, C_AMBER);
        g->score += d->score;
    }
    if (d->counts || type == E_USB || type == E_FLOPPY || type == E_PRINTER) g->kills++;
    if (g->kills > 0 && g->kills % 6 == 0 && type != E_BOSS)
        say(g, story_quip(Q_KILL, &g->rng, &g->last_quip[Q_KILL]), C_GREEN);

    if (type == E_BOSS) {
        for (int i = 0; i < MAX_ENEMY; i++)
            if (g->en[i].on) kill_enemy(g, &g->en[i], true);
        clear_hazards(g);
        g->flash_t = 10;
        g->shake = 20;
        say(g, "PID 1 TERMINATED. MACHINE STILL RUNNING.", C_BGREEN);
        return;
    }

    if (was_restart && type != E_MARKER) {   /* Restart=always: telegraphed single comeback */
        Enemy *m = spawn_enemy(g, E_MARKER, x, y, false, false);
        if (m) {
            m->sub = (uint8_t)type;
            m->t = MARKER_DELAY;
            m->restarted = true;       /* the comeback will not restart again */
            m->summoned = e->summoned;
            say(g, ENEMIES[E_MARKER].sig, C_AMBER);
            sfx(g, SFX_WARN, 1.0f);
        }
    }
    (void)restarted;

    float drop = (type == E_FLOPPY) ? FLOPPY_DROP_MINION : (d->counts ? FLOPPY_DROP_CHANCE : 0.0f);
    if (drop > 0 && rndf(g) < drop) {
        for (int i = 0; i < MAX_PICK; i++) {
            if (g->picks[i].on) continue;
            g->picks[i] = (Pickup){ true, x, y, 600 };
            break;
        }
    }
}

static void game_over(Game *g);

static void hurt_player(Game *g, int dmg, int src)
{
    Player *p = &g->p;
    if (g->god || p->inv > 0 || p->dash_t > 0 || p->shield_t > 0) return;
    if (src >= 0 && src < E_COUNT) g->hurt_src[src]++;
    if (g->wave >= 1 && g->wave <= NUM_WAVES) g->hurt_by_wave[g->wave]++;
    g->hurt_flash_t = 12;
    if (g->boss_wave && g->boss_phase >= 1 && g->boss_phase <= 4) g->hurt_boss_phase[g->boss_phase]++;
    p->hp -= dmg;
    p->inv = PLAYER_IFRAMES;
    p->hurt_flash = 12;
    g->shake = 8;
    sfx(g, SFX_HURT, 1.0f);
    emit(g, p->x, p->y, 8, C_RED);
    if (p->hp <= 0) { p->hp = 0; game_over(g); return; }
    if (p->hp == 1) say(g, story_quip(Q_LOW, &g->rng, &g->last_quip[Q_LOW]), C_RED);
    else say(g, story_quip(Q_HURT, &g->rng, &g->last_quip[Q_HURT]), C_AMBER);
}

/* ---------------------------------------------------------------- specials */

static void use_special(Game *g)
{
    Player *p = &g->p;
    const CharDef *c = &CHARS[p->ch];
    p->special_t = game_special_cd(g);
    sfx(g, SFX_SPECIAL, 1.0f);
    say(g, story_quip(Q_SPECIAL, &g->rng, &g->last_quip[Q_SPECIAL]), C_CYAN);
    switch (c->special) {
    case SP_PULSE:
        for (int i = 0; i < MAX_EBUL; i++)
            if (g->eb[i].on && dist2(g->eb[i].x, g->eb[i].y, p->x, p->y) < PULSE_RADIUS * PULSE_RADIUS)
                g->eb[i].on = false;
        for (int i = 0; i < MAX_ENEMY; i++) {
            Enemy *e = &g->en[i];
            if (!e->on || e->type == E_MARKER) continue;
            float w, h;
            enemy_size(e, &w, &h);
            float r = PULSE_RADIUS + w / 2;
            if (dist2(e->x, e->y, p->x, p->y) < r * r) damage_enemy(g, e, PULSE_DAMAGE);
        }
        for (int k = 0; k < 16; k++) {
            float a = (float)k * 0.3927f;
            emit(g, p->x + cosf(a) * 30, p->y + sinf(a) * 30, 1, C_LTGREY);
        }
        add_ring(g, p->x, p->y, (int)PULSE_RADIUS, C_WHITE);
        g->shake = 5;
        break;
    case SP_DASH: {
        float fx = p->fx, fy = p->fy;
        float l = sqrtf(fx * fx + fy * fy);
        if (l < 0.01f) { fx = 0; fy = -1; l = 1; }
        p->dashx = fx / l; p->dashy = fy / l;
        p->dash_t = DASH_FRAMES;
        p->inv = DASH_FRAMES + 12;
        break;
    }
    case SP_STASIS:
        p->stasis_t = STASIS_FRAMES;
        break;
    case SP_CLEAR:
        clear_hazards(g);
        add_ring(g, p->x, p->y, 150, C_AMBER);
        emit(g, p->x, p->y, 14, C_AMBER);
        break;
    case SP_SHIELD:
        p->shield_t = SHIELD_FRAMES;
        if (p->hp < p->maxhp) p->hp++;
        break;
    }
}

/* ------------------------------------------------------------------ player */

static bool barrier_blocks(const Game *g, const Rectf *r)
{
    for (int i = 0; i < MAX_BARRIER; i++) {
        const Barrier *b = &g->bars[i];
        if (b->on && b->age >= b->tele && rect_overlap(r, &b->r)) return true;
    }
    return false;
}

static void move_player(Game *g, float dx, float dy)
{
    Player *p = &g->p;
    float h = (float)CHARS[p->ch].half_hit;
    Rectf cur = player_rect(g);
    bool stuck = barrier_blocks(g, &cur);   /* never trap the player inside a barrier */
    Rectf r = { p->x + dx - h, p->y - h, 2 * h, 2 * h };
    if (stuck || !barrier_blocks(g, &r)) p->x += dx;
    r = (Rectf){ p->x - h, p->y + dy - h, 2 * h, 2 * h };
    if (stuck || !barrier_blocks(g, &r)) p->y += dy;
    p->x = clampf(p->x, AX0 + h + 1, AX1 - h - 1);
    p->y = clampf(p->y, AY0 + h + 1, AY1 - h - 1);
}

static Enemy *nearest_target(Game *g)
{
    Enemy *best = NULL;
    float bd = 1e9f;
    for (int i = 0; i < MAX_ENEMY; i++) {
        Enemy *e = &g->en[i];
        if (!e->on || e->warp > 0 || e->type == E_FAKE) continue;
        float d = dist2(e->x, e->y, g->p.x, g->p.y);
        if (d < bd) { bd = d; best = e; }
    }
    return best;
}

static void player_fire(Game *g)
{
    Player *p = &g->p;
    float ax = p->fx, ay = p->fy;
    Enemy *t = nearest_target(g);
    if (t) { ax = t->x - p->x; ay = t->y - p->y; }
    float l = sqrtf(ax * ax + ay * ay);
    if (l < 0.01f) { ax = 0; ay = -1; l = 1; }
    for (int i = 0; i < MAX_PBUL; i++) {
        if (g->pb[i].on) continue;
        float sp = CHARS[p->ch].bullet_speed;
        g->pb[i] = (PBullet){ true, p->x, p->y, ax / l * sp, ay / l * sp, game_player_damage(g), 80 };
        break;
    }
    p->fire_t = game_player_fire_cd(g);
    sfx(g, SFX_SHOOT, (p->ch == CH_SYSVINIT && g->sysv_weak) ? 0.8f : 1.0f);
}

static void update_player(Game *g, const Input *in)
{
    Player *p = &g->p;
    float dx = (in->held[K_RIGHT] ? 1.0f : 0.0f) - (in->held[K_LEFT] ? 1.0f : 0.0f);
    float dy = (in->held[K_DOWN] ? 1.0f : 0.0f) - (in->held[K_UP] ? 1.0f : 0.0f);
    float l = sqrtf(dx * dx + dy * dy);
    if (l > 0) {
        dx /= l; dy /= l;
        p->fx = dx; p->fy = dy;
        p->idle_t = 0;
    } else {
        p->idle_t++;
    }
    if (p->dash_t > 0) {
        move_player(g, p->dashx * DASH_SPEED, p->dashy * DASH_SPEED);
        if (p->dash_t % 3 == 0) emit(g, p->x, p->y, 1, C_BGREEN);
    } else if (l > 0) {
        float s = player_speed(g);
        move_player(g, dx * s, dy * s);
    }

    if (p->fire_t > 0) p->fire_t--;
    if (p->special_t > 0) p->special_t--;
    if (p->inv > 0) p->inv--;
    if (p->dash_t > 0) p->dash_t--;
    if (p->shield_t > 0) p->shield_t--;
    if (p->slow_t > 0) p->slow_t--;
    if (p->stasis_t > 0) p->stasis_t--;
    if (p->hurt_flash > 0) p->hurt_flash--;

    if (in->held[K_FIRE] && p->fire_t <= 0) player_fire(g);
    if (in->pressed[K_SPECIAL]) {
        if (p->special_t <= 0) use_special(g);
        else sfx(g, SFX_BEEP, 0.5f);     /* not ready yet */
    }
}

/* ------------------------------------------------------------------- enemy */

static void wander(Game *g, Enemy *e, float speed)
{
    e->t2 -= e->ts;
    if (e->t2 <= 0 || dist2(e->x, e->y, e->tx, e->ty) < 4.0f) {
        e->tx = AX0 + 20 + rndf(g) * (AX1 - AX0 - 40);
        e->ty = AY0 + 20 + rndf(g) * (AY1 - AY0 - 40);
        e->t2 = 90.0f + rndf(g) * 90.0f;
    }
    float dx = e->tx - e->x, dy = e->ty - e->y;
    float l = sqrtf(dx * dx + dy * dy);
    if (l > 0.01f) { e->x += dx / l * speed * e->ts; e->y += dy / l * speed * e->ts; }
}

static void chase(const Game *g, Enemy *e, float speed)
{
    float dx = g->p.x - e->x, dy = g->p.y - e->y;
    float l = sqrtf(dx * dx + dy * dy);
    if (l > 0.01f) { e->x += dx / l * speed * e->ts; e->y += dy / l * speed * e->ts; }
}

static void keep_distance(const Game *g, Enemy *e, float want, float speed)
{
    float dx = g->p.x - e->x, dy = g->p.y - e->y;
    float l = sqrtf(dx * dx + dy * dy);
    if (l < 0.01f) return;
    float dir = l > want + 10 ? 1.0f : (l < want - 10 ? -1.0f : 0.0f);
    e->x += dx / l * speed * dir * e->ts;
    e->y += dy / l * speed * dir * e->ts;
    /* gentle strafe so they are not sitting ducks */
    e->x += -dy / l * speed * 0.4f * e->ts;
    e->y +=  dx / l * speed * 0.4f * e->ts;
}

static void make_barrier(Game *g, const Enemy *e)
{
    Barrier *b = NULL;
    for (int i = 0; i < MAX_BARRIER; i++) if (!g->bars[i].on) { b = &g->bars[i]; break; }
    if (!b) return;
    float dx = e->x - g->p.x, dy = e->y - g->p.y;
    float l = sqrtf(dx * dx + dy * dy);
    if (l < 1) { dx = 0; dy = -1; l = 1; }
    float cx = g->p.x + dx / l * 34.0f, cy = g->p.y + dy / l * 34.0f;
    bool vertical = fabsf(dx) > fabsf(dy);
    float w = vertical ? 6.0f : 44.0f, h = vertical ? 44.0f : 6.0f;
    cx = clampf(cx, AX0 + w / 2 + 2, AX1 - w / 2 - 2);
    cy = clampf(cy, AY0 + h / 2 + 2, AY1 - h / 2 - 2);
    b->on = true; b->age = 0; b->tele = 45; b->life = 220;
    b->r = (Rectf){ cx - w / 2, cy - h / 2, w, h };
}

static void make_line(Game *g, const Enemy *e)
{
    NetLine *n = NULL;
    for (int i = 0; i < MAX_LINES; i++) if (!g->lines[i].on) { n = &g->lines[i]; break; }
    if (!n) return;
    bool vertical = (g->frame / 7 + (int)(e->x)) & 1;
    n->on = true; n->vertical = vertical; n->age = 0; n->tele = 30; n->life = 240;
    if (vertical) {
        n->x0 = n->x1 = clampf(e->x, AX0 + 2, AX1 - 2);
        n->y0 = clampf(e->y - 55, AY0, AY1); n->y1 = clampf(e->y + 55, AY0, AY1);
    } else {
        n->y0 = n->y1 = clampf(e->y, AY0 + 2, AY1 - 2);
        n->x0 = clampf(e->x - 55, AX0, AX1); n->x1 = clampf(e->x + 55, AX0, AX1);
    }
}

static void spawn_minion(Game *g, const Enemy *u)
{
    float r = rndf(g);
    int type = r < 0.45f ? E_USB : (r < 0.85f ? E_FLOPPY : E_PRINTER);
    Enemy *m = spawn_enemy(g, type, u->x, u->y, false, false);
    if (m) {
        m->summoned = u->summoned;
        if (type == E_PRINTER) say(g, ENEMIES[E_PRINTER].sig, C_LTGREY);
    }
}

static void spawn_fakes(Game *g, const Enemy *r)
{
    static const uint8_t imitate[4] = { E_JOURNALD, E_LOGIND, E_NETWORKD, E_TIMER };
    int fakes = count_type(g, E_FAKE, false);
    int want = 1 + (rndf(g) < 0.5f ? 1 : 0);
    for (int k = 0; k < want && fakes < MAX_FAKES; k++, fakes++) {
        Enemy *f = spawn_enemy(g, E_FAKE, r->x + (rndf(g) - 0.5f) * 40, r->y + (rndf(g) - 0.5f) * 40, false, false);
        if (!f) return;
        f->sub = imitate[rng_range(&g->rng, 0, 4)];
        f->life = 360;
        f->x = clampf(f->x, AX0 + 10, AX1 - 10);
        f->y = clampf(f->y, AY0 + 10, AY1 - 10);
    }
}

static void link_dependencies(Game *g, int blob_idx)
{
    Enemy *b = &g->en[blob_idx];
    for (int n = 0; n < 3; n++) {
        int best = -1;
        float bd = 120.0f * 120.0f;
        for (int i = 0; i < MAX_ENEMY; i++) {
            Enemy *e = &g->en[i];
            if (!e->on || e->warp > 0 || !ENEMIES[e->type].counts || e->link >= 0) continue;
            if (e->type == E_DEPTARGET || e->type == E_BOSS || e->type == E_MARKER) continue;
            float d = dist2(e->x, e->y, b->x, b->y);
            if (d < bd) { bd = d; best = i; }
        }
        if (best < 0) break;
        g->en[best].link = blob_idx;
        g->en[best].link_t = 360;
    }
}

static void radial(Game *g, float x, float y, int n, float off, float speed, int kind)
{
    for (int i = 0; i < n; i++)
        fire_ebullet(g, x, y, off + 6.2831853f * (float)i / (float)n, speed, kind, 260);
}

/* ---------------------------------------------------------------- the boss */

static int boss_phase_for(const Enemy *e)
{
    float f = e->hp / e->maxhp;
    return f > 0.75f ? 1 : (f > 0.50f ? 2 : (f > 0.25f ? 3 : 4));
}

static void boss_summon(Game *g, int type, bool restart)
{
    float x = AX0 + 30 + rndf(g) * (AX1 - AX0 - 60);
    float y = AY0 + 60 + rndf(g) * 80;
    Enemy *s = spawn_enemy(g, type, x, y, restart, true);
    if (s) s->summoned = true;
}

static void boss_enter_phase(Game *g, Enemy *e, int phase)
{
    g->boss_phase = phase;
    g->boss_inv = 90;
    g->shake = 10;
    g->flash_t = 8;
    {
        static const char *const PH_NAME[5] = { "", "SERVICE UNITS", "DEPENDENCY LINKS", "RESTART LOOP", "LAST STAND" };
        static const char *const PH_HINT[5] = { "", "kill the helpers", "break the blob", "shoot restart markers", "survive" };
        int ph = phase < 1 ? 1 : (phase > 4 ? 4 : phase);   /* table index; also bounds the text length */
        char t[48];
        snprintf(t, sizeof t, "PHASE %d/4: %s", ph, PH_HINT[ph]);
        set_banner(g, PH_NAME[ph], t, 100);
    }
    e->t = 60; e->t2 = 240;
    clear_hazards(g);
    sfx(g, SFX_WARN, 0.7f);
    switch (phase) {
    case 1:
        say(g, "SYSTEMD: SERVICE UNITS, ASSEMBLE.", C_RED);
        boss_summon(g, E_JOURNALD, false);
        boss_summon(g, E_JOURNALD, false);
        break;
    case 2:
        say(g, "DEPENDENCY LINKS ONLINE. BREAK THE TARGET FIRST.", C_AMBER);
        boss_summon(g, E_DEPTARGET, false);
        boss_summon(g, E_NETWORKD, false);
        break;
    case 3:
        say(g, "RESTART=ALWAYS. TIMED BARRAGE INCOMING.", C_AMBER);
        boss_summon(g, E_JOURNALD, true);
        boss_summon(g, E_LOGIND, true);
        break;
    default:
        for (int i = 0; i < MAX_ENEMY; i++)
            if (g->en[i].on && g->en[i].summoned) kill_enemy(g, &g->en[i], true);
        g->boss_survive = BOSS_SURVIVE_FRAMES;
        g->boss_exposed = false;
        say(g, "SURVIVE! BOSS INVULNERABLE FOR 9 SECONDS.", C_RED);
        break;
    }
}

/* The boss roams the whole upper arena instead of sliding along one line. Each phase has its own
 * path (figure-eight, orbit, diagonal sweeps, frantic weave); the boss always eases toward the
 * point on that path, so a phase change bends the route instead of teleporting him. */

static void boss_move(Game *g, Enemy *e)
{
    float t = (float)g->boss_t;
    float cx = (float)ARENA_CX, tx, ty;
    switch (g->boss_phase) {
    case 2:   /* slow wide orbit around the middle of the upper arena */
        tx = cx + 105.0f * cosf(t * 0.020f);
        ty = (float)(AY0 + 70) + 42.0f * sinf(t * 0.020f);
        break;
    case 3:   /* diagonal sweeps: three horizontal beats against two vertical ones */
        tx = cx + 108.0f * sinf(t * 0.022f);
        ty = (float)(AY0 + 72) + 46.0f * sinf(t * 0.0147f + 1.0f);
        break;
    case 4:   /* frantic weave, reaching the deepest into the arena */
        tx = cx + 112.0f * sinf(t * 0.025f);
        ty = (float)(AY0 + 74) + 50.0f * sinf(t * 0.0313f);
        break;
    default:  /* phase 1: lazy figure-eight */
        tx = cx + 96.0f * sinf(t * 0.018f);
        ty = (float)(AY0 + 62) + 30.0f * sinf(t * 0.036f);
        break;
    }
    tx = clampf(tx, BOSS_MIN_X, BOSS_MAX_X);
    ty = clampf(ty, BOSS_MIN_Y, BOSS_MAX_Y);
    float dx = tx - e->x, dy = ty - e->y;
    float d = sqrtf(dx * dx + dy * dy);
    float step = BOSS_MAX_STEP * g->enemy_tempo;
    if (d > step) { dx *= step / d; dy *= step / d; }
    e->x += dx; e->y += dy;
}

static void update_boss(Game *g, Enemy *e)
{
    g->boss_clock += g->enemy_tempo;
    g->boss_t = (int)g->boss_clock;
    if (g->boss_inv > 0) g->boss_inv--;
    boss_move(g, e);
    int want = boss_phase_for(e);
    if (want > g->boss_phase) boss_enter_phase(g, e, want);
    if (g->boss_inv > 0) return;

    e->t -= g->enemy_tempo; e->t2 -= g->enemy_tempo;
    float aim = aim_at_player(g, e->x, e->y + 10);
    switch (g->boss_phase) {
    case 1:
        if (e->t <= 0) {
            for (int k = -1; k <= 1; k++) fire_ebullet(g, e->x, e->y + 12, aim + 0.3f * (float)k, 1.5f, 0, 240);
            e->t = 115;
        }
        if (g->boss_t % 330 == 0 && count_type(g, E_JOURNALD, true) < 2) boss_summon(g, E_JOURNALD, false);
        break;
    case 2:
        if (e->t <= 0) {
            for (int k = -1; k <= 1; k++) fire_ebullet(g, e->x, e->y + 12, aim + 0.3f * (float)k, 1.5f, 0, 240);
            e->t = 190;
        }
        if (g->boss_t % 600 == 0 && count_type(g, E_DEPTARGET, true) < 1) boss_summon(g, E_DEPTARGET, false);
        break;
    case 3:
        if (e->t <= 0) {
            fire_ebullet(g, e->x, e->y + 12, aim, 1.6f, 0, 240);
            e->t = 150;
        }
        e->state = (e->t2 > 0 && e->t2 < 80) ? 2 : 0;       /* telegraph the barrage */
        if (e->t2 <= 0) {
            radial(g, e->x, e->y + 8, 16, rndf(g) * 6.28f, 1.1f, 1);
            fire_ebullet(g, e->x, e->y + 12, aim, 1.8f, 1, 260);
            sfx(g, SFX_SPECIAL, 0.5f);
            e->t2 = 260;
        }
        break;
    default:
        if (g->boss_survive > 0) {
            g->boss_survive -= (int)ceilf(g->enemy_tempo);
            if (g->boss_survive < 0) g->boss_survive = 0;
            if (g->boss_t % 6 == 0) {
                float a = (float)g->boss_t * 0.07f;
                fire_ebullet(g, e->x, e->y + 8, a, 1.2f, 0, 300);
                fire_ebullet(g, e->x, e->y + 8, a + 3.1416f, 1.2f, 0, 300);
            }
            if (g->boss_survive == 0) {
                g->boss_exposed = true;
                say(g, "BOSS EXPOSED! SHOOT HIM!", C_BGREEN);
                sfx(g, SFX_WAVE, 1.0f);
            }
        } else {
            if (g->boss_t % 18 == 0) {
                float a = (float)g->boss_t * 0.05f;
                fire_ebullet(g, e->x, e->y + 8, a, 1.1f, 0, 300);
                fire_ebullet(g, e->x, e->y + 8, a + 3.1416f, 1.1f, 0, 300);
            }
            if (e->t <= 0) {
                for (int k = -1; k <= 1; k++) fire_ebullet(g, e->x, e->y + 12, aim + 0.28f * (float)k, 1.6f, 0, 240);
                e->t = 110;
            }
        }
        break;
    }
}

/* -------------------------------------------------------- enemy update loop */

static void update_enemies(Game *g)
{
    Player *p = &g->p;
    for (int i = 0; i < MAX_ENEMY; i++) {
        Enemy *e = &g->en[i];
        if (!e->on) continue;
        e->ts = (g->endless || g->boss_rush) ? g->enemy_tempo : 1.0f;
        if (p->stasis_t > 0 && e->type != E_BOSS &&
            dist2(e->x, e->y, p->x, p->y) < STASIS_RADIUS * STASIS_RADIUS)
            e->ts *= STASIS_FACTOR;
        if (e->flash > 0) e->flash--;
        if (e->warp > 0) { e->warp--; continue; }
        if (e->link >= 0) {
            const Enemy *b = &g->en[e->link];
            if (!b->on || b->type != E_DEPTARGET || --e->link_t <= 0) e->link = -1;
        }
        e->state = 0;
        g->cur_src = e->type;

        switch (e->type) {
        case E_JOURNALD:
            wander(g, e, 0.55f);
            e->t -= e->ts;
            if (e->t < 22) e->state = 1;
            if (e->t <= 0) {
                float a = aim_at_player(g, e->x, e->y);
                for (int k = -1; k <= 1; k++) fire_ebullet(g, e->x, e->y, a + 0.35f * (float)k, 1.5f, 0, 220);
                e->t = 100.0f + rndf(g) * 40.0f;
            }
            break;
        case E_LOGIND:
            wander(g, e, 0.6f);
            e->t -= e->ts;
            if (e->t < 30) e->state = 1;
            if (e->t <= 0) { make_barrier(g, e); e->t = 220; }
            break;
        case E_NETWORKD:
            wander(g, e, 0.6f);
            e->t -= e->ts;
            if (e->t < 25) e->state = 1;
            if (e->t <= 0) { make_line(g, e); e->t = 170; }
            break;
        case E_TIMER:
            keep_distance(g, e, 90.0f, 0.4f);
            if (e->sub == 0) {                       /* cooling down */
                e->t -= e->ts;
                if (e->t <= 0) { e->sub = 1; e->t2 = 90; sfx(g, SFX_WARN, 1.0f); }
            } else {                                 /* visible countdown, then burst */
                e->t2 -= e->ts;
                e->state = 2;
                if (e->t2 <= 0) {
                    radial(g, e->x, e->y, 12, rndf(g) * 6.28f, 1.3f, 0);
                    float a = aim_at_player(g, e->x, e->y);
                    for (int k = -1; k <= 1; k++) fire_ebullet(g, e->x, e->y, a + 0.2f * (float)k, 1.7f, 0, 220);
                    e->sub = 0; e->t = 170;
                }
            }
            break;
        case E_UDEVD:
            wander(g, e, 0.4f);
            e->t -= e->ts;
            if (e->t < 25) e->state = 1;
            if (e->t <= 0) {
                if (game_minion_count(g) < MAX_MINIONS) spawn_minion(g, e);
                e->t = 200;
            }
            break;
        case E_RESOLVED:
            keep_distance(g, e, 80.0f, 0.5f);
            e->t -= e->ts;
            if (e->t <= 0) { spawn_fakes(g, e); e->t = 170; }
            break;
        case E_FAKE:
            wander(g, e, 0.5f);
            if (--e->life <= 0) { emit(g, e->x, e->y, 4, C_GREY); e->on = false; }
            break;
        case E_DEPTARGET:
            wander(g, e, 0.3f);
            e->t -= e->ts;
            if (e->t <= 0) { link_dependencies(g, i); e->t = 150; }
            break;
        case E_USB:    chase(g, e, 0.9f); break;
        case E_FLOPPY: {
            chase(g, e, 0.7f);
            float z = sinf((float)g->frame * 0.12f + (float)i) * 0.7f;
            float dx = p->x - e->x, dy = p->y - e->y, l = sqrtf(dx * dx + dy * dy) + 0.01f;
            e->x += -dy / l * z * e->ts; e->y += dx / l * z * e->ts;
            break;
        }
        case E_PRINTER:
            chase(g, e, 0.45f);
            e->t -= e->ts;
            if (e->t <= 0) { fire_ebullet(g, e->x, e->y, aim_at_player(g, e->x, e->y), 1.2f, 0, 200); e->t = 150; }
            break;
        case E_CRON:
            wander(g, e, 0.42f);
            e->t -= e->ts;
            if (e->t < 28) e->state = 1;
            if (e->t <= 0) {
                radial(g, e->x, e->y, 8, (float)g->frame * 0.035f, 1.25f, 0);
                e->t = 145.0f + rndf(g) * 35.0f;
                sfx(g, SFX_WARN, 0.85f);
            }
            break;
        case E_OOMD:
            if (e->sub == 0) {
                chase(g, e, 0.34f);
                e->t -= e->ts;
                if (e->t <= 0) {
                    e->ang = aim_at_player(g, e->x, e->y);
                    e->sub = 1;
                    e->t2 = 42.0f;
                    sfx(g, SFX_WARN, 0.75f);
                }
            } else if (e->sub == 1) {
                e->state = 2;
                e->t2 -= e->ts;
                if (e->t2 <= 0) {
                    e->sub = 2;
                    e->t2 = 24.0f;
                    e->vx = cosf(e->ang) * 3.0f;
                    e->vy = sinf(e->ang) * 3.0f;
                    sfx(g, SFX_HIT, 1.35f);
                }
            } else {
                e->x += e->vx * e->ts;
                e->y += e->vy * e->ts;
                e->t2 -= e->ts;
                if (e->t2 <= 0) { e->sub = 0; e->t = 115.0f; }
            }
            break;
        case E_MARKER:
            e->t -= e->ts;
            if (e->t <= 0) {
                int type = e->sub;
                float x = e->x, y = e->y;
                bool summ = e->summoned;
                e->on = false;
                Enemy *n = spawn_enemy(g, type, x, y, true, true);
                if (n) {
                    n->restarted = true;
                    n->summoned = summ;
                    n->hp = n->maxhp * MARKER_RESPAWN_HP;
                    say(g, "SERVICE RESTARTED. (ONCE.)", C_AMBER);
                }
            }
            break;
        case E_BOSS:
            update_boss(g, e);
            break;
        default: break;
        }
        e->x = clampf(e->x, AX0 + 6, AX1 - 6);
        if (e->type != E_BOSS) e->y = clampf(e->y, AY0 + 6, AY1 - 6);
    }
}

/* ------------------------------------------------------ bullets & hazards */

static void update_bullets(Game *g)
{
    Player *p = &g->p;
    for (int i = 0; i < MAX_PBUL; i++) {
        PBullet *b = &g->pb[i];
        if (!b->on) continue;
        b->x += b->vx; b->y += b->vy;
        if (--b->life <= 0 || b->x < AX0 || b->x > AX1 || b->y < AY0 || b->y > AY1) { b->on = false; continue; }
        Rectf br = { b->x - 1.5f, b->y - 1.5f, 3, 3 };
        for (int k = 0; k < MAX_ENEMY; k++) {
            Enemy *e = &g->en[k];
            if (!e->on || e->warp > 0) continue;
            Rectf er = enemy_rect(e);
            if (rect_overlap(&br, &er)) {
                b->on = false;
                damage_enemy(g, e, b->dmg);
                break;
            }
        }
    }

    Rectf pr = player_rect(g);
    for (int i = 0; i < MAX_EBUL; i++) {
        EBullet *b = &g->eb[i];
        if (!b->on) continue;
        float s = 1.0f;
        if (p->stasis_t > 0 && dist2(b->x, b->y, p->x, p->y) < STASIS_RADIUS * STASIS_RADIUS) s = STASIS_FACTOR;
        b->x += b->vx * s; b->y += b->vy * s;
        if (--b->life <= 0 || b->x < AX0 - 4 || b->x > AX1 + 4 || b->y < AY0 - 4 || b->y > AY1 + 4) { b->on = false; continue; }
        float h = b->kind ? 2.5f : 1.5f;
        Rectf br = { b->x - h, b->y - h, 2 * h, 2 * h };
        if (rect_overlap(&br, &pr)) { b->on = false; hurt_player(g, 1, b->src); if (g->state != ST_PLAY) return; }
    }
}

static void update_hazards(Game *g)
{
    Rectf pr = player_rect(g);
    for (int i = 0; i < MAX_BARRIER; i++) {
        Barrier *b = &g->bars[i];
        if (!b->on) continue;
        /* do not close a route on top of the player: hold the telegraph instead */
        if (b->age >= b->tele - 1 && b->age < b->tele + 2 && rect_overlap(&pr, &b->r)) { b->age = b->tele - 1; b->life++; }
        if (++b->age > b->life) b->on = false;
    }
    for (int i = 0; i < MAX_LINES; i++) {
        NetLine *n = &g->lines[i];
        if (!n->on) continue;
        if (n->age >= n->tele) {
            Rectf lr = { n->x0 - 1, n->y0 - 1, (n->x1 - n->x0) + 2, (n->y1 - n->y0) + 2 };
            if (rect_overlap(&pr, &lr)) g->p.slow_t = NETLINE_SLOW_FRAMES;
        }
        if (++n->age > n->life) n->on = false;
    }
    for (int i = 0; i < MAX_PICK; i++) {
        Pickup *k = &g->picks[i];
        if (!k->on) continue;
        if (--k->life <= 0) { k->on = false; continue; }
        if (dist2(k->x, k->y, g->p.x, g->p.y) < 100.0f) {
            k->on = false;
            if (g->p.hp < g->p.maxhp) g->p.hp++;
            g->score += 100;
            sfx(g, SFX_PICKUP, 1.0f);
            say(g, story_quip(Q_PICKUP, &g->rng, &g->last_quip[Q_PICKUP]), C_BGREEN);
        }
    }
    for (int i = 0; i < MAX_ENEMY; i++) {   /* contact damage */
        Enemy *e = &g->en[i];
        if (!e->on || e->warp > 0 || !ENEMIES[e->type].harmful) continue;
        Rectf er = enemy_hurt_rect(e);
        if (rect_overlap(&pr, &er)) { hurt_player(g, 1, e->type); if (g->state != ST_PLAY) return; }
    }
    for (int i = 0; i < MAX_PART; i++) {
        Particle *p = &g->parts[i];
        if (!p->on) continue;
        p->x += p->vx; p->y += p->vy; p->vx *= 0.94f; p->vy *= 0.94f;
        if (--p->life <= 0) p->on = false;
    }
    for (int i = 0; i < MAX_POPUP; i++) {
        Popup *p = &g->pops[i];
        if (!p->on) continue;
        p->y -= 0.3f;
        if (--p->life <= 0) p->on = false;
    }
}

/* -------------------------------------------------------- run / wave flow */

static void goto_wave(Game *g, int w);

static int upgrade_limit(int id)
{
    return id == UPG_SPECIAL ? 3 : 5;
}

static bool upgrade_available(const Game *g, int id)
{
    return g->p.up[id] < upgrade_limit(id);
}

static void pick_upgrades(Game *g)
{
    int pool[UPG_COUNT], n = 0;
    for (int i = 0; i < UPG_COUNT; i++)
        if (upgrade_available(g, i)) pool[n++] = i;
    if (n == 0) {
        /* Fully capped open-mode builds receive repair patches instead of dead upgrades. */
        g->upg_opt[0] = g->upg_opt[1] = g->upg_opt[2] = UPG_HP;
    } else {
        for (int i = n - 1; i > 0; i--) {
            int j = rng_range(&g->rng, 0, i + 1);
            int t = pool[i]; pool[i] = pool[j]; pool[j] = t;
        }
        for (int i = 0; i < 3; i++) g->upg_opt[i] = pool[i % n];
    }
    g->upg_sel = 0;
}

static void enter_wave(Game *g)
{
    Player *p = &g->p;
    memset(g->pb, 0, sizeof g->pb);
    memset(g->eb, 0, sizeof g->eb);
    memset(g->en, 0, sizeof g->en);
    memset(g->lines, 0, sizeof g->lines);
    memset(g->bars, 0, sizeof g->bars);
    memset(g->picks, 0, sizeof g->picks);
    memset(g->parts, 0, sizeof g->parts);
    memset(g->pops, 0, sizeof g->pops);
    p->x = ARENA_CX; p->y = AY1 - 24;
    p->fire_t = 0; p->inv = 60; p->dash_t = 0; p->shield_t = 0; p->slow_t = 0; p->stasis_t = 0;
    g->q_n = g->q_i = 0;
    g->wave_threat = 0;
    g->enemy_tempo = g->endless ? clampf(1.0f + 0.025f * (float)(g->wave - 1), 1.0f, 1.65f) :
                     (g->boss_rush ? clampf(1.0f + 0.04f * (float)(g->wave - 1), 1.0f, 1.5f) : 1.0f);
    g->boss_clock = 0.0f;
    g->boss_wave = g->boss_rush || (g->endless ? (g->wave % 10 == 0) : (g->wave == BOSS_WAVE));
    if (g->boss_rush) {
        g->queue[g->q_n++] = (QueueItem){ E_BOSS, 0 };
    } else if (g->endless) {
        if (g->boss_wave) {
            g->queue[g->q_n++] = (QueueItem){ E_BOSS, 0 };
        } else {
            static const uint8_t unlocks[] = {
                E_JOURNALD, E_LOGIND, E_NETWORKD, E_TIMER, E_UDEVD,
                E_RESOLVED, E_DEPTARGET, E_CRON, E_OOMD
            };
            static const uint8_t threat_cost[] = {
                [E_JOURNALD] = 3, [E_LOGIND] = 4, [E_NETWORKD] = 4,
                [E_TIMER] = 5, [E_UDEVD] = 5, [E_RESOLVED] = 4,
                [E_DEPTARGET] = 8, [E_CRON] = 7, [E_OOMD] = 9
            };
            static const uint8_t unlock_count[] = { 1, 2, 3, 4, 5, 7, 9 };
            int unlock_stage = g->wave < 7 ? g->wave - 1 : 6;
            int unlocked = unlock_count[unlock_stage];
            int budget = 12 + g->wave * 3;
            if (budget > 100) budget = 100;
            g->wave_threat = budget;
            while (budget >= 3 && g->q_n < MAX_QUEUE) {
                uint8_t candidates[9];
                int n = 0;
                for (int i = 0; i < unlocked; i++)
                    if (threat_cost[unlocks[i]] <= budget) candidates[n++] = unlocks[i];
                if (n == 0) break;
                int type = candidates[rng_range(&g->rng, 0, n)];
                budget -= threat_cost[type];
                bool restart = g->wave >= 4 &&
                    rndf(g) < clampf(0.04f + (float)(g->wave - 4) * 0.025f, 0.04f, 0.38f);
                g->queue[g->q_n++] = (QueueItem){ (uint8_t)type, restart ? 1u : 0u };
            }
        }
    } else {
        const WaveDef *w = &WAVES[g->wave < 1 ? 0 : (g->wave > NUM_WAVES ? NUM_WAVES - 1 : g->wave - 1)];   /* never read outside the table */
        for (int i = 0; i < w->n; i++)
            for (int k = 0; k < w->s[i].count && g->q_n < MAX_QUEUE; k++)
                g->queue[g->q_n++] = (QueueItem){ w->s[i].type, w->s[i].restart };
    }
    for (int i = g->q_n - 1; i > 0; i--) {
        int j = rng_range(&g->rng, 0, i + 1);
        QueueItem t = g->queue[i]; g->queue[i] = g->queue[j]; g->queue[j] = t;
    }
    g->spawn_t = 0;
    g->phase = PH_INTRO; g->phase_t = 0;
    g->hp_at_wave_start = p->hp;
    g->boss_phase = 0; g->boss_t = 0; g->boss_inv = 0; g->boss_survive = 0; g->boss_exposed = false;
    g->joke_t = 0;
    g->banner_t = 0;
    memset(g->rings, 0, sizeof g->rings);
    set_state(g, ST_PLAY);
    char buf[72];
    if (g->boss_rush) snprintf(buf, sizeof buf, "BOSS RUSH %d/8", g->wave);
    else if (g->endless) snprintf(buf, sizeof buf, g->boss_wave ? "ENDLESS %d: BOSS ENCOUNTER" : "ENDLESS WAVE %d", g->wave);
    else snprintf(buf, sizeof buf, "WAVE %d: %s", g->wave, WAVE_NAMES[g->wave < 1 ? 0 : (g->wave > NUM_WAVES ? NUM_WAVES - 1 : g->wave - 1)]);
    say(g, buf, C_BGREEN);
    if (!g->boss_wave && rndf(g) < 0.12f) {      /* rare fake error that is a joke */
        g->joke_t = 1;
        g->joke_idx = 0;
        const char *j = story_quip(Q_JOKE, &g->rng, &g->joke_idx);
        say(g, j, C_RED);
        sfx(g, SFX_WARN, 1.3f);
    } else {
        say(g, story_quip(Q_WAVE, &g->rng, &g->last_quip[Q_WAVE]), C_GREEN);
    }
    sfx(g, SFX_WAVE, 1.0f);
}

static void start_story(Game *g, int sc)
{
    g->scene = sc;
    g->scene_line = 0;
    g->scene_t = 0;
    g->scene_inplay = false;
    g->scene_quiet = story_scene((SceneId)sc)->quiet;
    set_state(g, ST_STORY);
}

/* A scene that interrupts the fight: the arena freezes where it is and the dialogue is drawn over it.
 * Enemy shots are cleared so that nothing is already in flight when the fight resumes. */
static void start_inplay_story(Game *g, int sc)
{
    memset(g->eb, 0, sizeof g->eb);
    start_story(g, sc);
    g->scene_inplay = true;
    g->midscene_seen = true;
    g->hurt_flash_t = 0;
}

static void goto_wave(Game *g, int w)
{
    g->wave = w;
    if (!g->endless && !g->boss_rush && REST_WAVE(w) && g->p.hp < g->p.maxhp) g->p.hp++;   /* campaign maintenance */
    if (g->endless && w > 1 && w % 7 == 0 && g->p.hp < g->p.maxhp) g->p.hp++;
    int sc = (g->endless || g->boss_rush) ? -1 : story_scene_before_wave(w);
    if (sc >= 0) start_story(g, sc);
    else enter_wave(g);
}

static void start_run(Game *g)
{
    Player *p = &g->p;
    memset(p, 0, sizeof *p);
    p->ch = g->ch_sel;
    p->maxhp = p->hp = CHARS[p->ch].max_hp;
    p->x = ARENA_CX; p->y = AY1 - 24; p->fy = -1;
    g->score = g->kills = g->run_frames = 0;
    g->victory = false; g->sysv_weak = false; g->run_active = true; g->last_rank = -1;
    g->midscene_seen = false;
    g->boss_rush_loadout = g->boss_rush ? 6 : 0;
    g->enemy_tempo = 1.0f;
    memset(g->en, 0, sizeof g->en);
    if (g->boss_rush_loadout > 0) {
        pick_upgrades(g);
        set_state(g, ST_UPGRADE);
        say(g, "BOSS RUSH LOADOUT: PICK SIX STARTING PATCHES.", C_AMBER);
    } else {
        goto_wave(g, 1);
    }
}

static void finish_run(Game *g, bool win)
{
    g->victory = win;
    g->run_active = false;
    int mode = g->boss_rush ? SCORE_BOSSRUSH : (g->endless ? SCORE_ENDLESS : SCORE_CAMPAIGN);
    profile_record_run(&g->profile, mode, g->score, g->wave, g->kills, g->run_frames, win);
    g->profile_save_failed = false;
    if (!g->no_save) g->profile_save_failed = !profile_save(&g->profile);
    uint32_t old_achievements = g->achievements;
    g->achievements = achievements_for_run(g->achievements, mode, g->p.ch, g->kills, g->wave,
                                           win ? 1 : 0, g->p.hp, g->p.maxhp);
    g->achievement_save_failed = false;
    if (!g->no_save && g->achievements != old_achievements)
        g->achievement_save_failed = !achievements_save(g->achievements);
    ScoreEntry e = { g->p.ch, g->score, g->wave, win ? 1 : 0, mode };
    g->last_rank = scores_add(&g->scores, e);
    g->save_failed = false;
    if (!g->no_save && g->last_rank >= 0) g->save_failed = !scores_save(&g->scores);
}

static void game_over(Game *g)
{
    finish_run(g, false);
    set_state(g, ST_PANIC);
    sfx(g, SFX_PANIC, 1.0f);
}

static void wave_cleared(Game *g)
{
    g->phase = PH_CLEAR; g->phase_t = 0;
    memset(g->eb, 0, sizeof g->eb);
    int bonus = 100 * g->wave;
    if (g->p.hp >= g->hp_at_wave_start) bonus += 250;      /* flawless bonus */
    g->score += bonus;
    char t[40];
    snprintf(t, sizeof t, "WAVE CLEARED  +%d", bonus);
    say(g, t, C_BGREEN);
    sfx(g, SFX_VICTORY, (g->p.ch == CH_SYSVINIT && g->sysv_weak) ? 0.75f : 1.0f);
}

static void update_play(Game *g, const Input *in)
{
    Player *p = &g->p;
    g->run_frames++;
    g->phase_t++;
    if (g->shake > 0) g->shake--;
    if (g->flash_t > 0) g->flash_t--;
    if (g->hurt_flash_t > 0) g->hurt_flash_t--;
    if (g->banner_t > 0) g->banner_t--;
    for (int i = 0; i < 2; i++)
        if (g->rings[i].on && ++g->rings[i].t > 16) g->rings[i].on = false;
    if (g->joke_t > 0) {
        g->joke_t++;
        if (g->joke_t == 150) say(g, story_joke_reveal(g->joke_idx), C_GREEN);
        if (g->joke_t > 300) g->joke_t = 0;
    }

    update_player(g, in);
    if (g->state != ST_PLAY) return;

    if (g->phase == PH_INTRO && g->phase_t >= WAVE_INTRO_FRAMES) {
        g->phase = PH_FIGHT; g->phase_t = 0;
    }
    if (g->phase == PH_FIGHT && g->q_i < g->q_n && --g->spawn_t <= 0) {
        QueueItem q = g->queue[g->q_i];
        float x, y;
        int tries = 0;
        do {
            x = AX0 + 16 + rndf(g) * (AX1 - AX0 - 32);
            y = AY0 + 16 + rndf(g) * (AY1 - AY0 - 32);
        } while (dist2(x, y, p->x, p->y) < 80.0f * 80.0f && ++tries < 20);
        if (q.type == E_BOSS) { x = ARENA_CX; y = AY0 + 34; }
        Enemy *e = spawn_enemy(g, q.type, x, y, q.restart != 0, true);
        if (e) {
            g->q_i++;
            g->spawn_t = SPAWN_INTERVAL;
            if (q.type == E_BOSS) {
                if (g->endless && g->wave > 10) {
                    float scale = 1.0f + 0.35f * (float)(g->wave / 10 - 1);
                    e->hp *= scale; e->maxhp = e->hp;
                } else if (g->boss_rush && g->wave > 1) {
                    float scale = 1.0f + 0.35f * (float)(g->wave - 1);
                    e->hp *= scale; e->maxhp = e->hp;
                } else if (!g->endless && !g->boss_rush) {
                    e->hp *= CAMPAIGN_BOSS_HP_MULT; e->maxhp = e->hp;
                }
                e->warp = 60; boss_enter_phase(g, e, 1); g->boss_inv = 120;
            }
            else if (ENEMIES[q.type].sig[0] && rndf(g) < 0.5f && strcasecmp(g->status[0], ENEMIES[q.type].sig) != 0) say(g, ENEMIES[q.type].sig, C_AMBER);
        }
    }

    if (!g->endless && !g->boss_rush && g->phase == PH_FIGHT && !g->midscene_seen && story_scene_midwave(g->wave) >= 0 &&
        g->q_i >= (g->q_n + 1) / 2 && game_enemies_alive(g) > 0) {
        start_inplay_story(g, story_scene_midwave(g->wave));
        return;
    }

    update_enemies(g);
    if (g->state != ST_PLAY) return;
    update_bullets(g);
    if (g->state != ST_PLAY) return;
    update_hazards(g);
    if (g->state != ST_PLAY) return;

    if (g->phase == PH_FIGHT && g->q_i >= g->q_n && game_enemies_alive(g) == 0) wave_cleared(g);
    if (g->phase == PH_CLEAR && g->phase_t >= WAVE_CLEAR_FRAMES) {
        if (!g->endless && !g->boss_rush && g->wave >= BOSS_WAVE) {
            start_story(g, SC_EPILOGUE);
            return;
        }
        if (g->boss_rush && g->wave >= 8) {
            finish_run(g, true);
            set_state(g, ST_RESULTS);
        } else {
            if (g->boss_rush && g->p.hp < g->p.maxhp)
                g->p.hp = g->p.hp + 3 > g->p.maxhp ? g->p.maxhp : g->p.hp + 3;
            pick_upgrades(g);
            set_state(g, ST_UPGRADE);
        }
    }
}

/* ---------------------------------------------------------------- menus etc */

static void apply_upgrade(Game *g, int id)
{
    Player *p = &g->p;
    if (p->up[id] >= upgrade_limit(id)) {
        if (id == UPG_HP && p->hp < p->maxhp) p->hp = p->hp + 2 > p->maxhp ? p->maxhp : p->hp + 2;
        return;
    }
    p->up[id]++;
    if (id == UPG_HP) { p->maxhp++; p->hp = p->hp + 2 > p->maxhp ? p->maxhp : p->hp + 2; }
}

static void finish_story(Game *g)
{
    bool inplay = g->scene_inplay;
    g->scene_inplay = false;
    if (g->scene == SC_EPILOGUE) {
        finish_run(g, true);
        set_state(g, ST_RESULTS);
        sfx(g, SFX_VICTORY, 0.85f);
        return;
    }
    if (g->scene == SC_REDHAT) g->sysv_weak = true;
    if (inplay) {                                   /* back into the fight, exactly where it stopped */
        set_state(g, ST_PLAY);
        if (g->p.inv < 90) g->p.inv = 90;           /* a moment to find your feet */
        g->spawn_t = 20;
        g->flash_t = 6;
        sfx(g, SFX_WARN, 0.7f);
    } else {
        enter_wave(g);
    }
    if (g->scene == SC_REDHAT && g->p.ch == CH_SYSVINIT) {
        char t[40];
        snprintf(t, sizeof t, "FIRE RATE -%d%%   DAMAGE -%d%%",
                 (int)(100.0f * (1.0f - 1.0f / SYSV_WEAK_FIRE_MULT) + 0.5f), (int)(100.0f * (1.0f - SYSV_WEAK_DMG_MULT) + 0.5f));
        set_banner(g, "LEGACY MODE", t, 220);
        say(g, "LEGACY MODE: SHOTS COME SLOWER. KEEP MOVING.", C_AMBER);
        if (inplay) add_ring(g, g->p.x, g->p.y, 40, C_AMBER);
    } else if (g->scene == SC_REDHAT && inplay) {
        say(g, "SYSVINIT HOLDS THE LINE. SO DO YOU.", C_AMBER);
    }
}

static void update_story(Game *g, const Input *in)
{
    const Scene *sc = story_scene((SceneId)g->scene);
    const StoryLine *ln = &sc->lines[g->scene_line];
    int len = (int)strlen(ln->text);
    int before = game_scene_chars(g);
    g->scene_t++;
    int chars = game_scene_chars(g);
    if (!sc->quiet && chars > before && chars <= len && chars % 3 == 0) sfx(g, SFX_BEEP, 0.8f);

    bool guard = g->scene_inplay && g->state_t < 40;   /* the player was fighting a moment ago: ignore button mashing */
    if (guard) return;
    if (in->pressed[K_ESC]) { finish_story(g); return; }
    if (in_confirm(in)) {
        if (chars < len) {
            if (!(sc->quiet && g->scene_t < ln->pause)) g->scene_t = ln->pause + len * 2 + 1;
        } else {
            if (++g->scene_line >= sc->n) { finish_story(g); return; }
            g->scene_t = 0;
            sfx(g, SFX_SELECT, 1.0f);
        }
    }
}

/* Volume lives in the game state (so the pause menu and hotkeys can change it); main.c forwards it to the mixer. */
static void change_volume(Game *g, int delta)
{
    int v = g->volume + delta;
    if (v < 0) v = 0;
    if (v > VOL_MAX) v = VOL_MAX;
    g->vol_show_t = 120;
    if (v == g->volume) return;
    g->volume = v;
    g->vol_save_t = 45;
    sfx(g, SFX_SELECT, 0.8f + 0.04f * (float)v);        /* audible sample at the new level */
}

static void write_settings(const Game *g)
{
    Settings s = { g->volume, g->crt ? 1 : 0, g->no_music ? 0 : 1, g->no_shake ? 0 : 1 };
    settings_save(&s);
}

void game_flush_settings(Game *g)
{
    if (g->vol_save_t > 0 && !g->no_save) write_settings(g);
    g->vol_save_t = 0;
}

/* Settings that persist are written after a short quiet moment, so holding a key does not hammer the disk. */
static void settings_changed(Game *g) { g->vol_save_t = 45; }

static void leave_settings(Game *g)
{
    sfx(g, SFX_SELECT, 0.9f);
    if (g->settings_from == ST_PAUSE) g->state = ST_PAUSE;   /* the paused run is untouched, as with resuming */
    else set_state(g, ST_MENU);
}

static void update_settings(Game *g, const Input *in)
{
    if (in->pressed[K_ESC]) { leave_settings(g); return; }
    if (in->pressed[K_UP])   { g->settings_sel = (g->settings_sel + SET_COUNT - 1) % SET_COUNT; sfx(g, SFX_SELECT, 1.0f); }
    if (in->pressed[K_DOWN]) { g->settings_sel = (g->settings_sel + 1) % SET_COUNT; sfx(g, SFX_SELECT, 1.0f); }
    int d = (in->pressed[K_RIGHT] ? 1 : 0) - (in->pressed[K_LEFT] ? 1 : 0);
    bool go = in_confirm(in);
    switch (g->settings_sel) {
    case SET_VOLUME:
        if (d) change_volume(g, d);
        else if (go) change_volume(g, g->volume >= VOL_MAX ? -VOL_MAX : 1);   /* Enter cycles; wraps to silent */
        break;
    case SET_MUSIC:      if (d || go) { g->no_music = !g->no_music; settings_changed(g); sfx(g, SFX_BEEP, 1.0f); } break;
    case SET_MUTE:       if (d || go) { g->mute = !g->mute; sfx(g, SFX_BEEP, 1.0f); } break;
    case SET_CRT:        if (d || go) { g->crt = !g->crt; settings_changed(g); sfx(g, SFX_BEEP, 1.0f); } break;
    case SET_SHAKE:      if (d || go) { g->no_shake = !g->no_shake; settings_changed(g); sfx(g, SFX_BEEP, 1.0f); } break;
    case SET_FULLSCREEN: if (d || go) { g->fs_toggle = true; sfx(g, SFX_BEEP, 1.0f); } break;
    case SET_BACK:       if (go) leave_settings(g); break;
    }
}

/* The Codex: LEFT/RIGHT switch tab, UP/DOWN pick an entry, Esc or Enter go back. */
static void update_codex(Game *g, const Input *in)
{
    if (in->pressed[K_ESC] || in->pressed[K_CONFIRM]) {
        sfx(g, SFX_SELECT, 0.9f);
        if (g->codex_from == ST_PAUSE) g->state = ST_PAUSE;   /* the paused run is untouched */
        else set_state(g, ST_MENU);
        return;
    }
    int d = (in->pressed[K_RIGHT] ? 1 : 0) - (in->pressed[K_LEFT] ? 1 : 0);
    if (d) {
        g->codex_tab = (g->codex_tab + CX_TAB_COUNT + d) % CX_TAB_COUNT;
        g->codex_sel = 0;
        sfx(g, SFX_SELECT, 1.1f);
    }
    int n = codex_entry_count(g->codex_tab);
    if (n > 0 && in->pressed[K_UP])   { g->codex_sel = (g->codex_sel + n - 1) % n; sfx(g, SFX_SELECT, 1.0f); }
    if (n > 0 && in->pressed[K_DOWN]) { g->codex_sel = (g->codex_sel + 1) % n; sfx(g, SFX_SELECT, 1.0f); }
}

static void update_menu(Game *g, const Input *in)
{
    if (in->pressed[K_UP]) { g->menu_sel = (g->menu_sel + MENU_COUNT - 1) % MENU_COUNT; sfx(g, SFX_SELECT, 1.0f); }
    if (in->pressed[K_DOWN]) { g->menu_sel = (g->menu_sel + 1) % MENU_COUNT; sfx(g, SFX_SELECT, 1.0f); }
    bool go = in_confirm(in);
    if (g->menu_sel == MENU_CRT && (in->pressed[K_LEFT] || in->pressed[K_RIGHT])) { g->crt = !g->crt; g->vol_save_t = 45; sfx(g, SFX_BEEP, 1.0f); }
    if (g->menu_sel == MENU_VOLUME) {
        if (in->pressed[K_LEFT]) change_volume(g, -1);
        if (in->pressed[K_RIGHT]) change_volume(g, +1);
    }
    if (!go) return;
    sfx(g, SFX_SELECT, 1.2f);
    switch (g->menu_sel) {
    case MENU_START:   g->select_mode = SCORE_CAMPAIGN; g->endless = false; g->boss_rush = false; set_state(g, ST_SELECT); break;
    case MENU_ENDLESS: g->select_mode = SCORE_ENDLESS; g->endless = true; g->boss_rush = false; set_state(g, ST_SELECT); break;
    case MENU_BOSSRUSH: g->select_mode = SCORE_BOSSRUSH; g->endless = false; g->boss_rush = true; set_state(g, ST_SELECT); break;
    case MENU_SCORES:  g->scores_mode = SCORE_CAMPAIGN; set_state(g, ST_SCORES); break;
    case MENU_ACHIEVEMENTS: set_state(g, ST_ACHIEVEMENTS); break;
    case MENU_STATS: g->stats_mode = SCORE_CAMPAIGN; set_state(g, ST_STATS); break;
    case MENU_CODEX:   g->codex_from = ST_MENU; g->codex_tab = 0; g->codex_sel = 0; set_state(g, ST_CODEX); break;
    case MENU_SETTINGS: g->settings_from = ST_MENU; g->settings_sel = 0; set_state(g, ST_SETTINGS); break;
    case MENU_VOLUME:  change_volume(g, g->volume >= VOL_MAX ? -VOL_MAX : 1); break;   /* Enter cycles; wraps to silent */
    case MENU_CRT:     g->crt = !g->crt; g->vol_save_t = 45; break;
    case MENU_CREDITS: set_state(g, ST_CREDITS); break;
    case MENU_QUIT:    g->quit = true; break;
    }
}

#define ENDING_FRAMES 420

static void update_music(Game *g)
{
    float m = 0.4f;
    switch (g->state) {
    case ST_BOOT: m = 0; break;
    case ST_PLAY: m = (g->boss_phase >= 4 && !g->boss_exposed) ? 0.8f : 0.55f; break;
    case ST_PAUSE: m = 0.15f; break;
    case ST_STORY: m = g->scene_quiet ? 0.0f : 0.3f; break;
    case ST_PANIC: m = 0; break;
    case ST_ENDING: m = 0.15f; break;
    default: break;
    }
    g->music = g->no_music ? 0.0f : m;
}

void game_init(Game *g, uint32_t seed)
{
    memset(g, 0, sizeof *g);
    g->rng = seed ? seed : 0xC0FFEEu;
    g->state = ST_BOOT;
    g->ch_sel = CH_RUNIT;
    g->select_mode = SCORE_CAMPAIGN;
    g->enemy_tempo = 1.0f;
    g->last_rank = -1;
    for (int i = 0; i < Q_COUNT; i++) g->last_quip[i] = -1;
    scores_load(&g->scores);
    g->achievements = achievements_load();
    profile_load(&g->profile);
    Settings st;
    settings_load(&st);
    g->volume = st.volume < 0 ? VOL_DEFAULT : st.volume;
    g->crt = st.crt == 1;
    g->no_music = st.music == 0;
    g->no_shake = st.shake == 0;
}

void game_update(Game *g, const Input *in)
{
    g->frame++;
    g->state_t++;
    g->status_t++;
    if (in->pressed[K_CRT]) { g->crt = !g->crt; g->vol_save_t = 45; }
    if (in->pressed[K_MUTE]) g->mute = !g->mute;
    if (in->pressed[K_VOL_DN]) change_volume(g, -1);
    if (in->pressed[K_VOL_UP]) change_volume(g, +1);
    if (g->vol_show_t > 0) g->vol_show_t--;
    if (g->vol_save_t > 0 && --g->vol_save_t == 0 && !g->no_save) write_settings(g);

    switch (g->state) {
    case ST_BOOT:
        if (g->state_t == 2) sfx(g, SFX_CHIME, 1.0f);
        if (g->state_t > BOOT_LINE_COUNT * 6 + 50 || (g->state_t > 10 && in->pressed[K_ANY])) set_state(g, ST_MENU);
        break;
    case ST_MENU:
        update_menu(g, in);
        break;
    case ST_SELECT:
        if (in->pressed[K_LEFT]) { g->ch_sel = (g->ch_sel + CH_COUNT - 1) % CH_COUNT; sfx(g, SFX_SELECT, 1.0f); }
        if (in->pressed[K_RIGHT]) { g->ch_sel = (g->ch_sel + 1) % CH_COUNT; sfx(g, SFX_SELECT, 1.0f); }
        if (in->pressed[K_UP]) {
            g->select_mode = (g->select_mode + SCORE_MODE_COUNT - 1) % SCORE_MODE_COUNT;
            g->endless = g->select_mode == SCORE_ENDLESS;
            g->boss_rush = g->select_mode == SCORE_BOSSRUSH;
            sfx(g, SFX_SELECT, 1.0f);
        }
        if (in->pressed[K_DOWN]) {
            g->select_mode = (g->select_mode + 1) % SCORE_MODE_COUNT;
            g->endless = g->select_mode == SCORE_ENDLESS;
            g->boss_rush = g->select_mode == SCORE_BOSSRUSH;
            sfx(g, SFX_SELECT, 1.0f);
        }
        if (in->pressed[K_ESC]) set_state(g, ST_MENU);
        else if (in_confirm(in)) { sfx(g, SFX_CHIME, 1.0f); start_run(g); }
        break;
    case ST_PLAY:
        if (in->pressed[K_ESC]) { g->pause_sel = 0; set_state(g, ST_PAUSE); break; }
        update_play(g, in);
        break;
    case ST_PAUSE:
        if (in->pressed[K_ESC]) { g->state = ST_PLAY; break; }
        if (in->pressed[K_UP]) { g->pause_sel = (g->pause_sel + PAUSE_COUNT - 1) % PAUSE_COUNT; sfx(g, SFX_SELECT, 1.0f); }
        if (in->pressed[K_DOWN]) { g->pause_sel = (g->pause_sel + 1) % PAUSE_COUNT; sfx(g, SFX_SELECT, 1.0f); }
        if (g->pause_sel == PAUSE_VOLUME) {
            if (in->pressed[K_LEFT]) change_volume(g, -1);
            if (in->pressed[K_RIGHT]) change_volume(g, +1);
        }
        if (in_confirm(in) && g->pause_sel != PAUSE_VOLUME) {
            if (g->pause_sel == PAUSE_RESUME) g->state = ST_PLAY;
            else if (g->pause_sel == PAUSE_CODEX) { g->codex_from = ST_PAUSE; g->codex_tab = 0; g->codex_sel = 0; g->state = ST_CODEX; sfx(g, SFX_SELECT, 1.2f); }
            else if (g->pause_sel == PAUSE_SETTINGS) { g->settings_from = ST_PAUSE; g->settings_sel = 0; g->state = ST_SETTINGS; sfx(g, SFX_SELECT, 1.2f); }
            else if (g->pause_sel == PAUSE_RESTART) start_run(g);
            else { g->run_active = false; set_state(g, ST_MENU); }
        }
        break;
    case ST_SETTINGS:
        update_settings(g, in);
        break;
    case ST_CODEX:
        update_codex(g, in);
        break;
    case ST_UPGRADE:
        if (in->pressed[K_LEFT]) { g->upg_sel = (g->upg_sel + 2) % 3; sfx(g, SFX_SELECT, 1.0f); }
        if (in->pressed[K_RIGHT]) { g->upg_sel = (g->upg_sel + 1) % 3; sfx(g, SFX_SELECT, 1.0f); }
        if (in->pressed[K_1]) g->upg_sel = 0, g->state_t = 999;
        if (in->pressed[K_2]) g->upg_sel = 1, g->state_t = 999;
        if (in->pressed[K_3]) g->upg_sel = 2, g->state_t = 999;
        if (in_confirm(in) || in->pressed[K_1] || in->pressed[K_2] || in->pressed[K_3]) {
            if (g->state_t > 12) {
                apply_upgrade(g, g->upg_opt[g->upg_sel]);
                sfx(g, SFX_PICKUP, 1.0f);
                if (g->boss_rush_loadout > 0) {
                    g->boss_rush_loadout--;
                    if (g->boss_rush_loadout > 0) {
                        pick_upgrades(g);
                        set_state(g, ST_UPGRADE);
                        say(g, "LOADOUT PATCH INSTALLED.", C_BGREEN);
                    } else {
                        say(g, "LOADOUT COMPLETE. PID 1 IS WAITING.", C_BGREEN);
                        goto_wave(g, 1);
                    }
                } else {
                    goto_wave(g, g->wave + 1);
                }
            }
        }
        break;
    case ST_STORY:
        update_story(g, in);
        break;
    case ST_PANIC:
        if (g->state_t > PANIC_LINE_COUNT * 12 + 60 && (in_confirm(in) || in->pressed[K_ANY])) set_state(g, ST_RESULTS);
        else if (g->state_t > PANIC_LINE_COUNT * 12 + 200) set_state(g, ST_RESULTS);
        break;
    case ST_RESULTS:
        if (g->state_t > 20) {
            if (in->pressed[K_ESC]) set_state(g, ST_MENU);
            else if (in_confirm(in)) set_state(g, g->victory ? ST_ENDING : ST_SELECT);
        }
        break;
    case ST_ENDING:
        if (g->state_t == 2) sfx(g, SFX_CHIME, 0.7f);
        if (in->pressed[K_ANY] && g->state_t > 20) {
            if (g->state_t < ENDING_FRAMES) g->state_t = ENDING_FRAMES;
            else set_state(g, ST_MENU);
        }
        break;
    case ST_CREDITS:
        if (g->state_t > 30 && (in->pressed[K_ESC] || in_confirm(in))) set_state(g, ST_MENU);
        else if (g->state_t > CREDITS_LINE_COUNT * 14 + 400) set_state(g, ST_MENU);
        break;
    case ST_ACHIEVEMENTS:
        if (in->pressed[K_ANY] || in->pressed[K_ESC]) set_state(g, ST_MENU);
        break;
    case ST_STATS:
        if (in->pressed[K_LEFT]) g->stats_mode = (g->stats_mode + SCORE_MODE_COUNT - 1) % SCORE_MODE_COUNT;
        if (in->pressed[K_RIGHT]) g->stats_mode = (g->stats_mode + 1) % SCORE_MODE_COUNT;
        if ((in->pressed[K_ANY] || in->pressed[K_ESC]) && !in->pressed[K_LEFT] && !in->pressed[K_RIGHT]) set_state(g, ST_MENU);
        break;
    case ST_SCORES:
        if (in->pressed[K_LEFT]) g->scores_mode = (g->scores_mode + SCORE_MODE_COUNT - 1) % SCORE_MODE_COUNT;
        if (in->pressed[K_RIGHT]) g->scores_mode = (g->scores_mode + 1) % SCORE_MODE_COUNT;
        if (g->state_t > 10 && in->pressed[K_ANY] && !in->pressed[K_LEFT] && !in->pressed[K_RIGHT]) set_state(g, ST_MENU);
        break;
    }
    update_music(g);
}

/* ---------------------------------------------------------------- test hooks */

void game_test_start(Game *g, int ch, int wave)
{
    g->ch_sel = ch;
    Player *p = &g->p;
    memset(p, 0, sizeof *p);
    p->ch = ch;
    p->maxhp = p->hp = CHARS[ch].max_hp;
    p->fy = -1;
    g->score = g->kills = g->run_frames = 0;
    g->victory = false; g->sysv_weak = false; g->run_active = true; g->last_rank = -1;
    g->midscene_seen = false;
    g->endless = false;
    g->boss_rush = false;
    g->select_mode = SCORE_CAMPAIGN;
    g->boss_rush_loadout = 0;
    g->wave = wave;
    enter_wave(g);
}

void game_test_start_endless(Game *g, int ch, int wave)
{
    g->ch_sel = ch;
    memset(&g->p, 0, sizeof g->p);
    g->p.ch = ch;
    g->p.maxhp = g->p.hp = CHARS[ch].max_hp;
    g->p.fy = -1;
    g->score = g->kills = g->run_frames = 0;
    g->victory = false;
    g->sysv_weak = false;
    g->run_active = true;
    g->last_rank = -1;
    g->midscene_seen = false;
    g->endless = true;
    g->boss_rush = false;
    g->select_mode = SCORE_ENDLESS;
    g->boss_rush_loadout = 0;
    g->wave = wave;
    enter_wave(g);
}

void game_test_start_bossrush(Game *g, int ch, int wave)
{
    g->ch_sel = ch;
    memset(&g->p, 0, sizeof g->p);
    g->p.ch = ch;
    g->p.maxhp = g->p.hp = CHARS[ch].max_hp;
    g->p.fy = -1;
    g->score = g->kills = g->run_frames = 0;
    g->victory = false;
    g->sysv_weak = false;
    g->run_active = true;
    g->last_rank = -1;
    g->midscene_seen = false;
    g->endless = false;
    g->boss_rush = true;
    g->select_mode = SCORE_BOSSRUSH;
    g->boss_rush_loadout = 0;
    g->wave = wave;
    enter_wave(g);
}

Enemy *game_test_spawn(Game *g, int type, float x, float y, bool restart)
{
    return spawn_enemy(g, type, x, y, restart, false);
}

void game_test_damage(Game *g, Enemy *e, float dmg) { damage_enemy(g, e, dmg); }

void game_test_story_done(Game *g) { finish_story(g); }

void game_test_finish(Game *g, bool victory)
{
    finish_run(g, victory);
}
