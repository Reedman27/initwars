#include "score.h"
#include "audio.h"
#include "balance.h"

#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

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
    if (mkdir(tmp, 0700) != 0 && errno != EEXIST) return false;
    return true;
}

static bool data_dir(char *out, size_t n)
{
    const char *e = getenv("INITWARS_DATA_DIR");
    int r;
    if (e && *e) {
        r = snprintf(out, n, "%s", e);
    } else if ((e = getenv("XDG_DATA_HOME")) && *e) {
        r = snprintf(out, n, "%s/init-wars", e);
    } else if ((e = getenv("HOME")) && *e) {
        r = snprintf(out, n, "%s/.local/share/init-wars", e);
    } else {
        return false;
    }
    return r > 0 && (size_t)r < n;
}

bool scores_path(char *out, size_t outsz)
{
    char dir[PATH_MAX];
    if (!data_dir(dir, sizeof dir)) return false;
    int r = snprintf(out, outsz, "%s/scores.txt", dir);
    return r > 0 && (size_t)r < outsz;
}

bool scores_path_mode(ScoreMode mode, char *out, size_t outsz)
{
    static const char *const names[SCORE_MODE_COUNT] = { "campaign", "endless", "bossrush" };
    if (mode < 0 || mode >= SCORE_MODE_COUNT) return false;
    char dir[PATH_MAX];
    if (!data_dir(dir, sizeof dir)) return false;
    int r = snprintf(out, outsz, "%s/scores-%s.txt", dir, names[mode]);
    return r > 0 && (size_t)r < outsz;
}

static void load_record_line(ScoreTable *t, const char *line, int forced_mode, bool legacy)
{
    ScoreEntry e = {0};
    int mode = forced_mode;
    int fields;
    if (legacy) {
        int extra;
        fields = sscanf(line, "%d %d %d %d %d %d", &mode, &e.ch, &e.score, &e.wave, &e.win, &extra);
        if (fields == 5) e.mode = mode;
        else {
            fields = sscanf(line, "%d %d %d %d %d", &e.ch, &e.score, &e.wave, &e.win, &extra);
            if (fields != 4) return;
            e.mode = SCORE_CAMPAIGN;
        }
    } else {
        int extra;
        fields = sscanf(line, "%d %d %d %d %d", &e.ch, &e.score, &e.wave, &e.win, &extra);
        if (fields != 4) return;
        e.mode = forced_mode;
    }
    if (e.mode < 0 || e.mode >= SCORE_MODE_COUNT || e.ch < 0 || e.ch >= CH_COUNT ||
        e.score < 0 || e.score > 99999999 || e.wave < 0 || e.wave > 99 ||
        (e.win != 0 && e.win != 1)) return;
    (void)scores_add(t, e);
}

static void load_records(ScoreTable *t, const char *path, int forced_mode, bool legacy)
{
    FILE *fp = fopen(path, "r");
    if (!fp) return;
    char line[128];
    for (int lines_read = 0; lines_read < 1000 && t->n < MAX_SCORES; lines_read++) {
        int ch;
        int len = 0;
        bool any = false;
        bool over = false;
        do {
            ch = fgetc(fp);
            if (ch == EOF || ch == '\n') break;
            any = true;
            if (len < (int)sizeof line - 1) line[len++] = (char)ch;
            else over = true;
        } while (true);
        bool reached_eof = ch == EOF;
        if (!any && reached_eof) break;
        line[len] = '\0';
        /* NUL bytes and overlong records are rejected without consuming the next line. */
        bool nul = false;
        for (int i = 0; i < len; i++) if (line[i] == '\0') nul = true;
        if (!over && !nul) load_record_line(t, line, forced_mode, legacy);
        /* A final line without a newline is processed once, then the stream is closed. */
        if (reached_eof) break;
    }
    fclose(fp);
}

void scores_load(ScoreTable *t)
{
    if (!t) return;
    t->n = 0;
    bool have_mode_file[SCORE_MODE_COUNT] = { false };
    char path[PATH_MAX];
    for (int mode = 0; mode < SCORE_MODE_COUNT; mode++) {
        if (!scores_path_mode((ScoreMode)mode, path, sizeof path)) continue;
        struct stat st;
        if (stat(path, &st) == 0 && S_ISREG(st.st_mode)) {
            have_mode_file[mode] = true;
            load_records(t, path, mode, false);
        }
    }

    /* Import the pre-1.4 combined file only for modes without a dedicated file. */
    if (!scores_path(path, sizeof path)) return;
    ScoreTable legacy = {0};
    load_records(&legacy, path, SCORE_CAMPAIGN, true);
    for (int i = 0; i < legacy.n; i++) {
        ScoreEntry e = legacy.e[i];
        if (e.mode >= 0 && e.mode < SCORE_MODE_COUNT && !have_mode_file[e.mode])
            (void)scores_add(t, e);
    }
}

