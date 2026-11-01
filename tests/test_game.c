/* Headless regression tests: run the real game logic and renderer without SDL. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>

#include "codex.h"
#include "game.h"
#include "render.h"
#include "view.h"

static int failures, checks;
#define CHECK(cond, ...) do { checks++; if (!(cond)) { failures++; printf("  FAIL %s:%d: ", __FILE__, __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)

static Frame frame;
static Game G;

static void tick(Game *g, Input *in) { game_update(g, in); memset(in->pressed, 0, sizeof in->pressed); }
static void ticks(Game *g, int n) { Input in; memset(&in, 0, sizeof in); for (int i = 0; i < n; i++) tick(g, &in); }

/* one update with exactly one key edge */
static void press(Game *g, Key k) { Input in; memset(&in, 0, sizeof in); in.pressed[k] = true; game_update(g, &in); }

static void fresh(uint32_t seed) { game_init(&G, seed); G.no_save = true; G.scores.n = 0; }

/* Number of lines view.c's greedy word wrap gives a text at the given column width. */
static int wrap_lines(const char *text, int maxc)
{
    int n = 0;
    const char *p = text;
    while (*p) {
        while (*p == ' ') p++;
        int len = (int)strlen(p);
        if (len <= maxc) { n++; break; }
        int cut = maxc;
        while (cut > 0 && p[cut] != ' ') cut--;
        if (cut == 0) cut = maxc;
        n++;
        p += cut;
    }
    return n;
}

/* ------------------------------------------------------------------ data */
static void test_data(void)
{
    printf("data tables\n");
    for (int i = 0; i < ALL_SPRITES_COUNT; i++) {
        const Sprite *s = ALL_SPRITES[i];
        for (int r = 0; r < s->h; r++) {
            CHECK((int)strlen(s->rows[r]) == s->w, "sprite %d row %d has width %d, want %d", i, r, (int)strlen(s->rows[r]), s->w);
            for (const char *c = s->rows[r]; *c; c++) CHECK(strchr(".kdgGaAxXrRwc12", *c) != NULL, "sprite %d bad char '%c'", i, *c);
        }
    }
    for (int w = 0; w < NUM_WAVES; w++) {
        int total = 0;
        for (int i = 0; i < WAVES[w].n; i++) total += WAVES[w].s[i].count;
        CHECK(total > 0 && total <= MAX_QUEUE, "wave %d spawns %d", w + 1, total);
    }
    bool used[E_COUNT] = {0};
    for (int w = 0; w < NUM_WAVES; w++) for (int i = 0; i < WAVES[w].n; i++) used[WAVES[w].s[i].type] = true;
    for (int t = E_JOURNALD; t <= E_DEPTARGET; t++) CHECK(used[t], "enemy %s never appears in a campaign wave", ENEMIES[t].name);
    CHECK(used[E_BOSS], "boss wave missing");
    CHECK(ENEMIES[E_CRON].counts && ENEMIES[E_OOMD].counts, "new enemy types count toward wave completion");
    CHECK(used[E_CRON] && used[E_OOMD], "the longer campaign brings in cron and the OOM killer");
    CHECK(NUM_WAVES == 24 && BOSS_WAVE == NUM_WAVES && WAVES[BOSS_WAVE - 1].s[0].type == E_BOSS && WAVES[NUM_WAVES - 2].n > 0,
          "the campaign has 24 waves and the boss is the last one");
    for (int w = 0; w < NUM_WAVES; w++) {
        CHECK(WAVE_NAMES[w] && WAVE_NAMES[w][0] && strlen(WAVE_NAMES[w]) <= 30, "wave %d has a name that fits the banner", w + 1);
        CHECK(WAVES[w].n > 0 && WAVES[w].n <= MAX_WAVE_SPAWNS, "wave %d lists 1..%d spawn groups (%d)", w + 1, MAX_WAVE_SPAWNS, WAVES[w].n);
        for (int i = 0; i < WAVES[w].n; i++) CHECK(WAVES[w].s[i].count > 0 && WAVES[w].s[i].type > E_NONE && WAVES[w].s[i].type < E_COUNT,
                                                   "wave %d group %d is valid", w + 1, i);
    }
    {   /* difficulty never collapses: the average of each act's waves rises, and the last regular wave is among the biggest */
        int act_total[4] = {0}, act_n[4] = {0};
        for (int w = 0; w < NUM_WAVES - 1; w++) {
            int t = 0; for (int i = 0; i < WAVES[w].n; i++) t += WAVES[w].s[i].count * (ENEMIES[WAVES[w].s[i].type].hp > 15 ? 2 : 1);
            int a = w < 5 ? 0 : (w < 12 ? 1 : (w < 18 ? 2 : 3));
            act_total[a] += t; act_n[a]++;
        }
        for (int a = 1; a < 4; a++)
            CHECK(act_total[a] * act_n[a - 1] > act_total[a - 1] * act_n[a], "act %d is harder on average than act %d", a + 1, a);
    }
    for (int s = 0; s < SC_COUNT; s++) {
        const Scene *scene = story_scene((SceneId)s);
        CHECK(scene->n > 0, "scene %d empty", s);
        for (int i = 0; i < scene->n; i++) {
            CHECK(strlen(scene->lines[i].text) <= 120,
                  "scene %d line %d fits the story box (%zu chars)", s, i, strlen(scene->lines[i].text));
            CHECK(wrap_lines(scene->lines[i].text, 41) <= 3,
                  "scene %d line %d wraps to at most 3 lines in the story box (%d)", s, i, wrap_lines(scene->lines[i].text, 41));
        }
    }
    CHECK(story_scene_before_wave(1) == SC_ACT1 && story_scene_before_wave(2) == SC_TRACE &&
          story_scene_before_wave(3) == SC_BRIDGE && story_scene_before_wave(4) == SC_QUIET &&
          story_scene_before_wave(5) == SC_UPSTART && story_scene_before_wave(6) == SC_ACT2 &&
          story_scene_before_wave(7) == SC_VOTE,
          "Act I and early Act II interludes are attached to waves 1-7");
    CHECK(story_scene_before_wave(10) == SC_HOLDOUTS && story_scene_before_wave(11) == SC_DINIT &&
          story_scene_before_wave(13) == SC_ACT3 && story_scene_before_wave(14) == SC_SCOPE &&
          story_scene_before_wave(16) == SC_LOOP && story_scene_before_wave(18) == SC_PLAN &&
          story_scene_before_wave(19) == SC_DESCENT && story_scene_before_wave(21) == SC_LAST_RUNLEVEL &&
          story_scene_before_wave(23) == SC_EVE && story_scene_before_wave(24) == SC_BOSS,
          "Dinit's introduction and the Act III/IV interludes are attached");
    CHECK(story_scene_midwave(9) == SC_REDHAT, "the migration-notice scene happens in the middle of wave nine");
    {   /* every scene is reachable from a wave (except the epilogue and the mid-fight scene) */
        bool seen[SC_COUNT] = {0};
        for (int w = 1; w <= NUM_WAVES; w++) { int sc = story_scene_before_wave(w); if (sc >= 0) seen[sc] = true; }
        seen[SC_REDHAT] = seen[SC_EPILOGUE] = true;
        for (int sc = 0; sc < SC_COUNT; sc++) CHECK(seen[sc], "scene %d is attached to some wave", sc);
    }
    {   /* the migration timeline names Red Hat, SUSE and Ubuntu, and the Debian vote */
        const Scene *rh = story_scene(SC_REDHAT);
        bool red = false, suse = false, ubuntu = false;
        for (int i = 0; i < rh->n; i++) {
            red |= strstr(rh->lines[i].text, "RHEL") != NULL;
            suse |= strstr(rh->lines[i].text, "SLE") != NULL || strstr(rh->lines[i].text, "SUSE") != NULL;
            ubuntu |= strstr(rh->lines[i].text, "UBUNTU") != NULL;
        }
        CHECK(red && suse && ubuntu, "the migration scene covers Red Hat, SUSE and Ubuntu");
        CHECK(strstr(story_scene(SC_VOTE)->lines[0].text, "DEBIAN") != NULL, "the vote scene is about Debian");
    }
}

/* ---------------------------------------------------------------- scores */
static void test_achievements(void)
{
    printf("achievements\n");
    char dir[] = "/tmp/initwars-achievements-XXXXXX";
    CHECK(mkdtemp(dir) != NULL, "create achievement data directory");
    setenv("INITWARS_DATA_DIR", dir, 1);
    uint32_t a = 0;
    a = achievements_for_run(a, SCORE_CAMPAIGN, CH_RUNIT, 1, 10, 0, 2, 3);
    CHECK(a & (UINT32_C(1) << ACH_FIRST_PROCESS), "first kill unlocks First Process");
    CHECK(!(a & (UINT32_C(1) << ACH_LAST_PID)), "failed campaign does not unlock campaign clear");
    a = achievements_for_run(a, SCORE_ENDLESS, CH_S6, 50, 10, 0, 1, 3);
    CHECK(a & (UINT32_C(1) << ACH_JANITOR), "50 kills unlock Janitor");
    CHECK(a & (UINT32_C(1) << ACH_LONG_HAUL), "endless wave ten unlocks Long Haul");
    a = achievements_for_run(a, SCORE_CAMPAIGN, CH_SYSVINIT, 20, 10, 1, 3, 3);
    CHECK(a & (UINT32_C(1) << ACH_LAST_PID), "campaign win unlocks Last PID");
    CHECK(a & (UINT32_C(1) << ACH_PRISTINE_BOOT), "full-health win unlocks Pristine Boot");
    CHECK(a & (UINT32_C(1) << ACH_LEGACY_SUPPORT), "SysVinit campaign win unlocks Legacy Support");
    a = achievements_for_run(a, SCORE_BOSSRUSH, CH_OPENRC, 10, 5, 1, 1, 3);
    CHECK(a & (UINT32_C(1) << ACH_BOSS_BREAKER), "Boss Rush win unlocks Boss Breaker");
    CHECK(a & (UINT32_C(1) << ACH_LAST_STAND), "one-HP victory unlocks Last Stand");
    CHECK(achievements_count(a) == 8, "all achievements count correctly (%d)", achievements_count(a));
    CHECK(achievement_info(ACH_LAST_STAND) != NULL && achievement_info(ACH_COUNT) == NULL, "achievement metadata bounds");
    CHECK(achievements_save(a), "achievement save succeeds");
    CHECK(achievements_load() == a, "achievement unlocks persist exactly");
    fresh(42);
    G.state = ST_MENU; G.menu_sel = MENU_ACHIEVEMENTS;
    press(&G, K_CONFIRM);
    CHECK(G.state == ST_ACHIEVEMENTS && G.achievements == a, "main menu opens persisted achievements screen");
    view_draw(&G, &frame);
    press(&G, K_ESC);
    CHECK(G.state == ST_MENU, "escape returns from achievements screen");
    char path[1024];
    snprintf(path, sizeof path, "%s/achievements.txt", getenv("INITWARS_DATA_DIR"));
    FILE *fp = fopen(path, "w");
    CHECK(fp != NULL, "open achievement corruption fixture");
    if (fp) { fputs("not an achievement file\n", fp); fclose(fp); }
    CHECK(achievements_load() == 0, "invalid achievement file fails closed");
}


static void test_profile(void)
{
    printf("lifetime profile statistics\n");
    char dir[] = "/tmp/initwars-profile-XXXXXX";
    CHECK(mkdtemp(dir) != NULL, "create profile data directory");
    setenv("INITWARS_DATA_DIR", dir, 1);
    ProfileStats stats = {0};
    profile_record_run(&stats, SCORE_CAMPAIGN, 1200, 4, 18, 3600, false);
    profile_record_run(&stats, SCORE_CAMPAIGN, 9000, 10, 64, 7200, true);
    profile_record_run(&stats, SCORE_ENDLESS, 3000, 12, 30, 9000, false);
    profile_record_run(&stats, SCORE_BOSSRUSH, 5000, 5, 25, 6000, true);
    CHECK(stats.mode[SCORE_CAMPAIGN].runs == 2 && stats.mode[SCORE_CAMPAIGN].wins == 1,
          "campaign totals are accumulated independently");
    CHECK(stats.mode[SCORE_CAMPAIGN].kills == 82 && stats.mode[SCORE_CAMPAIGN].frames == 10800,
          "campaign kills and uptime accumulate");
    CHECK(stats.mode[SCORE_CAMPAIGN].best_score == 9000 && stats.mode[SCORE_CAMPAIGN].best_wave == 10,
          "campaign best score and wave track maxima");
    CHECK(stats.mode[SCORE_ENDLESS].runs == 1 && stats.mode[SCORE_BOSSRUSH].wins == 1,
          "endless and boss rush keep separate lifetime records");
    CHECK(profile_save(&stats), "profile writes successfully");
    ProfileStats loaded;
    profile_load(&loaded);
    CHECK(loaded.mode[SCORE_CAMPAIGN].runs == 2 && loaded.mode[SCORE_CAMPAIGN].wins == 1,
          "profile run and win counts persist");
    CHECK(loaded.mode[SCORE_CAMPAIGN].kills == 82 && loaded.mode[SCORE_CAMPAIGN].frames == 10800,
          "profile totals persist exactly");
    CHECK(loaded.mode[SCORE_ENDLESS].best_wave == 12 && loaded.mode[SCORE_BOSSRUSH].best_score == 5000,
          "all mode records persist");
    char path[1024];
    snprintf(path, sizeof path, "%s/profile.txt", dir);
    FILE *fp = fopen(path, "wb");
    CHECK(fp != NULL, "open profile without-final-newline fixture");
    if (fp) {
        fputs("INITWARS_PROFILE 1\n0 5 2 10 1000 900 10\n1 3 0 20 2000 1000 20\n2 1 0 30 3000 1200 2", fp);
        fclose(fp);
    }
    profile_load(&loaded);
    CHECK(loaded.mode[SCORE_CAMPAIGN].runs == 5 && loaded.mode[SCORE_BOSSRUSH].best_wave == 2,
          "profile parser accepts a valid final record without a newline");
    fp = fopen(path, "wb");
    CHECK(fp != NULL, "open profile embedded-NUL fixture");
    if (fp) {
        static const char bad_profile[] = "INITWARS_PROFILE 1\n0 1 0 0 0 0 0\n\0x";
        fwrite(bad_profile, 1, sizeof bad_profile - 1, fp);
        fclose(fp);
    }
    profile_load(&loaded);
    CHECK(loaded.mode[SCORE_CAMPAIGN].runs == 0, "embedded-NUL profile data is rejected");
    fp = fopen(path, "wb");
    CHECK(fp != NULL, "open oversized profile fixture");
    if (fp) {
        char oversized[4096];
        memset(oversized, ' ', sizeof oversized);
        fwrite(oversized, 1, sizeof oversized, fp);
        fclose(fp);
    }
    profile_load(&loaded);
    CHECK(loaded.mode[SCORE_CAMPAIGN].runs == 0, "oversized profile file is rejected safely");
    CHECK(profile_save(&stats), "restore valid profile after hostile fixtures");
    fresh(91);
    CHECK(G.profile.mode[SCORE_CAMPAIGN].runs == 2, "game initialization loads lifetime profile");
    G.state = ST_MENU; G.menu_sel = MENU_STATS;
    press(&G, K_CONFIRM);
    CHECK(G.state == ST_STATS && G.stats_mode == SCORE_CAMPAIGN, "main menu opens run statistics");
    view_draw(&G, &frame);
    press(&G, K_RIGHT);
    CHECK(G.state == ST_STATS && G.stats_mode == SCORE_ENDLESS, "statistics screen switches mode without closing");
    press(&G, K_ESC);
    CHECK(G.state == ST_MENU, "escape closes statistics immediately");
    fp = fopen(path, "w");
    CHECK(fp != NULL, "open invalid profile fixture");
    if (fp) { fputs("broken profile\n", fp); fclose(fp); }
    profile_load(&loaded);
    CHECK(loaded.mode[SCORE_CAMPAIGN].runs == 0, "invalid profile fails closed to empty stats");
    fp = fopen(path, "w");
    CHECK(fp != NULL, "open overflowing profile fixture");
    if (fp) { fputs("INITWARS_PROFILE 1\n0 999999999999999999999999999999999999 0 0 0 0 0\n1 0 0 0 0 0 0\n2 0 0 0 0 0 0\n", fp); fclose(fp); }
    profile_load(&loaded);
    CHECK(loaded.mode[SCORE_CAMPAIGN].runs == 0, "overflowing counters are rejected safely");
    fp = fopen(path, "w");
    CHECK(fp != NULL, "open trailing-data profile fixture");
    if (fp) { fputs("INITWARS_PROFILE 1\n0 0 0 0 0 0 0\n1 0 0 0 0 0 0\n2 0 0 0 0 0 0\nextra\n", fp); fclose(fp); }
    profile_load(&loaded);
    CHECK(loaded.mode[SCORE_CAMPAIGN].runs == 0, "trailing profile data is rejected");
    ProfileStats saturated = {0};
    saturated.mode[SCORE_CAMPAIGN].runs = UINT64_MAX;
    profile_record_run(&saturated, SCORE_CAMPAIGN, 1, 1, 1, 1, true);
    CHECK(saturated.mode[SCORE_CAMPAIGN].runs == UINT64_MAX,
          "lifetime counters saturate instead of wrapping");
    profile_record_run(&saturated, -1, 1, 1, 1, 1, true);
    CHECK(saturated.mode[SCORE_CAMPAIGN].wins == 1, "invalid mode cannot corrupt profile records");
    remove(path);
    fresh(92);
    game_test_start(&G, CH_RUNIT, 1);
    game_test_finish(&G, false);
    CHECK(G.profile.mode[SCORE_CAMPAIGN].runs == 1, "completed run updates in-memory profile once");
    CHECK(access(path, F_OK) != 0, "no-save prevents profile writes on run completion");
    fresh(93);
    G.no_save = false;
    game_test_start(&G, CH_RUNIT, 1);
    game_test_finish(&G, false);
    profile_load(&loaded);
    CHECK(loaded.mode[SCORE_CAMPAIGN].runs == 1 && loaded.mode[SCORE_CAMPAIGN].wins == 0,
          "run completion persists a failed campaign record in lifetime stats");
}

