#include "world.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

static unsigned char blocks[WX * WY * WZ];

#define IDX(x, y, z) (((y) * WZ + (z)) * WX + (x))

/* ---------------------------------------------------------------- blocks */

int block_solid(int b)
{
    return b != B_AIR && b != B_WATER;
}

int block_opaque(int b)
{
    return b != B_AIR && b != B_WATER && b != B_LEAVES && b != B_GLASS;
}

/* Atlas layout (4x4 tiles):
 * 0 grass top, 1 grass side, 2 dirt, 3 stone, 4 sand, 5 wood side,
 * 6 wood top, 7 leaves, 8 planks, 9 glass, 10 brick, 11 water */
int block_tile(int b, int face)
{
    switch (b) {
    case B_GRASS:
        if (face == F_PY) return 0;
        if (face == F_NY) return 2;
        return 1;
    case B_DIRT:   return 2;
    case B_STONE:  return 3;
    case B_SAND:   return 4;
    case B_WOOD:   return (face == F_PY || face == F_NY) ? 6 : 5;
    case B_LEAVES: return 7;
    case B_PLANKS: return 8;
    case B_GLASS:  return 9;
    case B_BRICK:  return 10;
    case B_WATER:  return 11;
    default:       return 3;
    }
}

/* ----------------------------------------------------------------- access */

int world_get(int x, int y, int z)
{
    if (y < 0) return B_STONE;
    if (y >= WY || x < 0 || x >= WX || z < 0 || z >= WZ) return B_AIR;
    return blocks[IDX(x, y, z)];
}

void world_set(int x, int y, int z, int b)
{
    if (y < 0 || y >= WY || x < 0 || x >= WX || z < 0 || z >= WZ) return;
    blocks[IDX(x, y, z)] = (unsigned char)b;
}

int world_surface(int x, int z)
{
    int y;
    for (y = WY - 1; y >= 0; y--)
        if (block_solid(world_get(x, y, z))) return y;
    return -1;
}

/* ------------------------------------------------------------- generation */

static unsigned int g_seed;

static unsigned int hash3(int x, int y, int z)
{
    unsigned int h = g_seed ^ 0x9E3779B9u;
    h ^= (unsigned int)x * 0x85EBCA6Bu;
    h = (h << 13) | (h >> 19);
    h ^= (unsigned int)y * 0xC2B2AE35u;
    h = (h << 17) | (h >> 15);
    h ^= (unsigned int)z * 0x27D4EB2Fu;
    h ^= h >> 16;
    h *= 0x7FEB352Du;
    h ^= h >> 15;
    h *= 0x846CA68Bu;
    h ^= h >> 16;
    return h;
}

static float rnd2(int x, int z)
{
    return (hash3(x, 0, z) & 0xFFFF) / 65535.0f;
}

static float smooth(float t)
{
    return t * t * (3.0f - 2.0f * t);
}

static float value_noise(float x, float z)
{
    int ix = (int)floorf(x), iz = (int)floorf(z);
    float fx = smooth(x - ix), fz = smooth(z - iz);
    float a = rnd2(ix, iz), b = rnd2(ix + 1, iz);
    float c = rnd2(ix, iz + 1), d = rnd2(ix + 1, iz + 1);
    float top = a + (b - a) * fx;
    float bot = c + (d - c) * fx;
    return top + (bot - top) * fz;
}

static float fbm(float x, float z)
{
    float sum = 0.0f, amp = 0.5f, freq = 1.0f / 24.0f;
    int i;
    for (i = 0; i < 4; i++) {
        sum += value_noise(x * freq, z * freq) * amp;
        amp *= 0.5f;
        freq *= 2.0f;
    }
    return sum / 0.9375f; /* normalise to 0..1 */
}

static void place_tree(int x, int y, int z)
{
    int h = 4 + (int)(hash3(x, 7, z) % 2u);
    int dx, dy, dz, i;

    for (dy = h - 2; dy <= h + 1; dy++) {
        int r = (dy >= h) ? 1 : 2;
        for (dx = -r; dx <= r; dx++)
            for (dz = -r; dz <= r; dz++) {
                if (r == 2 && (dx == -2 || dx == 2) && (dz == -2 || dz == 2)) continue;
                if (world_get(x + dx, y + dy, z + dz) == B_AIR)
                    world_set(x + dx, y + dy, z + dz, B_LEAVES);
            }
    }
    for (i = 0; i < h; i++) world_set(x, y + i, z, B_WOOD);
}

