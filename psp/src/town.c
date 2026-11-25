#include "town.h"
#include "world.h"

#include <stdlib.h>

#define GROUND (TOWN_Y)
#define SET(yy, blk) blocks[BIDX(lx, (yy), lz)] = (unsigned char)(blk)

/* Story locations. They follow the fixed lot layouts below:
 * home block in lot (0,0), garages (1,0), market (-2,1), kiosk (2,-2), Kaban (-1,-2). */
const float POI_SPAWN[3] = { 17.5f, TOWN_Y + 1, 30.5f };
const float POI_SPAWN_YAW = -1.18f;
const float POI_GARAGE[3] = { 48 + 12.5f, TOWN_Y + 1, 12.5f };
const float POI_CAR[3] = { 48 + 18.5f, TOWN_Y + 1, 10.0f };
const float POI_CAR_YAW = 0.0f;
const float POI_MARKET[3] = { -96 + 23.5f, TOWN_Y + 1, 48 + 21.5f };
const float POI_KIOSK[3] = { 96 + 21.5f, TOWN_Y + 1, -96 + 18.0f };
const float POI_KABAN[3] = { -48 + 28.5f, TOWN_Y + 1, -96 + 40.5f };

static unsigned int hash2(int a, int b, int salt)
{
    unsigned int h = (unsigned int)a * 0x8DA6B343u ^ (unsigned int)b * 0xD8163841u ^ (unsigned int)salt * 0xCB1AB31Fu;
    h ^= h >> 13;
    h *= 0x5BD1E995u;
    h ^= h >> 15;
    return h;
}

int town_mod(int a)
{
    int m = a % LOT;
    return m < 0 ? m + LOT : m;
}

int town_div(int a)
{
    return (a - town_mod(a)) / LOT;
}

int town_contains(int x, int z)
{
    return x >= TOWN_MIN && x < TOWN_MAX && z >= TOWN_MIN && z < TOWN_MAX;
}

int town_distance(int x, int z)
{
    int dx = x < TOWN_MIN ? TOWN_MIN - x : (x >= TOWN_MAX ? x - TOWN_MAX + 1 : 0);
    int dz = z < TOWN_MIN ? TOWN_MIN - z : (z >= TOWN_MAX ? z - TOWN_MAX + 1 : 0);
    return dx > dz ? dx : dz;
}

int town_is_road(int x, int z)
{
    return town_contains(x, z) && (town_mod(x) < ROAD_W || town_mod(z) < ROAD_W);
}

int town_lot_type(int li, int lj)
{
    unsigned int h, r;
    if (li == 0 && lj == 0) return LOT_PANEL5;
    if (li == 1 && lj == 0) return LOT_GARAGES;
    if (li == -2 && lj == 1) return LOT_MARKET;
    if (li == 2 && lj == -2) return LOT_KIOSK;
    if (li == -1 && lj == -2) return LOT_PANEL9;
    h = hash2(li, lj, 7);
    r = h % 100u;
    if (li == TOWN_LOT_MIN || li == TOWN_LOT_MAX || lj == TOWN_LOT_MIN || lj == TOWN_LOT_MAX) {
        /* the outskirts are the private sector */
        if (r < 62) return LOT_HOUSES;
        if (r < 74) return LOT_GARAGES;
        if (r < 86) return LOT_EMPTY;
        return LOT_PARK;
    }
    if (r < 34) return LOT_PANEL5;
    if (r < 52) return LOT_PANEL9;
    if (r < 62) return LOT_HOUSES;
    if (r < 70) return LOT_GARAGES;
    if (r < 82) return LOT_PARK;
    if (r < 88) return LOT_SCHOOL;
    if (r < 93) return LOT_KIOSK;
    if (r < 96) return LOT_MARKET;
    return LOT_EMPTY;
}

/* ----------------------------------------------------------------- pieces */

/* Birch tree with its trunk at (tu, tv); writes this column's part of it. */
static void birch(unsigned char *blocks, int lx, int lz, int u, int v, int tu, int tv, int h)
{
    int du = abs(u - tu), dv = abs(v - tv), y;
    if (du > 2 || dv > 2) return;
    for (y = h - 3; y <= h + 1; y++) {
        int r = y <= h - 1 ? 2 : 1;
        if (du > r || dv > r) continue;
        if (r == 2 && du == 2 && dv == 2) continue;
        SET(GROUND + y, B_BIRCH_LEAVES);
    }
    if (du == 0 && dv == 0)
        for (y = 1; y <= h; y++) SET(GROUND + y, B_BIRCH_LOG);
}