static void test_scores(void)
{
    printf("high scores\n");
    char dir[] = "/tmp/initwars-test-XXXXXX";
    CHECK(mkdtemp(dir) != NULL, "mkdtemp");
    char sub[512];
    snprintf(sub, sizeof sub, "%s/nested/dir", dir);
    setenv("INITWARS_DATA_DIR", sub, 1);
    ScoreTable t = {0};
    ScoreEntry a = { CH_RUNIT, 500, 3, 0, SCORE_CAMPAIGN }, b = { CH_S6, 900, 10, 1, SCORE_CAMPAIGN }, c = { CH_OPENRC, 100, 1, 0, SCORE_CAMPAIGN };
    scores_add(&t, a); scores_add(&t, b);
    CHECK(scores_add(&t, c) == 2, "rank order");
    CHECK(t.e[0].score == 900, "sorted");
    CHECK(scores_save(&t), "save (creates nested dirs)");
    ScoreTable u; scores_load(&u);
    CHECK(u.n == 3 && u.e[0].score == 900 && u.e[0].win == 1, "round trip n=%d", u.n);
    ScoreTable modes = {0};
    for (int mode = 0; mode < SCORE_MODE_COUNT; mode++)
        for (int i = 0; i < MAX_SCORES_PER_MODE; i++) {
            ScoreEntry e = { CH_RUNIT, 1000 + mode * 100 + i, i + 1, i == 7, mode };
            CHECK(scores_add(&modes, e) >= 0, "mode %d accepts record %d", mode, i);
        }
    CHECK(modes.n == MAX_SCORES, "all three modes retain independent full tables");
    ScoreEntry low = { CH_S6, 1, 1, 0, SCORE_ENDLESS };
    CHECK(scores_add(&modes, low) == -1 && modes.n == MAX_SCORES, "low score cannot displace another mode or a better record");
    ScoreEntry high = { CH_S6, 99999, 12, 1, SCORE_ENDLESS };
    CHECK(scores_add(&modes, high) == 0 && modes.n == MAX_SCORES, "new mode record replaces only that mode's lowest entry");
    CHECK(scores_save(&modes), "save mode-separated records");
    for (int mode = 0; mode < SCORE_MODE_COUNT; mode++) {
        char mode_path[1200];
        CHECK(scores_path_mode((ScoreMode)mode, mode_path, sizeof mode_path), "resolve mode %d score path", mode);
        CHECK(access(mode_path, F_OK) == 0, "mode %d gets its own score file", mode);
    }
    scores_load(&u);
    int per_mode[SCORE_MODE_COUNT] = {0};
    for (int i = 0; i < u.n; i++) if (u.e[i].mode >= 0 && u.e[i].mode < SCORE_MODE_COUNT) per_mode[u.e[i].mode]++;
    CHECK(u.n == MAX_SCORES && per_mode[0] == 8 && per_mode[1] == 8 && per_mode[2] == 8,
          "mode records survive save/load independently (%d total)", u.n);
    for (int i = 0; i < 20; i++) { ScoreEntry e = { 0, 1000 + i, 1, 0, SCORE_CAMPAIGN }; scores_add(&t, e); }
    CHECK(t.n == MAX_SCORES_PER_MODE, "campaign table capped independently");
    char p[1100]; snprintf(p, sizeof p, "%s/scores.txt", sub);
    const char *mode_files[] = { "scores-campaign.txt", "scores-endless.txt", "scores-bossrush.txt" };
    for (size_t i = 0; i < sizeof mode_files / sizeof mode_files[0]; i++) {
        char mode_path[1200]; snprintf(mode_path, sizeof mode_path, "%s/%s", sub, mode_files[i]);
        unlink(mode_path);
    }
    FILE *fp = fopen(p, "w"); fputs("garbage\n99 5 5 5\n1 2 3 0\n", fp); fclose(fp);
    scores_load(&u);
    CHECK(u.n == 1, "corrupt lines skipped, n=%d", u.n);

    /* hostile / broken environments must never crash the game or corrupt the table */
    printf("high scores: hostile files\n");
    char bad[1024];
    /* 1. the Campaign score file is a directory */
    snprintf(bad, sizeof bad, "%s/asdir", dir); mkdir(bad, 0700);
    snprintf(p, sizeof p, "%s/scores-campaign.txt", bad); mkdir(p, 0700);
    setenv("INITWARS_DATA_DIR", bad, 1);
    scores_load(&u); CHECK(u.n == 0, "directory instead of file loads as empty");
    CHECK(!scores_save(&t), "saving over a directory fails cleanly");
    /* 2. data dir path runs through a regular file */
    snprintf(bad, sizeof bad, "%s/afile", dir); fp = fopen(bad, "w"); fputs("x", fp); fclose(fp);
    char through[2100]; snprintf(through, sizeof through, "%s/sub/dir", bad);
    setenv("INITWARS_DATA_DIR", through, 1);
    scores_load(&u); CHECK(u.n == 0, "path through a file loads as empty");
    CHECK(!scores_save(&t), "path through a file fails cleanly");
    /* 3. nonexistent, unset */
    setenv("INITWARS_DATA_DIR", "/nonexistent-initwars-dir/x", 1);
    scores_load(&u); CHECK(u.n == 0, "missing dir loads as empty");
    unsetenv("INITWARS_DATA_DIR"); char *home = getenv("HOME") ? strdup(getenv("HOME")) : NULL;
    unsetenv("HOME"); unsetenv("XDG_DATA_HOME");
    scores_load(&u); CHECK(u.n == 0 && !scores_save(&t), "no HOME/XDG: no crash, save reports failure");
    if (home) { setenv("HOME", home, 1); free(home); }
    /* 4. binary garbage, NULs, enormous lines, huge file */
    snprintf(bad, sizeof bad, "%s/garbage", dir); mkdir(bad, 0700);
    setenv("INITWARS_DATA_DIR", bad, 1);
    snprintf(p, sizeof p, "%s/scores.txt", bad);
    fp = fopen(p, "wb");
    srand(5);
    for (int i = 0; i < 200000; i++) fputc(rand() & 0xFF, fp);
    fputs("\n2 7 3 0\n", fp);
    for (int i = 0; i < 5000; i++) fputc('9', fp);
    fputs("\n1 8 2 1\n3 -5 2 0\n4 99999999999 2 0\n0 10 100 0\n0 10 5 7\n", fp);
    fclose(fp);
    scores_load(&u);
    CHECK(u.n <= MAX_SCORES, "garbage file never overflows the table (n=%d)", u.n);
    for (int i = 0; i < u.n; i++) CHECK(u.e[i].ch >= 0 && u.e[i].ch < CH_COUNT && u.e[i].score >= 0 && u.e[i].wave <= 99, "entry %d sane", i);
    /* 4b. line-format edge cases: no trailing newline, NUL bytes, over-long line between good lines */
    snprintf(bad, sizeof bad, "%s/edges", dir); mkdir(bad, 0700);
    setenv("INITWARS_DATA_DIR", bad, 1);
    snprintf(p, sizeof p, "%s/scores.txt", bad);
    fp = fopen(p, "wb"); fputs("1 100 3 0\n2 200 4 1", fp); fclose(fp);
    scores_load(&u); CHECK(u.n == 2 && u.e[0].score == 200, "final line without newline is kept (n=%d)", u.n);
    fp = fopen(p, "wb"); fputs("1 100 3 0\n", fp); fwrite("2 2\0 junk\n", 1, 10, fp); fputs("3 300 5 0\n", fp); fclose(fp);
    scores_load(&u); CHECK(u.n == 2 && u.e[0].score == 300, "a line with a NUL byte does not swallow the next line (n=%d)", u.n);
    fp = fopen(p, "wb"); fputs("1 100 3 0\n", fp); for (int i = 0; i < 400; i++) fputc('7', fp); fputs("\n4 400 6 1\n", fp); fclose(fp);
    scores_load(&u); CHECK(u.n == 2 && u.e[0].score == 400, "over-long line is skipped alone (n=%d)", u.n);
    fp = fopen(p, "wb"); fclose(fp);
    scores_load(&u); CHECK(u.n == 0, "empty file loads as empty");
    fp = fopen(p, "wb"); for (int i = 0; i < 100; i++) fputs("1 5 1 0\n", fp); fclose(fp);
    scores_load(&u); CHECK(u.n == MAX_SCORES_PER_MODE, "legacy file stops at the per-mode table size (n=%d)", u.n);

    /* 5. the game itself keeps running and flags the failed save */
    setenv("INITWARS_DATA_DIR", through, 1);
    game_init(&G, 1); G.scores.n = 0; G.no_save = false;
    game_test_start(&G, CH_RUNIT, 1);
    G.score = 777; G.p.hp = 1; G.god = false;
    game_test_spawn(&G, E_USB, G.p.x, G.p.y, false);
    G.p.inv = 0; ticks(&G, 3);
    CHECK(G.state == ST_PANIC && G.save_failed, "game over with unwritable score file: no crash, save_failed set");
    unsetenv("INITWARS_DATA_DIR");
}

/* ----------------------------------------------------------- rendering */
static int lit(const Frame *f) { int n = 0; for (int i = 0; i < LW * LH; i++) if (f->px[i] != color_argb(C_BLACK)) n++; return n; }

static void test_render(const char *shots)
{
    printf("rendering every screen\n");
    fresh(7);
    G.crt = true;
    GameState order[] = { ST_BOOT, ST_MENU, ST_SELECT, ST_PLAY, ST_PAUSE, ST_UPGRADE, ST_STORY, ST_PANIC,
                          ST_RESULTS, ST_ENDING, ST_CREDITS, ST_SCORES, ST_ACHIEVEMENTS, ST_STATS, ST_SETTINGS, ST_CODEX };
    const char *names[] = { "boot", "menu", "select", "play", "pause", "upgrade", "story", "panic",
                            "results", "ending", "credits", "scores", "achievements", "stats", "settings", "codex" };
    game_test_start(&G, CH_SYSVINIT, 6);
    for (int i = 0; i < 4; i++) game_test_spawn(&G, E_JOURNALD + i, 60.0f + 50.0f * (float)i, 100, i == 1);
    ticks(&G, 140);
    for (size_t i = 0; i < sizeof order / sizeof *order; i++) {
        G.state = order[i]; G.state_t = 200; G.scene = SC_REDHAT; G.scene_line = 2; G.scene_t = 200;
        G.upg_opt[0] = 0; G.upg_opt[1] = 1; G.upg_opt[2] = 2;
        view_draw(&G, &frame);
        CHECK(lit(&frame) > 300, "%s screen nearly blank", names[i]);
        if (shots) { char p[512]; snprintf(p, sizeof p, "%s/%s.ppm", shots, names[i]); fr_save_ppm(&frame, p); }
    }
}

