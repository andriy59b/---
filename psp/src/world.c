#include "world.h"
#include "gen.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <dirent.h>

/* ---------------------------------------------------------------- blocks */

#define T6(a) { a, a, a, a, a, a }
#define TSIDE(side, top, bot) { side, side, top, bot, side, side }

const BlockInfo BLOCKS[B_COUNT] = {
    /* name             shape         solid opaque tiles */
    { "Air",            SHAPE_NONE,   0, 0, T6(0) },
    { "Grass",          SHAPE_CUBE,   1, 1, TSIDE(1, 0, 2) },
    { "Dirt",           SHAPE_CUBE,   1, 1, T6(2) },
    { "Stone",          SHAPE_CUBE,   1, 1, T6(3) },
    { "Sand",           SHAPE_CUBE,   1, 1, T6(4) },
    { "Log",            SHAPE_CUBE,   1, 1, TSIDE(5, 6, 6) },
    { "Leaves",         SHAPE_CUBE,   1, 0, T6(7) },
    { "Planks",         SHAPE_CUBE,   1, 1, T6(8) },
    { "Glass",          SHAPE_CUBE,   1, 0, T6(9) },
    { "Bricks",         SHAPE_CUBE,   1, 1, T6(10) },
    { "Water",          SHAPE_LIQUID, 0, 0, T6(11) },
    { "Cobblestone",    SHAPE_CUBE,   1, 1, T6(12) },
    { "Bedrock",        SHAPE_CUBE,   1, 1, T6(13) },
    { "Gravel",         SHAPE_CUBE,   1, 1, T6(14) },
    { "Snowy Grass",    SHAPE_CUBE,   1, 1, TSIDE(16, 15, 2) },
    { "Snow",           SHAPE_CUBE,   1, 1, T6(15) },
    { "Ice",            SHAPE_CUBE,   1, 1, T6(17) },
    { "Cactus",         SHAPE_CUBE,   1, 1, TSIDE(18, 19, 19) },
    { "Coal Ore",       SHAPE_CUBE,   1, 1, T6(20) },
    { "Iron Ore",       SHAPE_CUBE,   1, 1, T6(21) },
    { "Gold Ore",       SHAPE_CUBE,   1, 1, T6(22) },
    { "Crystal Ore",    SHAPE_CUBE,   1, 1, T6(23) },
    { "Tall Grass",     SHAPE_CROSS,  0, 0, T6(24) },
    { "Red Flower",     SHAPE_CROSS,  0, 0, T6(25) },
    { "Yellow Flower",  SHAPE_CROSS,  0, 0, T6(26) },
    { "Birch Log",      SHAPE_CUBE,   1, 1, TSIDE(27, 6, 6) },
    { "Birch Leaves",   SHAPE_CUBE,   1, 0, T6(28) },
    { "Sandstone",      SHAPE_CUBE,   1, 1, TSIDE(29, 30, 30) },
    { "Stone Bricks",   SHAPE_CUBE,   1, 1, T6(31) },
};

int block_solid(int b)
{
    if (b == B_UNLOADED) return 1;
    return b < B_COUNT && BLOCKS[b].solid;
}

int block_opaque(int b)
{
    if (b == B_UNLOADED) return 1;
    return b < B_COUNT && BLOCKS[b].opaque;
}

int block_shape(int b)
{
    return b < B_COUNT ? BLOCKS[b].shape : SHAPE_NONE;
}

int block_tile(int b, int face)
{
    return b < B_COUNT ? BLOCKS[b].tile[face] : 3;
}

/* ---------------------------------------------------------------- storage */

static Chunk slots[SLOTS_W * SLOTS_W];
static unsigned int g_seed;
static char g_dir[128];

static int slot_index(int cx, int cz)
{
    int sx = cx % SLOTS_W, sz = cz % SLOTS_W;
    if (sx < 0) sx += SLOTS_W;
    if (sz < 0) sz += SLOTS_W;
    return sz * SLOTS_W + sx;
}

/* Set of chunk coordinates that have a save file, so missing chunks can be
 * generated without probing the memory stick. */
#define SAVED_CAP 8192
static unsigned int saved_keys[SAVED_CAP];
static unsigned char saved_used[SAVED_CAP];
static int saved_count, saved_overflow;

static unsigned int saved_key(int cx, int cz)
{
    return (((unsigned int)cx & 0xFFFFu) << 16) | ((unsigned int)cz & 0xFFFFu);
}

