#ifndef UC_TEXTURES_H
#define UC_TEXTURES_H

#include "mesh.h"

/* Atlas extra tiles (after the block tiles) */
#define TILE_SUN 32
#define TILE_MOON 33

/* Fills an ATLAS_SIZE x ATLAS_SIZE RGBA8888 (ABGR in memory) atlas. */
void textures_generate(unsigned int *atlas);

#define FONT_TEX_W 128
#define FONT_TEX_H 64
/* Fills a FONT_TEX_W x FONT_TEX_H RGBA8888 font texture: 16x8 cells of 8x8.
 * Glyphs: ASCII 32..95, Ukrainian capitals, a few symbols (lower case is
 * drawn as upper case). */
void font_generate(unsigned int *tex);

/* Decodes one UTF-8 code point and advances *p. */
unsigned int utf8_next(const char **p);
/* Glyph cell (0..127) for a code point; '?' for unknown characters. */
int font_glyph(unsigned int cp);

#endif