static void test_render_new(const char *shots)
{
    printf("rendering: in-fight scene, volume UI\n");
    fresh(7);
    game_test_start(&G, CH_SYSVINIT, 5);
    for (int i = 0; i < 4; i++) game_test_spawn(&G, E_JOURNALD + i, 60.0f + 50.0f * (float)i, 100, false);
    ticks(&G, 140);
    G.state = ST_STORY; G.scene = SC_REDHAT; G.scene_inplay = true; G.state_t = 100;
    const StoryLine *ln;
    for (int line = 0; line < story_scene(SC_REDHAT)->n; line++) {
        G.scene_line = line; G.scene_t = 400;
        ln = &story_scene(SC_REDHAT)->lines[line]; (void)ln;
        view_draw(&G, &frame);
        CHECK(lit(&frame) > 2000, "in-fight scene line %d draws something", line);
        if (shots) { char p[512]; snprintf(p, sizeof p, "%s/midscene%d.ppm", shots, line); fr_save_ppm(&frame, p); }
    }
    G.scene_inplay = false; G.state = ST_STORY; G.scene = SC_DINIT; G.scene_line = 1; G.scene_t = 1000;
    view_draw(&G, &frame);
    int cyan_graph = 0;
    for (int y = 82; y < 132; y++) for (int x = 110; x < 210; x++)
        cyan_graph += frame.px[y * LW + x] == color_argb(C_CYAN);
    CHECK(cyan_graph > 30, "Dinit introduction renders its dependency-graph mark (%d pixels)", cyan_graph);
    if (shots) { char p[512]; snprintf(p, sizeof p, "%s/dinit-intro.ppm", shots); fr_save_ppm(&frame, p); }
    G.scene = SC_EPILOGUE; G.scene_line = 1; G.scene_t = 1000;
    view_draw(&G, &frame);
    CHECK(lit(&frame) > 2000, "Dinit PID 1 acceptance scene renders");
    if (shots) { char p[512]; snprintf(p, sizeof p, "%s/dinit-handoff.ppm", shots); fr_save_ppm(&frame, p); }
    G.scene_line = 4;
    view_draw(&G, &frame);
    CHECK(lit(&frame) > 2000, "SysVinit last-runlevel dialogue renders");
    if (shots) { char p[512]; snprintf(p, sizeof p, "%s/sysvinit-handoff.ppm", shots); fr_save_ppm(&frame, p); }
    G.scene_line = 5;
    view_draw(&G, &frame);
    CHECK(lit(&frame) > 2000, "SysVinit final shutdown status renders");
    if (shots) { char p[512]; snprintf(p, sizeof p, "%s/sysvinit-exit.ppm", shots); fr_save_ppm(&frame, p); }
    G.state = ST_SETTINGS; G.settings_from = ST_PAUSE; G.settings_sel = SET_CRT; G.no_music = true; G.fullscreen = true;
    view_draw(&G, &frame);
    if (shots) { char p[512]; snprintf(p, sizeof p, "%s/settings_from_pause.ppm", shots); fr_save_ppm(&frame, p); }
    G.settings_from = ST_MENU; G.settings_sel = SET_VOLUME; G.no_music = false; G.fullscreen = false;
    view_draw(&G, &frame);
    CHECK(lit(&frame) > 300, "settings screen over the menu background renders");
    if (shots) { char p[512]; snprintf(p, sizeof p, "%s/settings_from_menu.ppm", shots); fr_save_ppm(&frame, p); }
    G.state = ST_PAUSE; G.pause_sel = PAUSE_SETTINGS;
    view_draw(&G, &frame);
    if (shots) { char p[512]; snprintf(p, sizeof p, "%s/pause_settings.ppm", shots); fr_save_ppm(&frame, p); }
    G.state = ST_PAUSE; G.pause_sel = PAUSE_VOLUME; G.vol_show_t = 60; G.volume = 4;
    view_draw(&G, &frame);
    if (shots) { char p[512]; snprintf(p, sizeof p, "%s/pause_volume.ppm", shots); fr_save_ppm(&frame, p); }
    G.state = ST_MENU; G.menu_sel = MENU_VOLUME;
    view_draw(&G, &frame);
    if (shots) { char p[512]; snprintf(p, sizeof p, "%s/menu_volume.ppm", shots); fr_save_ppm(&frame, p); }
    G.state = ST_PLAY; G.vol_show_t = 60; G.mute = true;
    view_draw(&G, &frame);
    if (shots) { char p[512]; snprintf(p, sizeof p, "%s/play_volume_readout.ppm", shots); fr_save_ppm(&frame, p); }
}


/* -------------------------------------------------------------- settings */
static void test_settings(void)
{
    printf("in-game settings screen\n");
    char dir[] = "/tmp/initwars-set-XXXXXX";
    if (!mkdtemp(dir)) { CHECK(false, "mkdtemp"); return; }
    setenv("INITWARS_DATA_DIR", dir, 1);

    /* open from the pause menu: the run is frozen and survives the round trip */
    fresh(11); G.no_save = false; game_test_start(&G, CH_RUNIT, 1); G.q_n = 0;
    ticks(&G, 30);
    int hp = G.p.hp, frame_before = G.run_frames;
    press(&G, K_ESC);
    CHECK(G.state == ST_PAUSE, "paused");
    press(&G, K_DOWN); press(&G, K_DOWN); press(&G, K_DOWN);
    CHECK(G.pause_sel == PAUSE_SETTINGS, "pause menu has a SETTINGS row after the CODEX row");
    press(&G, K_CONFIRM);
    CHECK(G.state == ST_SETTINGS && G.settings_from == ST_PAUSE && G.run_active, "Enter opens settings, run still active");
    ticks(&G, 120);
    CHECK(G.p.hp == hp && G.run_frames == frame_before, "game world stays frozen while settings are open");

    /* each row */
    G.volume = 5;
    press(&G, K_RIGHT); press(&G, K_RIGHT); press(&G, K_LEFT);
    CHECK(G.settings_sel == SET_VOLUME && G.volume == 6, "volume row: L/R adjust (got %d)", G.volume);
    press(&G, K_DOWN);
    CHECK(!G.no_music, "music defaults on");
    press(&G, K_CONFIRM); CHECK(G.no_music, "music row: Enter turns music off");
    press(&G, K_RIGHT);   CHECK(!G.no_music, "music row: Right toggles it back");
    press(&G, K_RIGHT);   CHECK(G.no_music, "music off again");
    ticks(&G, 3); CHECK(G.music == 0.0f, "music target is silent while music is off");
    press(&G, K_DOWN);
    press(&G, K_CONFIRM); CHECK(G.mute, "mute row toggles mute");
    press(&G, K_CONFIRM); CHECK(!G.mute, "and back");
    press(&G, K_DOWN);
    bool crt0 = G.crt;
    press(&G, K_CONFIRM); CHECK(G.crt != crt0, "CRT row toggles scanlines");
    press(&G, K_DOWN);
    CHECK(!G.no_shake, "screen shake defaults on");
    press(&G, K_CONFIRM); CHECK(G.no_shake, "shake row turns shake off");
    press(&G, K_DOWN);
    CHECK(!G.fs_toggle, "no fullscreen request yet");
    press(&G, K_CONFIRM); CHECK(G.fs_toggle, "fullscreen row raises the request for main.c");
    press(&G, K_DOWN);
    CHECK(G.settings_sel == SET_BACK, "last row is BACK");
    press(&G, K_DOWN); CHECK(G.settings_sel == SET_VOLUME, "selection wraps");
    press(&G, K_UP);   CHECK(G.settings_sel == SET_BACK, "and wraps backwards");

    /* shake really is suppressed in the renderer */
    G.state = ST_PLAY; G.shake = 10; G.frame = 0; G.no_shake = true;   /* frame 0 shifts the picture by one pixel */
    static Frame fa, fb;
    view_draw(&G, &fa);
    G.no_shake = false; view_draw(&G, &fb);
    CHECK(memcmp(fa.px, fb.px, sizeof fa.px) != 0, "shake on: the picture is shifted");
    G.shake = 0; view_draw(&G, &fb);
    CHECK(memcmp(fa.px, fb.px, sizeof fa.px) == 0, "shake off gives the same picture as no shake at all");
    G.state = ST_SETTINGS;

    /* BACK and Esc both return to the pause menu, never straight into the fight */
    press(&G, K_CONFIRM);
    CHECK(G.state == ST_PAUSE && G.run_active, "BACK returns to the pause menu");
    press(&G, K_CONFIRM);
    CHECK(G.state == ST_SETTINGS, "reopen");
    press(&G, K_ESC);
    CHECK(G.state == ST_PAUSE, "Esc returns to the pause menu");
    press(&G, K_ESC);
    CHECK(G.state == ST_PLAY, "Esc again resumes");
    CHECK(G.p.hp == hp, "player untouched by the whole trip");

    /* persistence: changes are written after the quiet moment and restored next start */
    G.volume = 7; G.no_music = true; G.no_shake = true; G.crt = true;
    G.vol_save_t = 5; ticks(&G, 8);
    Settings st; settings_load(&st);
    CHECK(st.volume == 7 && st.music == 0 && st.shake == 0 && st.crt == 1, "settings written (v%d m%d s%d c%d)", st.volume, st.music, st.shake, st.crt);
    game_init(&G, 12);
    CHECK(G.volume == 7 && G.no_music && G.no_shake && G.crt, "settings restored on the next start");
    CHECK(settings_save_volume(2) && (settings_load(&st), st.volume == 2 && st.music == 0 && st.shake == 0 && st.crt == 1),
          "saving only the volume keeps the other saved settings");

    /* opening from the main menu */
    fresh(13); G.state = ST_MENU; G.menu_sel = MENU_SETTINGS;
    press(&G, K_CONFIRM);
    CHECK(G.state == ST_SETTINGS && G.settings_from == ST_MENU && G.settings_sel == 0, "main menu SETTINGS row opens the screen");
    press(&G, K_ESC);
    CHECK(G.state == ST_MENU, "Esc returns to the main menu");

    /* hostile settings files */
    char p[1100]; snprintf(p, sizeof p, "%s/settings.txt", dir);
    FILE *fp = fopen(p, "w"); fputs("volume 4\ncrt 7\nmusic -1\nshake banana\nunknown 1\n", fp); fclose(fp);
    settings_load(&st);
    CHECK(st.volume == 4 && st.crt == -1 && st.music == -1 && st.shake == -1, "bad values are ignored per key");
    game_init(&G, 14);
    CHECK(G.volume == 4 && !G.crt && !G.no_music && !G.no_shake, "bad keys fall back to defaults (music and shake on, CRT off)");
    remove(p);

    /* no_save never writes */
    fresh(15); G.no_save = true; G.volume = 1; G.vol_save_t = 3; ticks(&G, 6); game_flush_settings(&G);
    settings_load(&st); CHECK(st.volume == -1, "no_save leaves the settings file alone");

    /* fuzz the screen: random keys never leave it in an invalid state */
    fresh(16); G.state = ST_SETTINGS; G.settings_from = ST_PAUSE;
    uint32_t r = 77;
    for (int i = 0; i < 20000 && G.state == ST_SETTINGS; i++) {
        Input in; memset(&in, 0, sizeof in);
        Key k = (Key)(rng_u32(&r) % 8);
        if (k == K_ESC || k == K_CONFIRM || k == K_FIRE) k = K_DOWN;
        in.pressed[k] = true; game_update(&G, &in);
        if (!(G.settings_sel >= 0 && G.settings_sel < SET_COUNT && G.volume >= 0 && G.volume <= VOL_MAX)) { CHECK(false, "settings fuzz broke state"); break; }
    }
    CHECK(G.state == ST_SETTINGS, "settings fuzz stayed on the screen");

    unsetenv("INITWARS_DATA_DIR");
}

/* ---------------------------------------------------------------- volume */
static int count_frame_color(const Frame *f, int x0, int y0, int x1, int y1, int pal)
{
    int count = 0;
    uint32_t color = color_argb(pal);
    for (int y = y0; y < y1; y++)
        for (int x = x0; x < x1; x++)
            if (f->px[y * LW + x] == color) count++;
    return count;
}

static void test_upgrade_layout(void)
{
    printf("upgrade and character-select layout regression\n");
    fresh(2401);
    game_test_start(&G, CH_RUNIT, 3);
    G.state = ST_UPGRADE;
    G.upg_opt[0] = UPG_HP;
    G.upg_opt[1] = UPG_FIRE;
    G.upg_opt[2] = UPG_DMG;
    view_draw(&G, &frame);
    CHECK(count_frame_color(&frame, 0, 26, LW, 45, C_BGREEN) > 40,
          "upgrade title is visible below the HUD rather than painted over");
    CHECK(count_frame_color(&frame, 0, 45, LW, 55, C_AMBER) > 20,
          "upgrade subtitle is visible between title and patch cards");

    fresh(2402);
    G.state = ST_SELECT;
    G.ch_sel = CH_SYSVINIT;
    G.select_mode = SCORE_CAMPAIGN;
    G.endless = false;
    G.boss_rush = false;
    view_draw(&G, &frame);
    CHECK(count_frame_color(&frame, 0, 181, LW, 190, C_GREY) > 5,
          "SysVinit special description remains visible");
    CHECK(count_frame_color(&frame, 0, 191, LW, 200, C_DKAMBER) > 5,
          "SysVinit legacy note has its own row below the special description");
}

