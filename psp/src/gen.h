#ifndef KUBIK_GEN_H
#define KUBIK_GEN_H

enum {
    BIOME_OCEAN = 0,
    BIOME_BEACH,
    BIOME_PLAINS,
    BIOME_FOREST,
    BIOME_DESERT,
    BIOME_SNOW,
    BIOME_MOUNTAINS,
    BIOME_COUNT
};

/* Terrain height (y of the top block) and biome of a world column. */
int gen_column(unsigned int seed, int x, int z, int *biome);
const char *biome_name(int biome);

/* Fills a chunk's CH_VOL block array. Deterministic for (seed, cx, cz). */
void gen_chunk(unsigned int seed, int cx, int cz, unsigned char *blocks);

#endif