static bool save_mode_file(const ScoreTable *t, ScoreMode mode)
{
    char dir[PATH_MAX], path[PATH_MAX], tmp[PATH_MAX];
    if (!data_dir(dir, sizeof dir) || !ensure_dir(dir) ||
        !scores_path_mode(mode, path, sizeof path)) return false;
    int n = snprintf(tmp, sizeof tmp, "%s.tmp", path);
    if (n <= 0 || (size_t)n >= sizeof tmp) return false;
    FILE *fp = fopen(tmp, "w");
    if (!fp) return false;
    bool bad = false;
    for (int i = 0; i < t->n && !bad; i++) {
        const ScoreEntry *e = &t->e[i];
        if ((ScoreMode)e->mode == mode)
            bad = fprintf(fp, "%d %d %d %d\n", e->ch, e->score, e->wave, e->win) < 0;
    }
    if (fclose(fp) != 0) bad = true;
    if (bad || rename(tmp, path) != 0) {
        remove(tmp);
        return false;
    }
    return true;
}

bool scores_save(const ScoreTable *t)
{
    if (!t || t->n < 0 || t->n > MAX_SCORES) return false;
    bool ok = true;
    for (int mode = 0; mode < SCORE_MODE_COUNT; mode++)
        if (!save_mode_file(t, (ScoreMode)mode)) ok = false;
    return ok;
}

int scores_add(ScoreTable *t, ScoreEntry e)
{
    if (e.mode < 0 || e.mode >= SCORE_MODE_COUNT) return -1;
    int mode_count = 0, lowest = -1, rank = 0;
    for (int i = 0; i < t->n; i++) {
        if (t->e[i].mode != e.mode) continue;
        mode_count++;
        if (t->e[i].score >= e.score) rank++;
        if (lowest < 0 || t->e[i].score < t->e[lowest].score) lowest = i;
    }
    if (mode_count >= MAX_SCORES_PER_MODE) {
        if (lowest < 0 || e.score <= t->e[lowest].score) return -1;
        for (int i = lowest; i + 1 < t->n; i++) t->e[i] = t->e[i + 1];
        t->n--;
        if (rank >= MAX_SCORES_PER_MODE) rank = MAX_SCORES_PER_MODE - 1;
    }
    if (t->n >= MAX_SCORES) return -1;
    int pos = t->n;
    for (int i = 0; i < t->n; i++) {
        if (e.score > t->e[i].score) { pos = i; break; }
    }
    t->n++;
    for (int i = t->n - 1; i > pos; i--) t->e[i] = t->e[i - 1];
    t->e[pos] = e;
    return rank;
}

static bool settings_path(char *out, size_t outsz)
{
    char dir[PATH_MAX];
    if (!data_dir(dir, sizeof dir)) return false;
    int r = snprintf(out, outsz, "%s/settings.txt", dir);
    return r > 0 && (size_t)r < outsz;
}

void settings_load(Settings *out)
{
    char path[PATH_MAX], line[128];
    out->volume = out->crt = out->music = out->shake = -1;
    if (!settings_path(path, sizeof path)) return;
    FILE *fp = fopen(path, "r");
    if (!fp) return;
    for (int n = 0; n < 50 && fgets(line, sizeof line, fp); n++) {
        int v;
        if (sscanf(line, "volume %d", &v) == 1) { if (v >= 0 && v <= VOL_MAX) out->volume = v; }
        else if (sscanf(line, "crt %d", &v) == 1)   { if (v == 0 || v == 1) out->crt = v; }
        else if (sscanf(line, "music %d", &v) == 1) { if (v == 0 || v == 1) out->music = v; }
        else if (sscanf(line, "shake %d", &v) == 1) { if (v == 0 || v == 1) out->shake = v; }
    }
    fclose(fp);
}

bool settings_save(const Settings *s)
{
    char dir[PATH_MAX], path[PATH_MAX], tmp[PATH_MAX];
    if (!data_dir(dir, sizeof dir) || !ensure_dir(dir)) return false;
    if (!settings_path(path, sizeof path)) return false;
    if (snprintf(tmp, sizeof tmp, "%s.tmp", path) >= (int)sizeof tmp) return false;
    FILE *fp = fopen(tmp, "w");
    if (!fp) return false;
    bool werr = false;
    if (s->volume >= 0 && s->volume <= VOL_MAX) werr |= fprintf(fp, "volume %d\n", s->volume) < 0;
    if (s->crt   == 0 || s->crt   == 1) werr |= fprintf(fp, "crt %d\n",   s->crt)   < 0;
    if (s->music == 0 || s->music == 1) werr |= fprintf(fp, "music %d\n", s->music) < 0;
    if (s->shake == 0 || s->shake == 1) werr |= fprintf(fp, "shake %d\n", s->shake) < 0;
    werr |= ferror(fp) != 0;
    if (fclose(fp) != 0 || werr) { remove(tmp); return false; }
    return rename(tmp, path) == 0;
}

int settings_load_volume(void)
{
    Settings s;
    settings_load(&s);
    return s.volume;
}

bool settings_save_volume(int step)
{
    if (step < 0 || step > VOL_MAX) return false;
    Settings s;
    settings_load(&s);
    s.volume = step;
    return settings_save(&s);
}