static void test_volume(void)
{
    printf("volume\n");
    char dir[] = "/tmp/initwars-vol-XXXXXX";
    CHECK(mkdtemp(dir) != NULL, "mkdtemp");
    setenv("INITWARS_DATA_DIR", dir, 1);
    Input in; memset(&in, 0, sizeof in);

    game_init(&G, 1);
    CHECK(G.volume == VOL_DEFAULT, "default volume is %d (got %d)", VOL_DEFAULT, G.volume);
    CHECK(audio_volume_gain(0) == 0.0f && audio_volume_gain(VOL_MAX) == 1.0f && audio_volume_gain(VOL_DEFAULT) < 0.5f, "gain curve: silent, full, and a quieter default");
    for (int i = 1; i <= VOL_MAX; i++) CHECK(audio_volume_gain(i) > audio_volume_gain(i - 1), "gain rises with every step (%d)", i);

    in.pressed[K_VOL_DN] = true; tick(&G, &in);
    CHECK(G.volume == VOL_DEFAULT - 1 && G.vol_show_t > 0, "hotkey lowers volume and shows the readout");
    for (int i = 0; i < 30; i++) { in.pressed[K_VOL_DN] = true; tick(&G, &in); }
    CHECK(G.volume == 0, "volume clamps at 0 (got %d)", G.volume);
    for (int i = 0; i < 30; i++) { in.pressed[K_VOL_UP] = true; tick(&G, &in); }
    CHECK(G.volume == VOL_MAX, "volume clamps at max (got %d)", G.volume);

    /* saved after a short quiet moment, and a new game picks it up */
    G.volume = 3; G.vol_save_t = 10; ticks(&G, 12);
    CHECK(settings_load_volume() == 3, "volume saved to disk (got %d)", settings_load_volume());
    game_init(&G, 2);
    CHECK(G.volume == 3, "volume restored on the next start (got %d)", G.volume);

    /* pending change is flushed on exit; no_save never writes */
    G.volume = 8; G.vol_save_t = 40; game_flush_settings(&G);
    CHECK(settings_load_volume() == 8, "flush on exit writes the pending change");
    G.no_save = true; G.volume = 2; G.vol_save_t = 5; ticks(&G, 10); game_flush_settings(&G);
    CHECK(settings_load_volume() == 8, "no_save leaves the settings file alone");

    /* pause menu: volume row adjusts with left/right and Enter on it does nothing harmful */
    fresh(3); game_test_start(&G, CH_RUNIT, 1); G.volume = 5; G.q_n = 0;
    Input esc; memset(&esc, 0, sizeof esc); esc.pressed[K_ESC] = true; tick(&G, &esc);
    CHECK(G.state == ST_PAUSE, "paused");
    Input dn; memset(&dn, 0, sizeof dn); dn.pressed[K_DOWN] = true; tick(&G, &dn);
    CHECK(G.pause_sel == PAUSE_VOLUME, "Down from RESUME lands on the volume row");
    press(&G, K_RIGHT); press(&G, K_RIGHT);
    CHECK(G.volume == 7 && G.state == ST_PAUSE, "Right raises volume in the pause menu (got %d)", G.volume);
    press(&G, K_LEFT);
    CHECK(G.volume == 6, "Left lowers volume (got %d)", G.volume);
    press(&G, K_CONFIRM);
    CHECK(G.state == ST_PAUSE && G.run_active, "Enter on the volume row neither resumes nor quits");
    Sfx id; float pt; bool sel = false; while (game_pop_sfx(&G, &id, &pt)) sel |= (id == SFX_SELECT);
    CHECK(sel, "changing volume plays a sample at the new level");
    press(&G, K_ESC);
    CHECK(G.state == ST_PLAY, "Esc still resumes");

    /* main menu row */
    fresh(4); G.state = ST_MENU; G.menu_sel = MENU_VOLUME; G.volume = 5;
    press(&G, K_RIGHT); CHECK(G.volume == 6, "menu: Right raises volume");
    press(&G, K_LEFT); press(&G, K_LEFT); CHECK(G.volume == 4, "menu: Left lowers volume");
    press(&G, K_CONFIRM); CHECK(G.volume == 5 && G.state == ST_MENU, "menu: Enter steps the volume up and stays in the menu");

    /* hostile settings files must not crash or leak odd values into the game */
    char p[1100]; snprintf(p, sizeof p, "%s/settings.txt", dir);
    FILE *fp = fopen(p, "w"); fputs("volume 99\nvolume -4\nvolume banana\n", fp); fclose(fp);
    CHECK(settings_load_volume() == -1, "out-of-range and garbage volume values are ignored");
    fp = fopen(p, "wb"); srand(9); for (int i = 0; i < 100000; i++) fputc(rand() & 0xFF, fp); fclose(fp);
    int v = settings_load_volume(); CHECK(v >= -1 && v <= VOL_MAX, "random bytes give a sane value (%d)", v);
    remove(p); mkdir(p, 0700);
    CHECK(settings_load_volume() == -1 && !settings_save_volume(4), "settings.txt as a directory: loads default, save fails cleanly");
    rmdir(p);
    char afile[1100], through[2300]; snprintf(afile, sizeof afile, "%s/afile", dir);
    fp = fopen(afile, "w"); fputs("x", fp); fclose(fp);
    snprintf(through, sizeof through, "%s/sub/dir", afile);
    setenv("INITWARS_DATA_DIR", through, 1);
    CHECK(settings_load_volume() == -1 && !settings_save_volume(4), "unwritable data dir fails cleanly");
    game_init(&G, 5); CHECK(G.volume == VOL_DEFAULT, "game starts at the default when settings are unavailable");
    unsetenv("INITWARS_DATA_DIR");
}

/* ------------------------------------------------ phase-1 regressions */
static void test_banners(void)
{
    printf("boss banners and text limits\n");
    for (int ph = 1; ph <= 4; ph++) {
        fresh(400 + (uint32_t)ph); game_test_start(&G, CH_RUNIT, BOSS_WAVE); G.god = true;
        ticks(&G, WAVE_INTRO_FRAMES + 80);
        /* force every phase entry through the real code path by killing the boss phase by phase */
        for (int k = 0; k < ph - 1; k++) {
            Enemy *b = NULL; for (int i = 0; i < MAX_ENEMY; i++) if (G.en[i].on && G.en[i].type == E_BOSS) b = &G.en[i];
            if (!b) break;
            G.boss_inv = 0; game_test_damage(&G, b, b->maxhp * 0.26f); ticks(&G, 5);
        }
        for (int i = 0; i < 2; i++) {
            size_t n = strlen(G.banner_text[i]);
            CHECK(n < sizeof G.banner_text[i], "banner %d length %d fits its buffer", i, (int)n);
            CHECK(text_width(G.banner_text[i]) <= 232 || i == 0, "banner line %d fits the banner box (%d px): '%s'", i, text_width(G.banner_text[i]), G.banner_text[i]);
        }
    }
    CHECK(sizeof G.banner_text[1] >= 48, "banner buffer holds the longest phase hint line");
}

/* random-input fuzz: arbitrary key edges and forced state jumps must never crash, hang or corrupt state
 * (run under ASan/UBSan with `make sanitize`). */
static uint32_t rnd_next(uint32_t *r) { *r = *r * 1664525u + 1013904223u; return *r >> 8; }

static void test_fuzz(void)
{
    printf("random-input fuzz\n");
    uint32_t r = 12345u;
#define RND() rnd_next(&r)
    for (int run = 0; run < 12; run++) {
        fresh(900u + (uint32_t)run);
        G.god = (run % 3) == 0;
        Input in; memset(&in, 0, sizeof in);
        int frames = 30000;
        for (int i = 0; i < frames; i++) {
            if ((RND() & 15) == 0) { Key k = (Key)(RND() % K_COUNT); in.held[k] = !in.held[k]; in.pressed[k] = in.held[k]; in.pressed[K_ANY] = true; }
            if ((RND() & 4095) == 0) { memset(in.held, 0, sizeof in.held); }
            if ((RND() & 8191) == 0) {                         /* jump into an arbitrary screen mid-run */
                int jc = (int)(RND() % CH_COUNT), jw = 1 + (int)(RND() % NUM_WAVES);
                game_test_start(&G, jc, jw);
                G.state = (GameState)(RND() % 12);
                if (G.state == ST_STORY) { G.scene = (int)(RND() % SC_COUNT); G.scene_line = 0; G.scene_t = 0; G.scene_inplay = (RND() & 1) != 0; }
            }
            game_update(&G, &in);
            memset(in.pressed, 0, sizeof in.pressed);
            if ((i % 97) == 0) view_draw(&G, &frame);
            Sfx id; float p; while (game_pop_sfx(&G, &id, &p)) {}
            if (G.quit) G.quit = false;
            if (G.p.hp < -1 || G.volume < 0 || G.volume > VOL_MAX || G.wave < 0 || G.wave > NUM_WAVES + 1) { CHECK(0, "state out of range (hp=%d vol=%d wave=%d)", G.p.hp, G.volume, G.wave); break; }
        }
        CHECK(G.volume >= 0 && G.volume <= VOL_MAX && G.pause_sel >= 0 && G.pause_sel < PAUSE_COUNT && G.menu_sel >= 0 && G.menu_sel < MENU_COUNT, "run %d ended with sane menu/volume state", run);
    }
#undef RND
}

/* ----------------------------------------------- in-fight SysVinit scene */
static int alive_total(const Game *g) { int n = 0; for (int i = 0; i < MAX_ENEMY; i++) if (g->en[i].on) n++; return n; }

static void test_midscene(void)
{
    printf("in-fight SysVinit scene (wave 9)\n");
    Input none; memset(&none, 0, sizeof none);
    CHECK(story_scene_before_wave(9) == -1, "no blank-screen scene before wave 9 any more");
    CHECK(story_scene_midwave(9) == SC_REDHAT && story_scene_midwave(5) == -1 && story_scene_midwave(10) == -1, "only wave 9 has the mid-fight scene");

    for (int ch = 0; ch < CH_COUNT; ch++) {
        fresh(300 + (uint32_t)ch); game_test_start(&G, ch, 9); G.god = false;
        int guard = 0;
        while (G.state == ST_PLAY && guard++ < 60 * 30) tick(&G, &none);
        CHECK(G.state == ST_STORY && G.scene == SC_REDHAT && G.scene_inplay, "%s: scene starts during the fight (state=%d)", CHARS[ch].name, G.state);
        CHECK(G.wave == 9 && G.phase == PH_FIGHT && G.q_i >= (G.q_n + 1) / 2 && G.q_i < G.q_n + 1, "%s: triggered once half the wave has spawned (q_i=%d/%d)", CHARS[ch].name, G.q_i, G.q_n);
        int en0 = alive_total(&G), qi0 = G.q_i, hp0 = G.p.hp, ph0 = G.phase_t;
        bool shots = false; for (int i = 0; i < MAX_EBUL; i++) shots |= G.eb[i].on;
        CHECK(en0 > 0 && !shots, "%s: enemies stay on the field, enemy shots cleared (en=%d)", CHARS[ch].name, en0);
        float ex[MAX_ENEMY]; for (int i = 0; i < MAX_ENEMY; i++) ex[i] = G.en[i].x;

        for (int i = 0; i < 20; i++) press(&G, K_CONFIRM);          /* mashing right after the fight must not skip it */
        CHECK(G.state == ST_STORY && G.scene_line == 0, "%s: button mashing in the first moments does not skip the scene", CHARS[ch].name);
        ticks(&G, 30);
        bool still = true; for (int i = 0; i < MAX_ENEMY; i++) still &= (G.en[i].x == ex[i]);
        CHECK(still && G.phase_t == ph0 && G.p.hp == hp0, "%s: the fight is frozen while the scene plays", CHARS[ch].name);

        guard = 0;
        while (G.state == ST_STORY && guard++ < 4000) { press(&G, K_CONFIRM); ticks(&G, 3); }
        CHECK(G.state == ST_PLAY && G.wave == 9 && !G.scene_inplay, "%s: back in the fight (state=%d wave=%d)", CHARS[ch].name, G.state, G.wave);
        CHECK(alive_total(&G) >= en0 && G.q_i >= qi0 && G.phase == PH_FIGHT, "%s: the same fight carries on, nothing reset (en %d->%d, q_i %d->%d)", CHARS[ch].name, en0, alive_total(&G), qi0, G.q_i);
        CHECK(G.p.inv > 0 && G.p.hp == hp0, "%s: a short grace period after resuming", CHARS[ch].name);
        CHECK(G.sysv_weak, "%s: legacy flag set", CHARS[ch].name);
        if (ch == CH_SYSVINIT) CHECK(G.banner_t > 0 && strstr(G.banner_text[0], "LEGACY"), "SysVinit sees the LEGACY MODE banner after the scene");
        else CHECK(G.banner_t == 0 && game_player_fire_cd(&G) == CHARS[ch].fire_cd, "%s: no banner and no weakening", CHARS[ch].name);

        ticks(&G, 600);
        CHECK(G.state != ST_STORY, "%s: scene does not play a second time", CHARS[ch].name);
    }

    /* Esc skips (after the short guard), same result as watching it */
    fresh(310); game_test_start(&G, CH_SYSVINIT, 9);
    int guard = 0; while (G.state == ST_PLAY && guard++ < 60 * 30) tick(&G, &none);
    Input esc; memset(&esc, 0, sizeof esc); esc.pressed[K_ESC] = true;
    ticks(&G, 45); tick(&G, &esc);
    CHECK(G.state == ST_PLAY && G.sysv_weak && G.wave == 9, "Esc skips the scene back into the fight");

    /* the wave can still be finished afterwards, and the next wave is plain */
    fresh(311); game_test_start(&G, CH_RUNIT, 9); G.god = true;
    guard = 0;
    while (G.wave == 9 && guard++ < 60 * 90) { Input in2; memset(&in2, 0, sizeof in2); in2.held[K_FIRE] = true; in2.pressed[K_CONFIRM] = (guard % 7) == 0; tick(&G, &in2); if (G.state == ST_UPGRADE) break; }
    CHECK(G.state == ST_UPGRADE && G.wave == 9, "wave 9 can be cleared after the scene (state=%d)", G.state);

    /* a new run plays the scene again */
    fresh(312); game_test_start(&G, CH_RUNIT, 9);
    guard = 0; while (G.state == ST_PLAY && guard++ < 60 * 30) tick(&G, &none);
    CHECK(G.state == ST_STORY && G.midscene_seen, "scene seen flag set");
    G.state = ST_PLAY; G.scene_inplay = false;
    Input esc2; memset(&esc2, 0, sizeof esc2); esc2.pressed[K_ESC] = true;
    tick(&G, &esc2); G.pause_sel = PAUSE_RESTART; press(&G, K_CONFIRM);
    CHECK(!G.midscene_seen && !G.sysv_weak, "restarting the run clears the scene flag");

    /* the old test hook still behaves for the non-fight path */
    fresh(313); game_test_start(&G, CH_SYSVINIT, 9); G.scene = SC_REDHAT; game_test_story_done(&G);
    CHECK(G.sysv_weak && G.state == ST_PLAY && G.banner_t > 0, "hook path (not in-fight) still weakens and shows the banner");
}

/* ----------------------------------------- extended story / PID 1 handoff */
static void test_story_handoff(void)
{
    printf("extended campaign story and Dinit handoff\n");
    fresh(0xd1717u);
    game_test_start(&G, CH_SYSVINIT, BOSS_WAVE);
    G.god = true;
    G.phase = PH_CLEAR;
    G.phase_t = 79;
    G.q_i = G.q_n;
    memset(G.en, 0, sizeof G.en);
    memset(G.eb, 0, sizeof G.eb);
    Input none; memset(&none, 0, sizeof none);
    tick(&G, &none);
    CHECK(G.state == ST_STORY && G.scene == SC_EPILOGUE,
          "campaign victory plays the Dinit/SysVinit handoff before results");
    CHECK(G.run_active && !G.victory,
          "run is not recorded before the player reaches the epilogue");
    const Scene *ep = story_scene(SC_EPILOGUE);
    bool has_dinit_pid1 = false, has_sysv_exit = false;
    for (int i = 0; i < ep->n; i++) {
        has_dinit_pid1 |= strcmp(ep->lines[i].who, "Dinit") == 0 && strstr(ep->lines[i].text, "PID 1") != NULL;
        has_sysv_exit |= strstr(ep->lines[i].text, "exit status 0") != NULL;
    }
    CHECK(has_dinit_pid1 && has_sysv_exit,
          "epilogue explicitly hands PID 1 to Dinit and records SysVinit's clean exit");
    game_test_story_done(&G);
    CHECK(G.state == ST_RESULTS && G.victory && !G.run_active,
          "skipping the epilogue completes and records the campaign victory");

    fresh(0xd1718u);
    game_test_start_bossrush(&G, CH_SYSVINIT, 1);
    CHECK(story_scene_before_wave(1) == SC_ACT1 && G.state == ST_PLAY,
          "Boss Rush starts without entering the Campaign story path");
}

