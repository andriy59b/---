#ifndef KUBIK_MESH_H
#define KUBIK_MESH_H

#include "world.h"

#define ATLAS_SIZE 256
#define ATLAS_TILES 8
#define TILE_PX (ATLAS_SIZE / ATLAS_TILES)

/* Builds the solid and water meshes of a chunk. All 8 neighbours must be
 * loaded (see world_neighbors_loaded). Returns 0 when out of memory. */
int mesh_build(Chunk *c);

#endif