static int saved_find(unsigned int k)
{
    unsigned int i = (k * 2654435761u) % SAVED_CAP;
    while (saved_used[i] && saved_keys[i] != k) i = (i + 1) % SAVED_CAP;
    return (int)i;
}

static void saved_add(int cx, int cz)
{
    unsigned int k = saved_key(cx, cz);
    int i;
    if (saved_count >= SAVED_CAP * 3 / 4) {
        saved_overflow = 1;
        return;
    }
    i = saved_find(k);
    if (!saved_used[i]) {
        saved_used[i] = 1;
        saved_keys[i] = k;
        saved_count++;
    }
}

int world_has_saved(int cx, int cz)
{
    return saved_overflow || saved_used[saved_find(saved_key(cx, cz))];
}

void world_init(unsigned int seed, const char *dir)
{
    DIR *d;
    struct dirent *e;
    int i;
    for (i = 0; i < SLOTS_W * SLOTS_W; i++) {
        free(slots[i].blocks);
        free(slots[i].solid);
        free(slots[i].water);
    }
    memset(slots, 0, sizeof(slots));
    memset(saved_used, 0, sizeof(saved_used));
    saved_count = saved_overflow = 0;
    g_seed = seed;
    strncpy(g_dir, dir, sizeof(g_dir) - 1);
    g_dir[sizeof(g_dir) - 1] = 0;
    mkdir(g_dir, 0777);

    d = opendir(g_dir);
    if (d) {
        while ((e = readdir(d)) != NULL) {
            int cx, cz;
            if (sscanf(e->d_name, "c.%d.%d", &cx, &cz) == 2) saved_add(cx, cz);
        }
        closedir(d);
    } else {
        saved_overflow = 1; /* cannot list: probe files instead */
    }
}

unsigned int world_seed(void) { return g_seed; }
const char *world_dir(void) { return g_dir; }
Chunk *world_slot(int i) { return &slots[i]; }

static void chunk_path(char *buf, size_t n, int cx, int cz)
{
    snprintf(buf, n, "%s/c.%d.%d", g_dir, cx, cz);
}

static int chunk_save(Chunk *c)
{
    char path[192];
    FILE *f;
    int ok;
    chunk_path(path, sizeof(path), c->cx, c->cz);
    f = fopen(path, "wb");
    if (!f) return 0;
    ok = fwrite(c->blocks, 1, CH_VOL, f) == CH_VOL;
    fclose(f);
    if (ok) {
        c->modified = 0;
        saved_add(c->cx, c->cz);
    }
    return ok;
}

static int chunk_read(Chunk *c)
{
    char path[192];
    FILE *f;
    int ok;
    chunk_path(path, sizeof(path), c->cx, c->cz);
    f = fopen(path, "rb");
    if (!f) return 0;
    ok = fread(c->blocks, 1, CH_VOL, f) == CH_VOL;
    fclose(f);
    return ok;
}

static void free_mesh(Chunk *c)
{
    free(c->solid);
    free(c->water);
    c->solid = c->water = NULL;
    c->nsolid = c->nwater = 0;
    c->meshed = 0;
    c->mesh_dirty = 1;
}

static void column_height(Chunk *c, int x, int z)
{
    int y;
    for (y = WH - 1; y >= 0; y--)
        if (block_opaque(c->blocks[BIDX(x, y, z)])) break;
    c->height[z * CH + x] = (unsigned char)(y + 1);
}

static void evict(Chunk *c)
{
    if (c->loaded && c->modified) chunk_save(c);
    free_mesh(c);
    c->loaded = 0;
    c->modified = 0;
}

Chunk *world_chunk(int cx, int cz)
{
    Chunk *c = &slots[slot_index(cx, cz)];
    return (c->loaded && c->cx == cx && c->cz == cz) ? c : NULL;
}

/* Frees the slot for (cx, cz) and makes sure it has block storage. */
static Chunk *claim_slot(int cx, int cz)
{
    Chunk *c = &slots[slot_index(cx, cz)];
    evict(c);
    if (!c->blocks) {
        c->blocks = (unsigned char *)malloc(CH_VOL);
        if (!c->blocks) return NULL;
    }
    c->cx = cx;
    c->cz = cz;
    return c;
}

static Chunk *finish_slot(Chunk *c)
{
    int x, z;
    for (z = 0; z < CH; z++)
        for (x = 0; x < CH; x++) column_height(c, x, z);
    c->loaded = 1;
    c->modified = 0;
    c->mesh_dirty = 1;
    c->meshed = 0;
    return c;
}

