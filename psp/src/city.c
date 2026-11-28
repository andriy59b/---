#include "city.h"
#include "texgen.h"

#include <stdlib.h>
#include <string.h>

#define WHITE 0xFFFFFFFFu
#define GRID 16.0f
#define GRID_N 84
#define GRID_MIN (-672.0f)

/* --------------------------------------------------------------- story */

const V3 POI_SPAWN = { 26.0f, WALK_H, 43.5f };
const float POI_SPAWN_YAW = PI_F;
const V3 POI_GARAGE = { 86.5f, WALK_H, 19.6f };
const V3 POI_TARAS = { 86.0f, WALK_H, 22.3f };
const V3 POI_CAR = { 95.0f, WALK_H, 19.0f };
const float POI_CAR_YAW = 0.0f;
const V3 POI_MARKET = { -91.6f, WALK_H, 95.0f };
const V3 POI_GALYA = { -90.7f, WALK_H, 97.4f };
const V3 POI_KIOSK = { 165.3f, WALK_H, -95.5f };
const V3 POI_SELLER = { 167.5f, WALK_H, -92.8f };
const V3 POI_KABAN = { -26.0f, WALK_H, -78.5f };
const V3 POI_KABAN_NPC = { -26.0f, WALK_H, -83.5f };

/* -------------------------------------------------------------- storage */

static CityCell cells[CELLS][CELLS];
static Box *boxes;
static int nboxes, capboxes;
static int *grid_items[GRID_N * GRID_N];
static int grid_count[GRID_N * GRID_N], grid_cap[GRID_N * GRID_N];
static unsigned int *stamps;
static unsigned int epoch;
static GMesh *M, *G;
static unsigned int g_seed;

static unsigned int hash3(int a, int b, int c)
{
    unsigned int h = (unsigned int)a * 0x8DA6B343u ^ (unsigned int)b * 0xD8163841u ^ (unsigned int)c * 0xCB1AB31Fu ^ g_seed;
    h ^= h >> 13;
    h *= 0x5BD1E995u;
    h ^= h >> 15;
    return h;
}
static float hf(int a, int b, int c) { return (hash3(a, b, c) & 0xFFFF) / 65535.0f; }

static int fl(float v) { return (int)floorf(v); }

int city_band_index(float c) { return fl(c / LOT); }
float city_band_offset(float c) { return c - city_band_index(c) * LOT; }
int city_cell_index(float c) { return fl(c / LOT); }

CityCell *city_cell(int i, int j)
{
    if (i < CELL_MIN || j < CELL_MIN || i >= CELL_MIN + CELLS || j >= CELL_MIN + CELLS) return NULL;
    return &cells[j - CELL_MIN][i - CELL_MIN];
}

static void add_box(float x0, float y0, float z0, float x1, float y1, float z1)
{
    Box *b;
    if (nboxes >= capboxes) {
        int cap = capboxes ? capboxes * 2 : 1024;
        Box *nb = (Box *)realloc(boxes, (size_t)cap * sizeof(Box));
        if (!nb) return;
        boxes = nb;
        capboxes = cap;
    }
    b = &boxes[nboxes++];
    b->x0 = x0 < x1 ? x0 : x1; b->x1 = x0 < x1 ? x1 : x0;
    b->y0 = y0; b->y1 = y1;
    b->z0 = z0 < z1 ? z0 : z1; b->z1 = z0 < z1 ? z1 : z0;
}

static void grid_build(void)
{
    int k, gx, gz;
    free(stamps);
    stamps = (unsigned int *)calloc((size_t)(nboxes > 0 ? nboxes : 1), sizeof(unsigned int));
    for (k = 0; k < nboxes; k++) {
        const Box *b = &boxes[k];
        int gx0 = fl((b->x0 - GRID_MIN) / GRID), gx1 = fl((b->x1 - GRID_MIN) / GRID);
        int gz0 = fl((b->z0 - GRID_MIN) / GRID), gz1 = fl((b->z1 - GRID_MIN) / GRID);
        for (gz = gz0; gz <= gz1; gz++)
            for (gx = gx0; gx <= gx1; gx++) {
                int g;
                if (gx < 0 || gz < 0 || gx >= GRID_N || gz >= GRID_N) continue;
                g = gz * GRID_N + gx;
                if (grid_count[g] >= grid_cap[g]) {
                    int cap = grid_cap[g] ? grid_cap[g] * 2 : 8;
                    int *ni = (int *)realloc(grid_items[g], (size_t)cap * sizeof(int));
                    if (!ni) continue;
                    grid_items[g] = ni;
                    grid_cap[g] = cap;
                }
                grid_items[g][grid_count[g]++] = k;
            }
    }
}

int city_box_count(void) { return nboxes; }

int city_boxes(float x0, float z0, float x1, float z1, const Box **out, int max)
{
    int gx, gz, n = 0, i;
    int gx0 = fl((x0 - GRID_MIN) / GRID), gx1 = fl((x1 - GRID_MIN) / GRID);
    int gz0 = fl((z0 - GRID_MIN) / GRID), gz1 = fl((z1 - GRID_MIN) / GRID);
    epoch++;
    for (gz = gz0; gz <= gz1; gz++)
        for (gx = gx0; gx <= gx1; gx++) {
            int g;
            if (gx < 0 || gz < 0 || gx >= GRID_N || gz >= GRID_N) continue;
            g = gz * GRID_N + gx;
            for (i = 0; i < grid_count[g]; i++) {
                int k = grid_items[g][i];
                const Box *b = &boxes[k];
                if (stamps[k] == epoch) continue;
                stamps[k] = epoch;
                if (b->x1 < x0 || b->x0 > x1 || b->z1 < z0 || b->z0 > z1) continue;
                if (n < max) out[n++] = b;
            }
        }
    return n;
}

/* -------------------------------------------------------------- layout */

int city_in_town(float x, float z)
{
    return x >= TOWN_MIN && x < TOWN_MAX && z >= TOWN_MIN && z < TOWN_MAX;
}

int city_band_ok(int band) { return band >= TOWN_LOT_MIN && band <= TOWN_LOT_MAX + 1; }

float city_lane(int dir, int band)
{
    float base = band * LOT;
    switch (dir & 3) {
    case 0: return base + 10.5f; /* +x drives on the +z half */
    case 2: return base + 5.5f;
    case 1: return base + 5.5f;  /* +z drives on the -x half */
    default: return base + 10.5f;
    }
}

