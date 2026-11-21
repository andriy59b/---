#ifndef KUBIK_TEXTURES_H
#define KUBIK_TEXTURES_H

#include "mesh.h"

/* Fills a ATLAS_SIZE x ATLAS_SIZE RGBA8888 (ABGR in memory) atlas
 * with procedurally drawn block textures. */
void textures_generate(unsigned int *atlas);

#endif