/* ------------------------------------------------------------ mechanics */
static void test_endless(void)
{
    printf("endless mode\n");
    fresh(9001); game_test_start_endless(&G, CH_RUNIT, 1);
    CHECK(G.endless && !G.boss_wave && G.wave == 1, "endless starts on a regular wave");
    CHECK(G.q_n >= 4 && G.q_n <= MAX_QUEUE, "endless wave has a bounded generated queue (%d)", G.q_n);
    for (int i = 0; i < G.q_n; i++) CHECK(G.queue[i].type > E_NONE && G.queue[i].type < E_COUNT, "endless queue type %u valid", G.queue[i].type);
    static const int unlock_types[] = { E_JOURNALD, E_LOGIND, E_NETWORKD, E_TIMER, E_UDEVD, E_RESOLVED, E_DEPTARGET, E_CRON, E_OOMD };
    static const int unlock_counts[] = { 1, 2, 3, 4, 5, 7, 9 };
    for (int wave = 2; wave <= 7; wave++) {
        game_test_start_endless(&G, CH_RUNIT, wave);
        int allowed = unlock_counts[wave - 1];
        for (int i = 0; i < G.q_n; i++) {
            bool found = false;
            for (int j = 0; j < allowed; j++) if (G.queue[i].type == unlock_types[j]) found = true;
            CHECK(found, "wave %d queue type %u respects staged unlocks", wave, G.queue[i].type);
        }
    }
    game_test_start_endless(&G, CH_RUNIT, 9);
    CHECK(!G.boss_wave && G.q_n > 0, "wave nine is a generated survival wave");
    CHECK(G.wave_threat == 39, "wave nine starts with the expected threat budget");
    int threat9 = G.wave_threat;
    game_test_start_endless(&G, CH_RUNIT, 49);
    CHECK(G.wave_threat == 100, "Endless threat budget is capped at 100");
    game_test_start_endless(&G, CH_RUNIT, 10);
    CHECK(G.boss_wave && G.q_n == 1 && G.queue[0].type == E_BOSS, "every tenth endless wave is a boss round");
    game_test_start_endless(&G, CH_RUNIT, 11);
    CHECK(!G.boss_wave && G.q_n >= 7 && G.wave_threat > threat9, "endless resumes with a larger threat budget after the boss round");
    G.phase = PH_CLEAR; G.phase_t = WAVE_CLEAR_FRAMES - 1;
    Input in = {0}; game_update(&G, &in);
    CHECK(G.state == ST_UPGRADE && G.run_active, "endless mode continues instead of ending after campaign boss index");
    fresh(9002); game_test_start_endless(&G, CH_BUSYBOX, 20);
    CHECK(G.boss_wave && G.wave == 20, "later tenth-wave boss rounds remain scheduled");
    CHECK(G.enemy_tempo > 1.0f, "endless tempo increases with wave number");
    Enemy *scaled = game_test_spawn(&G, E_JOURNALD, 80, 80, false);
    CHECK(scaled != NULL && scaled->maxhp > ENEMIES[E_JOURNALD].hp,
          "regular Endless enemies gain health as the run advances");
    CHECK(scaled != NULL && scaled->maxhp <= ENEMIES[E_JOURNALD].hp * 1.75f,
          "regular Endless enemy health has a defined upper bound");
    CHECK(G.enemy_tempo <= 1.65f, "Endless tempo remains under its difficulty cap");
    game_test_start_endless(&G, CH_RUNIT, 100);
    CHECK(fabsf(G.enemy_tempo - 1.65f) < 0.001f, "Endless tempo reaches but does not exceed its 1.65 cap");
    G.phase = PH_FIGHT; G.phase_t = WAVE_INTRO_FRAMES; G.q_i = 0; G.spawn_t = 0; G.god = true;
    Input idle = {0};
    for (int i = 0; i < 65; i++) game_update(&G, &idle);
    const Enemy *boss = game_boss(&G);
    CHECK(boss != NULL && boss->maxhp >= ENEMIES[E_BOSS].hp * 1.34f, "endless boss health scales by 35 percent per boss tier");

    fresh(9005); game_test_start_endless(&G, CH_RUNIT, 6);
    G.p.hp = 2;
    G.phase = PH_CLEAR; G.phase_t = WAVE_CLEAR_FRAMES - 1;
    game_update(&G, &idle);
    CHECK(G.state == ST_UPGRADE, "wave six clear reaches the next upgrade");
    G.upg_opt[0] = UPG_FIRE; G.upg_sel = 0;
    ticks(&G, 13); press(&G, K_CONFIRM);
    CHECK(G.wave == 7 && G.p.hp == 3, "entering wave seven restores one HP when below maximum");

    fresh(9003); G.state = ST_MENU; G.menu_sel = MENU_ENDLESS;
    press(&G, K_CONFIRM);
    CHECK(G.state == ST_SELECT && G.endless, "main menu opens character select in endless mode");
    view_draw(&G, &frame);
    press(&G, K_UP);
    CHECK(G.state == ST_SELECT && G.select_mode == SCORE_CAMPAIGN && !G.endless && !G.boss_rush,
          "character select cycles up from Endless to Campaign");
    press(&G, K_DOWN);
    CHECK(G.select_mode == SCORE_ENDLESS && G.endless, "character select cycles back to Endless");
    press(&G, K_CONFIRM);
    CHECK(G.endless && G.run_active, "selected character starts an endless run");
    view_draw(&G, &frame);

    fresh(9004); game_test_start(&G, CH_RUNIT, 1); G.god = true; G.q_n = 0;
    Enemy *cron = game_test_spawn(&G, E_CRON, 100, 80, false);
    Enemy *oomd = game_test_spawn(&G, E_OOMD, 220, 80, false);
    CHECK(cron != NULL && oomd != NULL, "new enemy types spawn");
    cron->t = 1.0f; oomd->t = 1.0f;
    G.phase = PH_FIGHT;
    for (int i = 0; i < 50; i++) game_update(&G, &idle);
    CHECK(oomd->on && (oomd->sub == 1 || oomd->sub == 2 || oomd->t > 0), "OOM killer cycles through telegraph and charge states");
    CHECK(cron->on && cron->t > 0, "cron daemon resets its scheduled attack timer");
    view_draw(&G, &frame);
}

static void test_endless_generator_properties(void)
{
    printf("endless generator properties (406 seeds x 32 waves)\n");
    static const uint8_t unlocks[] = {
        E_JOURNALD, E_LOGIND, E_NETWORKD, E_TIMER, E_UDEVD,
        E_RESOLVED, E_DEPTARGET, E_CRON, E_OOMD
    };
    static const int unlock_counts[] = { 1, 2, 3, 4, 5, 7, 9 };
    static const int threat_cost[E_COUNT] = {
        [E_JOURNALD] = 3, [E_LOGIND] = 4, [E_NETWORKD] = 4,
        [E_TIMER] = 5, [E_UDEVD] = 5, [E_RESOLVED] = 4,
        [E_DEPTARGET] = 8, [E_CRON] = 7, [E_OOMD] = 9
    };
    fresh(0x51A7E);
    for (uint32_t seed = 1; seed <= 406; seed++) {
        for (int wave = 1; wave <= 32; wave++) {
            G.rng = (seed * UINT32_C(0x9e3779b9)) ^ ((uint32_t)wave * UINT32_C(0x85ebca6b));
            if (G.rng == 0) G.rng = 1;
            game_test_start_endless(&G, CH_RUNIT, wave);
            bool boss = wave % 10 == 0;
            CHECK(G.boss_wave == boss, "seed %u wave %d boss schedule", seed, wave);
            float tempo = clampf(1.0f + 0.025f * (float)(wave - 1), 1.0f, 1.65f);
            CHECK(fabsf(G.enemy_tempo - tempo) < 0.001f,
                  "seed %u wave %d tempo matches formula", seed, wave);
            CHECK(G.q_n > 0 && G.q_n <= MAX_QUEUE,
                  "seed %u wave %d queue bounded and nonempty (%d)", seed, wave, G.q_n);
            if (boss) {
                CHECK(G.q_n == 1 && G.queue[0].type == E_BOSS,
                      "seed %u wave %d queues exactly one boss", seed, wave);
                CHECK(G.wave_threat == 0, "seed %u wave %d boss wave has no stale threat budget", seed, wave);
                continue;
            }
            int budget = 12 + wave * 3;
            if (budget > 100) budget = 100;
            CHECK(G.wave_threat == budget, "seed %u wave %d threat budget", seed, wave);
            int unlocked = unlock_counts[wave < 7 ? wave - 1 : 6];
            int spent = 0;
            for (int i = 0; i < G.q_n; i++) {
                QueueItem q = G.queue[i];
                CHECK(q.type > E_NONE && q.type < E_COUNT,
                      "seed %u wave %d queue item %d type is valid", seed, wave, i);
                bool in_pool = false;
                for (int j = 0; j < unlocked; j++) if (unlocks[j] == q.type) in_pool = true;
                CHECK(in_pool, "seed %u wave %d queue item %d respects enemy unlock stage", seed, wave, i);
                CHECK(q.restart == 0 || wave >= 4,
                      "seed %u wave %d restart modifier is not early", seed, wave);
                spent += threat_cost[q.type];
            }
            CHECK(spent <= budget,
                  "seed %u wave %d generated threat %d does not exceed budget %d",
                  seed, wave, spent, budget);
        }
    }
}

static void test_boss_rush(void)
{
    printf("boss rush\n");
    fresh(9100); game_test_start_bossrush(&G, CH_RUNIT, 1);
    CHECK(G.boss_rush && !G.endless && G.boss_wave, "boss rush starts in its own mode");
    CHECK(G.q_n == 1 && G.queue[0].type == E_BOSS, "each boss-rush round queues exactly one boss");
    CHECK(G.state == ST_PLAY, "boss rush skips campaign story scenes");

    G.phase = PH_FIGHT; G.phase_t = WAVE_INTRO_FRAMES; G.q_i = 0; G.spawn_t = 0; G.god = true;
    Input idle = {0};
    for (int i = 0; i < 65; i++) game_update(&G, &idle);
    const Enemy *boss1 = game_boss(&G);
    CHECK(boss1 != NULL, "first boss spawns");
    float hp1 = boss1 ? boss1->maxhp : 0.0f;

    game_test_start_bossrush(&G, CH_RUNIT, 3);
    G.phase = PH_FIGHT; G.phase_t = WAVE_INTRO_FRAMES; G.q_i = 0; G.spawn_t = 0; G.god = true;
    for (int i = 0; i < 65; i++) game_update(&G, &idle);
    const Enemy *boss3 = game_boss(&G);
    CHECK(boss3 != NULL && boss3->maxhp > hp1, "later boss-rush encounters gain health");

    fresh(9101); G.state = ST_MENU; G.menu_sel = MENU_BOSSRUSH;
    press(&G, K_CONFIRM);
    CHECK(G.state == ST_SELECT && G.boss_rush && !G.endless, "main menu selects boss rush");
    view_draw(&G, &frame);
    press(&G, K_CONFIRM);
    CHECK(G.state == ST_UPGRADE && G.boss_rush_loadout == 6, "character selection opens six-pick Boss Rush loadout");
    int picks = 0;
    for (int i = 0; i < UPG_COUNT; i++) picks += G.p.up[i];
    CHECK(picks == 0, "loadout does not apply a patch before player confirmation");
    for (int i = 0; i < 6; i++) {
        ticks(&G, 13); press(&G, K_CONFIRM);
    }
    picks = 0;
    for (int i = 0; i < UPG_COUNT; i++) picks += G.p.up[i];
    CHECK(G.state == ST_PLAY && G.boss_rush && G.q_n == 1 && G.wave == 1,
          "six starting choices launch the first Boss Rush fight");
    CHECK(picks == 6, "all six starting patches are applied (%d)", picks);

    fresh(9102); game_test_start_bossrush(&G, CH_RUNIT, 1);
    G.p.hp = 1;
    G.phase = PH_CLEAR; G.phase_t = WAVE_CLEAR_FRAMES - 1;
    game_update(&G, &idle);
    CHECK(G.state == ST_UPGRADE && G.run_active, "cleared boss rush round offers an upgrade");
    CHECK(G.p.hp == 4, "Boss Rush intermission restores three HP (bounded by max health)");
    ticks(&G, 13); press(&G, K_CONFIRM);
    CHECK(G.wave == 2 && G.boss_rush && G.boss_wave && G.q_n == 1 && G.queue[0].type == E_BOSS,
          "upgrade advances to the next boss encounter");

    fresh(9103); game_test_start_bossrush(&G, CH_BUSYBOX, 1); G.god = true;
    for (int round = 1; round <= 8; round++) {
        G.phase = PH_FIGHT; G.phase_t = WAVE_INTRO_FRAMES; G.q_i = 0; G.spawn_t = 0;
        int guard = 0;
        while (game_boss(&G) == NULL && guard++ < 100) game_update(&G, &idle);
        const Enemy *boss = game_boss(&G);
        CHECK(boss != NULL, "boss rush round %d spawns its boss", round);
        if (boss) {
            float expected = ENEMIES[E_BOSS].hp * (1.0f + 0.35f * (float)(round - 1));
            CHECK(fabsf(boss->maxhp - expected) < 0.01f,
                  "Boss Rush round %d uses the documented 35%% base-health increment", round);
        }
        CHECK(fabsf(G.enemy_tempo - fminf(1.0f + 0.04f * (float)(round - 1), 1.5f)) < 0.001f,
              "Boss Rush round %d uses the documented tempo multiplier", round);
        if (boss) memset(G.en, 0, sizeof G.en); /* simulate the boss and its summoned units being defeated */
        game_update(&G, &idle);
        if (round < 8) {
            CHECK(G.phase == PH_CLEAR, "round %d enters clear phase", round);
            ticks(&G, WAVE_CLEAR_FRAMES);
            CHECK(G.state == ST_UPGRADE, "round %d awards an upgrade", round);
            ticks(&G, 13); press(&G, K_CONFIRM);
            CHECK(G.wave == round + 1 && G.state == ST_PLAY, "upgrade advances to boss %d", round + 1);
        } else {
            ticks(&G, WAVE_CLEAR_FRAMES);
            CHECK(G.state == ST_RESULTS && G.victory && !G.run_active, "eighth boss completes Boss Rush as a victory");
        }
    }
    CHECK(G.scores.n == 1 && G.scores.e[0].mode == SCORE_BOSSRUSH, "boss-rush victory is saved to the boss-rush leaderboard");
    CHECK(G.achievements & (UINT32_C(1) << ACH_BOSS_BREAKER), "completing Boss Rush unlocks Boss Breaker");
    view_draw(&G, &frame);

    fresh(9104); G.state = ST_SCORES; G.scores_mode = SCORE_CAMPAIGN; G.state_t = 20;
    press(&G, K_RIGHT);
    CHECK(G.state == ST_SCORES && G.scores_mode == SCORE_ENDLESS, "right arrow switches score tabs without leaving the screen");
    press(&G, K_RIGHT);
    CHECK(G.scores_mode == SCORE_BOSSRUSH, "score tabs cycle to Boss Rush");
    press(&G, K_LEFT);
    CHECK(G.scores_mode == SCORE_ENDLESS, "left arrow cycles score tabs backwards");
    view_draw(&G, &frame);
}