static int asphalt_in_band(float o) { return o >= 3.0f && o < 13.0f; }

float city_ground(float x, float z)
{
    if (city_in_town(x, z)) {
        float bx = city_band_offset(x), bz = city_band_offset(z);
        int inx = bz < ROAD, inz = bx < ROAD;
        if (inx && inz) return (asphalt_in_band(bx) || asphalt_in_band(bz)) ? 0.0f : WALK_H;
        if (inx) return asphalt_in_band(bz) ? 0.0f : WALK_H;
        if (inz) return asphalt_in_band(bx) ? 0.0f : WALK_H;
        return WALK_H;
    }
    return 0.0f;
}

int city_on_road(float x, float z)
{
    if (city_in_town(x, z)) {
        float bx = city_band_offset(x), bz = city_band_offset(z);
        if (bz < ROAD && asphalt_in_band(bz)) return 1;
        if (bx < ROAD && asphalt_in_band(bx)) return 1;
        return 0;
    }
    return z >= 3.0f && z < 13.0f && x > -WORLD_HALF && x < WORLD_HALF;
}

int city_lot_type(int li, int lj)
{
    unsigned int r;
    if (li == 0 && lj == 0) return LOT_PANEL5;
    if (li == 1 && lj == 0) return LOT_GARAGES;
    if (li == -2 && lj == 1) return LOT_MARKET;
    if (li == 2 && lj == -2) return LOT_KIOSK;
    if (li == -1 && lj == -2) return LOT_PANEL9;
    r = hash3(li, lj, 7) % 100u;
    if (li == TOWN_LOT_MIN || li == TOWN_LOT_MAX || lj == TOWN_LOT_MIN || lj == TOWN_LOT_MAX) {
        if (r < 60) return LOT_HOUSES;
        if (r < 72) return LOT_GARAGES;
        if (r < 84) return LOT_EMPTY;
        return LOT_PARK;
    }
    if (r < 34) return LOT_PANEL5;
    if (r < 52) return LOT_PANEL9;
    if (r < 60) return LOT_HOUSES;
    if (r < 68) return LOT_GARAGES;
    if (r < 80) return LOT_PARK;
    if (r < 87) return LOT_SCHOOL;
    if (r < 93) return LOT_KIOSK;
    if (r < 96) return LOT_MARKET;
    return LOT_EMPTY;
}

/* --------------------------------------------------------- mesh helpers */

/* Horizontal quad; u runs along x (or along z when rot) */
static void ground(float x0, float z0, float x1, float z1, float y, int tile, int rot, unsigned int tint)
{
    UVRect uv = uv_tile(tile);
    V3 a = v3(x0, y, z1), b = v3(x0, y, z0), c = v3(x1, y, z0), d = v3(x1, y, z1);
    unsigned int col = gfx_shade(v3(0, 1, 0), tint);
    if (!rot) {
        gm_quad_flat(M, a, b, c, d, uv, col);
    } else {
        gm_tri(M, a, b, c, uv.u1, uv.v1, uv.u0, uv.v1, uv.u0, uv.v0, col);
        gm_tri(M, a, c, d, uv.u1, uv.v1, uv.u0, uv.v0, uv.u1, uv.v0, col);
    }
}

/* Ground split into squares of `step` metres so the texture keeps its scale */
static void ground_tiled(float x0, float z0, float x1, float z1, float y, float step, int tile, int rot)
{
    float x, z;
    for (z = z0; z < z1 - 0.01f; z += step)
        for (x = x0; x < x1 - 0.01f; x += step)
            ground(x, z, x + step < x1 ? x + step : x1, z + step < z1 ? z + step : z1, y, tile, rot, WHITE);
}

/* Vertical quad from (ax, az) to (bx, bz), outward normal = d x up */
static void vquad(float ax, float az, float bx, float bz, float y0, float y1, UVRect uv, unsigned int tint)
{
    gm_quad(M, v3(ax, y0, az), v3(ax, y1, az), v3(bx, y1, bz), v3(bx, y0, bz), uv, tint);
}

static void box(float x0, float y0, float z0, float x1, float y1, float z1, int side, int top, unsigned int tint, int collide)
{
    gm_box(M, v3(x0, y0, z0), v3(x1, y1, z1), uv_tile(side), uv_tile(top), tint, BOX_NO_BOTTOM);
    if (collide) add_box(x0, y0, z0, x1, y1, z1);
}

static void billboard(float x, float y, float z, float w, float h, int tile, int collide_trunk)
{
    UVRect uv = uv_tile(tile);
    unsigned int c = gfx_mul(WHITE, 0.9f + 0.1f * hf((int)x, (int)z, 3));
    gm_quad_flat(M, v3(x - w / 2, y, z), v3(x - w / 2, y + h, z), v3(x + w / 2, y + h, z), v3(x + w / 2, y, z), uv, c);
    gm_quad_flat(M, v3(x, y, z - w / 2), v3(x, y + h, z - w / 2), v3(x, y + h, z + w / 2), v3(x, y, z + w / 2), uv, c);
    if (collide_trunk) add_box(x - 0.2f, y, z - 0.2f, x + 0.2f, y + 3.0f, z + 0.2f);
}

/* ------------------------------------------------------------ buildings */

typedef int (*PickFn)(int floor, int bay, int bays, int side, unsigned int seed);

static int pick_panel(int floor, int bay, int bays, int side, unsigned int seed)
{
    unsigned int h = hash3(floor * 31 + side, bay, (int)seed);
    (void)bays;
    if (side < 0) return T_PANEL_BLANK;
    return T_PANEL_WIN1 + (int)(h % 3u);
}

static int pick_brick(int floor, int bay, int bays, int side, unsigned int seed)
{
    (void)floor; (void)bays; (void)seed;
    if (side < 0) return bay % 2 ? T_BRICK_WIN : T_BRICK;
    return T_BRICK_WIN;
}

static int pick_plaster(int floor, int bay, int bays, int side, unsigned int seed)
{
    (void)floor; (void)side; (void)seed;
    return (bay == 0 || bay == bays - 1) && bays > 2 ? T_PLASTER : T_PLASTER_WIN;
}

