#include "achievement.h"
#include "balance.h"
#include "score.h"

#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

static const AchievementInfo INFO[ACH_COUNT] = {
    { "FIRST PROCESS", "Destroy your first enemy" },
    { "JANITOR", "Destroy 50 enemies in one run" },
    { "LONG HAUL", "Reach wave 10 in Endless Mode" },
    { "THE LAST PID", "Complete the campaign" },
    { "BOSS BREAKER", "Clear all eight Boss Rush rounds" },
    { "PRISTINE BOOT", "Win a run without losing HP" },
    { "LEGACY SUPPORT", "Complete the campaign as SysVinit" },
    { "LAST STAND", "Win with exactly 1 HP remaining" }
};

const AchievementInfo *achievement_info(AchievementId id)
{
    return id >= 0 && id < ACH_COUNT ? &INFO[id] : NULL;
}

static bool data_dir(char *out, size_t n)
{
    const char *e = getenv("INITWARS_DATA_DIR");
    int r;
    if (e && *e) r = snprintf(out, n, "%s", e);
    else if ((e = getenv("XDG_DATA_HOME")) && *e) r = snprintf(out, n, "%s/init-wars", e);
    else if ((e = getenv("HOME")) && *e) r = snprintf(out, n, "%s/.local/share/init-wars", e);
    else return false;
    return r > 0 && (size_t)r < n;
}

static bool ensure_dir(const char *path)
{
    char tmp[PATH_MAX];
    if (strlen(path) >= sizeof tmp) return false;
    snprintf(tmp, sizeof tmp, "%s", path);
    for (char *p = tmp + 1; *p; p++) {
        if (*p != '/') continue;
        *p = '\0';
        if (mkdir(tmp, 0700) != 0 && errno != EEXIST) return false;
        *p = '/';
    }
    return mkdir(tmp, 0700) == 0 || errno == EEXIST;
}

static bool achievement_path(char *out, size_t n)
{
    char dir[PATH_MAX];
    if (!data_dir(dir, sizeof dir)) return false;
    int r = snprintf(out, n, "%s/achievements.txt", dir);
    return r > 0 && (size_t)r < n;
}

uint32_t achievements_load(void)
{
    char path[PATH_MAX];
    if (!achievement_path(path, sizeof path)) return 0;
    FILE *fp = fopen(path, "r");
    if (!fp) return 0;
    unsigned int value = 0;
    int ok = fscanf(fp, "INITWARS_ACHIEVEMENTS 1 %x", &value);
    fclose(fp);
    uint32_t valid = (UINT32_C(1) << ACH_COUNT) - 1U;
    return ok == 1 ? ((uint32_t)value & valid) : 0;
}

bool achievements_save(uint32_t unlocked)
{
    char dir[PATH_MAX], path[PATH_MAX], tmp[PATH_MAX];
    if (!data_dir(dir, sizeof dir) || !ensure_dir(dir) || !achievement_path(path, sizeof path)) return false;
    if (snprintf(tmp, sizeof tmp, "%s.tmp", path) >= (int)sizeof tmp) return false;
    FILE *fp = fopen(tmp, "w");
    if (!fp) return false;
    uint32_t valid = (UINT32_C(1) << ACH_COUNT) - 1U;
    bool bad = fprintf(fp, "INITWARS_ACHIEVEMENTS 1 %08x\n", unlocked & valid) < 0 || ferror(fp) != 0;
    if (fclose(fp) != 0 || bad) { remove(tmp); return false; }
    if (rename(tmp, path) != 0) { remove(tmp); return false; }
    return true;
}

uint32_t achievements_for_run(uint32_t unlocked, int mode, int character,
                              int kills, int wave, int victory, int hp, int max_hp)
{
    if (kills >= 1) unlocked |= UINT32_C(1) << ACH_FIRST_PROCESS;
    if (kills >= 50) unlocked |= UINT32_C(1) << ACH_JANITOR;
    if (mode == SCORE_ENDLESS && wave >= 10) unlocked |= UINT32_C(1) << ACH_LONG_HAUL;
    if (mode == SCORE_CAMPAIGN && victory) unlocked |= UINT32_C(1) << ACH_LAST_PID;
    if (mode == SCORE_BOSSRUSH && victory) unlocked |= UINT32_C(1) << ACH_BOSS_BREAKER;
    if (victory && hp == max_hp && max_hp > 0) unlocked |= UINT32_C(1) << ACH_PRISTINE_BOOT;
    if (mode == SCORE_CAMPAIGN && victory && character == CH_SYSVINIT) unlocked |= UINT32_C(1) << ACH_LEGACY_SUPPORT;
    if (victory && hp == 1) unlocked |= UINT32_C(1) << ACH_LAST_STAND;
    return unlocked;
}

int achievements_count(uint32_t unlocked)
{
    int n = 0;
    for (int i = 0; i < ACH_COUNT; i++) if (unlocked & (UINT32_C(1) << i)) n++;
    return n;
}
