/* profile.h - persistent lifetime statistics, separated by game mode. */
#ifndef PROFILE_H
#define PROFILE_H

#include <stdbool.h>
#include <stdint.h>

#include "score.h"

typedef struct {
    uint64_t runs;
    uint64_t wins;
    uint64_t kills;
    uint64_t frames;
    int best_score;
    int best_wave;
} ModeStats;

typedef struct {
    ModeStats mode[SCORE_MODE_COUNT];
} ProfileStats;

void profile_load(ProfileStats *stats);
bool profile_save(const ProfileStats *stats);
void profile_record_run(ProfileStats *stats, int mode, int score, int wave,
                        int kills, int frames, bool victory);

#endif