/* A facade from a to b (seen from outside, left to right). side < 0 = end wall. */
static void facade(float ax, float az, float bx, float bz, float y0, int floors, float fh, float bay_w,
                   PickFn pick, int side, unsigned int seed, const float *doors, int ndoors)
{
    float len = sqrtf((bx - ax) * (bx - ax) + (bz - az) * (bz - az));
    int bays = (int)(len / bay_w + 0.5f), f, b, k;
    float nx, nz;
    if (bays < 1) bays = 1;
    /* outward normal = d x up = (-dz, 0, dx) */
    nx = -(bz - az) / len;
    nz = (bx - ax) / len;
    for (f = 0; f < floors; f++)
        for (b = 0; b < bays; b++) {
            float t0 = (float)b / bays, t1 = (float)(b + 1) / bays;
            float x0 = ax + (bx - ax) * t0, z0 = az + (bz - az) * t0, x1 = ax + (bx - ax) * t1, z1 = az + (bz - az) * t1;
            float ya = y0 + f * fh, yb = ya + fh;
            int tile = pick(f, b, bays, side, seed), door = 0;
            for (k = 0; k < ndoors; k++) {
                float dd = doors[k] - (t0 + t1) * 0.5f * len;
                if (f == 0 && dd > -bay_w * 0.5f && dd <= bay_w * 0.5f) door = 1;
            }
            if (door) tile = T_PANEL_DOOR;
            vquad(x0, z0, x1, z1, ya, yb, uv_tile(tile), WHITE);
            if (!door && (tile == T_PANEL_WIN1 || tile == T_PANEL_WIN2 || tile == T_PANEL_WIN3 || tile == T_BRICK_WIN ||
                          tile == T_PLASTER_WIN) && hf(f * 13 + b, side * 7 + (int)seed, 5) < 0.4f) {
                GMesh *keep = M;
                M = G;
                gm_quad_flat(M, v3(x0 + nx * 0.08f, ya, z0 + nz * 0.08f), v3(x0 + nx * 0.08f, yb, z0 + nz * 0.08f),
                             v3(x1 + nx * 0.08f, yb, z1 + nz * 0.08f), v3(x1 + nx * 0.08f, ya, z1 + nz * 0.08f),
                             uv_tile(T_WINDOW_LIT), WHITE);
                M = keep;
            }
        }
}

/* Rectangular block of flats in world coordinates. Doors on the +z side. */
static void block_of_flats(float x0, float z0, float x1, float z1, int floors, PickFn pick, unsigned int seed,
                           const float *doors, int ndoors, int balconies)
{
    const float fh = 2.8f, y0 = WALK_H, H = y0 + floors * fh;
    float x;
    int f;
    facade(x0, z1, x1, z1, y0, floors, fh, 3.2f, pick, 0, seed, doors, ndoors);   /* +z */
    facade(x1, z0, x0, z0, y0, floors, fh, 3.2f, pick, 1, seed, NULL, 0);         /* -z */
    facade(x1, z1, x1, z0, y0, floors, fh, 3.2f, pick, -1, seed, NULL, 0);        /* +x */
    facade(x0, z0, x0, z1, y0, floors, fh, 3.2f, pick, -1, seed, NULL, 0);        /* -x */
    ground(x0, z0, x1, z1, H, T_ROOF_FLAT, 0, WHITE);
    /* parapet and a lift room on the roof */
    box(x0, H, z0, x1, H + 0.5f, z0 + 0.25f, T_CONCRETE, T_CONCRETE, WHITE, 0);
    box(x0, H, z1 - 0.25f, x1, H + 0.5f, z1, T_CONCRETE, T_CONCRETE, WHITE, 0);
    box(x0, H, z0 + 0.25f, x0 + 0.25f, H + 0.5f, z1 - 0.25f, T_CONCRETE, T_CONCRETE, WHITE, 0); /* no overlap at the corners */
    box(x1 - 0.25f, H, z0 + 0.25f, x1, H + 0.5f, z1 - 0.25f, T_CONCRETE, T_CONCRETE, WHITE, 0);
    box((x0 + x1) / 2 - 1.5f, H, (z0 + z1) / 2 - 1.5f, (x0 + x1) / 2 + 1.5f, H + 2.5f, (z0 + z1) / 2 + 1.5f,
        T_PANEL_BLANK, T_ROOF_FLAT, WHITE, 0);
    add_box(x0, 0.0f, z0, x1, H, z1);
    /* entrance canopies */
    for (f = 0; f < ndoors; f++) {
        float dx = x0 + doors[f];
        gm_box(M, v3(dx - 1.2f, y0 + 2.5f, z1), v3(dx + 1.2f, y0 + 2.65f, z1 + 1.3f), uv_tile(T_CONCRETE),
               uv_tile(T_CONCRETE), WHITE, BOX_ALL & ~32); /* no face against the wall */
    }
    /* balconies on the back (-z) side, every other bay, from the 2nd floor */
    if (balconies)
        for (f = 1; f < floors; f++)
            for (x = x0 + 3.2f * 1.5f; x < x1 - 3.0f; x += 6.4f) {
                float ya = y0 + f * fh;
                gm_box(M, v3(x - 1.4f, ya - 0.1f, z0 - 1.1f), v3(x + 1.4f, ya + 0.05f, z0), uv_tile(T_CONCRETE),
                       uv_tile(T_CONCRETE), WHITE, BOX_ALL & ~16);
                vquad(x + 1.4f, z0 - 1.1f, x - 1.4f, z0 - 1.1f, ya + 0.05f, ya + 1.05f, uv_tile(T_BALCONY), WHITE);
                vquad(x - 1.4f, z0 - 1.1f, x - 1.4f, z0, ya + 0.05f, ya + 1.05f, uv_sub(T_BALCONY, 0, 22, 32, 42), WHITE);
                vquad(x + 1.4f, z0, x + 1.4f, z0 - 1.1f, ya + 0.05f, ya + 1.05f, uv_sub(T_BALCONY, 32, 22, 32, 42), WHITE);
            }
}

