/* balance.h - character, enemy and wave definitions. All tuning lives in
 * balance.c (tables) and in the #defines below. */
#ifndef BALANCE_H
#define BALANCE_H

#include "common.h"

typedef enum { CH_SYSVINIT, CH_RUNIT, CH_S6, CH_OPENRC, CH_BUSYBOX, CH_COUNT } CharId;
typedef enum { SP_PULSE, SP_DASH, SP_STASIS, SP_CLEAR, SP_SHIELD } SpecialKind;

typedef struct {
    const char *name;
    const char *tagline;
    const char *special_name;
    const char *special_desc;
    int max_hp;
    float speed;        /* pixels per frame */
    int fire_cd;        /* frames between shots */
    float damage;
    float bullet_speed;
    int half_hit;       /* half-size of the hitbox in pixels */
    int special_cd;     /* frames */
    SpecialKind special;
    int color;
} CharDef;
extern const CharDef CHARS[CH_COUNT];

typedef enum {
    E_NONE, E_JOURNALD, E_LOGIND, E_NETWORKD, E_TIMER, E_UDEVD, E_RESOLVED,
    E_DEPTARGET, E_USB, E_FLOPPY, E_PRINTER, E_FAKE, E_MARKER, E_CRON, E_OOMD, E_BOSS, E_COUNT
} EnemyType;

typedef struct {
    const char *name;
    const char *sig;    /* signature message shown in the status line */
    float hp;
    int w, h;
    int score;
    bool harmful;       /* touching it hurts */
    bool counts;        /* must be cleared to finish the wave */
} EnemyDef;
extern const EnemyDef ENEMIES[E_COUNT];

typedef struct { uint8_t type, count, restart; } SpawnDef;
#define MAX_WAVE_SPAWNS 7
typedef struct { SpawnDef s[MAX_WAVE_SPAWNS]; int n; } WaveDef;
#define NUM_WAVES 24
#define BOSS_WAVE 24
extern const WaveDef WAVES[NUM_WAVES];
extern const char *const WAVE_NAMES[NUM_WAVES];

typedef enum { UPG_HP, UPG_FIRE, UPG_DMG, UPG_SPEED, UPG_SPECIAL, UPG_COUNT } UpgradeId;
extern const char *const UPG_NAME[UPG_COUNT];
extern const char *const UPG_DESC[UPG_COUNT];

/* ---- global tuning (frames unless noted) ---- */
#define PLAYER_IFRAMES      75
#define SYSV_WEAK_FIRE_MULT 1.20f   /* SysVinit fire cooldown after the Act II scene */
#define SPAWN_WARP          28
#define SPAWN_INTERVAL      24
#define WAVE_INTRO_FRAMES   100
#define WAVE_CLEAR_FRAMES   80
#define MARKER_DELAY        150     /* Restart=always telegraph */
#define MARKER_RESPAWN_HP   0.6f
#define LINK_DAMAGE_MULT    0.5f    /* dependency.target links */
#define FLOPPY_DROP_CHANCE  0.04f
#define FLOPPY_DROP_MINION  0.15f
#define BOSS_SURVIVE_FRAMES 540
#define MAX_MINIONS         6
#define MAX_FAKES           4
#define SYSV_WEAK_DMG_MULT  0.90f   /* SysVinit damage after the Act II scene */
#define PULSE_RADIUS        56.0f
#define PULSE_DAMAGE        6.0f
#define DASH_FRAMES         14
#define DASH_SPEED          5.0f
#define STASIS_RADIUS       80.0f
#define STASIS_FRAMES       180
#define STASIS_FACTOR       0.4f
#define SHIELD_FRAMES       120
#define NETLINE_SLOW_FRAMES 60
#define NETLINE_SLOW        0.5f
#define BOSS_LINK_MULT      0.5f    /* boss damage while its dependency blob lives */
/* the boss roams this box (centre of the sprite) at up to BOSS_MAX_STEP pixels per frame */
#define BOSS_MIN_X (AX0 + 32)
#define BOSS_MAX_X (AX1 - 32)
#define BOSS_MIN_Y (AY0 + 24)
#define BOSS_MAX_Y (AY0 + 124)
#define BOSS_MAX_STEP 3.2f
#define CAMPAIGN_BOSS_HP_MULT 1.25f /* the 24-wave campaign boss meets a fully patched player */
/* Campaign foes toughen gently after wave 9 so a long patch chain does not trivialise the late waves. */
#define CAMPAIGN_HP_START   9
#define CAMPAIGN_HP_STEP    0.025f

/* waves that begin with a maintenance break: heal 1 HP when the next wave starts */
#define REST_WAVE(w) ((w) % 4 == 0)

#endif
