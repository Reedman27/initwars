/* game.h - game state and the fixed-timestep update. No SDL in here: the game is
 * driven by an Input struct and reports sound through a small event queue, so the
 * headless tests and the bot can run the real game logic. */
#ifndef GAME_H
#define GAME_H

#include "achievement.h"
#include "audio.h"
#include "balance.h"
#include "common.h"
#include "input.h"
#include "profile.h"
#include "score.h"
#include "story.h"

typedef enum {
    ST_BOOT, ST_MENU, ST_SELECT, ST_PLAY, ST_PAUSE, ST_UPGRADE, ST_STORY,
    ST_PANIC, ST_RESULTS, ST_ENDING, ST_CREDITS, ST_SCORES, ST_ACHIEVEMENTS, ST_STATS, ST_SETTINGS, ST_CODEX
} GameState;

typedef enum { PH_INTRO, PH_FIGHT, PH_CLEAR } WavePhase;
typedef enum { MENU_START, MENU_ENDLESS, MENU_BOSSRUSH, MENU_SCORES, MENU_ACHIEVEMENTS, MENU_STATS, MENU_CODEX, MENU_SETTINGS, MENU_VOLUME, MENU_CRT, MENU_CREDITS, MENU_QUIT, MENU_COUNT } MenuItem;
typedef enum { PAUSE_RESUME, PAUSE_VOLUME, PAUSE_CODEX, PAUSE_SETTINGS, PAUSE_RESTART, PAUSE_MENU, PAUSE_COUNT } PauseItem;
typedef enum { SET_VOLUME, SET_MUSIC, SET_MUTE, SET_CRT, SET_SHAKE, SET_FULLSCREEN, SET_BACK, SET_COUNT } SettingsItem;

#define MAX_PBUL    48
#define MAX_EBUL    192
#define MAX_ENEMY   40
#define MAX_LINES   4
#define MAX_BARRIER 3
#define MAX_PICK    3
#define MAX_PART    96
#define MAX_POPUP   8
#define MAX_QUEUE   24
#define MAX_SFXQ    24

typedef struct { bool on; float x, y, vx, vy, dmg; int life; } PBullet;
typedef struct { bool on; float x, y, vx, vy; int life; uint8_t kind, src; } EBullet;

typedef struct {
    bool on;
    uint8_t type;
    uint8_t sub;          /* marker: type to respawn; fake: type it imitates */
    float x, y, vx, vy;   /* centre */
    float hp, maxhp;
    float t, t2;          /* behaviour timers (advance slower under s6-stasis) */
    float tx, ty;         /* wander target */
    float ang;
    int   link_t;         /* frames a dependency link has left */
    int   warp;           /* frames of warp-in left (harmless and untargetable) */
    int   flash;
    int   life;           /* fakes: frames to live */
    int   link;           /* index of the dependency.target linking this enemy, -1 none */
    uint8_t state;
    bool restart;         /* carries the Restart=always modifier */
    bool restarted;       /* has already come back once */
    bool summoned;        /* boss helper: removed when the boss dies */
    float ts;             /* time scale this frame (1.0 normal) */
} Enemy;

typedef struct { bool on; bool vertical; float x0, y0, x1, y1; int age, tele, life; } NetLine;
typedef struct { bool on; Rectf r; int age, tele, life; } Barrier;
typedef struct { bool on; float x, y; int life; } Pickup;
typedef struct { bool on; float x, y, vx, vy; int life; uint8_t color; } Particle;
typedef struct { bool on; float x, y; int life; uint8_t color; char text[16]; } Popup;

typedef struct {
    float x, y, fx, fy;
    int ch, hp, maxhp;
    int fire_t, special_t, inv, dash_t, shield_t, slow_t, stasis_t, hurt_flash, idle_t;
    float dashx, dashy;
    int up[UPG_COUNT];
} Player;

typedef struct { uint8_t type, restart; } QueueItem;
typedef struct { uint8_t id; uint8_t pitch100; } SfxEvent;