static void gable_house(float x0, float z0, float x1, float z1, int roof_tile, unsigned int seed)
{
    const float y0 = WALK_H, h = 2.8f, rh = 2.2f, ov = 0.5f;
    float mx = (x0 + x1) / 2;
    UVRect r = uv_tile(roof_tile);
    float door = (x1 - x0) / 2;
    facade(x0, z1, x1, z1, y0, 1, h, 3.0f, pick_plaster, 0, seed, &door, 1);
    facade(x1, z0, x0, z0, y0, 1, h, 3.0f, pick_plaster, 1, seed, NULL, 0);
    facade(x1, z1, x1, z0, y0, 1, h, 3.0f, pick_plaster, -1, seed, NULL, 0);
    facade(x0, z0, x0, z1, y0, 1, h, 3.0f, pick_plaster, -1, seed, NULL, 0);
    /* gables (ridge along z) */
    gm_tri(M, v3(x0, y0 + h, z1), v3(mx, y0 + h + rh, z1), v3(x1, y0 + h, z1),
           uv_tile(T_PLASTER).u0, uv_tile(T_PLASTER).v1, 0.5f * (uv_tile(T_PLASTER).u0 + uv_tile(T_PLASTER).u1),
           uv_tile(T_PLASTER).v0, uv_tile(T_PLASTER).u1, uv_tile(T_PLASTER).v1, gfx_shade(v3(0, 0, 1), WHITE));
    gm_tri(M, v3(x1, y0 + h, z0), v3(mx, y0 + h + rh, z0), v3(x0, y0 + h, z0),
           uv_tile(T_PLASTER).u0, uv_tile(T_PLASTER).v1, 0.5f * (uv_tile(T_PLASTER).u0 + uv_tile(T_PLASTER).u1),
           uv_tile(T_PLASTER).v0, uv_tile(T_PLASTER).u1, uv_tile(T_PLASTER).v1, gfx_shade(v3(0, 0, -1), WHITE));
    /* roof slopes with overhang */
    gm_quad(M, v3(x0 - ov, y0 + h - 0.3f, z1 + ov), v3(mx, y0 + h + rh, z1 + ov), v3(mx, y0 + h + rh, z0 - ov),
            v3(x0 - ov, y0 + h - 0.3f, z0 - ov), r, WHITE);
    gm_quad(M, v3(x1 + ov, y0 + h - 0.3f, z0 - ov), v3(mx, y0 + h + rh, z0 - ov), v3(mx, y0 + h + rh, z1 + ov),
            v3(x1 + ov, y0 + h - 0.3f, z1 + ov), r, WHITE);
    add_box(x0, 0, z0, x1, y0 + h + rh, z1);
}

static void fence(float ax, float az, float bx, float bz)
{
    float len = sqrtf((bx - ax) * (bx - ax) + (bz - az) * (bz - az)), t;
    for (t = 0; t < len - 0.01f; t += 4.0f) {
        float t1 = t + 4.0f < len ? t + 4.0f : len;
        float x0 = ax + (bx - ax) * t / len, z0 = az + (bz - az) * t / len;
        float x1 = ax + (bx - ax) * t1 / len, z1 = az + (bz - az) * t1 / len;
        UVRect uv = uv_sub(T_FENCE, 0, 0, (int)(64 * (t1 - t) / 4.0f), 64);
        /* one quad, seen from both sides (no face culling): a back-to-back
         * pair in the same plane fights for the depth buffer and flickers */
        vquad(x0, z0, x1, z1, WALK_H, WALK_H + 1.5f, uv, WHITE);
    }
    add_box(ax < bx ? ax : bx, 0, az < bz ? az : bz, (ax > bx ? ax : bx) + 0.05f, WALK_H + 1.5f, (az > bz ? az : bz) + 0.05f);
}

static void kiosk(float x, float z, int sign, int facing_neg_z)
{
    const float w = 2.6f, h = 2.6f, y0 = WALK_H;
    unsigned int blue = gfx_rgba(120, 170, 240, 255);
    float zf = facing_neg_z ? z : z + w;
    /* the shop front replaces the box face instead of floating just in front of it */
    gm_box(M, v3(x, y0, z), v3(x + w, y0 + h, z + w), uv_tile(T_WHITE), uv_tile(T_METAL), blue,
           BOX_NO_BOTTOM & ~(facing_neg_z ? 32 : 16));
    add_box(x, y0, z, x + w, y0 + h, z + w);
    if (facing_neg_z) {
        vquad(x + w, zf, x, zf, y0, y0 + h - 0.4f, uv_tile(T_KIOSK_FRONT), WHITE);
        vquad(x + w, zf, x, zf, y0 + h - 0.4f, y0 + h, uv_sub(T_KIOSK_FRONT, 0, 52, 64, 12), WHITE);
        vquad(x + w + 0.2f, zf - 0.15f, x - 0.2f, zf - 0.15f, y0 + h - 0.3f, y0 + h + 0.7f, uv_sub(sign, 0, 16, 64, 32), WHITE);
    } else {
        vquad(x, zf, x + w, zf, y0, y0 + h - 0.4f, uv_tile(T_KIOSK_FRONT), WHITE);
        vquad(x, zf, x + w, zf, y0 + h - 0.4f, y0 + h, uv_sub(T_KIOSK_FRONT, 0, 52, 64, 12), WHITE);
        vquad(x - 0.2f, zf + 0.15f, x + w + 0.2f, zf + 0.15f, y0 + h - 0.3f, y0 + h + 0.7f, uv_sub(sign, 0, 16, 64, 32), WHITE);
    }
}

static void stall(float x, float z)
{
    const float y0 = WALK_H;
    unsigned int wood = gfx_rgba(170, 120, 70, 255);
    UVRect tent = uv_tile(T_TENT);
    box(x, y0, z, x + 3.6f, y0 + 0.95f, z + 0.9f, T_WHITE, T_WHITE, wood, 1);
    box(x, y0, z + 1.8f, x + 0.12f, y0 + 2.4f, z + 1.92f, T_WHITE, T_WHITE, wood, 0);
    box(x + 3.48f, y0, z + 1.8f, x + 3.6f, y0 + 2.4f, z + 1.92f, T_WHITE, T_WHITE, wood, 0);
    gm_quad(M, v3(x - 0.2f, y0 + 2.0f, z - 0.4f), v3(x - 0.2f, y0 + 2.5f, z + 2.0f), v3(x + 3.8f, y0 + 2.5f, z + 2.0f),
            v3(x + 3.8f, y0 + 2.0f, z - 0.4f), tent, WHITE);
    /* goods: watermelons and boxes */
    box(x + 0.3f, y0 + 0.95f, z + 0.2f, x + 1.0f, y0 + 1.3f, z + 0.7f, T_WHITE, T_WHITE, gfx_rgba(60, 130, 50, 255), 0);
    box(x + 1.4f, y0 + 0.95f, z + 0.15f, x + 2.2f, y0 + 1.25f, z + 0.75f, T_WHITE, T_WHITE, gfx_rgba(230, 160, 40, 255), 0);
    box(x + 2.6f, y0 + 0.95f, z + 0.2f, x + 3.3f, y0 + 1.2f, z + 0.7f, T_WHITE, T_WHITE, gfx_rgba(200, 50, 50, 255), 0);
}

