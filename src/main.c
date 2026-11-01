/* main.c - SDL2 startup, window, input mapping and the fixed-timestep loop. */
#include <SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "assets.h"
#include "audio.h"
#include "game.h"
#include "render.h"
#include "view.h"

static Frame frame;

/* Some window managers (Window Maker among them) restore a window from fullscreen
 * without accounting for the title bar, so each round trip shrank it. We remember the
 * windowed size and re-apply it once the WM reports that fullscreen is over. */
static int saved_w = 800, saved_h = 600;
static bool restore_size_pending;

static void toggle_fullscreen(SDL_Window *win)
{
    Uint32 fl = SDL_GetWindowFlags(win);
    bool is_fs = (fl & SDL_WINDOW_FULLSCREEN_DESKTOP) == SDL_WINDOW_FULLSCREEN_DESKTOP;
    if (!is_fs) SDL_GetWindowSize(win, &saved_w, &saved_h);
    if (SDL_SetWindowFullscreen(win, is_fs ? 0 : SDL_WINDOW_FULLSCREEN_DESKTOP) != 0)
        fprintf(stderr, "init-wars: fullscreen not available: %s\n", SDL_GetError());
    else
        restore_size_pending = is_fs;
}

static void usage(const char *argv0)
{
    printf("%s %s\n\nUsage: %s [options]\n"
           "  --crt          start with the optional CRT scanline effect on\n"
           "  --no-audio     do not open an audio device\n"
           "  --seed N       seed the random number generator (default: time based)\n"
           "  --no-save      do not write the high-score file\n"
           "  --windowed-size WxH   initial window size (default 800x600)\n"
           "  --version, --help\n"
           "\nAll in-game system text is simulated; the game never touches real services.\n",
           GAME_TITLE, GAME_VERSION, argv0);
}

static void map_key(Input *in, SDL_Keycode k, bool down, bool repeat)
{
    int key = -1;
    switch (k) {
    case SDLK_UP: case SDLK_w:        key = K_UP; break;
    case SDLK_DOWN: case SDLK_s:      key = K_DOWN; break;
    case SDLK_LEFT: case SDLK_a:      key = K_LEFT; break;
    case SDLK_RIGHT: case SDLK_d:     key = K_RIGHT; break;
    case SDLK_SPACE:                  key = K_FIRE; break;
    case SDLK_LSHIFT: case SDLK_RSHIFT: case SDLK_x: key = K_SPECIAL; break;
    case SDLK_RETURN: case SDLK_KP_ENTER: key = K_CONFIRM; break;
    case SDLK_ESCAPE:                 key = K_ESC; break;
    case SDLK_1: case SDLK_KP_1:      key = K_1; break;
    case SDLK_2: case SDLK_KP_2:      key = K_2; break;
    case SDLK_3: case SDLK_KP_3:      key = K_3; break;
    case SDLK_F2:                     key = K_CRT; break;
    case SDLK_m:                      key = K_MUTE; break;
    case SDLK_MINUS: case SDLK_KP_MINUS: case SDLK_LEFTBRACKET:  key = K_VOL_DN; break;
    case SDLK_EQUALS: case SDLK_PLUS: case SDLK_KP_PLUS: case SDLK_RIGHTBRACKET: key = K_VOL_UP; break;
    default: break;
    }
    if (down && !repeat) {
        in->pressed[K_ANY] = true;
        if (key >= 0) in->pressed[key] = true;
    }
    if (key >= 0) in->held[key] = down;
}

