/* Sonyashnykivsk as polygon geometry: roads with kerbs, sidewalks, buildings,
 * countryside with fields, tree belts and a highway; static collision. */
#ifndef UC_CITY_H
#define UC_CITY_H

#include "gfx.h"

#define LOT 64.0f            /* road grid period (m) */
#define ROAD 16.0f           /* road band: 3 m sidewalk, 10 m asphalt, 3 m sidewalk */
#define WALK_H 0.15f         /* sidewalk / yard height above asphalt */
#define TOWN_LOT_MIN (-4)
#define TOWN_LOT_MAX 3
#define TOWN_MIN (TOWN_LOT_MIN * LOT)
#define TOWN_MAX ((TOWN_LOT_MAX + 1) * LOT + ROAD)
#define WORLD_HALF 640.0f
#define CELLS 20             /* world = CELLS x CELLS cells of LOT metres */
#define CELL_MIN (-10)

enum {
    LOT_PANEL5 = 0, LOT_PANEL9, LOT_HOUSES, LOT_GARAGES, LOT_MARKET, LOT_PARK, LOT_SCHOOL,
    LOT_EMPTY, LOT_KIOSK, LOT_COUNT
};

typedef struct { float x0, y0, z0, x1, y1, z1; } Box;

typedef struct {
    int i, j;
    GMesh solid;   /* opaque + alpha-tested */
    GMesh glow;    /* lit windows, faded in at night */
    float cx, cz;  /* centre */
    GChunk *chunks; /* runs of nearby triangles of solid, for cutting near the camera */
    int nchunks;
} CityCell;

void city_build(unsigned int seed);   /* builds all geometry and collision */
void city_free(void);
CityCell *city_cell(int i, int j);    /* i, j in CELL_MIN .. CELL_MIN+CELLS-1, or NULL */
int  city_cell_index(float c);        /* world coordinate -> cell index */

int  city_lot_type(int li, int lj);
int  city_in_town(float x, float z);
float city_ground(float x, float z);  /* surface height (asphalt 0, sidewalks WALK_H) */
int  city_on_road(float x, float z);  /* on asphalt of the town grid or highway */

/* Static collision boxes overlapping the XZ rectangle. Returns count. */
int  city_boxes(float x0, float z0, float x1, float z1, const Box **out, int max);
int  city_box_count(void);
/* Ray against boxes and the ground, for cameras. Returns hit distance or maxd. */
float city_raycast(V3 from, V3 dir, float maxd);

/* Lanes (drive on the right). dir: 0 +x, 1 +z, 2 -x, 3 -z. band = grid index. */
float city_lane(int dir, int band);
int  city_band_ok(int band);
int  city_band_index(float c);        /* floor(c / LOT) */
float city_band_offset(float c);      /* c - band * LOT, 0..LOT */

/* Story locations */
extern const V3 POI_SPAWN, POI_GARAGE, POI_CAR, POI_MARKET, POI_KIOSK, POI_KABAN;
extern const V3 POI_TARAS, POI_GALYA, POI_SELLER, POI_KABAN_NPC;
extern const float POI_SPAWN_YAW, POI_CAR_YAW;

#endif