static void bench(float x, float z, int along_x)
{
    UVRect uv = uv_tile(T_BENCH);
    float w = 1.8f;
    if (along_x) {
        vquad(x - w / 2, z, x + w / 2, z, WALK_H, WALK_H + 1.0f, uv, WHITE);
        add_box(x - w / 2, 0, z - 0.3f, x + w / 2, WALK_H + 0.5f, z + 0.3f);
    } else {
        vquad(x, z + w / 2, x, z - w / 2, WALK_H, WALK_H + 1.0f, uv, WHITE);
        add_box(x - 0.3f, 0, z - w / 2, x + 0.3f, WALK_H + 0.5f, z + w / 2);
    }
}

static void lamp(float x, float z)
{
    const unsigned int grey = gfx_rgba(170, 168, 160, 255);
    box(x - 0.12f, WALK_H, z - 0.12f, x + 0.12f, WALK_H + 7.0f, z + 0.12f, T_CONCRETE, T_CONCRETE, grey, 1);
    box(x - 0.6f, WALK_H + 6.9f, z - 0.15f, x + 0.6f, WALK_H + 7.1f, z + 0.15f, T_WHITE, T_WHITE, gfx_rgba(70, 70, 74, 255), 0);
}

/* -------------------------------------------------------------- roads */

static void road_x(float x0, float x1, float oz)
{
    float x;
    for (x = x0; x < x1 - 0.01f; x += 4.0f) {
        float xe = x + 4.0f < x1 ? x + 4.0f : x1;
        int crossing = x - x0 < 4.0f || x1 - xe < 0.01f;
        ground(x, oz, xe, oz + 3.0f, WALK_H, T_SIDEWALK, 0, WHITE);
        ground(x, oz + 13.0f, xe, oz + 16.0f, WALK_H, T_SIDEWALK, 0, WHITE);
        vquad(x, oz + 3.0f, xe, oz + 3.0f, 0.0f, WALK_H, uv_sub(T_CURB, 0, 0, 64, 16), WHITE);
        vquad(xe, oz + 13.0f, x, oz + 13.0f, 0.0f, WALK_H, uv_sub(T_CURB, 0, 0, 64, 16), WHITE);
        if (crossing && x - x0 < 4.0f) {
            ground(x, oz + 3.0f, xe, oz + 13.0f, 0.0f, T_CROSSWALK, 1, WHITE);
        } else {
            ground(x, oz + 3.0f, xe, oz + 5.5f, 0.0f, T_ASPHALT, 0, WHITE);
            ground(x, oz + 5.5f, xe, oz + 10.5f, 0.0f, T_ROAD_LINE, 0, WHITE);
            ground(x, oz + 10.5f, xe, oz + 13.0f, 0.0f, T_ASPHALT, 0, WHITE);
        }
    }
}

static void road_z(float z0, float z1, float ox)
{
    float z;
    for (z = z0; z < z1 - 0.01f; z += 4.0f) {
        float ze = z + 4.0f < z1 ? z + 4.0f : z1;
        ground(ox, z, ox + 3.0f, ze, WALK_H, T_SIDEWALK, 0, WHITE);
        ground(ox + 13.0f, z, ox + 16.0f, ze, WALK_H, T_SIDEWALK, 0, WHITE);
        vquad(ox + 3.0f, ze, ox + 3.0f, z, 0.0f, WALK_H, uv_sub(T_CURB, 0, 0, 64, 16), WHITE);
        vquad(ox + 13.0f, z, ox + 13.0f, ze, 0.0f, WALK_H, uv_sub(T_CURB, 0, 0, 64, 16), WHITE);
        if (z - z0 < 4.0f) {
            ground(ox + 3.0f, z, ox + 13.0f, ze, 0.0f, T_CROSSWALK, 0, WHITE);
        } else {
            ground(ox + 3.0f, z, ox + 5.5f, ze, 0.0f, T_ASPHALT, 1, WHITE);
            ground(ox + 5.5f, z, ox + 10.5f, ze, 0.0f, T_ROAD_LINE, 1, WHITE);
            ground(ox + 10.5f, z, ox + 13.0f, ze, 0.0f, T_ASPHALT, 1, WHITE);
        }
    }
}

static void intersection(float ox, float oz)
{
    const float c[4][2] = { { 0, 0 }, { 13, 0 }, { 0, 13 }, { 13, 13 } };
    int k;
    ground_tiled(ox + 3.0f, oz, ox + 13.0f, oz + 16.0f, 0.0f, 5.0f, T_ASPHALT, 0);
    ground(ox, oz + 3.0f, ox + 3.0f, oz + 13.0f, 0.0f, T_ASPHALT, 0, WHITE);
    ground(ox + 13.0f, oz + 3.0f, ox + 16.0f, oz + 13.0f, 0.0f, T_ASPHALT, 0, WHITE);
    for (k = 0; k < 4; k++) {
        float x0 = ox + c[k][0], z0 = oz + c[k][1];
        ground(x0, z0, x0 + 3.0f, z0 + 3.0f, WALK_H, T_SIDEWALK, 0, WHITE);
    }
    /* kerbs around the corners */
    vquad(ox + 3, oz + 3, ox, oz + 3, 0, WALK_H, uv_sub(T_CURB, 0, 0, 48, 16), WHITE);
    vquad(ox + 3, oz, ox + 3, oz + 3, 0, WALK_H, uv_sub(T_CURB, 0, 0, 48, 16), WHITE);
    vquad(ox + 16, oz + 3, ox + 13, oz + 3, 0, WALK_H, uv_sub(T_CURB, 0, 0, 48, 16), WHITE);
    vquad(ox + 13, oz + 3, ox + 13, oz, 0, WALK_H, uv_sub(T_CURB, 0, 0, 48, 16), WHITE);
    vquad(ox, oz + 13, ox + 3, oz + 13, 0, WALK_H, uv_sub(T_CURB, 0, 0, 48, 16), WHITE);
    vquad(ox + 3, oz + 13, ox + 3, oz + 16, 0, WALK_H, uv_sub(T_CURB, 0, 0, 48, 16), WHITE);
    vquad(ox + 13, oz + 13, ox + 16, oz + 13, 0, WALK_H, uv_sub(T_CURB, 0, 0, 48, 16), WHITE);
    vquad(ox + 13, oz + 16, ox + 13, oz + 13, 0, WALK_H, uv_sub(T_CURB, 0, 0, 48, 16), WHITE);
}