void world_generate(unsigned int seed)
{
    int x, y, z;
    g_seed = seed;
    memset(blocks, 0, sizeof(blocks));

    for (z = 0; z < WZ; z++)
        for (x = 0; x < WX; x++) {
            int h = 6 + (int)(fbm((float)x, (float)z) * 16.0f);
            if (h > WY - 8) h = WY - 8;
            for (y = 0; y <= h; y++) {
                int b;
                if (y < h - 3) b = B_STONE;
                else if (h <= WATER_LEVEL + 1) b = B_SAND;
                else if (y == h) b = B_GRASS;
                else b = B_DIRT;
                world_set(x, y, z, b);
            }
            for (y = h + 1; y <= WATER_LEVEL; y++) world_set(x, y, z, B_WATER);
        }

    for (z = 3; z < WZ - 3; z++)
        for (x = 3; x < WX - 3; x++) {
            int s = world_surface(x, z);
            if (s >= 0 && world_get(x, s, z) == B_GRASS && hash3(x, 1, z) % 70u == 0)
                place_tree(x, s + 1, z);
        }
}

/* ------------------------------------------------------------ persistence */

static const char MAGIC[8] = { 'K', 'U', 'B', 'I', 'K', 'W', '0', '1' };

int world_save(const char *path)
{
    FILE *f = fopen(path, "wb");
    int ok;
    if (!f) return 0;
    ok = fwrite(MAGIC, 1, sizeof(MAGIC), f) == sizeof(MAGIC) &&
         fwrite(blocks, 1, sizeof(blocks), f) == sizeof(blocks);
    fclose(f);
    return ok;
}

int world_load(const char *path)
{
    char magic[8];
    FILE *f = fopen(path, "rb");
    int ok;
    if (!f) return 0;
    ok = fread(magic, 1, sizeof(magic), f) == sizeof(magic) &&
         memcmp(magic, MAGIC, sizeof(MAGIC)) == 0 &&
         fread(blocks, 1, sizeof(blocks), f) == sizeof(blocks);
    fclose(f);
    return ok;
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
        int b = world_get(cell[0], cell[1], cell[2]);
        if (b != B_AIR && b != B_WATER && cell[1] >= 0) {
            for (i = 0; i < 3; i++) hit[i] = cell[i];
            return 1;
        }
        for (i = 0; i < 3; i++) prev[i] = cell[i];
        /* advance along the axis with the nearest boundary */
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

    /* the world edge acts as a wall */
    if (pos[0] - PLAYER_HALF_W < 0.0f || pos[0] + PLAYER_HALF_W > WX ||
        pos[2] - PLAYER_HALF_W < 0.0f || pos[2] + PLAYER_HALF_W > WZ)
        return 1;

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
    float step = 0.05f;
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
    int in_water = world_get((int)floorf(p->pos[0]), (int)floorf(p->pos[1] + 0.5f),
                             (int)floorf(p->pos[2])) == B_WATER;

    p->vel[0] = move_x;
    p->vel[2] = move_z;

    if (p->flying) {
        p->vel[1] = up * 6.0f;
    } else if (in_water) {
        p->vel[1] -= 6.0f * dt;
        if (up > 0) p->vel[1] = 3.0f;
        if (p->vel[1] < -3.0f) p->vel[1] = -3.0f;
    } else {
        p->vel[1] -= 25.0f * dt;
        if (up > 0 && p->on_ground) p->vel[1] = 8.2f;
        if (p->vel[1] < -40.0f) p->vel[1] = -40.0f;
    }

    move_axis(p, 0, p->vel[0] * dt);
    move_axis(p, 2, p->vel[2] * dt);
    p->on_ground = 0;
    if (move_axis(p, 1, p->vel[1] * dt)) {
        if (p->vel[1] < 0.0f) p->on_ground = 1;
        p->vel[1] = 0.0f;
    }

    /* fell out of the world: put the player back on top */
    if (p->pos[1] < -10.0f) {
        p->pos[1] = (float)(world_surface((int)p->pos[0], (int)p->pos[2]) + 1);
        p->vel[1] = 0.0f;
    }
}
