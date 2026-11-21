/* Kubik — voxel sandbox for PSP. World data, generation, raycast, physics. */
#ifndef KUBIK_WORLD_H
#define KUBIK_WORLD_H

#define WX 64
#define WY 32
#define WZ 64
#define CHUNK 16
#define NCX (WX / CHUNK)
#define NCZ (WZ / CHUNK)
#define WATER_LEVEL 11

enum {
    B_AIR = 0,
    B_GRASS,
    B_DIRT,
    B_STONE,
    B_SAND,
    B_WOOD,
    B_LEAVES,
    B_PLANKS,
    B_GLASS,
    B_BRICK,
    B_WATER,
    B_COUNT
};

/* Face indices: +X, -X, +Y (top), -Y (bottom), +Z, -Z */
enum { F_PX = 0, F_NX, F_PY, F_NY, F_PZ, F_NZ };

int block_solid(int b);   /* collides with the player */
int block_opaque(int b);  /* hides neighbouring faces */
int block_tile(int b, int face); /* index into the texture atlas */

int  world_get(int x, int y, int z);
void world_set(int x, int y, int z, int b);
void world_generate(unsigned int seed);
int  world_surface(int x, int z); /* y of the highest solid block, -1 if none */
int  world_save(const char *path);
int  world_load(const char *path);

/* DDA raycast. Returns 1 when a non-air, non-water block is hit within maxd.
 * hit = block coordinates, prev = empty cell in front of the hit face. */
int world_raycast(const float o[3], const float d[3], float maxd, int hit[3], int prev[3]);

typedef struct {
    float pos[3];   /* feet position (centre of the AABB bottom) */
    float vel[3];
    int on_ground;
    int flying;
} Player;

#define PLAYER_HALF_W 0.3f
#define PLAYER_H 1.8f
#define PLAYER_EYE 1.62f

/* move[] is the wanted horizontal velocity (x, z); up: 1 jump / fly up, -1 fly down */
void player_update(Player *p, float move_x, float move_z, int up, float dt);
int  player_intersects_block(const Player *p, int bx, int by, int bz);

#endif
