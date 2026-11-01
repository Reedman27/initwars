/* score.h - local high-score table (plain text file in the user's data directory). */
#ifndef SCORE_H
#define SCORE_H

#include <stdbool.h>
#include <stddef.h>

#define MAX_SCORES_PER_MODE 8
#define SCORE_MODE_COUNT 3
#define MAX_SCORES (MAX_SCORES_PER_MODE * SCORE_MODE_COUNT)

typedef enum { SCORE_CAMPAIGN, SCORE_ENDLESS, SCORE_BOSSRUSH } ScoreMode;
typedef struct { int ch; int score; int wave; int win; int mode; } ScoreEntry;
typedef struct { ScoreEntry e[MAX_SCORES]; int n; } ScoreTable;

/* $INITWARS_DATA_DIR, else $XDG_DATA_HOME/init-wars, else $HOME/.local/share/init-wars */
bool scores_path(char *out, size_t outsz); /* legacy combined file, retained for migration */
bool scores_path_mode(ScoreMode mode, char *out, size_t outsz);
void scores_load(ScoreTable *t);
bool scores_save(const ScoreTable *t);
/* Keeps up to MAX_SCORES_PER_MODE entries per mode; returns that mode's 0-based rank or -1. */
int  scores_add(ScoreTable *t, ScoreEntry e);

/* Settings: one small text file next to the scores. -1 means "no usable saved value". */
typedef struct { int volume, crt, music, shake; } Settings;   /* volume 0..VOL_MAX; the others 0/1 */
void settings_load(Settings *s);                 /* every field is -1 when missing or invalid */
bool settings_save(const Settings *s);           /* writes only the fields that are not -1 */
int  settings_load_volume(void);
bool settings_save_volume(int step);             /* updates the volume and keeps the other saved values */

#endif