typedef struct Game {
    GameState state, prev_state;
    uint32_t rng;
    int frame;                  /* global update counter */
    int state_t;                /* frames spent in the current state */

    /* menus */
    int menu_sel, ch_sel, pause_sel, scores_mode, stats_mode;
    int settings_sel;            /* highlighted row of the settings screen */
    int codex_tab, codex_sel;    /* Codex screen: current tab and highlighted entry */
    GameState codex_from;        /* ST_PAUSE or ST_MENU: where the Codex returns to */
    GameState settings_from;     /* ST_PAUSE or ST_MENU: where the settings screen returns to */
    int select_mode;             /* Campaign / Endless / Boss Rush selector */
    bool crt, mute, quit;
    bool no_music;              /* settings: background music off (sound effects stay) */
    bool no_shake;              /* settings: screen shake off */
    bool fullscreen;            /* display only: main.c reports the window state here */
    bool fs_toggle;             /* request for main.c to toggle fullscreen; main.c clears it */
    int volume;                 /* master volume step 0..VOL_MAX, saved between runs */
    int vol_show_t;             /* >0: draw the small volume readout */
    int vol_save_t;             /* >0: frames until changed settings (volume, CRT, music, shake) are written to disk */

    /* run */
    Player p;
    PBullet pb[MAX_PBUL];
    EBullet eb[MAX_EBUL];
    Enemy en[MAX_ENEMY];
    NetLine lines[MAX_LINES];
    Barrier bars[MAX_BARRIER];
    Pickup picks[MAX_PICK];
    Particle parts[MAX_PART];
    Popup pops[MAX_POPUP];

    int wave;                   /* 1-based */
    int wave_threat;             /* initial threat budget used by Endless generation */
    int boss_rush_loadout;       /* remaining picks before Boss Rush round one */
    float enemy_tempo;           /* open-mode movement, attack and projectile multiplier */
    float boss_clock;            /* fractional boss cadence clock for tempo scaling */
    WavePhase phase;
    int phase_t;
    QueueItem queue[MAX_QUEUE];
    int q_n, q_i, spawn_t;
    int score, kills, run_frames, hp_at_wave_start;
    bool victory, sysv_weak, run_active;
    bool endless, boss_rush, boss_wave;
    int last_rank;

    /* boss */
    int boss_phase, boss_t, boss_inv, boss_survive;
    bool boss_exposed;

    /* story */
    int scene, scene_line, scene_t, scene_chars;
    bool scene_quiet;
    bool scene_inplay;          /* scene is overlaid on a frozen fight and returns to it */
    bool midscene_seen;         /* the mid-wave scene of this run has already played */

    /* upgrade pick */
    int upg_opt[3], upg_sel;

    /* presentation helpers (read by view.c / main.c) */
    char status[2][72];
    uint8_t status_col[2];
    int status_t;
    int shake;
    int flash_t;
    int joke_t, joke_idx;
    int last_quip[Q_COUNT];
    float music;
    float sysv_pitch;

    ScoreTable scores;
    ProfileStats profile;
    uint32_t achievements;
    bool achievement_save_failed;
    bool profile_save_failed;

    /* sound events for main.c to drain */
    SfxEvent sfxq[MAX_SFXQ];
    int sfx_n;

    /* statistics (used by the balance report and the results screen) */
    int hurt_src[E_COUNT + 2];   /* hits taken, indexed by enemy type; E_COUNT = bullets */
    int hurt_by_wave[NUM_WAVES + 1];
    int hurt_boss_phase[6];      /* hits taken while the boss was in phase 1..4 */
    bool save_failed;
    int cur_src;                /* enemy type currently acting (tags its bullets) */
    int hurt_flash_t;           /* red arena border after the player is hit */
    int banner_t;               /* >0: show banner_text over the arena */
    char banner_text[2][48];
    struct { bool on; float x, y; int t, max_r; uint8_t color; } rings[2];

    /* debug / test hooks */
    bool god, no_save;
} Game;

void game_init(Game *g, uint32_t seed);
void game_update(Game *g, const Input *in);   /* exactly one 1/60 s step */
void game_flush_settings(Game *g);            /* write a pending volume change now (call on exit) */
bool game_pop_sfx(Game *g, Sfx *id, float *pitch);

/* helpers shared with view.c */
int  game_player_fire_cd(const Game *g);
float game_player_damage(const Game *g);
int  game_special_cd(const Game *g);
bool game_boss_alive(const Game *g);
int  game_enemies_alive(const Game *g);
const Enemy *game_boss(const Game *g);
int  game_scene_chars(const Game *g);   /* characters of the current story line already typed */
int  game_minion_count(const Game *g);


/* Test hooks: let the headless tests set up situations directly. */
void   game_test_start(Game *g, int ch, int wave);   /* new campaign run as 'ch', jumping straight into 'wave' */
void   game_test_start_endless(Game *g, int ch, int wave);
void   game_test_start_bossrush(Game *g, int ch, int wave);
Enemy *game_test_spawn(Game *g, int type, float x, float y, bool restart);
void   game_test_damage(Game *g, Enemy *e, float dmg);
void   game_test_story_done(Game *g);                /* finish the current story scene as if skipped */
void   game_test_finish(Game *g, bool victory);        /* finish a run through the real persistence path */

#endif
