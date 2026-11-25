/* The town of Sonyashnykivsk: road grid, lots and story locations. */
#ifndef UC_TOWN_H
#define UC_TOWN_H

#define TOWN_Y 30            /* y of the ground / road surface block */
#define LOT 48               /* road grid period */
#define ROAD_W 7             /* sidewalk, 5 asphalt cells, sidewalk */
#define TOWN_LOT_MIN (-4)    /* lots -4..3 in x and z */
#define TOWN_LOT_MAX 3
#define TOWN_MIN (TOWN_LOT_MIN * LOT)                    /* -192 */
#define TOWN_MAX ((TOWN_LOT_MAX + 1) * LOT + ROAD_W)     /*  199 (exclusive) */
#define HIGHWAY_Z0 0         /* east-west highway leaves town on this road band */

enum {
    LOT_PANEL5 = 0, /* five-storey prefab block */
    LOT_PANEL9,     /* nine-storey block */
    LOT_HOUSES,     /* private houses with gardens */
    LOT_GARAGES,
    LOT_MARKET,
    LOT_PARK,
    LOT_SCHOOL,
    LOT_EMPTY,      /* waste ground */
    LOT_KIOSK,      /* small square with a beer kiosk */
    LOT_COUNT
};

int town_mod(int a);                 /* a mod LOT, always 0..LOT-1 */
int town_div(int a);                 /* floor(a / LOT) */
int town_contains(int x, int z);     /* inside the town including its edge roads */
int town_distance(int x, int z);     /* blocks outside the town (0 inside) */
int town_is_road(int x, int z);      /* asphalt or sidewalk of the town grid */
int town_lot_type(int li, int lj);

/* Writes the town blocks of column (x, z) into a chunk at local (lx, lz).
 * The terrain below TOWN_Y must already be filled. */
void town_column(int x, int z, unsigned char *blocks, int lx, int lz);

/* Highway outside the town: returns 1 and fills the column when (x, z) is on it. */
int town_highway_column(int x, int z, int ground, unsigned char *blocks, int lx, int lz);

/* Story locations (feet positions) */
extern const float POI_SPAWN[3];
extern const float POI_SPAWN_YAW;
extern const float POI_GARAGE[3];
extern const float POI_CAR[3];
extern const float POI_CAR_YAW;
extern const float POI_MARKET[3];
extern const float POI_KIOSK[3];
extern const float POI_KABAN[3];

#endif
