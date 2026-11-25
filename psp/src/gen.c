#include "gen.h"
#include "world.h"
#include "town.h"

#include <math.h>
#include <string.h>

#ifndef CAVE_R2
#define CAVE_R2 0.0016f /* squared radius of the cave tunnels in noise space */
#endif

/* ------------------------------------------------------------------ noise */

static unsigned int hash3(unsigned int seed, int x, int y, int z)
{
    unsigned int h = seed * 0x9E3779B9u;
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

static float lat2(unsigned int seed, int x, int z)
{
    return (hash3(seed, x, 0, z) & 0xFFFFFF) * (1.0f / 16777215.0f);
}

static float lat3(unsigned int seed, int x, int y, int z)
{
    return (hash3(seed, x, y, z) & 0xFFFFFF) * (1.0f / 16777215.0f);
}

static float smooth(float t) { return t * t * (3.0f - 2.0f * t); }
static float lerp(float a, float b, float t) { return a + (b - a) * t; }

static float noise2(unsigned int seed, float x, float z)
{
    int ix = (int)floorf(x), iz = (int)floorf(z);
    float fx = smooth(x - ix), fz = smooth(z - iz);
    return lerp(lerp(lat2(seed, ix, iz), lat2(seed, ix + 1, iz), fx),
                lerp(lat2(seed, ix, iz + 1), lat2(seed, ix + 1, iz + 1), fx), fz);
}

static float noise3(unsigned int seed, float x, float y, float z)
{
    int ix = (int)floorf(x), iy = (int)floorf(y), iz = (int)floorf(z);
    float fx = smooth(x - ix), fy = smooth(y - iy), fz = smooth(z - iz);
    float a = lerp(lerp(lat3(seed, ix, iy, iz), lat3(seed, ix + 1, iy, iz), fx),
                   lerp(lat3(seed, ix, iy, iz + 1), lat3(seed, ix + 1, iy, iz + 1), fx), fz);
    float b = lerp(lerp(lat3(seed, ix, iy + 1, iz), lat3(seed, ix + 1, iy + 1, iz), fx),
                   lerp(lat3(seed, ix, iy + 1, iz + 1), lat3(seed, ix + 1, iy + 1, iz + 1), fx), fz);
    return lerp(a, b, fy);
}

static float fbm2(unsigned int seed, float x, float z, int octaves)
{
    float sum = 0.0f, amp = 0.5f, norm = 0.0f;
    int i;
    for (i = 0; i < octaves; i++) {
        sum += noise2(seed + (unsigned int)i * 101u, x, z) * amp;
        norm += amp;
        amp *= 0.5f;
        x *= 2.0f;
        z *= 2.0f;
    }
    return sum / norm;
}

static float smoothstep(float a, float b, float x)
{
    float t = (x - a) / (b - a);
    if (t < 0.0f) t = 0.0f;
    if (t > 1.0f) t = 1.0f;
    return smooth(t);
}

/* ----------------------------------------------------------------- terrain */

static const char *BIOME_NAMES[BIOME_COUNT] = {
    "Озеро", "Берег", "Степ", "Ліс", "Піски", "Сніги", "Гори", "Соняшниківськ",
};

const char *biome_name(int biome)
{
    return (biome >= 0 && biome < BIOME_COUNT) ? BIOME_NAMES[biome] : "?";
}

int gen_column(unsigned int seed, int x, int z, int *biome)
{
    float fx = (float)x, fz = (float)z;
    float cont = fbm2(seed + 11u, fx / 192.0f, fz / 192.0f, 3);
    float hills = fbm2(seed + 23u, fx / 48.0f, fz / 48.0f, 3);
    float mnt = fbm2(seed + 37u, fx / 128.0f, fz / 128.0f, 2);
    float ridge = fbm2(seed + 41u, fx / 20.0f, fz / 20.0f, 2);
    float temp = fbm2(seed + 53u, fx / 256.0f, fz / 256.0f, 2);
    float hum = fbm2(seed + 67u, fx / 224.0f, fz / 224.0f, 2);
    float m = smoothstep(0.55f, 0.72f, mnt);
    float h = 15.5f + cont * 34.0f + (hills - 0.5f) * 12.0f;
    int hi, b;

    h += m * (12.0f + ridge * 18.0f) * smoothstep(0.35f, 0.5f, cont);
    hi = (int)h;
    if (hi < 4) hi = 4;
    if (hi > WH - 6) hi = WH - 6;

    if (hi >= 44) b = BIOME_MOUNTAINS;
    else if (temp < 0.37f) b = BIOME_SNOW;
    else if (hi < SEA_LEVEL - 1) b = BIOME_OCEAN;
    else if (hi <= SEA_LEVEL) b = BIOME_BEACH;
    else if (temp > 0.58f && hum < 0.5f) b = BIOME_DESERT;
    else if (hum > 0.53f) b = BIOME_FOREST;
    else b = BIOME_PLAINS;

    {
        int d = town_distance(x, z);
        if (d == 0) {
            if (biome) *biome = BIOME_TOWN;
            return TOWN_Y;
        }
        if (d < 64) {
            float t = smoothstep(0.0f, 64.0f, (float)d);
            hi = (int)(TOWN_Y + (hi - TOWN_Y) * t + 0.5f);
            if (b == BIOME_SNOW || b == BIOME_DESERT || b == BIOME_MOUNTAINS) b = BIOME_PLAINS;
            if (hi >= SEA_LEVEL + 1 && (b == BIOME_OCEAN || b == BIOME_BEACH)) b = BIOME_PLAINS;
        } else if (d < 220 && (b == BIOME_SNOW || b == BIOME_DESERT)) {
            b = BIOME_PLAINS; /* temperate surroundings */
        }
    }

    if (biome) *biome = b;
    return hi;
}

/* ------------------------------------------------------------------ chunk */

typedef struct {
    unsigned char *b;
    unsigned int rng;
} Ctx;

static unsigned int rnd(Ctx *c)
{
    c->rng ^= c->rng << 13;
    c->rng ^= c->rng >> 17;
    c->rng ^= c->rng << 5;
    return c->rng;
}

/* Places a block into an empty cell of this chunk; out-of-chunk cells are skipped. */
static void put(unsigned char *b, int x, int y, int z, int blk)
{
    unsigned char *cell;
    if (x < 0 || x >= CH || z < 0 || z >= CH || y < 0 || y >= WH) return;
    cell = &b[BIDX(x, y, z)];
    if (*cell == B_AIR) *cell = (unsigned char)blk;
}

static void trunk(unsigned char *b, int x, int y, int z, int h, int log)
{
    int i;
    for (i = 0; i < h; i++) {
        if (x < 0 || x >= CH || z < 0 || z >= CH || y + i >= WH) continue;
        b[BIDX(x, y + i, z)] = (unsigned char)log;
    }
}

static void tree_round(unsigned char *b, int x, int y, int z, int h, int log, int leaves, unsigned int hs)
{
    int top = y + h - 1, dx, dz, yy;
    for (yy = top - 2; yy <= top + 1; yy++) {
        int r = (yy <= top - 1) ? 2 : 1;
        for (dz = -r; dz <= r; dz++)
            for (dx = -r; dx <= r; dx++) {
                int corner = (dx == -r || dx == r) && (dz == -r || dz == r);
                if (corner && (r == 1 ? yy == top + 1 : ((hs >> ((dx + 2) * 5 + dz + 2)) & 1)))
                    continue;
                put(b, x + dx, yy, z + dz, leaves);
            }
    }
    trunk(b, x, y, z, h, log);
}

static void tree_spruce(unsigned char *b, int x, int y, int z, int h)
{
    int top = y + h, yy, dx, dz;
    put(b, x, top, z, B_LEAVES);
    put(b, x, top + 1, z, B_LEAVES);
    for (yy = y + 2; yy < top; yy++) {
        int r = ((top - yy) % 2 == 1) ? 1 : 2;
        if (top - yy <= 1) r = 1;
        if (yy == y + 2) r = 2;
        for (dz = -r; dz <= r; dz++)
            for (dx = -r; dx <= r; dx++) {
                if (r == 2 && (dx == -2 || dx == 2) && (dz == -2 || dz == 2)) continue;
                put(b, x + dx, yy, z + dz, B_LEAVES);
            }
    }
    trunk(b, x, y, z, h, B_LOG);
}

static void vein(Ctx *c, int ore, int count, int maxy, int size)
{
    int i, j;
    for (i = 0; i < count; i++) {
        int x = (int)(rnd(c) % CH), z = (int)(rnd(c) % CH);
        int y = 3 + (int)(rnd(c) % (unsigned int)(maxy - 3));
        int n = size / 2 + (int)(rnd(c) % (unsigned int)size);
        for (j = 0; j < n; j++) {
            unsigned int r;
            if (x >= 0 && x < CH && z >= 0 && z < CH && y > 0 && y < WH &&
                c->b[BIDX(x, y, z)] == B_STONE)
                c->b[BIDX(x, y, z)] = (unsigned char)ore;
            r = rnd(c) % 6u;
            if (r == 0) x++; else if (r == 1) x--;
            else if (r == 2) y++; else if (r == 3) y--;
            else if (r == 4) z++; else z--;
        }
    }
}

#define PAD 3
#define CW (CH + 2 * PAD)

void gen_chunk(unsigned int seed, int cx, int cz, unsigned char *b)
{
    static unsigned char col_h[CW][CW], col_b[CW][CW];
    static float cave1[17][5][5], cave2[17][5][5];
    int x0 = cx * CH, z0 = cz * CH;
    int x, y, z, i;
    Ctx ctx;

    memset(b, 0, CH_VOL);

    for (z = 0; z < CW; z++)
        for (x = 0; x < CW; x++) {
            int bio;
            col_h[z][x] = (unsigned char)gen_column(seed, x0 + x - PAD, z0 + z - PAD, &bio);
            col_b[z][x] = (unsigned char)bio;
        }

    /* ---- base terrain ---- */
    for (z = 0; z < CH; z++)
        for (x = 0; x < CH; x++) {
            int h = col_h[z + PAD][x + PAD], bio = col_b[z + PAD][x + PAD];
            int wx = x0 + x, wz = z0 + z;
            for (y = 0; y <= h; y++) {
                int blk, d = h - y;
                if (y == 0 || (y <= 2 && (hash3(seed, wx, y, wz) % (unsigned)(y + 1)) == 0)) {
                    blk = B_BEDROCK;
                } else switch (bio) {
                case BIOME_OCEAN:
                    blk = d < 3 ? ((hash3(seed + 5u, wx >> 2, 0, wz >> 2) & 3u) == 0 ? B_GRAVEL : B_SAND) : B_STONE;
                    break;
                case BIOME_BEACH:
                    blk = d < 4 ? B_SAND : (d < 6 ? B_SANDSTONE : B_STONE);
                    break;
                case BIOME_DESERT:
                    blk = d < 4 ? B_SAND : (d < 8 ? B_SANDSTONE : B_STONE);
                    break;
                case BIOME_SNOW:
                    blk = d == 0 ? (h >= SEA_LEVEL ? B_SNOW_GRASS : B_DIRT) : (d < 4 ? B_DIRT : B_STONE);
                    break;
                case BIOME_MOUNTAINS:
                    if (h >= 52) blk = (d == 0 && h >= 54) ? B_SNOW : B_STONE;
                    else blk = d == 0 ? B_GRASS : (d < 3 ? B_DIRT : B_STONE);
                    break;
                default:
                    blk = d == 0 ? B_GRASS : (d < 4 ? B_DIRT : B_STONE);
                }
                b[BIDX(x, y, z)] = (unsigned char)blk;
            }
            for (y = h + 1; y <= SEA_LEVEL; y++)
                b[BIDX(x, y, z)] = (bio == BIOME_SNOW && y == SEA_LEVEL) ? B_ICE : B_WATER;
        }

    /* ---- caves: two noise fields, tunnels where both are near 0.5 ---- */
    for (y = 0; y < 17; y++)
        for (z = 0; z < 5; z++)
            for (x = 0; x < 5; x++) {
                float wx = (float)(x0 + x * 4), wy = (float)(y * 4), wz = (float)(z0 + z * 4);
                cave1[y][z][x] = noise3(seed + 101u, wx / 22.0f, wy / 14.0f, wz / 22.0f);
                cave2[y][z][x] = noise3(seed + 131u, wx / 22.0f, wy / 14.0f, wz / 22.0f);
            }
    for (y = 3; y < WH - 2; y++) {
        int gy = y >> 2;
        float fy = (y & 3) * 0.25f;
        for (z = 0; z < CH; z++) {
            int gz = z >> 2;
            float fz = (z & 3) * 0.25f;
            for (x = 0; x < CH; x++) {
                int gx = x >> 2, h = col_h[z + PAD][x + PAD];
                float fx = (x & 3) * 0.25f, a, c;
                unsigned char *cell = &b[BIDX(x, y, z)];
                if (*cell == B_AIR || *cell == B_WATER || *cell == B_BEDROCK || *cell == B_ICE) continue;
                if (col_b[z + PAD][x + PAD] == BIOME_TOWN ? y >= h - 6 : !(y < h - 4 || (h > SEA_LEVEL + 3 && y <= h))) continue;
#define TRI(f) lerp(lerp(lerp(f[gy][gz][gx], f[gy][gz][gx + 1], fx), lerp(f[gy][gz + 1][gx], f[gy][gz + 1][gx + 1], fx), fz), \
                    lerp(lerp(f[gy + 1][gz][gx], f[gy + 1][gz][gx + 1], fx), lerp(f[gy + 1][gz + 1][gx], f[gy + 1][gz + 1][gx + 1], fx), fz), fy)
                a = TRI(cave1) - 0.5f;
                c = TRI(cave2) - 0.5f;
#undef TRI
                if (a * a + c * c < CAVE_R2) *cell = B_AIR;
            }
        }
    }

    /* ---- ores ---- */
    ctx.b = b;
    ctx.rng = hash3(seed + 999u, cx, 0, cz) | 1u;
    vein(&ctx, B_COAL_ORE, 16, 60, 10);
    vein(&ctx, B_IRON_ORE, 10, 44, 8);
    vein(&ctx, B_GOLD_ORE, 3, 28, 7);
    vein(&ctx, B_CRYSTAL_ORE, 2, 16, 6);

    /* ---- trees: sources from neighbouring chunks too, so crowns cross borders ---- */
    for (z = -PAD; z < CH + PAD; z++)
        for (x = -PAD; x < CH + PAD; x++) {
            int h = col_h[z + PAD][x + PAD], bio = col_b[z + PAD][x + PAD];
            int wx = x0 + x, wz = z0 + z, chance;
            unsigned int hs = hash3(seed + 777u, wx, 1, wz);
            if (h <= SEA_LEVEL) continue;
            switch (bio) {
            case BIOME_FOREST:    chance = 13; break;
            case BIOME_PLAINS:    chance = 150; break;
            case BIOME_SNOW:      chance = 40; break;
            case BIOME_MOUNTAINS: chance = h < 50 ? 70 : 0; break;
            default:              chance = 0;
            }
            if (!chance || hs % (unsigned int)chance) continue;
            /* inside this chunk the ground must still be there (caves) */
            if (x >= 0 && x < CH && z >= 0 && z < CH) {
                int g = b[BIDX(x, h, z)];
                if (g != B_GRASS && g != B_SNOW_GRASS && g != B_DIRT) continue;
                b[BIDX(x, h, z)] = B_DIRT;
            }
            if (bio == BIOME_SNOW) {
                tree_spruce(b, x, h + 1, z, 6 + (int)((hs >> 10) % 3u));
            } else if (bio == BIOME_FOREST && (hs >> 8) % 3u == 0) {
                tree_round(b, x, h + 1, z, 5 + (int)((hs >> 10) % 3u), B_BIRCH_LOG, B_BIRCH_LEAVES, hs >> 12);
            } else {
                tree_round(b, x, h + 1, z, 4 + (int)((hs >> 10) % 3u), B_LOG, B_LEAVES, hs >> 12);
            }
        }

    /* ---- cacti and small plants (single column, chunk-local) ---- */
    for (z = 0; z < CH; z++)
        for (x = 0; x < CH; x++) {
            int h = col_h[z + PAD][x + PAD], bio = col_b[z + PAD][x + PAD];
            int wx = x0 + x, wz = z0 + z, top;
            unsigned int hs = hash3(seed + 555u, wx, 2, wz);
            if (h <= SEA_LEVEL || h + 3 >= WH) continue;
            top = b[BIDX(x, h, z)];
            if (b[BIDX(x, h + 1, z)] != B_AIR) continue;
            if (bio == BIOME_DESERT && top == B_SAND && hs % 90u == 0) {
                int n = 1 + (int)((hs >> 8) % 3u);
                for (i = 1; i <= n; i++) b[BIDX(x, h + i, z)] = B_CACTUS;
            } else if (top == B_GRASS && bio == BIOME_PLAINS &&
                       noise2(seed + 808u, wx / 36.0f, wz / 36.0f) > 0.62f) {
                /* fields: rows of sunflowers or wheat with furrows between */
                int sun = noise2(seed + 809u, wx / 90.0f, wz / 90.0f) > 0.5f;
                if (wx % 4 != 0) {
                    b[BIDX(x, h, z)] = B_DIRT;
                    b[BIDX(x, h + 1, z)] = sun ? B_SUNFLOWER : B_WHEAT;
                }
            } else if (top == B_GRASS) {
                unsigned int grass = bio == BIOME_PLAINS ? 4u : (bio == BIOME_FOREST ? 7u : 10u);
                if (hs % 41u == 0)
                    b[BIDX(x, h + 1, z)] = ((hs >> 8) & 1u) ? B_FLOWER_RED : B_FLOWER_YELLOW;
                else if (hs % grass == 0)
                    b[BIDX(x, h + 1, z)] = B_TALL_GRASS;
            }
        }

    /* ---- town and highway on top of everything ---- */
    for (z = 0; z < CH; z++)
        for (x = 0; x < CH; x++) {
            int wx = x0 + x, wz = z0 + z;
            if (town_contains(wx, wz)) town_column(wx, wz, b, x, z);
            else town_highway_column(wx, wz, col_h[z + PAD][x + PAD], b, x, z);
        }
}
