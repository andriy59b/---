/* Kubik — voxel sandbox for PSP. Streaming chunked world, raycast, physics. */
#ifndef KUBIK_WORLD_H
#define KUBIK_WORLD_H

#define CH 16                 /* chunk width (x and z) */
#define WH 64                 /* world height */
#define CH_VOL (CH * CH * WH)
#define SEA_LEVEL 28
#define MAX_RADIUS 8          /* max render distance in chunks */
#define SLOTS_W (2 * (MAX_RADIUS + 2) + 1)
#define B_UNLOADED 255

enum {
    B_AIR = 0,
    B_GRASS,
    B_DIRT,
    B_STONE,
    B_SAND,
    B_LOG,
    B_LEAVES,
    B_PLANKS,
    B_GLASS,
    B_BRICK,
    B_WATER,
    B_COBBLE,
    B_BEDROCK,
    B_GRAVEL,
    B_SNOW_GRASS,
    B_SNOW,
    B_ICE,
    B_CACTUS,
    B_COAL_ORE,
    B_IRON_ORE,
    B_GOLD_ORE,
    B_CRYSTAL_ORE,
    B_TALL_GRASS,
    B_FLOWER_RED,
    B_FLOWER_YELLOW,
    B_BIRCH_LOG,
    B_BIRCH_LEAVES,
    B_SANDSTONE,
    B_STONE_BRICKS,
    B_COUNT
};

/* Face indices: +X, -X, +Y (top), -Y (bottom), +Z, -Z */
enum { F_PX = 0, F_NX, F_PY, F_NY, F_PZ, F_NZ };

enum { SHAPE_NONE = 0, SHAPE_CUBE, SHAPE_CROSS, SHAPE_LIQUID };

typedef struct {
    const char *name;
    unsigned char shape;
    unsigned char solid;   /* collides with the player */
    unsigned char opaque;  /* hides neighbouring faces, blocks sky light */
    unsigned char tile[6]; /* atlas tile per face */
} BlockInfo;

extern const BlockInfo BLOCKS[B_COUNT];

int block_solid(int b);
int block_opaque(int b);
int block_shape(int b);
int block_tile(int b, int face);

/* Compact vertex: GU_TEXTURE_16BIT | GU_COLOR_8888 | GU_VERTEX_16BIT.
 * Positions are chunk-local * 256, UVs are atlas-normalised * 16384. */
typedef struct {
    unsigned short u, v;
    unsigned int color;
    short x, y, z, pad;
} Vertex;

#define VPOS_SCALE 256
#define VUV_ONE 16384

typedef struct {
    int cx, cz;
    unsigned char loaded;
    unsigned char modified;   /* differs from generated terrain: save it */
    unsigned char mesh_dirty;
    unsigned char meshed;
    unsigned char *blocks;    /* CH_VOL bytes, see BIDX */
    unsigned char height[CH * CH]; /* first y open to the sky per column */
    Vertex *solid;            /* 4 vertices per quad */
    int nsolid;               /* quads */
    Vertex *water;
    int nwater;
} Chunk;

#define BIDX(x, y, z) ((((y) * CH) + (z)) * CH + (x))

void world_init(unsigned int seed, const char *dir);
unsigned int world_seed(void);
const char *world_dir(void);
void world_shutdown(void);           /* saves modified chunks, frees memory */
int  world_save_all(void);

Chunk *world_chunk(int cx, int cz);       /* loaded chunk or NULL */
Chunk *world_load_chunk(int cx, int cz);  /* load from disk or generate; NULL on OOM */
int  world_neighbors_loaded(const Chunk *c);
void world_free_far_meshes(int pcx, int pcz, int radius);
Chunk *world_slot(int i);                 /* 0 .. SLOTS_W*SLOTS_W-1 */

int  world_get(int x, int y, int z);      /* B_UNLOADED outside loaded chunks */
void world_set(int x, int y, int z, int b);
int  world_top(int x, int z);             /* y of highest solid block, -1 if none/unloaded */

/* DDA raycast. Returns 1 when a block (not air/water) is hit within maxd.
 * hit = block coordinates, prev = cell in front of the hit face. */
int world_raycast(const float o[3], const float d[3], float maxd, int hit[3], int prev[3]);

typedef struct {
    float pos[3];   /* feet position (centre of the AABB bottom) */
    float vel[3];
    int on_ground;
    int flying;
    int in_water;
} Player;

#define PLAYER_HALF_W 0.3f
#define PLAYER_H 1.8f
#define PLAYER_EYE 1.62f

/* move_x/z: wanted horizontal velocity; up: 1 jump / fly up, -1 fly down */
void player_update(Player *p, float move_x, float move_z, int up, float dt);
int  player_intersects_block(const Player *p, int bx, int by, int bz);

static inline int floor_div(int a, int b) { return (a >= 0) ? a / b : -((-a + b - 1) / b); }

#endif