static void test_open_mode_balance(void)
{
    printf("open mode balance safeguards\n");
    fresh(9200);
    G.state = ST_SELECT;
    press(&G, K_DOWN);
    CHECK(G.select_mode == SCORE_ENDLESS && G.endless && !G.boss_rush, "select screen switches to Endless");
    press(&G, K_DOWN);
    CHECK(G.select_mode == SCORE_BOSSRUSH && G.boss_rush && !G.endless, "select screen switches to Boss Rush");
    press(&G, K_UP);
    CHECK(G.select_mode == SCORE_ENDLESS && G.endless && !G.boss_rush, "select screen switches backwards");

    game_test_start(&G, CH_BUSYBOX, 1);
    int campaign_cd = game_special_cd(&G);
    Enemy *campaign_enemy = game_test_spawn(&G, E_JOURNALD, 80, 80, false);
    CHECK(campaign_enemy != NULL && campaign_enemy->maxhp == ENEMIES[E_JOURNALD].hp,
          "Campaign enemy health retains its original balance");
    game_test_start_endless(&G, CH_BUSYBOX, 1);
    CHECK(game_special_cd(&G) >= (int)((float)campaign_cd * 1.49f),
          "BusyBox emergency special recharges 50 percent slower in Endless");
    game_test_start_endless(&G, CH_RUNIT, 1);
    CHECK(game_special_cd(&G) == CHARS[CH_RUNIT].special_cd,
          "open-mode cooldown penalty applies only to BusyBox");

    fresh(9201); G.state = ST_MENU; G.menu_sel = MENU_BOSSRUSH;
    press(&G, K_CONFIRM); press(&G, K_CONFIRM);
    CHECK(G.state == ST_UPGRADE && G.boss_rush_loadout == 6, "Boss Rush loadout opens for cap test");
    for (int i = 0; i < 3; i++) {
        ticks(&G, 13); G.upg_sel = 0; G.upg_opt[0] = UPG_SPECIAL; press(&G, K_CONFIRM);
    }
    CHECK(G.p.up[UPG_SPECIAL] == 3, "Boss Rush starting loadout permits three special patches");
    CHECK(G.upg_opt[0] != UPG_SPECIAL && G.upg_opt[1] != UPG_SPECIAL && G.upg_opt[2] != UPG_SPECIAL,
          "special patch is removed from the choice pool at its three-stack cap");
    for (int i = 0; i < 3; i++) { ticks(&G, 13); G.upg_sel = 0; press(&G, K_CONFIRM); }
    CHECK(G.state == ST_PLAY && G.p.up[UPG_SPECIAL] == 3, "remaining loadout picks still launch the first boss");
    G.state = ST_UPGRADE; G.state_t = 13; G.upg_sel = 0; G.upg_opt[0] = UPG_SPECIAL;
    press(&G, K_CONFIRM);
    CHECK(G.p.up[UPG_SPECIAL] == 3, "defensive upgrade application cannot exceed special cap");

    fresh(9202); game_test_start_endless(&G, CH_RUNIT, 6);
    G.p.hp = G.p.maxhp - 1; G.phase = PH_CLEAR; G.phase_t = WAVE_CLEAR_FRAMES - 1;
    Input idle = {0}; game_update(&G, &idle);
    CHECK(G.state == ST_UPGRADE, "wave six clear offers its normal patch");
    G.upg_opt[0] = G.upg_opt[1] = G.upg_opt[2] = UPG_FIRE;
    ticks(&G, 13); press(&G, K_CONFIRM);
    CHECK(G.wave == 7 && G.p.hp == G.p.maxhp,
          "Endless maintenance restores one HP on every seventh wave");
}

static void test_mechanics(void)
{
    printf("mechanics\n");
    Input in; memset(&in, 0, sizeof in);

    /* movement + attack + damage */
    fresh(1); game_test_start(&G, CH_RUNIT, 1); G.q_n = 0;
    float x0 = G.p.x; in.held[K_LEFT] = true;
    for (int i = 0; i < 30; i++) tick(&G, &in);
    in.held[K_LEFT] = false;
    CHECK(G.p.x < x0 - 30, "player moves left (%f -> %f)", (double)x0, (double)G.p.x);
    Enemy *e = game_test_spawn(&G, E_JOURNALD, G.p.x, G.p.y - 60, false);
    float hp0 = e->hp;
    in.held[K_FIRE] = true;
    for (int i = 0; i < 60; i++) tick(&G, &in);
    in.held[K_FIRE] = false;
    CHECK(!e->on || e->hp < hp0, "auto-aimed shots hurt the enemy");

    /* invulnerability frames and damage */
    fresh(2); game_test_start(&G, CH_RUNIT, 1); G.q_n = 0;
    ticks(&G, 70);   /* wave-start grace period */
    game_test_spawn(&G, E_USB, G.p.x, G.p.y, false);
    ticks(&G, 2);
    CHECK(G.p.hp == G.p.maxhp - 1, "contact damage hp=%d", G.p.hp);
    ticks(&G, 10);
    CHECK(G.p.hp == G.p.maxhp - 1, "i-frames prevent double hit");

    /* every enemy type runs for a long time without crashing, and shoots/spawns things */
    for (int t = E_JOURNALD; t <= E_DEPTARGET; t++) {
        fresh(30u + (uint32_t)t); game_test_start(&G, CH_BUSYBOX, 1); G.q_n = 0; G.god = true;
        game_test_spawn(&G, t, 160, 80, false);
        game_test_spawn(&G, E_JOURNALD, 100, 90, false);
        int bullets = 0, maxlines = 0, maxbars = 0, minions = 0, fakes = 0, links = 0;
        for (int i = 0; i < 900; i++) {
            tick(&G, &in);
            for (int k = 0; k < MAX_EBUL; k++) bullets += G.eb[k].on;
            for (int k = 0; k < MAX_LINES; k++) maxlines += G.lines[k].on;
            for (int k = 0; k < MAX_BARRIER; k++) maxbars += G.bars[k].on;
            minions += game_minion_count(&G);
            for (int k = 0; k < MAX_ENEMY; k++) { fakes += G.en[k].on && G.en[k].type == E_FAKE; links += G.en[k].on && G.en[k].link >= 0; }
        }
        bool ok = true;
        switch (t) {
        case E_JOURNALD: case E_TIMER: ok = bullets > 0; break;
        case E_LOGIND: ok = maxbars > 0; break;
        case E_NETWORKD: ok = maxlines > 0; break;
        case E_UDEVD: ok = minions > 0; break;
        case E_RESOLVED: ok = fakes > 0; break;
        case E_DEPTARGET: ok = links > 0; break;
        }
        CHECK(ok, "%s shows its signature behaviour", ENEMIES[t].name);
    }

    /* fake targets vanish when hit and never hurt */
    fresh(3); game_test_start(&G, CH_RUNIT, 1); G.q_n = 0;
    Enemy *f = game_test_spawn(&G, E_FAKE, G.p.x, G.p.y, false); f->sub = E_JOURNALD; f->life = 999;
    ticks(&G, 3);
    CHECK(G.p.hp == G.p.maxhp, "fake targets are harmless");
    game_test_damage(&G, f, 0.1f);
    CHECK(!f->on, "fake vanishes on any hit");

    /* dependency link: linked enemy takes reduced damage, link breaks when blob dies */
    fresh(4); game_test_start(&G, CH_RUNIT, 1); G.q_n = 0; G.god = true;
    Enemy *blob = game_test_spawn(&G, E_DEPTARGET, 200, 100, false);
    Enemy *v = game_test_spawn(&G, E_LOGIND, 180, 100, false);
    int bi = (int)(blob - G.en);
    v->link = bi; v->link_t = 999;
    float vh = v->hp;
    game_test_damage(&G, v, 4.0f);
    CHECK(fabsf((vh - v->hp) - 4.0f * LINK_DAMAGE_MULT) < 0.01f, "linked enemies are tougher");
    game_test_damage(&G, blob, 9999);
    ticks(&G, 2);
    CHECK(v->link < 0, "destroying the blob breaks the links");

    /* Restart=always: telegraphed marker, one comeback, no second comeback */
    fresh(5); game_test_start(&G, CH_RUNIT, 1); G.q_n = 0; G.god = true;
    Enemy *r = game_test_spawn(&G, E_JOURNALD, 100, 100, true);
    game_test_damage(&G, r, 9999);
    int markers = 0; for (int i = 0; i < MAX_ENEMY; i++) markers += G.en[i].on && G.en[i].type == E_MARKER;
    CHECK(markers == 1, "restart marker spawned");
    CHECK(game_enemies_alive(&G) == 1, "marker blocks wave clear");
    ticks(&G, MARKER_DELAY + 5);
    int back = 0; Enemy *comeback = NULL;
    for (int i = 0; i < MAX_ENEMY; i++) if (G.en[i].on && G.en[i].type == E_JOURNALD) { back++; comeback = &G.en[i]; }
    CHECK(back == 1 && comeback && comeback->restarted, "service came back exactly once");
    game_test_damage(&G, comeback, 9999);
    markers = 0; for (int i = 0; i < MAX_ENEMY; i++) markers += G.en[i].on && G.en[i].type == E_MARKER;
    CHECK(markers == 0, "no second restart");
    /* shooting the marker cancels the restart */
    Enemy *r2 = game_test_spawn(&G, E_LOGIND, 100, 100, true);
    game_test_damage(&G, r2, 9999);
    for (int i = 0; i < MAX_ENEMY; i++) if (G.en[i].on && G.en[i].type == E_MARKER) game_test_damage(&G, &G.en[i], 5);
    ticks(&G, MARKER_DELAY + 5);
    int logs = 0; for (int i = 0; i < MAX_ENEMY; i++) logs += G.en[i].on && G.en[i].type == E_LOGIND;
    CHECK(logs == 0, "clearing the marker cancels the restart");

    /* specials */
    for (int c = 0; c < CH_COUNT; c++) {
        fresh(10u + (uint32_t)c); game_test_start(&G, c, 1); G.q_n = 0;
        Enemy *en = game_test_spawn(&G, E_LOGIND, G.p.x + 20, G.p.y, false);
        float h = en->hp;
        game_test_spawn(&G, E_JOURNALD, 20, 40, false);
        for (int i = 0; i < 5; i++) { G.eb[i].on = true; G.eb[i].x = G.p.x + 10; G.eb[i].y = G.p.y; G.eb[i].life = 100; }
        Input sp; memset(&sp, 0, sizeof sp); sp.pressed[K_SPECIAL] = true; sp.held[K_UP] = (c == CH_RUNIT);
        float py = G.p.y;
        game_update(&G, &sp);
        CHECK(G.p.special_t > 0, "%s special goes on cooldown", CHARS[c].name);
        switch (CHARS[c].special) {
        case SP_PULSE: CHECK(en->hp < h && !G.eb[0].on, "pulse hurts and clears shots"); break;
        case SP_DASH: ticks(&G, 6); CHECK(G.p.y < py - 15 && G.p.inv > 0, "dash moves with invulnerability"); break;
        case SP_STASIS: ticks(&G, 1); CHECK(en->ts < 1.0f, "stasis slows nearby enemies"); break;
        case SP_CLEAR: CHECK(!G.eb[0].on, "rc-update clears shots"); break;
        case SP_SHIELD: CHECK(G.p.shield_t > 0, "shield up"); break;
        }
    }

    /* SysVinit weakening only after the Act II scene, and only for SysVinit */
    fresh(6); game_test_start(&G, CH_SYSVINIT, 5);
    int cd0 = game_player_fire_cd(&G); float d0 = game_player_damage(&G);
    G.scene = SC_REDHAT; game_test_story_done(&G);
    CHECK(G.sysv_weak && game_player_fire_cd(&G) > cd0 && game_player_damage(&G) < d0, "SysVinit weakens (cd %d->%d)", cd0, game_player_fire_cd(&G));
    CHECK(G.p.hp > 0 && G.state == ST_PLAY, "SysVinit stays playable");
    fresh(6); game_test_start(&G, CH_RUNIT, 5);
    cd0 = game_player_fire_cd(&G); G.scene = SC_REDHAT; game_test_story_done(&G);
    CHECK(game_player_fire_cd(&G) == cd0, "other characters are unaffected");

    /* feedback + usability behaviours added in the polish phase */
    fresh(21); game_test_start(&G, CH_RUNIT, 3); G.p.hp = 2; G.p.up[UPG_HP] = 0;
    G.state = ST_UPGRADE; G.state_t = 30; G.upg_opt[0] = UPG_SPEED; G.upg_opt[1] = UPG_FIRE; G.upg_opt[2] = UPG_DMG; G.upg_sel = 0;
    Input cf; memset(&cf, 0, sizeof cf); cf.pressed[K_CONFIRM] = true;
    game_update(&G, &cf);
    CHECK(G.wave == 4 && G.p.hp == 3, "maintenance break heals 1 HP before wave 4 (hp=%d wave=%d)", G.p.hp, G.wave);
    fresh(22); game_test_start(&G, CH_SYSVINIT, 5); G.scene = SC_REDHAT; game_test_story_done(&G);
    CHECK(G.banner_t > 0 && strstr(G.banner_text[0], "LEGACY"), "SysVinit sees a LEGACY MODE banner with the numbers: '%s' / '%s'", G.banner_text[0], G.banner_text[1]);
    fresh(23); game_test_start(&G, CH_RUNIT, 5); G.scene = SC_REDHAT; game_test_story_done(&G);
    CHECK(G.banner_t == 0, "other characters get no legacy banner");
    fresh(24); game_test_start(&G, CH_BUSYBOX, BOSS_WAVE); G.god = true; ticks(&G, WAVE_INTRO_FRAMES + 80);
    CHECK(game_boss_alive(&G) && G.boss_phase == 1 && G.banner_t > 0, "boss phase 1 announced (phase=%d)", G.boss_phase);
    fresh(25); game_test_start(&G, CH_RUNIT, 1); G.q_n = 0;
    Input sp2; memset(&sp2, 0, sizeof sp2); sp2.pressed[K_SPECIAL] = true;
    game_update(&G, &sp2); Sfx sid; float spp; while (game_pop_sfx(&G, &sid, &spp)) {}
    ticks(&G, 3); game_update(&G, &sp2);
    bool beeped = false; while (game_pop_sfx(&G, &sid, &spp)) beeped |= (sid == SFX_BEEP);
    CHECK(beeped, "pressing special while recharging gives audible feedback");
    fresh(26); game_test_start(&G, CH_RUNIT, 1); G.q_n = 0; G.god = false;
    G.p.inv = 0; game_test_spawn(&G, E_USB, G.p.x, G.p.y, false); ticks(&G, 2);
    CHECK(G.hurt_flash_t > 0 && G.hurt_src[E_USB] == 1, "hit sets red-border flash and is attributed to its source");

    /* pause menu: resume / restart run / quit to menu; Esc toggles */
    fresh(27); game_test_start(&G, CH_S6, 5); G.score = 1234; G.p.hp = 2;
    Input esc; memset(&esc, 0, sizeof esc); esc.pressed[K_ESC] = true;
    game_update(&G, &esc); CHECK(G.state == ST_PAUSE, "Esc pauses");
    int f0 = G.run_frames; ticks(&G, 30); CHECK(G.run_frames == f0, "game is frozen while paused");
    game_update(&G, &esc); CHECK(G.state == ST_PLAY, "Esc resumes");
    game_update(&G, &esc); G.pause_sel = PAUSE_RESTART;
    Input cf2; memset(&cf2, 0, sizeof cf2); cf2.pressed[K_CONFIRM] = true;
    game_update(&G, &cf2);
    CHECK(G.state == ST_STORY && G.wave == 1 && G.score == 0 && G.p.hp == G.p.maxhp && G.p.ch == CH_S6 && !G.sysv_weak,
          "restart run: fresh run, same character (state=%d wave=%d score=%d)", G.state, G.wave, G.score);
    game_test_story_done(&G); game_update(&G, &esc); G.pause_sel = PAUSE_MENU; game_update(&G, &cf2);
    CHECK(G.state == ST_MENU && !G.run_active, "quit to menu");

    /* player cannot be trapped by a barrier closing on top of them */
    fresh(8); game_test_start(&G, CH_RUNIT, 1); G.q_n = 0; G.god = true;
    G.bars[0] = (Barrier){ true, { G.p.x - 3, G.p.y - 20, 6, 40 }, 0, 10, 100 };
    ticks(&G, 20);
    in.held[K_LEFT] = true; float px = G.p.x;
    for (int i = 0; i < 20; i++) tick(&G, &in);
    in.held[K_LEFT] = false;
    CHECK(G.p.x < px - 5, "player can leave a barrier that closed on them");
}

