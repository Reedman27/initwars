/* story.h - short scripted scenes, quips and fake system text. Pure data + tiny helpers. */
#ifndef STORY_H
#define STORY_H

#include "common.h"

typedef struct {
    const char *who;    /* speaker label, "" for a system/log line */
    const char *text;
    int color;          /* palette index */
    int pause;          /* frames of silence before the line starts */
} StoryLine;

typedef struct {
    const char *name;
    const StoryLine *lines;
    int n;
    bool quiet;         /* subdued palette, music fades out */
} Scene;

typedef enum {
    SC_ACT1,
    SC_TRACE,
    SC_BRIDGE,
    SC_QUIET,
    SC_UPSTART,
    SC_ACT2,
    SC_VOTE,
    SC_REDHAT,
    SC_HOLDOUTS,
    SC_DINIT,
    SC_ACT3,
    SC_SCOPE,
    SC_LOOP,
    SC_PLAN,
    SC_DESCENT,
    SC_LAST_RUNLEVEL,
    SC_EVE,
    SC_BOSS,
    SC_EPILOGUE,
    SC_COUNT
} SceneId;

const Scene *story_scene(SceneId id);
/* Scene played before the given wave (1-based), or -1 for none. */
int story_scene_before_wave(int wave);
/* Scene that interrupts the fight once half of the wave has spawned, or -1 for none. */
int story_scene_midwave(int wave);

typedef enum { Q_WAVE, Q_KILL, Q_HURT, Q_LOW, Q_SPECIAL, Q_PICKUP, Q_JOKE, Q_COUNT } QuipKind;
/* Picks a quip, never repeating the previous pick for that category (*last). */
const char *story_quip(QuipKind k, uint32_t *rng, int *last);

const char *story_joke_reveal(int idx);

extern const char *const BOOT_LINES[];
extern const int BOOT_LINE_COUNT;
extern const char *const PANIC_LINES[];
extern const int PANIC_LINE_COUNT;
extern const char *const CREDITS_LINES[];
extern const int CREDITS_LINE_COUNT;
extern const char *const HIDDEN_MESSAGE;

#endif