/* Lot ground as 4 m cells, one surface each: no overlapping decals (they
 * flicker with the PSP's 16-bit depth buffer) and small triangles near the camera. */
typedef int (*GroundFn)(float u, float v, int li, int lj);

static void lot_ground(float ox, float oz, GroundFn fn, int li, int lj)
{
    float u, v;
    for (v = 16.0f; v < 64.0f; v += 4.0f)
        for (u = 16.0f; u < 64.0f; u += 4.0f)
            ground(ox + u, oz + v, ox + u + 4.0f, oz + v + 4.0f, WALK_H, fn(u + 2.0f, v + 2.0f, li, lj), 0, WHITE);
}

static int in(float a, float lo, float hi) { return a >= lo && a < hi; }

static int g_panel5(float u, float v, int li, int lj) { (void)li; (void)lj; return in(u, 24, 28) && v >= 40 ? T_SIDEWALK : T_YARD; }
static int g_panel9(float u, float v, int li, int lj) { (void)li; (void)lj; return in(u, 32, 44) && in(v, 44, 56) ? T_DIRT : T_YARD; }
static int g_school(float u, float v, int li, int lj) { (void)li; (void)lj; return in(u, 24, 56) && in(v, 44, 60) ? T_DIRT : T_GRASS; }
static int g_concrete(float u, float v, int li, int lj) { (void)u; (void)v; (void)li; (void)lj; return T_CONCRETE; }
static int g_market(float u, float v, int li, int lj) { (void)u; (void)v; (void)li; (void)lj; return T_SIDEWALK; }
static int g_kiosk(float u, float v, int li, int lj) { (void)li; (void)lj; return in(u, 28, 52) && in(v, 24, 48) ? T_SIDEWALK : T_GRASS; }
static int g_park(float u, float v, int li, int lj) { (void)li; (void)lj; return in(u, 36, 40) || in(v, 36, 40) ? T_SIDEWALK : T_GRASS; }
static int g_yard(float u, float v, int li, int lj) { (void)u; (void)v; (void)li; (void)lj; return T_YARD; }
static int g_houses(float u, float v, int li, int lj)
{
    int q = (u >= 41.0f ? 1 : 0) + (v >= 41.0f ? 2 : 0);
    float pu = u - (17.0f + (q % 2) * 24.0f), pv = v - (17.0f + (q / 2) * 24.0f);
    if ((hash3(li * 4 + q, lj, 21) & 7u) == 0) return T_GRASS;
    return in(pu, 2, 20) && in(pv, 13, 21) ? T_DIRT : T_GRASS;
}

/* ---------------------------------------------------------------- lots */

static void lot_panel5(float ox, float oz, unsigned int s)
{
    static const float DOORS[3] = { 6.0f, 20.0f, 34.0f };
    lot_ground(ox, oz, g_panel5, 0, 0);
    block_of_flats(ox + 20, oz + 28, ox + 60, oz + 40, 5, pick_panel, s, DOORS, 3, 1);
    billboard(ox + 18, WALK_H, oz + 54, 6, 8, T_BIRCH, 1);
    billboard(ox + 38, WALK_H, oz + 56, 6, 9, T_BIRCH, 1);
    billboard(ox + 58, WALK_H, oz + 50, 5, 14, T_POPLAR, 1);
    billboard(ox + 46, WALK_H, oz + 22, 4, 3, T_BUSH, 0);
    bench(ox + 31, oz + 44, 1);
    bench(ox + 45, oz + 44, 1);
}

static void lot_panel9(float ox, float oz, unsigned int s)
{
    static const float DOORS[2] = { 7.0f, 21.0f };
    lot_ground(ox, oz, g_panel9, 0, 0);
    block_of_flats(ox + 24, oz + 22, ox + 52, oz + 36, 9, pick_panel, s, DOORS, 2, 1);
    billboard(ox + 20, WALK_H, oz + 46, 6, 9, T_BIRCH, 1);
    billboard(ox + 58, WALK_H, oz + 56, 5, 15, T_POPLAR, 1);
    billboard(ox + 56, WALK_H, oz + 20, 6, 8, T_BIRCH, 1);
    bench(ox + 30, oz + 41, 1);
}

static void lot_school(float ox, float oz, unsigned int s)
{
    static const float DOORS[1] = { 17.0f };
    lot_ground(ox, oz, g_school, 0, 0);
    block_of_flats(ox + 20, oz + 20, ox + 56, oz + 36, 3, pick_brick, s, DOORS, 1, 0);
    billboard(ox + 18, WALK_H, oz + 40, 5, 14, T_POPLAR, 1);
    billboard(ox + 60, WALK_H, oz + 40, 5, 15, T_POPLAR, 1);
}

static void lot_houses(float ox, float oz, int li, int lj)
{
    int q;
    lot_ground(ox, oz, g_houses, li, lj);
    for (q = 0; q < 4; q++) {
        float px0 = ox + 17 + (q % 2) * 24, pz0 = oz + 17 + (q / 2) * 24;
        unsigned int h = hash3(li * 4 + q, lj, 21);
        int k;
        if ((h & 7u) == 0) {
            billboard(px0 + 11, WALK_H, pz0 + 11, 6, 8, T_BIRCH, 1);
            continue;
        }
        /* fence with a gate on the -z side */
        fence(px0, pz0, px0 + 9, pz0);
        fence(px0 + 13, pz0, px0 + 22, pz0);
        fence(px0, pz0 + 22, px0 + 22, pz0 + 22);
        fence(px0, pz0, px0, pz0 + 22);
        fence(px0 + 22, pz0, px0 + 22, pz0 + 22);
        gable_house(px0 + 6, pz0 + 4, px0 + 14, pz0 + 10, (h >> 4) & 1 ? T_SLATE : T_ROOF_RED, h);
        for (k = 0; k < 4; k++) billboard(px0 + 4 + k * 4.5f, WALK_H, pz0 + 17, 3.6f, 2.4f, T_SUNFLOWERS, 0);
        billboard(px0 + 18, WALK_H, pz0 + 6, 5, 7, (h >> 6) & 1 ? T_BIRCH : T_BUSH, 1);
    }
}