int main(int argc, char **argv)
{
    bool crt = false, want_audio = true, no_save = false;
    uint32_t seed = (uint32_t)SDL_GetTicks() ^ 0x5eedu;
    int win_w = 800, win_h = 600;

    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--help") || !strcmp(argv[i], "-h")) { usage(argv[0]); return 0; }
        else if (!strcmp(argv[i], "--version")) { printf("%s %s\n", GAME_TITLE, GAME_VERSION); return 0; }
        else if (!strcmp(argv[i], "--crt")) crt = true;
        else if (!strcmp(argv[i], "--no-audio")) want_audio = false;
        else if (!strcmp(argv[i], "--no-save")) no_save = true;
        else if (!strcmp(argv[i], "--seed") && i + 1 < argc) seed = (uint32_t)strtoul(argv[++i], NULL, 10);
        else if (!strcmp(argv[i], "--windowed-size") && i + 1 < argc) {
            int w, h;
            if (sscanf(argv[++i], "%dx%d", &w, &h) == 2 && w >= LW && h >= LH && w <= 8192 && h <= 8192) { win_w = w; win_h = h; }
            else { fprintf(stderr, "bad --windowed-size (minimum %dx%d)\n", LW, LH); return 2; }
        } else {
            fprintf(stderr, "unknown option: %s\n", argv[i]);
            usage(argv[0]);
            return 2;
        }
    }

    assets_init(argv[0]);
    setenv("SDL_VIDEO_X11_WMCLASS", "InitWars", 0);   /* lets Window Maker keep per-app settings */
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "0");  /* nearest-neighbour, always */
    SDL_SetHint(SDL_HINT_VIDEO_ALLOW_SCREENSAVER, "1");

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS) != 0) {
        fprintf(stderr, "init-wars: SDL_Init failed: %s\n", SDL_GetError());
        return 1;
    }
    SDL_Window *win = SDL_CreateWindow(GAME_TITLE, SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
                                       win_w, win_h, SDL_WINDOW_RESIZABLE | SDL_WINDOW_SHOWN);
    if (!win) {
        fprintf(stderr, "init-wars: cannot create window: %s\n", SDL_GetError());
        SDL_Quit();
        return 1;
    }
    SDL_SetWindowMinimumSize(win, LW, LH);

    bool vsync = true;
    SDL_Renderer *ren = SDL_CreateRenderer(win, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!ren) ren = SDL_CreateRenderer(win, -1, SDL_RENDERER_ACCELERATED);
    if (!ren) { ren = SDL_CreateRenderer(win, -1, SDL_RENDERER_SOFTWARE); vsync = false; }
    if (!ren) {
        fprintf(stderr, "init-wars: cannot create renderer: %s\n", SDL_GetError());
        SDL_DestroyWindow(win);
        SDL_Quit();
        return 1;
    }
    SDL_RendererInfo info;
    if (SDL_GetRendererInfo(ren, &info) == 0 && !(info.flags & SDL_RENDERER_PRESENTVSYNC)) vsync = false;

    /* Logical size + integer scale = distortion-free nearest-neighbour scaling with letterboxing. */
    SDL_RenderSetLogicalSize(ren, LW, LH);
    SDL_RenderSetIntegerScale(ren, SDL_TRUE);
    SDL_Texture *tex = SDL_CreateTexture(ren, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, LW, LH);
    if (!tex) {
        fprintf(stderr, "init-wars: cannot create texture: %s\n", SDL_GetError());
        SDL_DestroyRenderer(ren);
        SDL_DestroyWindow(win);
        SDL_Quit();
        return 1;
    }

    if (want_audio) audio_init();

    static Game game;
    game_init(&game, seed);
    game.crt = game.crt || crt;      /* --crt forces it on; otherwise the saved setting stands */
    game.no_save = no_save;

    Input in;
    memset(&in, 0, sizeof in);
    const double step = 1.0 / FPS;
    double acc = 0;
    Uint64 last = SDL_GetPerformanceCounter();
    const double freq = (double)SDL_GetPerformanceFrequency();
    bool running = true;
    int applied_vol = -1;

    while (running && !game.quit) {
        SDL_Event ev;
        while (SDL_PollEvent(&ev)) {
            switch (ev.type) {
            case SDL_QUIT: running = false; break;
            case SDL_KEYDOWN:
                if (ev.key.repeat == 0 && (ev.key.keysym.sym == SDLK_F11 || ev.key.keysym.sym == SDLK_f ||
                    ((ev.key.keysym.mod & KMOD_ALT) && (ev.key.keysym.sym == SDLK_RETURN || ev.key.keysym.sym == SDLK_KP_ENTER)))) {
                    toggle_fullscreen(win);
                    break;
                }
                map_key(&in, ev.key.keysym.sym, true, ev.key.repeat != 0);
                break;
            case SDL_KEYUP:   map_key(&in, ev.key.keysym.sym, false, false); break;
            case SDL_WINDOWEVENT:
                if (ev.window.event == SDL_WINDOWEVENT_SIZE_CHANGED && restore_size_pending &&
                    !(SDL_GetWindowFlags(win) & SDL_WINDOW_FULLSCREEN_DESKTOP)) {
                    restore_size_pending = false;
                    SDL_SetWindowSize(win, saved_w, saved_h);
                }
                if (ev.window.event == SDL_WINDOWEVENT_FOCUS_LOST || ev.window.event == SDL_WINDOWEVENT_MINIMIZED) {
                    memset(in.held, 0, sizeof in.held);          /* no stuck keys after alt-tab / WM focus change */
                    if (game.state == ST_PLAY) in.pressed[K_ESC] = true;   /* auto-pause; resuming is always the player's choice */
                }
                break;
            default: break;
            }
        }

        Uint64 now = SDL_GetPerformanceCounter();
        acc += (double)(now - last) / freq;
        last = now;
        if (acc > 0.25) acc = 0.25;           /* after a stall, do not run a burst of catch-up frames */
        while (acc >= step) {
            game_update(&game, &in);
            memset(in.pressed, 0, sizeof in.pressed);
            acc -= step;
        }

        if (game.fs_toggle) { toggle_fullscreen(win); game.fs_toggle = false; }
        game.fullscreen = (SDL_GetWindowFlags(win) & SDL_WINDOW_FULLSCREEN_DESKTOP) == SDL_WINDOW_FULLSCREEN_DESKTOP;

        Sfx id; float pitch;
        while (game_pop_sfx(&game, &id, &pitch)) audio_play(id, pitch);
        audio_set_music(game.music);
        audio_set_mute(game.mute);
        if (game.volume != applied_vol) { audio_set_volume(game.volume); applied_vol = game.volume; }

        view_draw(&game, &frame);
        SDL_UpdateTexture(tex, NULL, frame.px, LW * (int)sizeof(uint32_t));
        SDL_SetRenderDrawColor(ren, 0, 0, 0, 255);
        SDL_RenderClear(ren);
        SDL_RenderCopy(ren, tex, NULL, NULL);
        SDL_RenderPresent(ren);
        if (!vsync) SDL_Delay(4);
    }

    game_flush_settings(&game);
    audio_shutdown();
    SDL_DestroyTexture(tex);
    SDL_DestroyRenderer(ren);
    SDL_DestroyWindow(win);
    SDL_Quit();
    return 0;
}