static int is_door(int u, const int *doors, int n)
{
    int i;
    for (i = 0; i < n; i++)
        if (u == doors[i]) return 1;
    return 0;
}

/* Box-shaped block of flats; doors on the v1 (south, +z) side. */
static void flats(unsigned char *blocks, int lx, int lz, int u, int v, int u0, int u1, int v0, int v1,
                  int floors, int wall, int window, const int *doors, int ndoors)
{
    int height = floors * 3, y;
    if (u < u0 || u > u1 || v < v0 || v > v1) {
        /* canopies over the entrances and paths to the road */
        if (v == v1 + 1 && (is_door(u, doors, ndoors) || is_door(u - 1, doors, ndoors) || is_door(u + 1, doors, ndoors)))
            SET(GROUND + 3, B_CONCRETE);
        if (v > v1 && is_door(u, doors, ndoors)) SET(GROUND, B_SIDEWALK);
        return;
    }
    for (y = 1; y <= height; y++) {
        int rel = y - 1, f = rel % 3, floor = rel / 3, blk = wall;
        int edge_v = (v == v0 || v == v1), edge_u = (u == u0 || u == u1);
        int along = edge_v ? u - u0 : v - v0;
        if (!edge_u && !edge_v) {
            SET(GROUND + y, B_CONCRETE);
            continue;
        }
        if (!(edge_u && edge_v) && f == 1 && along % 3 != 0) blk = window;
        if (floor == 0 && v == v1 && f < 2 && is_door(u, doors, ndoors)) blk = B_DOOR;
        if (floor == 0 && v == v1 && f == 1 && (is_door(u - 1, doors, ndoors) || is_door(u + 1, doors, ndoors)))
            blk = wall;
        SET(GROUND + y, blk);
    }
    SET(GROUND + height + 1, B_CONCRETE);
    if (u == u0 || u == u1 || v == v0 || v == v1) SET(GROUND + height + 2, B_CONCRETE);
}

static void kiosk(unsigned char *blocks, int lx, int lz, int u, int v, int u0, int v0, int front_v, int sign)
{
    int y;
    if (u < u0 || u > u0 + 2 || v < v0 || v > v0 + 2) return;
    for (y = 1; y <= 3; y++) {
        int blk = B_KIOSK;
        if (v == front_v && u == u0 + 1 && y == 2) blk = B_GLASS;
        if (v == front_v && y == 3 && sign) blk = B_SIGN_BEER;
        SET(GROUND + y, blk);
    }
    SET(GROUND + 4, B_METAL);
}

/* ------------------------------------------------------------------- lots */

static void lot_panel5(unsigned char *blocks, int lx, int lz, int u, int v)
{
    static const int DOORS[3] = { 17, 27, 37 };
    flats(blocks, lx, lz, u, v, 12, 43, 16, 28, 5, B_PANEL, B_PANEL_WINDOW, DOORS, 3);
    birch(blocks, lx, lz, u, v, 10, 39, 6);
    birch(blocks, lx, lz, u, v, 23, 41, 7);
    birch(blocks, lx, lz, u, v, 41, 38, 6);
}

static void lot_panel9(unsigned char *blocks, int lx, int lz, int u, int v)
{
    static const int DOORS[2] = { 20, 35 };
    flats(blocks, lx, lz, u, v, 14, 41, 12, 27, 9, B_PANEL, B_PANEL_WINDOW, DOORS, 2);
    birch(blocks, lx, lz, u, v, 16, 38, 6);
    birch(blocks, lx, lz, u, v, 40, 42, 7);
    /* sandpit in the yard */
    if (u >= 24 && u <= 27 && v >= 31 && v <= 34) SET(GROUND, B_SAND);
}

static void lot_school(unsigned char *blocks, int lx, int lz, int u, int v)
{
    static const int DOORS[1] = { 26 };
    flats(blocks, lx, lz, u, v, 10, 43, 12, 29, 3, B_BRICK, B_GLASS, DOORS, 1);
    if (v >= 33 && v <= 45 && u >= 12 && u <= 42) SET(GROUND, B_DIRT); /* sports ground */
}