Chunk *world_load_saved(int cx, int cz)
{
    Chunk *c = world_chunk(cx, cz);
    if (c) return c;
    c = claim_slot(cx, cz);
    if (!c) return NULL;
    if (!chunk_read(c)) return NULL;
    return finish_slot(c);
}

Chunk *world_commit_chunk(int cx, int cz, const unsigned char *blocks)
{
    Chunk *c = world_chunk(cx, cz);
    if (c) return c;
    c = claim_slot(cx, cz);
    if (!c) return NULL;
    memcpy(c->blocks, blocks, CH_VOL);
    return finish_slot(c);
}

Chunk *world_load_chunk(int cx, int cz)
{
    static unsigned char tmp[CH_VOL];
    Chunk *c = world_chunk(cx, cz);
    if (c) return c;
    if (world_has_saved(cx, cz) && (c = world_load_saved(cx, cz)) != NULL) return c;
    gen_chunk(g_seed, cx, cz, tmp);
    return world_commit_chunk(cx, cz, tmp);
}

int world_neighbors_loaded(const Chunk *c)
{
    int dx, dz;
    for (dz = -1; dz <= 1; dz++)
        for (dx = -1; dx <= 1; dx++)
            if (!world_chunk(c->cx + dx, c->cz + dz)) return 0;
    return 1;
}

void world_free_far_meshes(int pcx, int pcz, int radius)
{
    int i;
    for (i = 0; i < SLOTS_W * SLOTS_W; i++) {
        Chunk *c = &slots[i];
        if (!c->loaded || !c->meshed) continue;
        if (abs(c->cx - pcx) > radius || abs(c->cz - pcz) > radius) free_mesh(c);
    }
}

int world_save_all(void)
{
    int i, ok = 1;
    for (i = 0; i < SLOTS_W * SLOTS_W; i++)
        if (slots[i].loaded && slots[i].modified) ok &= chunk_save(&slots[i]);
    return ok;
}

void world_shutdown(void)
{
    int i;
    world_save_all();
    for (i = 0; i < SLOTS_W * SLOTS_W; i++) {
        free_mesh(&slots[i]);
        free(slots[i].blocks);
        slots[i].blocks = NULL;
        slots[i].loaded = 0;
    }
}

/* ----------------------------------------------------------------- access */

int world_get(int x, int y, int z)
{
    Chunk *c;
    if (y < 0) return B_BEDROCK;
    if (y >= WH) return B_AIR;
    c = world_chunk(x >> 4, z >> 4);
    if (!c) return B_UNLOADED;
    return c->blocks[BIDX(x & 15, y, z & 15)];
}

void world_set(int x, int y, int z, int b)
{
    int cx = x >> 4, cz = z >> 4, lx = x & 15, lz = z & 15, dx, dz;
    Chunk *c;
    if (y < 0 || y >= WH) return;
    c = world_chunk(cx, cz);
    if (!c) return;
    c->blocks[BIDX(lx, y, lz)] = (unsigned char)b;
    c->modified = 1;
    column_height(c, lx, lz);

    /* neighbours sample this block for faces, light and ambient occlusion */
    for (dz = -1; dz <= 1; dz++)
        for (dx = -1; dx <= 1; dx++) {
            Chunk *n;
            if ((dx == -1 && lx != 0) || (dx == 1 && lx != CH - 1)) continue;
            if ((dz == -1 && lz != 0) || (dz == 1 && lz != CH - 1)) continue;
            n = world_chunk(cx + dx, cz + dz);
            if (n) n->mesh_dirty = 1;
        }
}

int world_top(int x, int z)
{
    int y;
    for (y = WH - 1; y >= 0; y--) {
        int b = world_get(x, y, z);
        if (b == B_UNLOADED) return -1;
        if (block_solid(b)) return y;
    }
    return -1;
}

/* ---------------------------------------------------------------- raycast */