/* ------------------------------------------------------------------ bot */
#include "bot.h"
typedef struct { int steps; bool reached_ending, reached_boss, saw_redhat, saw_panic; int max_wave; } BotResult;

static char last_long[80];
static BotResult run_bot(uint32_t seed, int ch, bool god, int max_steps, const BotSkill *sk)
{
    fresh(seed);
    G.god = god;
    BotResult r = {0};
    Bot bot = { 0, 0, 0, seed * 2654435761u + 7u };
    Input in; memset(&in, 0, sizeof in);
    for (r.steps = 0; r.steps < max_steps; r.steps++) {
        bot_input(&bot, sk, &G, &in, ch, r.steps);
        game_update(&G, &in);
        if (G.state == ST_PLAY && G.wave > r.max_wave) r.max_wave = G.wave;
        if (G.state == ST_PLAY && G.wave >= BOSS_WAVE) r.reached_boss = true;
        if (G.state == ST_STORY && G.scene == SC_REDHAT) r.saw_redhat = true;
        if (G.state == ST_PANIC) { r.saw_panic = true; if (!god) break; }
        if (G.state == ST_ENDING) { r.reached_ending = true; break; }
        Sfx id; float p; while (game_pop_sfx(&G, &id, &p)) {}
        if (strlen(G.status[0]) > 51 && strcmp(G.status[0], last_long) != 0) {
            snprintf(last_long, sizeof last_long, "%s", G.status[0]);
            CHECK(0, "status line too long to fit the screen (%d chars): %s", (int)strlen(G.status[0]), G.status[0]);
        }
    }
    return r;
}

static void test_bot(void)
{
    printf("full-run bot (god mode, every character): menu -> select -> 24 waves -> boss -> results -> ending\n");
    for (int ch = 0; ch < CH_COUNT; ch++) {
        BotResult r = run_bot(100 + (uint32_t)ch, ch, true, 60 * 60 * 25, &BOT_AVERAGE);
        printf("  %-12s ending=%d boss=%d redhat=%d wave=%d frames=%d (%.1f min game time)\n", CHARS[ch].name,
               r.reached_ending, r.reached_boss, r.saw_redhat, r.max_wave, r.steps, r.steps / 3600.0);
        CHECK(r.reached_ending, "%s: bot did not reach the ending (stuck in state %d wave %d)", CHARS[ch].name, G.state, G.wave);
        CHECK(r.saw_redhat && r.reached_boss, "%s: story/boss missed", CHARS[ch].name);
        CHECK(G.victory && G.score > 5000, "%s: victory flag/score (%d)", CHARS[ch].name, G.score);
    }
    printf("normal-mode bot (no god): must never crash or hang\n");
    for (int ch = 0; ch < CH_COUNT; ch++)
        for (uint32_t s = 1; s <= 3; s++) {
            BotResult r = run_bot(s * 977 + (uint32_t)ch, ch, false, 60 * 60 * 15, &BOT_NOVICE);
            CHECK(r.reached_ending || r.saw_panic || r.steps >= 60 * 60 * 15, "run ended in odd state");
        }
}

typedef struct {
    int max_wave;
    int bosses_defeated;
    int frames;
    int kills;
    bool victory;
    bool capped;
} OpenBotResult;

static OpenBotResult run_open_bot(uint32_t seed, int ch, int mode, int max_steps,
                                  const BotSkill *skill)
{
    fresh(seed);
    G.state = ST_SELECT;
    G.ch_sel = 0;
    G.select_mode = mode;
    G.endless = mode == SCORE_ENDLESS;
    G.boss_rush = mode == SCORE_BOSSRUSH;
    G.god = false;

    Bot bot = { 0, 0, 0, seed * 2654435761u + 7u };
    Input in;
    memset(&in, 0, sizeof in);
    int steps = 0;
    for (; steps < max_steps; steps++) {
        bot_input(&bot, skill, &G, &in, ch, steps);
        game_update(&G, &in);
        if (G.state == ST_PANIC || G.state == ST_RESULTS) break;
        Sfx id;
        float pitch;
        while (game_pop_sfx(&G, &id, &pitch)) {}
    }

    OpenBotResult result = {0};
    result.max_wave = G.wave;
    result.victory = G.victory;
    result.frames = G.run_frames;
    result.kills = G.kills;
    result.capped = steps >= max_steps;
    if (mode == SCORE_BOSSRUSH)
        result.bosses_defeated = G.victory ? 8 : (G.wave > 0 ? G.wave - 1 : 0);
    return result;
}

static uint32_t open_benchmark_seed(uint32_t base, int mode, int ch, int sample)
{
    uint32_t seed = base ^ ((uint32_t)mode * UINT32_C(0x9e3779b9)) ^
                    ((uint32_t)ch * UINT32_C(0x85ebca6b)) ^
                    ((uint32_t)sample * UINT32_C(0xc2b2ae35));
    return seed ? seed : 1u;
}

static void open_balance_report(int runs, uint32_t base_seed)
{
    if (runs < 1) runs = 10;
    if (runs > 100) runs = 100;
    const int max_steps = 60 * 60 * 15;
    printf("INIT WARS open-mode balance report\n");
    printf("skill=expert runs_per_character=%d base_seed=0x%08x max_simulated_frames=%d (15m at 60Hz)\n", runs, base_seed, max_steps);
    printf("seed = base ^ (mode * 0x9e3779b9) ^ (character * 0x85ebca6b) ^ (sample * 0xc2b2ae35); zero is remapped to one\n");
    printf("modes are started through the real character-select/start path; bot uses tests/bot.h BOT_EXPERT\n\n");
    printf("ENDLESS\n");
    printf("%-13s %12s %14s %12s %12s\n", "character", "mean_wave", "survival_min", "mean_kills", "cap_runs");
    for (int ch = 0; ch < CH_COUNT; ch++) {
        double waves = 0.0, mins = 0.0, kills = 0.0;
        int capped = 0;
        for (int sample = 0; sample < runs; sample++) {
            uint32_t seed = open_benchmark_seed(base_seed, SCORE_ENDLESS, ch, sample);
            OpenBotResult r = run_open_bot(seed, ch, SCORE_ENDLESS, max_steps, &BOT_EXPERT);
            waves += r.max_wave;
            mins += (double)r.frames / (double)FPS / 60.0;
            kills += r.kills;
            if (r.capped) capped++;
        }
        printf("%-13s %12.2f %14.2f %12.2f %12d/%-8d\n", CHARS[ch].name,
               waves / runs, mins / runs, kills / runs, capped, runs);
    }
    printf("\nBOSS RUSH\n");
    printf("%-13s %12s %14s %15s %12s\n", "character", "win_rate", "mean_bosses", "survival_min", "cap_runs");
    for (int ch = 0; ch < CH_COUNT; ch++) {
        int wins = 0, capped = 0;
        double bosses = 0.0, mins = 0.0;
        for (int sample = 0; sample < runs; sample++) {
            uint32_t seed = open_benchmark_seed(base_seed, SCORE_BOSSRUSH, ch, sample);
            OpenBotResult r = run_open_bot(seed, ch, SCORE_BOSSRUSH, max_steps, &BOT_EXPERT);
            wins += r.victory;
            bosses += r.bosses_defeated;
            mins += (double)r.frames / (double)FPS / 60.0;
            if (r.capped) capped++;
        }
        printf("%-13s %11.1f%% %14.2f %15.2f %12d/%-8d\n", CHARS[ch].name,
               100.0 * (double)wins / (double)runs, bosses / runs, mins / runs, capped, runs);
    }
}

/* ------------------------------------------------- balance report (not a test) */
#include <time.h>
static void balance_report(int runs)
{
    const BotSkill *skills[] = { &BOT_NOVICE, &BOT_AVERAGE, &BOT_EXPERT };
    printf("Balance report: %d runs per cell, bots react like a human with latency.\n", runs);
    printf("win%% = reached the ending; wave = mean wave reached; hits/run = mean hits taken; time = mean minutes\n\n");
    for (int k = 0; k < 3; k++) {
        printf("skill: %s\n  %-13s %5s %6s %8s %6s   hits by wave 1..10\n", skills[k]->name, "character", "win%", "wave", "hits/run", "time");
        for (int ch = 0; ch < CH_COUNT; ch++) {
            int wins = 0; double wave = 0, hits = 0, mins = 0; int bywave[NUM_WAVES + 1] = {0};
            for (int s = 0; s < runs; s++) {
                BotResult r = run_bot(1000u + (uint32_t)s * 31u + (uint32_t)ch, ch, false, 60 * 60 * 20, skills[k]);
                wins += r.reached_ending; wave += r.max_wave; mins += r.steps / 3600.0;
                int h = 0; for (int w = 1; w <= NUM_WAVES; w++) { h += G.hurt_by_wave[w]; bywave[w] += G.hurt_by_wave[w]; }
                hits += h;
            }
            printf("  %-13s %4.0f%% %6.1f %8.1f %6.1f   ", CHARS[ch].name, 100.0 * wins / runs, wave / runs, hits / runs, mins / runs);
            for (int w = 1; w <= NUM_WAVES; w++) printf("%4.1f ", (double)bywave[w] / runs);
            printf("\n");
        }
        printf("\n");
    }
    /* who hurts: aggregate hit sources for the average bot over all characters */
    int src[E_COUNT + 2] = {0}; int total = 0;
    for (int ch = 0; ch < CH_COUNT; ch++)
        for (int s = 0; s < runs; s++) {
            run_bot(5000u + (uint32_t)s, ch, false, 60 * 60 * 20, &BOT_AVERAGE);
            for (int i = 0; i < E_COUNT; i++) { src[i] += G.hurt_src[i]; total += G.hurt_src[i]; }
        }
    printf("hit sources (average bot, all characters, %% of hits):\n");
    for (int i = 1; i < E_COUNT; i++) if (src[i]) printf("  %-18s %5.1f%%\n", ENEMIES[i].name, 100.0 * src[i] / (total ? total : 1));
    int bp[6] = {0}, tb = 0;
    for (int ch = 0; ch < CH_COUNT; ch++)
        for (int s = 0; s < runs; s++) {
            run_bot(5000u + (uint32_t)s, ch, false, 60 * 60 * 20, &BOT_AVERAGE);
            for (int i = 1; i <= 4; i++) { bp[i] += G.hurt_boss_phase[i]; tb += G.hurt_boss_phase[i]; }
        }
    printf("boss hits by phase (average bot): P1 %.0f%%  P2 %.0f%%  P3 %.0f%%  P4(survive+exposed) %.0f%%\n",
           100.0 * bp[1] / (tb ? tb : 1), 100.0 * bp[2] / (tb ? tb : 1), 100.0 * bp[3] / (tb ? tb : 1), 100.0 * bp[4] / (tb ? tb : 1));
    clock_t t0 = clock();
    int frames = 0;
    fresh(1); G.god = true;
    Bot bot = { 0, 0, 0, 9 }; Input in; memset(&in, 0, sizeof in);
    for (int i = 0; i < 40000; i++, frames++) { bot_input(&bot, &BOT_AVERAGE, &G, &in, 1, i); game_update(&G, &in); view_draw(&G, &frame); Sfx id; float p; while (game_pop_sfx(&G, &id, &p)) {} }
    printf("\nspeed: %.1f us per frame (update + full draw) on this machine; budget at 60 fps is 16666 us\n",
           1e6 * (double)(clock() - t0) / CLOCKS_PER_SEC / frames);
}


