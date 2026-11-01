/* capture.c - developer tool: drives the REAL game (game_update + view_draw + the SDL2 window and
 * renderer) with the test bot and a few scripted key presses, and saves what the renderer actually
 * put on screen. Nothing is drawn or composited here; frames come from SDL_RenderReadPixels() on the
 * window's own renderer, so they are exactly what a player sees at the window's integer scale.
 *
 *   make capture                       builds build/capture (not part of the normal build)
 *   build/capture SCENARIO OUTDIR [SEED] [CHARACTER 0-4] [--nogod]
 *
 * Fast-forward shortcuts (game_test_start*, god mode for the long boss fights) are the same hooks the
 * test-suite uses; they are listed per scenario in screenshots/README.md. */
#include <SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "game.h"
#include "render.h"
#include "view.h"
#include "story.h"
#include "../tests/bot.h"

static Frame frame;
static Game G;
static Bot bot;
static const char *outdir = ".";
static char pending[64];
static int wanted;          /* number of snapshots the scenario needs */
static int taken;
static int seed = 1, chr = CH_SYSVINIT;
static bool use_god = true;

static void snap(const char *name) { if (!pending[0]) snprintf(pending, sizeof pending, "%s", name); }

static void save_shot(SDL_Renderer *ren, SDL_Window *win, const char *name)
{
    int w, h;
    SDL_GetRendererOutputSize(ren, &w, &h);
    SDL_Surface *s = SDL_CreateRGBSurfaceWithFormat(0, w, h, 32, SDL_PIXELFORMAT_ARGB8888);
    if (!s) return;
    if (SDL_RenderReadPixels(ren, NULL, SDL_PIXELFORMAT_ARGB8888, s->pixels, s->pitch) == 0) {
        char bmp[512];
        snprintf(bmp, sizeof bmp, "%s/%s.bmp", outdir, name);
        SDL_SaveBMP(s, bmp);
        fprintf(stderr, "captured %s (%dx%d, state=%d wave=%d)\n", name, w, h, (int)G.state, G.wave);
        taken++;
    }
    SDL_FreeSurface(s);
    (void)win;
}

static int distinct_enemy_types(const Game *g)
{
    int seen[32] = {0}, n = 0;
    for (int i = 0; i < MAX_ENEMY; i++) {
        const Enemy *e = &g->en[i];
        if (!e->on || e->warp > 0 || e->type >= 32 || seen[e->type]) continue;
        seen[e->type] = 1; n++;
    }
    return n;
}
static int bullets(const Game *g) { int n = 0; for (int i = 0; i < MAX_EBUL; i++) n += g->eb[i].on; return n; }

/* Per-frame scripted driver. Fills `in`; may call snap(). Returns true when the scenario is complete. */
static int stage, t_stage, since_shot;

static void go(int s) { stage = s; t_stage = 0; }

static void press(Input *in, Key k) { in->pressed[k] = true; in->pressed[K_ANY] = true; }

