#ifndef UC_MESH_H
#define UC_MESH_H

#include "world.h"

#define ATLAS_SIZE 256
#define ATLAS_TILES 8
#define TILE_PX (ATLAS_SIZE / ATLAS_TILES)

/* Builds the solid and water meshes of a chunk. All 8 neighbours must be
 * loaded (see world_neighbors_loaded). Returns 0 when out of memory. */
int mesh_build(Chunk *c);

/* The same in three steps, so the heavy part can run on a worker thread:
 * mesh_gather  (main thread) snapshots the chunk + border, clears mesh_dirty;
 * mesh_compute (any thread)  builds the vertices into internal scratch buffers;
 * mesh_commit  (main thread) copies them into the chunk.
 * Only one gather/compute/commit sequence may be in flight at a time. */
void mesh_gather(Chunk *c);
void mesh_compute(void);
int  mesh_commit(Chunk *c);

#endif