static void lot_garages(float ox, float oz)
{
    float x;
    int k = 0;
    lot_ground(ox, oz, g_concrete, 0, 0);
    for (x = ox + 20; x < ox + 60 - 0.1f; x += 4.0f, k++) {
        int r;
        for (r = 0; r < 2; r++) {
            float z0 = oz + 24 + r * 6, z1 = z0 + 6;
            unsigned int tint = gfx_mul(WHITE, 0.85f + 0.15f * hf(k, r, 9));
            /* side walls only at the ends of the row: neighbours share them */
            gm_box(M, v3(x, WALK_H, z0), v3(x + 4, WALK_H + 2.6f, z1), uv_tile(T_METAL), uv_tile(T_METAL), tint,
                   4 | (x + 4.0f > ox + 60 - 0.1f ? 1 : 0) | (k == 0 ? 2 : 0));
            if (r == 0) vquad(x + 4, z0, x, z0, WALK_H, WALK_H + 2.6f, uv_tile(T_GARAGE_DOOR), tint);
            else vquad(x, z1, x + 4, z1, WALK_H, WALK_H + 2.6f, uv_tile(T_GARAGE_DOOR), tint);
        }
    }
    add_box(ox + 20, 0, oz + 24, ox + 60, WALK_H + 2.6f, oz + 36);
    billboard(ox + 62, WALK_H, oz + 30, 5, 14, T_POPLAR, 1);
    billboard(ox + 18, WALK_H, oz + 46, 4, 3, T_BUSH, 0);
}

static void lot_market(float ox, float oz)
{
    static const int SIGNS[4] = { T_SIGN_PIVO, T_SIGN_KAVA, T_SIGN_TSYHARKY, T_SIGN_HAZETY };
    int k;
    lot_ground(ox, oz, g_market, 0, 0);
    for (k = 0; k < 7; k++) kiosk(ox + 18 + k * 6.0f, oz + 20, SIGNS[k % 4], 1);
    for (k = 0; k < 6; k++) {
        stall(ox + 20 + k * 6.5f, oz + 33);
        stall(ox + 20 + k * 6.5f, oz + 46);
    }
    billboard(ox + 62, WALK_H, oz + 58, 5, 14, T_POPLAR, 1);
}

static void lot_kiosk(float ox, float oz)
{
    lot_ground(ox, oz, g_kiosk, 0, 0);
    kiosk(ox + 36, oz + 36, T_SIGN_PIVO, 1);
    bench(ox + 31, oz + 30, 1);
    bench(ox + 46, oz + 30, 1);
    billboard(ox + 22, WALK_H, oz + 22, 6, 9, T_BIRCH, 1);
    billboard(ox + 58, WALK_H, oz + 24, 6, 8, T_BIRCH, 1);
    billboard(ox + 22, WALK_H, oz + 56, 5, 14, T_POPLAR, 1);
    billboard(ox + 58, WALK_H, oz + 58, 4, 3, T_BUSH, 0);
}

static void lot_park(float ox, float oz, int li, int lj)
{
    int a, b;
    lot_ground(ox, oz, g_park, li, lj);
    for (a = 0; a < 4; a++)
        for (b = 0; b < 4; b++) {
            float tx = ox + 22 + a * 12 + hf(li * 8 + a, lj * 8 + b, 33) * 4 - 2;
            float tz = oz + 22 + b * 12 + hf(li * 8 + a, lj * 8 + b, 34) * 4 - 2;
            if (hf(li * 8 + a, lj * 8 + b, 35) < 0.2f || (tx > ox + 35 && tx < ox + 45) || (tz > oz + 35 && tz < oz + 45))
                continue;
            billboard(tx, WALK_H, tz, 6, hf(a, b, 36) > 0.7f ? 14 : 8, hf(a, b, 36) > 0.7f ? T_POPLAR : T_BIRCH, 1);
        }
    bench(ox + 36, oz + 30, 0);
    bench(ox + 44, oz + 50, 0);
}

static void lot_empty(float ox, float oz, int li, int lj)
{
    int k;
    lot_ground(ox, oz, g_yard, li, lj);
    for (k = 0; k < 6; k++)
        billboard(ox + 20 + hf(li, lj, 40 + k) * 40, WALK_H, oz + 20 + hf(li, lj, 50 + k) * 40, 4, 3, T_BUSH, 0);
}

/* --------------------------------------------------------- countryside */

static void highway(float x0, float x1, float oz)
{
    float x;
    for (x = x0; x < x1 - 0.01f; x += 4.0f) {
        ground(x, oz, x + 4, oz + 3, 0.0f, T_DIRT, 0, WHITE);
        ground(x, oz + 13, x + 4, oz + 16, 0.0f, T_DIRT, 0, WHITE);
        ground(x, oz + 3, x + 4, oz + 5.5f, 0.0f, T_ASPHALT, 0, WHITE);
        ground(x, oz + 5.5f, x + 4, oz + 10.5f, 0.0f, T_ROAD_LINE, 0, WHITE);
        ground(x, oz + 10.5f, x + 4, oz + 13, 0.0f, T_ASPHALT, 0, WHITE);
        /* guardrails (vidbiynyky) on both sides */
        /* single-sided rails lit as facing the road */
        vquad(x, oz + 2.4f, x + 4, oz + 2.4f, 0.0f, 0.9f, uv_tile(T_GUARDRAIL), WHITE);
        vquad(x + 4, oz + 13.6f, x, oz + 13.6f, 0.0f, 0.9f, uv_tile(T_GUARDRAIL), WHITE);
    }
    add_box(x0, 0, oz + 2.3f, x1, 0.9f, oz + 2.5f);
    add_box(x0, 0, oz + 13.5f, x1, 0.9f, oz + 13.7f);
    for (x = x0 + 6; x < x1; x += 10.0f) {
        billboard(x, 0, oz - 4, 5, 15, T_POPLAR, 1);
        billboard(x + 5, 0, oz + 20, 5, 15, T_POPLAR, 1);
    }
}

