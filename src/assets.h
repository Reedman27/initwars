/* assets.h - optional external asset lookup.
 *
 * The game ships with no required external files: sprites, font and sounds are
 * generated in code. Optional overrides (currently sfx/<name>.wav) are looked
 * up in this order:
 *   1. $INITWARS_ASSETS
 *   2. ./assets                      (development tree)
 *   3. <directory of the executable>/assets
 *   4. <directory of the executable>/../share/init-wars
 *   5. INITWARS_DATADIR              (compile-time, default /usr/local/share/init-wars)
 *   6. /usr/share/init-wars
 */
#ifndef ASSETS_H
#define ASSETS_H

#include <stdbool.h>
#include <stddef.h>

void assets_init(const char *argv0);
/* Writes the first existing path for 'rel' into out; false if none found. */
bool assets_find(const char *rel, char *out, size_t outsz);

#endif