static void lot_houses(unsigned char *blocks, int lx, int lz, int u, int v, int li, int lj)
{
    int q;
    for (q = 0; q < 4; q++) {
        int u0 = 9 + (q % 2) * 20, v0 = 9 + (q / 2) * 20, pu = u - u0, pv = v - v0, y;
        unsigned int h = hash2(li * 4 + q, lj, 21);
        if (pu < 0 || pu > 17 || pv < 0 || pv > 17) continue;
        if ((h & 7u) == 0) return; /* empty plot */
        /* fence with a gate */
        if ((pu == 0 || pu == 17 || pv == 0 || pv == 17) && !(pv == 0 && (pu == 8 || pu == 9))) {
            SET(GROUND + 1, B_PLANKS);
            return;
        }
        if (pv == 0 || (pu == 8 && pv < 4) || (pu == 9 && pv < 4)) { SET(GROUND, B_GRAVEL); return; }
        /* house: walls 4..11 x 4..9, roof steps above */
        if (pu >= 3 && pu <= 12 && pv >= 3 && pv <= 10) {
            int in_walls = pu >= 4 && pu <= 11 && pv >= 4 && pv <= 9;
            if (in_walls) {
                int wall = pu == 4 || pu == 11 || pv == 4 || pv == 9;
                SET(GROUND, B_PLANKS);
                for (y = 1; y <= 3 && wall; y++) {
                    int blk = B_PLASTER, along = (pv == 4 || pv == 9) ? pu - 4 : pv - 4;
                    if (y == 2 && along % 3 == 2) blk = B_GLASS;
                    if (pv == 4 && pu == 8 && y <= 2) blk = B_DOOR;
                    SET(GROUND + y, blk);
                }
            }
            SET(GROUND + 4, B_ROOF);
            if (pu >= 5 && pu <= 10 && pv >= 5 && pv <= 8) SET(GROUND + 5, B_ROOF);
            if (pu >= 6 && pu <= 9 && pv >= 6 && pv <= 7) SET(GROUND + 6, B_ROOF);
            return;
        }
        /* garden: sunflowers and wheat behind the house */
        if (pv >= 12 && pv <= 15 && pu >= 2 && pu <= 15) {
            SET(GROUND, B_DIRT);
            if (pv % 2 == 0) SET(GROUND + 1, (h >> 8) & 1 ? B_SUNFLOWER : B_WHEAT);
            return;
        }
        if (pu == 14 && pv == 5) birch(blocks, lx, lz, u, v, u, v, 5);
    }
}

static void lot_garages(unsigned char *blocks, int lx, int lz, int u, int v)
{
    int y;
    SET(GROUND, B_CONCRETE);
    if (u < 10 || u > 44 || v < 14 || v > 25) return;
    for (y = 1; y <= 3; y++) {
        int a = (u - 10) % 5, blk = B_METAL;
        if (y < 3 && a >= 1 && a <= 3 && (v == 14 || v == 25)) blk = B_GARAGE_DOOR;
        SET(GROUND + y, blk);
    }
}

static void lot_market(unsigned char *blocks, int lx, int lz, int u, int v)
{
    int k;
    SET(GROUND, B_SIDEWALK);
    for (k = 0; k < 6; k++) kiosk(blocks, lx, lz, u, v, 10 + k * 6, 10, 12, (k & 1) == 0);
    for (k = 0; k < 12; k++) {
        int vc = k < 6 ? 24 : 34, u0 = 10 + (k % 6) * 6;
        if (u < u0 || u > u0 + 3 || v < vc - 1 || v > vc + 1) continue;
        if (v == vc) SET(GROUND + 1, B_PLANKS);
        if (v == vc + 1 && (u == u0 || u == u0 + 3)) {
            SET(GROUND + 1, B_LOG);
            SET(GROUND + 2, B_LOG);
        }
        SET(GROUND + 3, B_ROOF);
    }
}

static void lot_park(unsigned char *blocks, int lx, int lz, int u, int v, int li, int lj)
{
    int a, b;
    if ((u >= 26 && u <= 28) || (v >= 26 && v <= 28)) {
        SET(GROUND, B_SIDEWALK);
        return;
    }
    for (a = 0; a < 4; a++)
        for (b = 0; b < 4; b++) {
            unsigned int h = hash2(li * 8 + a, lj * 8 + b, 33);
            int tu = 11 + a * 10 + (int)(h % 3u) - 1, tv = 11 + b * 10 + (int)((h >> 4) % 3u) - 1;
            if ((h >> 8) % 5u == 0) continue;
            birch(blocks, lx, lz, u, v, tu, tv, 5 + (int)((h >> 12) % 3u));
        }
    if ((hash2(u + li * LOT, v + lj * LOT, 34) & 31u) == 0 && blocks[BIDX(lx, GROUND + 1, lz)] == B_AIR)
        SET(GROUND + 1, (u & 1) ? B_FLOWER_RED : B_FLOWER_YELLOW);
}

