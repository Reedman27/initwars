/* view.h - draws the current game state into a 320x240 Frame. */
#ifndef VIEW_H
#define VIEW_H

#include "game.h"
#include "render.h"

void view_draw(const Game *g, Frame *f);

#endif
