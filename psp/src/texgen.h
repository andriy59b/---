/* Procedural pixel-art texture atlas (512x512, 64x64 tiles) and the HUD font. */
#ifndef UC_TEXGEN_H
#define UC_TEXGEN_H

#include "gfx.h"

enum {
    T_WHITE = 0, T_ASPHALT, T_ROAD_LINE, T_CROSSWALK, T_SIDEWALK, T_CURB, T_GRASS, T_YARD,
    T_DIRT, T_PANEL_WIN1, T_PANEL_WIN2, T_PANEL_WIN3, T_PANEL_BLANK, T_PANEL_DOOR, T_BALCONY, T_BRICK_WIN,
    T_BRICK, T_ROOF_FLAT, T_SLATE, T_ROOF_RED, T_PLASTER_WIN, T_PLASTER, T_FENCE, T_GARAGE_DOOR,
    T_METAL, T_KIOSK_FRONT, T_SIGN_PIVO, T_SIGN_KAVA, T_SIGN_TSYHARKY, T_SIGN_HAZETY, T_TENT, T_GUARDRAIL,
    T_CONCRETE, T_BIRCH, T_POPLAR, T_BUSH, T_SUNFLOWERS, T_FIELD_WHEAT, T_FIELD_SUNFLOWER, T_CAR_PARTS2,
    T_WINDOW_LIT, T_CAR_SIDE, T_CAR_PARTS, T_TIRE, T_CAR_GLASS, T_FACES_A, T_FACES_B, T_FACES_C,
    T_HAIR, T_CLOTH_TRACK, T_CLOTH_LEATHER, T_CLOTH_BLAZER, T_CLOTH_DRESS, T_CLOTH_OVERALLS, T_CLOTH_DENIM, T_CLOTH_ARMY,
    T_FACES_D, T_FACES_E, T_CLOTH_KNIT, T_LIVERY, T_CAR_INTERIOR, T_BENCH, T_SHADOW, T_FLARE,
    T_COUNT
};

/* Sub-rectangles inside T_CAR_PARTS (pixels) */
#define CP_GRILLE_CHROME   0, 0, 64, 12
#define CP_GRILLE_BLACK    0, 12, 64, 12
#define CP_HEAD_RECT       0, 24, 24, 12
#define CP_HEAD_ROUND      24, 24, 16, 16
#define CP_TAIL            40, 24, 24, 10
#define CP_PLATE           0, 40, 32, 8
#define CP_INDICATOR       40, 34, 8, 6
#define CP_BUMPER          0, 48, 64, 6
#define CP_DARK            32, 40, 8, 8
#define CP_CHROME          48, 34, 16, 6
#define CP_BLACK_PLASTIC   40, 40, 8, 8

/* T_CAR_PARTS2: grilles, lights and wheels that make each model its own */
#define C2_KIDNEYS         0, 0, 32, 12    /* twin chrome-framed kidneys */
#define C2_SLOTS7          32, 0, 32, 12   /* seven vertical slots */
#define C2_UAZ             0, 12, 32, 12   /* tall vertical slats */
#define C2_MOSK            32, 12, 32, 8   /* chrome horizontal bars */
#define C2_TRUCK           32, 20, 32, 12  /* big truck grille */
#define C2_QUAD            0, 24, 32, 8    /* four round headlights in a black band */
#define C2_RIM_ALLOY       0, 32, 16, 16
#define C2_RIM_STEEL       16, 32, 16, 16
#define C2_RIM_TRUCK       32, 32, 16, 16
#define C2_RIM_SPOKES      48, 32, 16, 16
#define C2_CANVAS          0, 48, 16, 16
#define C2_BEACON_BLUE     16, 48, 16, 8
#define C2_BEACON_RED      16, 56, 16, 8
#define C2_TANK            32, 48, 16, 16  /* motorcycle tank */
#define C2_RECT_SLANT      48, 48, 16, 8   /* wedge headlight */
#define C2_MIRROR          48, 56, 16, 8   /* mirror glass */

/* T_LIVERY: police, taxi, ambulance and fire markings */
#define LV_POLICE          0, 0, 64, 12    /* white МІЛІЦІЯ on blue */
#define LV_CHECKER         0, 12, 64, 8
#define LV_TAXI            0, 20, 64, 12   /* ТАКСІ roof sign */
#define LV_AMBULANCE       0, 32, 64, 12   /* red ШВИДКА on white */
#define LV_CROSS           0, 44, 16, 16
#define LV_FIRE            16, 44, 48, 10  /* white ПОЖЕЖНА on red */
#define LV_03              16, 54, 24, 10
#define LV_01              40, 54, 24, 10

/* T_CAR_INTERIOR */
#define IN_DASH            0, 0, 32, 16    /* instrument cluster */
#define IN_CONSOLE         32, 0, 32, 16   /* radio and heater */
#define IN_VELOUR          0, 16, 16, 16
#define IN_LEATHER         16, 16, 16, 16
#define IN_VINYL           32, 16, 16, 16
#define IN_DOOR            48, 16, 16, 16
#define IN_CARPET          0, 32, 16, 16
#define IN_WOOD            16, 32, 16, 16

/* Small cloth patterns, 32x32 quarters of T_HAIR (pixels) */
#define SC_HAIR   0, 0, 32, 32
#define SC_SHIRT  32, 0, 32, 32
#define SC_SHOE   0, 32, 32, 32
#define SC_SCARF  32, 32, 32, 32

/* Faces: T_FACES_A..C hold two characters x (closed, open mouth) in 32x32
 * cells; the passers-by (who do not talk) have one closed-mouth cell each,
 * four to a tile in T_FACES_D and T_FACES_E. */
enum {
    FACE_ANDRIY = 0, FACE_TARAS, FACE_GALYA, FACE_KABAN, FACE_SELLER, FACE_THUG,
    FACE_NADIA, FACE_PETRO, FACE_OLIA, FACE_VOVAN, FACE_STEPAN, FACE_IRYNA, FACE_MYKHAILO, FACE_PETRYK,
    FACE_COUNT
};
#define FACE_TALKING 6   /* faces below this have an open-mouth cell */
/* pixel origin of a face cell */
void face_cell(int face, int open, int *tile, int *x, int *y);
UVRect uv_face(int face, int mouth_open);

void texgen_atlas(unsigned int *atlas);   /* TEX_SIZE x TEX_SIZE, ABGR */

/* Ground texture: the ground tiles stacked in rows of one 64 px wide
 * texture that repeats along u, so a whole street of asphalt is one long
 * quad instead of a dozen 4 m ones. Last row: the kerb. */
#define GROUND_W 64
#define GROUND_H 512
#define GROUND_ROW_KERB 7
int  ground_row(int tile);                 /* row of an atlas ground tile, -1 if not in the ground texture */
void ground_row_v(int row, float *v0, float *v1);
void texgen_ground(const unsigned int *atlas, unsigned int *out); /* GROUND_W x GROUND_H */
void texgen_mip(const unsigned int *src, int src_size, unsigned int *dst); /* half-size level */
void texgen_mip_rect(const unsigned int *src, int src_w, int src_h, unsigned int *dst);

#define FONT_TEX_W 128
#define FONT_TEX_H 64
void font_generate(unsigned int *tex);
unsigned int utf8_next(const char **p);
int font_glyph(unsigned int cp);

#endif