static void lot_kiosk(unsigned char *blocks, int lx, int lz, int u, int v, int li, int lj)
{
    if (v >= 14 && v <= 26 && u >= 14 && u <= 28) SET(GROUND, B_SIDEWALK);
    kiosk(blocks, lx, lz, u, v, 20, 20, 20, 1);
    birch(blocks, lx, lz, u, v, 34, 30, 6);
    birch(blocks, lx, lz, u, v, 12, 36, 7);
    birch(blocks, lx, lz, u, v, 40, 14, 5);
    (void)li; (void)lj;
}

static void lot_empty(unsigned char *blocks, int lx, int lz, int u, int v, int li, int lj)
{
    unsigned int h = hash2(u + li * LOT, v + lj * LOT, 41);
    if (((u / 5 + v / 4 + li) & 3) == 0 && (h & 3u)) SET(GROUND, B_GRAVEL);
    else if ((h & 7u) == 0) SET(GROUND + 1, B_TALL_GRASS);
}

/* --------------------------------------------------------------- columns */

static void road(unsigned char *blocks, int lx, int lz, int x, int z, int rx, int rz)
{
    int along_x = rz < ROAD_W, along_z = rx < ROAD_W, top = B_ASPHALT, y;
    if (along_x && along_z) {
        if ((rx == 0 || rx == ROAD_W - 1) && (rz == 0 || rz == ROAD_W - 1)) top = B_SIDEWALK;
    } else if (along_x) {
        if (rz == 0 || rz == ROAD_W - 1) top = B_SIDEWALK;
        else if (rz == 3 && town_mod(x) % 4 < 2) top = B_ROAD_LINE_X;
    } else {
        if (rx == 0 || rx == ROAD_W - 1) top = B_SIDEWALK;
        else if (rx == 3 && town_mod(z) % 4 < 2) top = B_ROAD_LINE_Z;
    }
    SET(GROUND, top);
    /* concrete lamp posts along the sidewalks */
    if (top == B_SIDEWALK && !(along_x && along_z)) {
        int c = along_x ? town_mod(x) : town_mod(z);
        if (c % 16 == 12)
            for (y = 1; y <= 5; y++) SET(GROUND + y, B_CONCRETE);
    }
}

void town_column(int x, int z, unsigned char *blocks, int lx, int lz)
{
    int rx = town_mod(x), rz = town_mod(z), y;
    int li = town_div(x), lj = town_div(z);

    for (y = GROUND + 1; y < WH; y++) SET(y, B_AIR);
    SET(GROUND, B_GRASS);
    SET(GROUND - 1, B_DIRT);
    SET(GROUND - 2, B_DIRT);

    if (rx < ROAD_W || rz < ROAD_W) {
        road(blocks, lx, lz, x, z, rx, rz);
        return;
    }
    if (li < TOWN_LOT_MIN || li > TOWN_LOT_MAX || lj < TOWN_LOT_MIN || lj > TOWN_LOT_MAX) return;

    switch (town_lot_type(li, lj)) {
    case LOT_PANEL5:  lot_panel5(blocks, lx, lz, rx, rz); break;
    case LOT_PANEL9:  lot_panel9(blocks, lx, lz, rx, rz); break;
    case LOT_HOUSES:  lot_houses(blocks, lx, lz, rx, rz, li, lj); break;
    case LOT_GARAGES: lot_garages(blocks, lx, lz, rx, rz); break;
    case LOT_MARKET:  lot_market(blocks, lx, lz, rx, rz); break;
    case LOT_PARK:    lot_park(blocks, lx, lz, rx, rz, li, lj); break;
    case LOT_SCHOOL:  lot_school(blocks, lx, lz, rx, rz); break;
    case LOT_KIOSK:   lot_kiosk(blocks, lx, lz, rx, rz, li, lj); break;
    default:          lot_empty(blocks, lx, lz, rx, rz, li, lj); break;
    }
}

int town_highway_column(int x, int z, int ground, unsigned char *blocks, int lx, int lz)
{
    int rz = z - HIGHWAY_Z0, y, top;
    if (rz < 0 || rz >= ROAD_W || town_contains(x, z)) return 0;
    if (x < -1600 || x > 1600) return 0;
    if (ground < SEA_LEVEL) ground = SEA_LEVEL;
    for (y = ground + 1; y < WH && y <= ground + 8; y++) SET(y, B_AIR);
    for (y = ground - 1; y >= ground - 2 && y > 0; y--)
        if (blocks[BIDX(lx, y, lz)] == B_AIR || blocks[BIDX(lx, y, lz)] == B_WATER) SET(y, B_CONCRETE);
    top = B_ASPHALT;
    if (rz == 0 || rz == ROAD_W - 1) top = B_SIDEWALK;
    else if (rz == 3 && town_mod(x) % 4 < 2) top = B_ROAD_LINE_X;
    SET(ground, top);
    return 1;
}