int world_raycast(const float o[3], const float d[3], float maxd, int hit[3], int prev[3])
{
    int cell[3], step[3], i;
    float tmax[3], tdelta[3], t = 0.0f;

    for (i = 0; i < 3; i++) {
        cell[i] = (int)floorf(o[i]);
        prev[i] = cell[i];
        if (d[i] > 0.0f) {
            step[i] = 1;
            tdelta[i] = 1.0f / d[i];
            tmax[i] = (cell[i] + 1.0f - o[i]) / d[i];
        } else if (d[i] < 0.0f) {
            step[i] = -1;
            tdelta[i] = -1.0f / d[i];
            tmax[i] = (o[i] - cell[i]) / -d[i];
        } else {
            step[i] = 0;
            tdelta[i] = 1e30f;
            tmax[i] = 1e30f;
        }
    }

    while (t <= maxd) {
        int b;
        if (cell[1] < 0 || cell[1] >= WH) {
            if ((cell[1] < 0 && step[1] <= 0) || (cell[1] >= WH && step[1] >= 0)) return 0;
        } else {
            b = world_get(cell[0], cell[1], cell[2]);
            if (b == B_UNLOADED) return 0;
            if (b != B_AIR && b != B_WATER) {
                for (i = 0; i < 3; i++) hit[i] = cell[i];
                return 1;
            }
        }
        for (i = 0; i < 3; i++) prev[i] = cell[i];
        i = (tmax[0] < tmax[1]) ? (tmax[0] < tmax[2] ? 0 : 2) : (tmax[1] < tmax[2] ? 1 : 2);
        t = tmax[i];
        tmax[i] += tdelta[i];
        cell[i] += step[i];
    }
    return 0;
}

/* ---------------------------------------------------------------- physics */

static int box_collides(const float pos[3])
{
    int x0 = (int)floorf(pos[0] - PLAYER_HALF_W), x1 = (int)floorf(pos[0] + PLAYER_HALF_W);
    int y0 = (int)floorf(pos[1]),                 y1 = (int)floorf(pos[1] + PLAYER_H);
    int z0 = (int)floorf(pos[2] - PLAYER_HALF_W), z1 = (int)floorf(pos[2] + PLAYER_HALF_W);
    int x, y, z;

    for (y = y0; y <= y1; y++)
        for (z = z0; z <= z1; z++)
            for (x = x0; x <= x1; x++)
                if (block_solid(world_get(x, y, z))) return 1;
    return 0;
}

int player_intersects_block(const Player *p, int bx, int by, int bz)
{
    return p->pos[0] + PLAYER_HALF_W > bx && p->pos[0] - PLAYER_HALF_W < bx + 1 &&
           p->pos[1] + PLAYER_H > by && p->pos[1] < by + 1 &&
           p->pos[2] + PLAYER_HALF_W > bz && p->pos[2] - PLAYER_HALF_W < bz + 1;
}

/* Move along one axis in small steps, stopping at the first collision. */
static int move_axis(Player *p, int axis, float delta)
{
    const float step = 0.05f;
    while (delta != 0.0f) {
        float s = delta > step ? step : (delta < -step ? -step : delta);
        float old = p->pos[axis];
        p->pos[axis] += s;
        if (box_collides(p->pos)) {
            p->pos[axis] = old;
            return 1;
        }
        delta -= s;
    }
    return 0;
}

void player_update(Player *p, float move_x, float move_z, int up, float dt)
{
    int feet = world_get((int)floorf(p->pos[0]), (int)floorf(p->pos[1] + 0.4f), (int)floorf(p->pos[2]));
    p->in_water = feet == B_WATER;

    p->vel[0] = move_x;
    p->vel[2] = move_z;

    if (p->flying) {
        p->vel[1] = up * 7.0f;
    } else if (p->in_water) {
        p->vel[0] *= 0.6f;
        p->vel[2] *= 0.6f;
        p->vel[1] -= 6.0f * dt;
        if (up > 0) p->vel[1] = 3.2f;
        if (p->vel[1] < -3.0f) p->vel[1] = -3.0f;
    } else {
        p->vel[1] -= 28.0f * dt;
        if (up > 0 && p->on_ground) p->vel[1] = 8.6f;
        if (p->vel[1] < -45.0f) p->vel[1] = -45.0f;
    }

    move_axis(p, 0, p->vel[0] * dt);
    move_axis(p, 2, p->vel[2] * dt);
    p->on_ground = 0;
    if (move_axis(p, 1, p->vel[1] * dt)) {
        if (p->vel[1] < 0.0f) p->on_ground = 1;
        p->vel[1] = 0.0f;
    }

    if (p->pos[1] < -20.0f) {
        int top = world_top((int)floorf(p->pos[0]), (int)floorf(p->pos[2]));
        p->pos[1] = (float)(top >= 0 ? top + 1 : WH);
        p->vel[1] = 0.0f;
    }
}