static void countryside(int i, int j)
{
    float ox = i * LOT, oz = j * LOT, x, z;
    int hw = j == 0;
    for (z = oz; z < oz + LOT; z += 16.0f)
        for (x = ox; x < ox + LOT; x += 16.0f) {
            int fx = fl((x + 2048.0f) / 128.0f), fz = fl((z + 2048.0f) / 96.0f), tile;
            unsigned int h = hash3(fx, fz, 60);
            if (hw && z < oz + 16.0f) continue;
            tile = (h % 5u) < 2 ? T_FIELD_WHEAT : (h % 5u) < 4 ? T_FIELD_SUNFLOWER : T_GRASS;
            if (hw && z < oz + 32.0f) tile = T_GRASS;
            ground(x, z, x + 16.0f, z + 16.0f, 0.0f, tile, 0, WHITE);
        }
    /* tree belts (lisosmuhy) between fields */
    if (!hw && (j & 1) == 0)
        for (x = ox + 4; x < ox + LOT; x += 8.0f) {
            int pop = hf((int)x, j, 61) > 0.45f;
            billboard(x, 0, oz + 2, pop ? 5 : 6, pop ? 15 : 8, pop ? T_POPLAR : T_BIRCH, 1);
        }
    if (hw) highway(ox, ox + LOT, oz);
}

/* --------------------------------------------------------------- build */

static void build_cell(int i, int j)
{
    CityCell *c = city_cell(i, j);
    float ox = i * LOT, oz = j * LOT;
    M = &c->solid;
    G = &c->glow;
    c->i = i;
    c->j = j;
    c->cx = ox + LOT / 2;
    c->cz = oz + LOT / 2;

    if (i >= TOWN_LOT_MIN && i <= TOWN_LOT_MAX + 1 && j >= TOWN_LOT_MIN && j <= TOWN_LOT_MAX + 1) {
        intersection(ox, oz);
        if (i <= TOWN_LOT_MAX) road_x(ox + ROAD, ox + LOT, oz);
        if (j <= TOWN_LOT_MAX) road_z(oz + ROAD, oz + LOT, ox);
        if (i <= TOWN_LOT_MAX) { lamp(ox + 28, oz + 0.6f); lamp(ox + 52, oz + 15.4f); }
        if (j <= TOWN_LOT_MAX) { lamp(ox + 0.6f, oz + 40); lamp(ox + 15.4f, oz + 28); }
        if (i <= TOWN_LOT_MAX && j <= TOWN_LOT_MAX) {
            unsigned int s = hash3(i, j, 1);
            switch (city_lot_type(i, j)) {
            case LOT_PANEL5:  lot_panel5(ox, oz, s); break;
            case LOT_PANEL9:  lot_panel9(ox, oz, s); break;
            case LOT_HOUSES:  lot_houses(ox, oz, i, j); break;
            case LOT_GARAGES: lot_garages(ox, oz); break;
            case LOT_MARKET:  lot_market(ox, oz); break;
            case LOT_PARK:    lot_park(ox, oz, i, j); break;
            case LOT_SCHOOL:  lot_school(ox, oz, s); break;
            case LOT_KIOSK:   lot_kiosk(ox, oz); break;
            default:          lot_empty(ox, oz, i, j); break;
            }
        } else if (i <= TOWN_LOT_MAX) {
            ground_tiled(ox, oz + ROAD, ox + LOT, oz + LOT, 0.0f, 16.0f, T_GRASS, 0);
        } else if (j <= TOWN_LOT_MAX) {
            ground_tiled(ox + ROAD, oz + (j == 0 ? ROAD : 0.0f), ox + LOT, oz + LOT, 0.0f, 16.0f, T_GRASS, 0);
            if (j == 0) highway(ox + ROAD, ox + LOT, oz);
        } else {
            ground_tiled(ox + ROAD, oz, ox + LOT, oz + LOT, 0.0f, 16.0f, T_GRASS, 0);
            ground_tiled(ox, oz + ROAD, ox + ROAD, oz + LOT, 0.0f, 16.0f, T_GRASS, 0);
        }
    } else {
        countryside(i, j);
    }
}

void city_free(void)
{
    int a, b;
    for (a = 0; a < CELLS; a++)
        for (b = 0; b < CELLS; b++) {
            gm_free(&cells[a][b].solid);
            gm_free(&cells[a][b].glow);
            free(cells[a][b].chunks);
            cells[a][b].chunks = NULL;
            cells[a][b].nchunks = 0;
        }
    for (a = 0; a < GRID_N * GRID_N; a++) {
        free(grid_items[a]);
        grid_items[a] = NULL;
        grid_count[a] = grid_cap[a] = 0;
    }
    free(boxes);
    boxes = NULL;
    nboxes = capboxes = 0;
    free(stamps);
    stamps = NULL;
}

void city_build(unsigned int seed)
{
    int i, j;
    city_free();
    g_seed = seed;
    memset(cells, 0, sizeof(cells));
    for (j = CELL_MIN; j < CELL_MIN + CELLS; j++)
        for (i = CELL_MIN; i < CELL_MIN + CELLS; i++) {
            CityCell *c;
            build_cell(i, j);
            c = city_cell(i, j);
            c->chunks = (GChunk *)malloc(sizeof(GChunk) * (size_t)(c->solid.n / 3 + 1));
            c->nchunks = c->chunks ? gm_chunks(c->solid.v, c->solid.n, c->chunks, c->solid.n / 3 + 1) : 0;
            if (c->nchunks < 0) c->nchunks = 0;
        }
    /* world border */
    add_box(-WORLD_HALF - 2, 0, -WORLD_HALF - 2, WORLD_HALF + 2, 20, -WORLD_HALF);
    add_box(-WORLD_HALF - 2, 0, WORLD_HALF, WORLD_HALF + 2, 20, WORLD_HALF + 2);
    add_box(-WORLD_HALF - 2, 0, -WORLD_HALF, -WORLD_HALF, 20, WORLD_HALF);
    add_box(WORLD_HALF, 0, -WORLD_HALF, WORLD_HALF + 2, 20, WORLD_HALF);
    grid_build();
}

float city_raycast(V3 from, V3 dir, float maxd)
{
    float t;
    for (t = 0.0f; t < maxd; t += 0.2f) {
        V3 p = v3_add(from, v3_scale(dir, t));
        const Box *hit[8];
        int n, k;
        if (p.y < city_ground(p.x, p.z) + 0.05f) return t;
        n = city_boxes(p.x - 0.01f, p.z - 0.01f, p.x + 0.01f, p.z + 0.01f, hit, 8);
        for (k = 0; k < n; k++)
            if (p.x >= hit[k]->x0 && p.x <= hit[k]->x1 && p.z >= hit[k]->z0 && p.z <= hit[k]->z1 &&
                p.y >= hit[k]->y0 && p.y <= hit[k]->y1)
                return t;
    }
    return maxd;
}