static bool drive(Game *g, Input *in, const char *sc, int step)
{
    t_stage++; since_shot++;
    if (pending[0]) return false;
    if (!strcmp(sc, "menu")) {
        switch (stage) {
        case 0: if (g->state != ST_BOOT) { go(1); }
                else if (t_stage > 90) { snap("01-title-screen"); go(1); }
                break;
        case 1: if (g->state == ST_BOOT) { if (t_stage % 5 == 0) press(in, K_ANY); }
                else if (g->state == ST_MENU && t_stage > 30 && ((g->frame / 30) & 1)) { snap("02-main-menu"); go(2); }
                break;
        case 2: if (t_stage > 20) { press(in, K_CONFIRM); go(3); } break;
        case 3: if (g->state == ST_SELECT && t_stage > 25) { snap("03-character-select"); go(4); } break;
        case 4: return true;
        }
        return false;
    }
    if (!strcmp(sc, "campaign")) {
        switch (stage) {
        case 0: game_test_start(g, chr, 3); g->god = use_god; go(1); break;
        case 1: bot_input(&bot, &BOT_EXPERT, g, in, chr, step);
                if (g->state == ST_PLAY && g->phase == PH_FIGHT && g->phase_t > 400 && game_enemies_alive(g) >= 2 && bullets(g) >= 8) { snap("04-campaign-gameplay"); go(2); }
                break;
        case 2: bot_input(&bot, &BOT_EXPERT, g, in, chr, step);
                if (g->state == ST_PLAY && g->phase == PH_CLEAR && g->phase_t > 25) { snap("06-wave-clear"); go(3); } break;
        case 3: if (g->state != ST_UPGRADE) bot_input(&bot, &BOT_EXPERT, g, in, chr, step);   /* hold the pick screen */
                if (g->state == ST_UPGRADE && g->state_t > 40) { snap("07-upgrade-screen"); go(4); } break;
        case 4: return true;
        }
        if (t_stage > 60 * 150) { fprintf(stderr, "scenario timed out in stage %d\n", stage); return true; }
        return false;
    }
    if (!strcmp(sc, "enemies")) {
        switch (stage) {
        case 0: game_test_start(g, chr, 7); g->god = use_god; go(1); break;
        case 1: bot_input(&bot, &BOT_EXPERT, g, in, chr, step);
                if (g->state == ST_PLAY && g->phase == PH_FIGHT && distinct_enemy_types(g) >= 4 && bullets(g) >= 6) { snap("05-enemy-showcase"); go(2); }
                break;
        case 2: return true;
        }
        if (t_stage > 60 * 150) { fprintf(stderr, "scenario timed out\n"); return true; }
        return false;
    }
    if (!strcmp(sc, "story")) {
        switch (stage) {
        case 0: game_test_start(g, CH_SYSVINIT, 9); g->god = use_god; go(1); break;
        case 1: bot_input(&bot, &BOT_EXPERT, g, in, CH_SYSVINIT, step);
                if (g->state == ST_STORY && g->scene == SC_REDHAT && g->scene_line >= 2 && game_scene_chars(g) > 30) { snap("08-sysvinit-story"); go(2); }
                break;
        case 2: return true;
        }
        if (t_stage > 60 * 150) { fprintf(stderr, "scenario timed out\n"); return true; }
        return false;
    }
    if (!strcmp(sc, "boss")) {
        switch (stage) {
        case 0: game_test_start(g, chr, BOSS_WAVE); g->god = use_god; go(1); break;
        case 1: bot_input(&bot, &BOT_EXPERT, g, in, chr, step);
                if (g->state == ST_PLAY && game_boss_alive(g) && g->boss_phase == 1 && g->boss_t > 300 && bullets(g) >= 6) { snap("09-systemd-boss"); go(2); }
                break;
        case 2: bot_input(&bot, &BOT_EXPERT, g, in, chr, step);
                if (g->state == ST_PLAY && game_boss_alive(g) && g->boss_phase >= 3 && g->boss_t > 200 && bullets(g) >= 4) { snap("10-boss-phase"); go(3); }
                break;
        case 3: return true;
        }
        if (t_stage > 60 * 400) { fprintf(stderr, "scenario timed out in stage %d\n", stage); return true; }
        return false;
    }
    if (!strcmp(sc, "endless")) {
        switch (stage) {
        case 0: game_test_start_endless(g, chr, 8); g->god = use_god; go(1); break;
        case 1: bot_input(&bot, &BOT_EXPERT, g, in, chr, step);
                if (g->state == ST_PLAY && g->phase == PH_FIGHT && game_enemies_alive(g) >= 5 && bullets(g) >= 8) { snap("11-endless-mode"); go(2); }
                break;
        case 2: return true;
        }
        if (t_stage > 60 * 150) { fprintf(stderr, "scenario timed out\n"); return true; }
        return false;
    }
    if (!strcmp(sc, "bossrush")) {
        switch (stage) {
        case 0: game_test_start_bossrush(g, chr, 3); g->god = use_god; go(1); break;
        case 1: bot_input(&bot, &BOT_EXPERT, g, in, chr, step);
                if (g->state == ST_PLAY && game_boss_alive(g) && g->boss_t > 300 && bullets(g) >= 8) { snap("12-boss-rush"); go(2); }
                break;
        case 2: return true;
        }
        if (t_stage > 60 * 300) { fprintf(stderr, "scenario timed out\n"); return true; }
        return false;
    }
    if (!strcmp(sc, "ending")) {
        switch (stage) {
        case 0: game_test_start(g, chr, BOSS_WAVE); g->god = use_god; go(1); break;
        case 1: bot_input(&bot, &BOT_EXPERT, g, in, chr, step);
                if (g->state == ST_ENDING && g->state_t > 380) { snap("16-results-or-ending"); go(2); }
                if (g->state == ST_RESULTS && g->state_t > 60) { snap("16-results-or-ending"); go(2); }
                break;
        case 2: return true;
        }
        if (t_stage > 60 * 500) { fprintf(stderr, "scenario timed out\n"); return true; }
        return false;
    }
    if (!strcmp(sc, "tabs")) {      /* scores, achievements, statistics: opened with real key presses */
        switch (stage) {
        case 0: if (g->state == ST_BOOT) { if (t_stage % 5 == 0) press(in, K_ANY); } else if (g->state == ST_MENU) go(1); break;
        case 1: if (t_stage % 12 == 0) { if (g->menu_sel < MENU_SCORES) press(in, K_DOWN); else { press(in, K_CONFIRM); go(2); } } break;
        case 2: if (g->state == ST_SCORES && t_stage > 30) { snap("13-high-scores"); go(3); } break;
        case 3: if (t_stage > 15) { press(in, K_ESC); go(4); } break;
        case 4: if (g->state == ST_MENU && t_stage > 20) go(5); break;
        case 5: if (t_stage % 12 == 0) { if (g->menu_sel < MENU_ACHIEVEMENTS) press(in, K_DOWN); else { press(in, K_CONFIRM); go(6); } } break;
        case 6: if (g->state == ST_ACHIEVEMENTS && t_stage > 30) { snap("14-achievements"); go(7); } break;
        case 7: if (t_stage > 15) { press(in, K_ESC); go(8); } break;
        case 8: if (g->state == ST_MENU && t_stage > 20) go(9); break;
        case 9: if (t_stage % 12 == 0) { if (g->menu_sel < MENU_STATS) press(in, K_DOWN); else { press(in, K_CONFIRM); go(10); } } break;
        case 10: if (g->state == ST_STATS && t_stage > 30) { snap("15-run-statistics"); go(11); } break;
        case 11: return true;
        }
        if (t_stage > 60 * 60) { fprintf(stderr, "scenario timed out in stage %d\n", stage); return true; }
        return false;
    }
    if (!strcmp(sc, "storyA") || !strcmp(sc, "storyB")) {   /* 1.5.0 story scenes: A starts at wave 10 (Dinit scene plays before wave 11), B at wave 20 (Last Runlevel before wave 21) */
        static int got;
        if (stage == 0) { game_test_start(g, chr, sc[5] == 'A' ? 10 : 20); g->god = use_god; got = 0; go(1); return false; }
        if (g->state != ST_STORY && g->state != ST_ENDING) {
            bot_input(&bot, &BOT_EXPERT, g, in, chr, step);
        } else if (g->state == ST_STORY) {
            const Scene *scn = story_scene((SceneId)g->scene);
            const StoryLine *ln = &scn->lines[g->scene_line];
            bool dinit = !strcmp(ln->who, "Dinit"), typed = game_scene_chars(g) > 28;
            bool hold = false;
            if (sc[5] == 'A' && g->scene == SC_DINIT && dinit && typed && !(got & 1)) { snap("17-dinit-introduction"); got |= 1; hold = true; }
            if (sc[5] == 'B' && g->scene == SC_LAST_RUNLEVEL && g->scene_line >= 2 && typed && !(got & 2)) { snap("18-last-runlevel"); got |= 2; hold = true; }
            if (sc[5] == 'B' && g->scene == SC_BOSS && dinit && typed && !(got & 4)) { snap("19-pid1-handoff"); got |= 4; hold = true; }
            if (sc[5] == 'B' && g->scene == SC_EPILOGUE && dinit && typed && !(got & 8)) { snap("20-epilogue-dinit"); got |= 8; hold = true; }
            if (sc[5] == 'B' && g->scene == SC_EPILOGUE && !strcmp(ln->who, "SysVinit") && typed && !(got & 16)) { snap("21-epilogue-sysvinit"); got |= 16; hold = true; }
            if (!hold) bot_input(&bot, &BOT_EXPERT, g, in, chr, step);
        } else if (g->state == ST_ENDING && sc[5] == 'B' && g->state_t > 380 && !(got & 32)) { snap("22-ending-card"); got |= 32; }
        else if (g->state == ST_ENDING) { /* wait for the card to finish typing */ }
        if (sc[5] == 'A' && (got & 1)) return true;
        if (sc[5] == 'B' && (got & 62) == 62) return true;
        if (t_stage > 60 * 420) { fprintf(stderr, "timed out, got mask %d\n", got); return true; }
        return false;
    }
    fprintf(stderr, "unknown scenario '%s'\n", sc);
    return true;
}

