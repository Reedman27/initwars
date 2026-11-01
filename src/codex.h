/* codex.h - the in-game Codex: names and plain-language descriptions of everything in the game.
 * Pure data plus tiny accessors (no SDL, no Game struct), so the tests can check every entry. */
#ifndef CODEX_H
#define CODEX_H

#include "balance.h"

typedef enum { CX_UNITS, CX_INITS, CX_PATCHES, CX_RULES, CX_TAB_COUNT } CodexTab;
typedef enum { CK_ENEMY, CK_HERO, CK_GUEST, CK_PATCH, CK_RULE } CodexKind;

typedef struct {
    CodexKind kind;
    int ref;               /* CK_ENEMY: EnemyType. CK_HERO: CharId. CK_PATCH: UpgradeId. Otherwise unused. */
    const char *name;      /* NULL: take the name from the game tables (enemies, heroes, patches) */
    const char *text;      /* what it is and what it does */
    const char *tip;       /* how to deal with it ("" for none) */
} CodexEntry;

const char *codex_tab_name(int tab);
int codex_entry_count(int tab);
const CodexEntry *codex_entry(int tab, int idx);   /* NULL when out of range */
const char *codex_name(const CodexEntry *e);

/* Largest text and tip lengths allowed, so every entry fits its pane (checked by the tests). */
#define CODEX_TEXT_MAX 176
#define CODEX_TIP_MAX  84

#endif
