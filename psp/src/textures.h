#ifndef KUBIK_TEXTURES_H
#define KUBIK_TEXTURES_H

#include "mesh.h"

/* Atlas extra tiles (after the block tiles) */
#define TILE_SUN 32
#define TILE_MOON 33

/* Fills an ATLAS_SIZE x ATLAS_SIZE RGBA8888 (ABGR in memory) atlas. */
void textures_generate(unsigned int *atlas);

#define FONT_TEX_W 128
#define FONT_TEX_H 32
/* Fills a FONT_TEX_W x FONT_TEX_H RGBA8888 font texture: 16x4 cells of 8x8
 * for ASCII 32..95 (lower case is drawn as upper case). */
void font_generate(unsigned int *tex);

#endif
