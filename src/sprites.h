/* sprites.h - original pixel art, stored as character maps (see render.c for the legend). */
#ifndef SPRITES_H
#define SPRITES_H

typedef struct {
    int w, h;
    const char *const *rows;
} Sprite;

#define SPRITE_PLAYERS 5
extern const Sprite SPR_PLAYER[SPRITE_PLAYERS];
extern const Sprite SPR_JOURNALD, SPR_LOGIND, SPR_NETWORKD, SPR_TIMER, SPR_UDEVD,
                    SPR_RESOLVED, SPR_DEPTARGET, SPR_USB, SPR_FLOPPY, SPR_PRINTER,
                    SPR_MARKER, SPR_MANUAL;

/* Every sprite, for the self-check in the tests. */
extern const Sprite *const ALL_SPRITES[];
extern const int ALL_SPRITES_COUNT;

#endif
