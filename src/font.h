/* font.h - built-in 5x7 pixel font (public-domain style ASCII glyph table) */
#ifndef FONT_HEADER_H
#define FONT_HEADER_H

#include <stdint.h>

#define FONT_W 6   /* advance in pixels (5 glyph + 1 spacing) */
#define FONT_H 8   /* line height in pixels (7 glyph + 1 spacing) */

/* Returns 5 column bytes (bit0 = top row) for an ASCII char; blank if unknown. */
const uint8_t *font_glyph(int ch);

#endif
