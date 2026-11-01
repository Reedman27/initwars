/* profile.c - bounded, versioned, atomically-written lifetime statistics. */
#include "profile.h"

#include <errno.h>
#include <inttypes.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

static bool profile_dir(char *out, size_t n)
{
    const char *e = getenv("INITWARS_DATA_DIR");
    int r;
    if (e && *e) r = snprintf(out, n, "%s", e);
    else if ((e = getenv("XDG_DATA_HOME")) && *e) r = snprintf(out, n, "%s/init-wars", e);
    else if ((e = getenv("HOME")) && *e) r = snprintf(out, n, "%s/.local/share/init-wars", e);
    else return false;
    return r > 0 && (size_t)r < n;
}

static bool make_dirs(const char *path)
{
    char tmp[PATH_MAX];
    size_t len = strlen(path);
    if (len == 0 || len >= sizeof tmp) return false;
    memcpy(tmp, path, len + 1);
    for (char *p = tmp + 1; *p; p++) {
        if (*p != '/') continue;
        *p = '\0';
        if (mkdir(tmp, 0700) != 0 && errno != EEXIST) return false;
        *p = '/';
    }
    return mkdir(tmp, 0700) == 0 || errno == EEXIST;
}

static bool profile_path(char *out, size_t n)
{
    char dir[PATH_MAX];
    if (!profile_dir(dir, sizeof dir)) return false;
    int r = snprintf(out, n, "%s/profile.txt", dir);
    return r > 0 && (size_t)r < n;
}

static uint64_t add_sat(uint64_t a, uint64_t b)
{
    return UINT64_MAX - a < b ? UINT64_MAX : a + b;
}

void profile_record_run(ProfileStats *stats, int mode, int score, int wave,
                        int kills, int frames, bool victory)
{
    if (!stats || mode < 0 || mode >= SCORE_MODE_COUNT) return;
    ModeStats *m = &stats->mode[mode];
    m->runs = add_sat(m->runs, 1);
    if (victory) m->wins = add_sat(m->wins, 1);
    if (kills > 0) m->kills = add_sat(m->kills, (uint64_t)kills);
    if (frames > 0) m->frames = add_sat(m->frames, (uint64_t)frames);
    if (score > m->best_score) m->best_score = score;
    if (wave > m->best_wave) m->best_wave = wave;
}

static bool parse_u64(char **cursor, uint64_t *out)
{
    char *p = *cursor;
    while (*p == ' ' || *p == '\t') p++;
    if (*p < '0' || *p > '9') return false;
    uint64_t value = 0;
    do {
        unsigned int digit = (unsigned int)(*p - '0');
        if (value > (UINT64_MAX - digit) / 10U) return false;
        value = value * 10U + digit;
        p++;
    } while (*p >= '0' && *p <= '9');
    if (*p != ' ' && *p != '\t' && *p != '\r' && *p != '\n' && *p != '\0') return false;
    *cursor = p;
    *out = value;
    return true;
}

static bool parse_int_nonnegative(char **cursor, int *out)
{
    uint64_t value;
    if (!parse_u64(cursor, &value) || value > (uint64_t)INT_MAX) return false;
    *out = (int)value;
    return true;
}

static char *profile_next_line(char **cursor, char *limit)
{
    if (*cursor >= limit) return NULL;
    char *line = *cursor;
    char *end = memchr(line, '\n', (size_t)(limit - line));
    if (end) {
        *end = '\0';
        *cursor = end + 1;
    } else {
        *limit = '\0';
        *cursor = limit;
    }
    size_t len = strlen(line);
    if (len > 0 && line[len - 1] == '\r') line[len - 1] = '\0';
    return line;
}

void profile_load(ProfileStats *stats)
{
    if (!stats) return;
    memset(stats, 0, sizeof *stats);
    char path[PATH_MAX];
    if (!profile_path(path, sizeof path)) return;
    FILE *fp = fopen(path, "rb");
    if (!fp) return;

    /* The format is tiny and fixed-size. Read it once into bounded memory, then
     * parse only that snapshot; never attempt another stream read after EOF. */
    char data[4096];
    size_t size = fread(data, 1, sizeof data - 1, fp);
    bool valid = !ferror(fp) && size < sizeof data - 1;
    if (fclose(fp) != 0) valid = false;
    if (!valid || memchr(data, '\0', size) != NULL) return;
    data[size] = '\0';

    char *cursor = data;
    char *limit = data + size;
    char *line = profile_next_line(&cursor, limit);
    if (!line || strcmp(line, "INITWARS_PROFILE 1") != 0) return;

    ProfileStats loaded = {0};
    for (int i = 0; i < SCORE_MODE_COUNT; i++) {
        uint64_t mode, runs, wins, kills, frames;
        int best_score, best_wave;
        line = profile_next_line(&cursor, limit);
        if (!line) return;
        char *field = line;
        if (!parse_u64(&field, &mode) || mode != (uint64_t)i ||
            !parse_u64(&field, &runs) || !parse_u64(&field, &wins) ||
            !parse_u64(&field, &kills) || !parse_u64(&field, &frames) ||
            !parse_int_nonnegative(&field, &best_score) ||
            !parse_int_nonnegative(&field, &best_wave)) return;
        while (*field == ' ' || *field == '\t' || *field == '\r' || *field == '\n') field++;
        if (*field != '\0' || wins > runs) return;
        loaded.mode[i].runs = runs;
        loaded.mode[i].wins = wins;
        loaded.mode[i].kills = kills;
        loaded.mode[i].frames = frames;
        loaded.mode[i].best_score = best_score;
        loaded.mode[i].best_wave = best_wave;
    }
    while (cursor < limit && (*cursor == ' ' || *cursor == '\t' || *cursor == '\r' || *cursor == '\n')) cursor++;
    if (cursor != limit) return;
    *stats = loaded;
}

bool profile_save(const ProfileStats *stats)
{
    if (!stats) return false;
    char dir[PATH_MAX], path[PATH_MAX], tmp[PATH_MAX];
    if (!profile_dir(dir, sizeof dir) || !make_dirs(dir) || !profile_path(path, sizeof path)) return false;
    int n = snprintf(tmp, sizeof tmp, "%s.tmp", path);
    if (n <= 0 || (size_t)n >= sizeof tmp) return false;
    FILE *fp = fopen(tmp, "w");
    if (!fp) return false;
    bool bad = fprintf(fp, "INITWARS_PROFILE 1\n") < 0;
    for (int i = 0; i < SCORE_MODE_COUNT && !bad; i++) {
        const ModeStats *m = &stats->mode[i];
        if (m->wins > m->runs || m->best_score < 0 || m->best_wave < 0 ||
            fprintf(fp, "%d %" PRIu64 " %" PRIu64 " %" PRIu64 " %" PRIu64 " %d %d\n",
                    i, m->runs, m->wins, m->kills, m->frames,
                    m->best_score, m->best_wave) < 0) bad = true;
    }
    if (fclose(fp) != 0) bad = true;
    if (bad || rename(tmp, path) != 0) {
        remove(tmp);
        return false;
    }
    return true;
}
