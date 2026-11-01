/* achievement.h - persistent, mode-aware run achievements. */
#ifndef ACHIEVEMENT_H
#define ACHIEVEMENT_H

#include <stdbool.h>
#include <stdint.h>

typedef enum {
    ACH_FIRST_PROCESS,
    ACH_JANITOR,
    ACH_LONG_HAUL,
    ACH_LAST_PID,
    ACH_BOSS_BREAKER,
    ACH_PRISTINE_BOOT,
    ACH_LEGACY_SUPPORT,
    ACH_LAST_STAND,
    ACH_COUNT
} AchievementId;

typedef struct {
    const char *name;
    const char *description;
} AchievementInfo;

const AchievementInfo *achievement_info(AchievementId id);
uint32_t achievements_load(void);
bool achievements_save(uint32_t unlocked);
uint32_t achievements_for_run(uint32_t unlocked, int mode, int character,
                              int kills, int wave, int victory, int hp, int max_hp);
int achievements_count(uint32_t unlocked);

#endif