/* ------------------------------------------------------------ codex */
static void test_codex(void)
{
    printf("codex (names and descriptions of everything)\n");
    bool enemy_seen[E_COUNT] = {0}, hero_seen[CH_COUNT] = {0}, patch_seen[UPG_COUNT] = {0};
    for (int t = 0; t < CX_TAB_COUNT; t++) {
        CHECK(codex_entry_count(t) > 0 && codex_tab_name(t)[0], "tab %d has a name and entries", t);
        for (int i = 0; i < codex_entry_count(t); i++) {
            const CodexEntry *e = codex_entry(t, i);
            CHECK(e && codex_name(e)[0], "tab %d entry %d has a name", t, i);
            if (!e) continue;
            CHECK(e->text && strlen(e->text) > 20 && strlen(e->text) <= CODEX_TEXT_MAX, "%s: text length %zu", codex_name(e), e->text ? strlen(e->text) : 0);
            CHECK(e->tip && strlen(e->tip) <= CODEX_TIP_MAX, "%s: tip length", codex_name(e));
            int room = e->kind == CK_HERO ? 5 : (e->kind == CK_PATCH ? 5 : 7);   /* lines left under the header of each kind */
            CHECK(wrap_lines(e->text, 29) <= room, "%s: description fits the pane (%d lines, room for %d)", codex_name(e), wrap_lines(e->text, 29), room);
            CHECK(wrap_lines(e->tip, 29) <= 2, "%s: tip fits the pane (%d lines)", codex_name(e), wrap_lines(e->tip, 29));
            CHECK(text_width(codex_name(e)) <= 102, "%s: name fits the list column", codex_name(e));
            if (e->kind == CK_ENEMY) enemy_seen[e->ref] = true;
            if (e->kind == CK_HERO) hero_seen[e->ref] = true;
            if (e->kind == CK_PATCH) patch_seen[e->ref] = true;
        }
    }
    CHECK(codex_entry(0, -1) == NULL && codex_entry(0, 999) == NULL && codex_entry(99, 0) == NULL, "out-of-range codex lookups are safe");
    for (int t = E_JOURNALD; t < E_COUNT; t++) CHECK(enemy_seen[t], "codex lists %s", ENEMIES[t].name);
    for (int c = 0; c < CH_COUNT; c++) CHECK(hero_seen[c], "codex lists %s", CHARS[c].name);
    for (int u = 0; u < UPG_COUNT; u++) CHECK(patch_seen[u], "codex lists patch %s", UPG_NAME[u]);

    /* main menu opens it, tabs and entries move and wrap, Esc returns */
    fresh(61); G.state = ST_MENU; G.menu_sel = MENU_CODEX;
    press(&G, K_CONFIRM);
    CHECK(G.state == ST_CODEX && G.codex_from == ST_MENU && G.codex_tab == 0 && G.codex_sel == 0, "main menu opens the Codex");
    press(&G, K_UP);
    CHECK(G.codex_sel == codex_entry_count(0) - 1, "Up wraps to the last entry");
    press(&G, K_DOWN);
    CHECK(G.codex_sel == 0, "Down wraps back to the first");
    press(&G, K_RIGHT); press(&G, K_DOWN);
    CHECK(G.codex_tab == 1 && G.codex_sel == 1, "Right changes tab, Down picks entry");
    press(&G, K_RIGHT);
    CHECK(G.codex_tab == 2 && G.codex_sel == 0, "changing tab resets the selection");
    press(&G, K_LEFT); press(&G, K_LEFT); press(&G, K_LEFT);
    CHECK(G.codex_tab == CX_TAB_COUNT - 1, "tabs wrap");
    press(&G, K_ESC);
    CHECK(G.state == ST_MENU, "Esc returns to the main menu");

    /* from the pause menu the run stays frozen and comes back to the pause menu */
    fresh(62); game_test_start(&G, CH_RUNIT, 1); G.q_n = 0;
    ticks(&G, 30);
    int hp = G.p.hp, rf = G.run_frames;
    press(&G, K_ESC); press(&G, K_DOWN); press(&G, K_DOWN);
    CHECK(G.pause_sel == PAUSE_CODEX, "pause menu has a CODEX row");
    press(&G, K_CONFIRM);
    CHECK(G.state == ST_CODEX && G.codex_from == ST_PAUSE && G.run_active, "pause menu opens the Codex, run stays active");
    ticks(&G, 90);
    CHECK(G.p.hp == hp && G.run_frames == rf, "the game world is frozen while the Codex is open");
    press(&G, K_ESC);
    CHECK(G.state == ST_PAUSE && G.run_active, "Esc returns to the pause menu");
    press(&G, K_ESC);
    CHECK(G.state == ST_PLAY, "and the run resumes");

    /* every entry of every tab renders (and the screen is not blank) */
    fresh(63); G.state = ST_CODEX; G.codex_from = ST_MENU;
    for (int t = 0; t < CX_TAB_COUNT; t++)
        for (int i = 0; i < codex_entry_count(t); i++) {
            G.codex_tab = t; G.codex_sel = i;
            view_draw(&G, &frame);
            CHECK(lit(&frame) > 1200, "codex tab %d entry %d draws", t, i);
        }
    /* hostile selection values never crash the renderer */
    G.codex_tab = 1; G.codex_sel = 9999; view_draw(&G, &frame);
    CHECK(lit(&frame) > 600, "out-of-range selection still draws");

    /* fuzz: random keys in the Codex keep the state sane */
    fresh(64); G.state = ST_CODEX; G.codex_from = ST_MENU;
    uint32_t r = 1234567u;
    for (int i = 0; i < 20000; i++) {
        Input in; memset(&in, 0, sizeof in);
        r ^= r << 13; r ^= r >> 17; r ^= r << 5;
        in.pressed[r % K_COUNT] = true;
        game_update(&G, &in);
        if (G.state != ST_CODEX) { G.state = ST_CODEX; G.state_t = 0; }
        CHECK(G.codex_tab >= 0 && G.codex_tab < CX_TAB_COUNT && G.codex_sel >= 0 && G.codex_sel < codex_entry_count(G.codex_tab), "codex selection stays in range");
        if (failures > 20) break;
    }
}

/* ------------------------------------------- boss moves in every direction */
static void test_boss_movement(void)
{
    printf("boss roams the arena in every direction\n");
    for (int ph = 1; ph <= 4; ph++) {
        fresh(7000 + (uint32_t)ph); game_test_start(&G, CH_BUSYBOX, BOSS_WAVE); G.god = true;
        Input none; memset(&none, 0, sizeof none);
        ticks(&G, WAVE_INTRO_FRAMES + 150);
        Enemy *b = NULL; for (int k = 0; k < MAX_ENEMY; k++) if (G.en[k].on && G.en[k].type == E_BOSS) b = &G.en[k];
        CHECK(b != NULL, "phase %d: boss exists", ph);
        if (!b) continue;
        /* put the fight into the requested phase through the real damage path */
        while (G.boss_phase < ph && b->on) { game_test_damage(&G, b, 1e9f * 0 + b->maxhp * 0.26f); ticks(&G, 1); G.boss_inv = 0; }
        for (int i = 0; i < MAX_ENEMY; i++) if (G.en[i].on && G.en[i].type != E_BOSS) G.en[i].on = false;
        float minx = 1e9f, maxx = -1e9f, miny = 1e9f, maxy = -1e9f, maxstep = 0, lx = b->x, ly = b->y;
        int dirs[8] = {0};   /* movement octants seen */
        for (int i = 0; i < 60 * 40; i++) {
            G.god = true; G.p.hp = G.p.maxhp; G.boss_inv = 120;   /* stay in this phase; invulnerable */
            tick(&G, &none);
            if (!b->on) break;
            minx = b->x < minx ? b->x : minx; maxx = b->x > maxx ? b->x : maxx;
            miny = b->y < miny ? b->y : miny; maxy = b->y > maxy ? b->y : maxy;
            float dx = b->x - lx, dy = b->y - ly, st = sqrtf(dx * dx + dy * dy);
            if (st > maxstep) maxstep = st;
            if (st > 0.3f) { int o = (int)floorf((atan2f(dy, dx) + 3.14159265f) / (3.14159265f / 4.0f)); dirs[o & 7]++; }
            lx = b->x; ly = b->y;
            CHECK(b->x >= AX0 + 20 && b->x <= AX1 - 20 && b->y >= AY0 + 10 && b->y <= AY0 + 140, "phase %d: boss stays inside the arena (%.0f,%.0f)", ph, (double)b->x, (double)b->y);
        }
        int octants = 0; for (int o = 0; o < 8; o++) if (dirs[o] > 20) octants++;
        CHECK(maxx - minx > 100.0f, "phase %d: boss covers a wide horizontal range (%.0f)", ph, (double)(maxx - minx));
        CHECK(maxy - miny > 40.0f, "phase %d: boss also moves vertically (%.0f)", ph, (double)(maxy - miny));
        CHECK(octants >= 6, "phase %d: boss moves in at least six of eight directions (%d)", ph, octants);
        CHECK(maxstep <= BOSS_MAX_STEP + 0.01f, "phase %d: boss never teleports (max step %.2f)", ph, (double)maxstep);
    }
}

/* -------------------------------------------------- 24-wave campaign rules */
static void test_long_campaign(void)
{
    printf("24-wave campaign rules\n");
    /* upgrades cap at five stacks in the campaign too, the special recharge at three */
    fresh(71); game_test_start(&G, CH_RUNIT, 3); G.q_n = 0;
    for (int i = 0; i < 9; i++) { G.state = ST_UPGRADE; G.state_t = 30; G.upg_opt[0] = UPG_FIRE; G.upg_sel = 0; G.wave = 3; press(&G, K_CONFIRM); }
    CHECK(G.p.up[UPG_FIRE] == 5, "fire-rate patches cap at 5 in the campaign (%d)", G.p.up[UPG_FIRE]);
    for (int i = 0; i < 6; i++) { G.state = ST_UPGRADE; G.state_t = 30; G.upg_opt[0] = UPG_SPECIAL; G.upg_sel = 0; G.wave = 3; press(&G, K_CONFIRM); }
    CHECK(G.p.up[UPG_SPECIAL] == 3, "special recharge caps at 3 (%d)", G.p.up[UPG_SPECIAL]);
    /* a fully capped build is offered repair patches, never a dead pick */
    memset(G.p.up, 0, sizeof G.p.up);
    for (int u = 0; u < UPG_COUNT; u++) G.p.up[u] = u == UPG_SPECIAL ? 3 : 5;
    G.state = ST_PLAY; G.phase = PH_CLEAR; G.phase_t = WAVE_CLEAR_FRAMES; G.q_i = G.q_n; memset(G.en, 0, sizeof G.en); G.wave = 8;
    ticks(&G, 3);
    CHECK(G.state == ST_UPGRADE && G.upg_opt[0] == UPG_HP && G.upg_opt[1] == UPG_HP, "capped build is offered repair patches");

    /* maintenance breaks every fourth wave */
    for (int w = 1; w <= NUM_WAVES; w++) CHECK(REST_WAVE(w) == (w % 4 == 0), "wave %d maintenance break rule", w);

    /* waves 1-9 are as tough as before, later campaign units toughen gently, the boss is stronger than in Boss Rush round 1 */
    fresh(72); game_test_start(&G, CH_RUNIT, 9);
    Enemy *e9 = game_test_spawn(&G, E_LOGIND, 100, 100, false);
    CHECK(e9 && fabsf(e9->maxhp - ENEMIES[E_LOGIND].hp) < 0.01f, "wave 9 units have base health");
    fresh(73); game_test_start(&G, CH_RUNIT, 21);
    Enemy *e21 = game_test_spawn(&G, E_LOGIND, 100, 100, false);
    float want = ENEMIES[E_LOGIND].hp * (1.0f + CAMPAIGN_HP_STEP * (float)(21 - CAMPAIGN_HP_START));
    CHECK(e21 && fabsf(e21->maxhp - want) < 0.01f && e21->maxhp > ENEMIES[E_LOGIND].hp, "wave 21 units are tougher (%.1f vs %.1f)", e21 ? (double)e21->maxhp : 0.0, (double)ENEMIES[E_LOGIND].hp);
    fresh(74); game_test_start(&G, CH_RUNIT, BOSS_WAVE); G.god = true; ticks(&G, WAVE_INTRO_FRAMES + 120);
    const Enemy *bb = game_boss(&G);
    CHECK(bb && fabsf(bb->maxhp - ENEMIES[E_BOSS].hp * CAMPAIGN_BOSS_HP_MULT) < 0.01f, "the campaign boss has %.0f%% of base health", (double)(CAMPAIGN_BOSS_HP_MULT * 100.0f));

    /* cron and the OOM killer actually act in the campaign now */
    fresh(75); game_test_start(&G, CH_RUNIT, 18); G.god = true; G.q_n = 0;
    Enemy *oom = game_test_spawn(&G, E_OOMD, 200, 80, false); oom->warp = 0;
    Enemy *cron = game_test_spawn(&G, E_CRON, 100, 80, false); cron->warp = 0;
    float ox = oom->x, oy = oom->y, cx0 = cron->x, cy0 = cron->y;
    ticks(&G, 240);
    CHECK(fabsf(oom->x - ox) + fabsf(oom->y - oy) > 5.0f, "the OOM killer moves in the campaign");
    CHECK(fabsf(cron->x - cx0) + fabsf(cron->y - cy0) > 2.0f, "cron moves in the campaign");
    bool shots = false; for (int i = 0; i < MAX_EBUL; i++) shots |= G.eb[i].on;
    ticks(&G, 240);
    for (int i = 0; i < MAX_EBUL; i++) shots |= G.eb[i].on;
    CHECK(shots, "cron fires its ring in the campaign");

    /* the version string is a proper x.y.z that has moved past 1.5.1 */
    int a = 0, b2 = 0, c = 0;
    CHECK(sscanf(GAME_VERSION, "%d.%d.%d", &a, &b2, &c) == 3, "version %s is major.minor.patch", GAME_VERSION);
    CHECK(a > 1 || (a == 1 && (b2 > 5 || (b2 == 5 && c > 1))), "version %s is newer than 1.5.1", GAME_VERSION);
}

int main(int argc, char **argv)
{
    if (argc > 1 && !strcmp(argv[1], "--open-balance")) {
        int runs = argc > 2 ? atoi(argv[2]) : 10;
        uint32_t seed = argc > 3 ? (uint32_t)strtoul(argv[3], NULL, 0) : UINT32_C(0x1a2b3c4d);
        open_balance_report(runs, seed);
        return 0;
    }
    if (argc > 1 && !strcmp(argv[1], "--balance")) { balance_report(argc > 2 ? atoi(argv[2]) : 20); return 0; }
    const char *shots = argc > 1 ? argv[1] : NULL;
    test_data();
    test_scores();
    test_achievements();
    test_profile();
    test_render(shots);
    test_render_new(shots);
    test_upgrade_layout();
    test_volume();
    test_settings();
    test_midscene();
    test_story_handoff();
    test_banners();
    test_fuzz();
    test_mechanics();
    test_endless();
    test_endless_generator_properties();
    test_boss_rush();
    test_open_mode_balance();
    test_codex();
    test_boss_movement();
    test_long_campaign();
    test_bot();
    printf("\n%d checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
}