/* Headless: play one complete run in the given mode with the bot, through the real persistence path. */
static void headless_run(int mode, int ch, uint32_t sd, bool god)
{
    static Game H;
    game_init(&H, sd);
    Input in; memset(&in, 0, sizeof in);
    Bot b = {0}; b.rng = sd * 2654435761u + 1u;
    if (mode == 0) game_test_start(&H, ch, 1);
    else if (mode == 1) game_test_start_endless(&H, ch, 1);
    else game_test_start_bossrush(&H, ch, 1);
    H.god = god;
    for (int step = 0; step < 60 * 60 * 25 && !H.quit; step++) {
        bot_input(&b, &BOT_EXPERT, &H, &in, ch, step);
        game_update(&H, &in);
        if (H.state == ST_RESULTS || H.state == ST_ENDING) break;
        if (H.state == ST_PANIC) { for (int k = 0; k < 400; k++) { bot_input(&b, &BOT_EXPERT, &H, &in, ch, k); game_update(&H, &in); if (H.state != ST_PANIC) break; } break; }
        if (mode == 1 && H.wave >= 12 + ch * 3) { game_test_finish(&H, false); break; }   /* retire an Endless run at a modest wave */
    }
    game_flush_settings(&H);
}

int main(int argc, char **argv)
{
    if (argc < 3) { fprintf(stderr, "usage: %s SCENARIO OUTDIR [SEED] [CHARACTER] [--nogod]\n", argv[0]); return 2; }
    const char *sc = argv[1];
    outdir = argv[2];
    for (int i = 3; i < argc; i++) {
        if (!strcmp(argv[i], "--nogod")) use_god = false;
        else if (i == 3) seed = atoi(argv[i]);
        else if (i == 4) chr = atoi(argv[i]);
    }
    bot.rng = (uint32_t)seed * 2246822519u + 7u;

    if (!strcmp(sc, "tabs")) {      /* fill the three leaderboards / profile / achievements with real bot runs first */
        for (int m = 0; m < 3; m++)
            for (int c = 0; c < 5; c++) headless_run(m, c, (uint32_t)(100 + m * 10 + c), m != 1);
    }

    SDL_SetHint(SDL_HINT_VIDEO_ALLOW_SCREENSAVER, "1");
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS) != 0) { fprintf(stderr, "SDL_Init: %s\n", SDL_GetError()); return 1; }
    SDL_Window *win = SDL_CreateWindow("INIT WARS capture", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, LW * 3, LH * 3, SDL_WINDOW_SHOWN);
    SDL_Renderer *ren = SDL_CreateRenderer(win, -1, SDL_RENDERER_ACCELERATED);
    if (!ren) ren = SDL_CreateRenderer(win, -1, SDL_RENDERER_SOFTWARE);
    if (!win || !ren) { fprintf(stderr, "window/renderer: %s\n", SDL_GetError()); return 1; }
    SDL_RenderSetLogicalSize(ren, LW, LH);
    SDL_RenderSetIntegerScale(ren, SDL_TRUE);
    SDL_Texture *tex = SDL_CreateTexture(ren, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, LW, LH);

    game_init(&G, (uint32_t)seed);
    Input in; memset(&in, 0, sizeof in);
    const double step_t = 1.0 / FPS;
    double acc = 0;
    Uint64 last = SDL_GetPerformanceCounter();
    const double freq = (double)SDL_GetPerformanceFrequency();
    int step = 0;
    bool done = false, quit = false;
    int tail = 0;
    while (!quit && !G.quit && tail < 20) {
        SDL_Event ev;
        while (SDL_PollEvent(&ev)) if (ev.type == SDL_QUIT) quit = true;
        Uint64 now = SDL_GetPerformanceCounter();
        acc += (double)(now - last) / freq; last = now;
        if (acc > 0.25) acc = 0.25;
        while (acc >= step_t) {
            memset(in.pressed, 0, sizeof in.pressed);
            if (!done) done = drive(&G, &in, sc, step++);
            game_update(&G, &in);
            acc -= step_t;
        }
        view_draw(&G, &frame);
        SDL_UpdateTexture(tex, NULL, frame.px, LW * (int)sizeof(uint32_t));
        SDL_SetRenderDrawColor(ren, 0, 0, 0, 255);
        SDL_RenderClear(ren);
        SDL_RenderCopy(ren, tex, NULL, NULL);
        if (pending[0]) { save_shot(ren, win, pending); pending[0] = '\0'; }
        SDL_RenderPresent(ren);
        if (done) tail++;
        SDL_Delay(4);
    }
    (void)wanted;
    SDL_DestroyTexture(tex); SDL_DestroyRenderer(ren); SDL_DestroyWindow(win); SDL_Quit();
    return taken > 0 ? 0 : 3;
}
