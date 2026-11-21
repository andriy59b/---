#include "textures.h"

#define TS (ATLAS_SIZE / ATLAS_TILES) /* tile size in texels */

static unsigned int clamp8(int v)
{
    return (unsigned int)(v < 0 ? 0 : (v > 255 ? 255 : v));
}

static unsigned int rgba(int r, int g, int b, int a)
{
    return (clamp8(a) << 24) | (clamp8(b) << 16) | (clamp8(g) << 8) | clamp8(r);
}

static unsigned int noise(int tile, int x, int y)
{
    unsigned int h = (unsigned int)tile * 7919u + (unsigned int)x * 374761393u +
                     (unsigned int)y * 668265263u;
    h = (h ^ (h >> 13)) * 1274126177u;
    return h ^ (h >> 16);
}

/* -16..15 */
static int jitter(int tile, int x, int y)
{
    return (int)(noise(tile, x, y) & 31u) - 16;
}

static unsigned int texel(int tile, int x, int y)
{
    int j = jitter(tile, x, y);
    switch (tile) {
    case 0: /* grass top */
        return rgba(86 + j, 150 + j, 60 + j / 2, 255);
    case 1: { /* grass side: dirt with a ragged green fringe */
        int fringe = 3 + (int)(noise(tile, x, 0) % 3u);
        if (y < fringe) return rgba(86 + j, 150 + j, 60 + j / 2, 255);
        return rgba(125 + j, 88 + j, 58 + j / 2, 255);
    }
    case 2: /* dirt */
        return rgba(125 + j, 88 + j, 58 + j / 2, 255);
    case 3: { /* stone */
        int spot = (noise(tile, x / 3, y / 2) & 7u) == 0 ? -30 : 0;
        return rgba(128 + j + spot, 128 + j + spot, 132 + j + spot, 255);
    }
    case 4: /* sand */
        return rgba(222 + j / 2, 208 + j / 2, 150 + j / 2, 255);
    case 5: { /* wood side: vertical bark stripes */
        int stripe = (x % 4 == 0) ? -25 : 0;
        return rgba(105 + j / 2 + stripe, 76 + j / 2 + stripe, 45 + stripe, 255);
    }
    case 6: { /* wood top: rings */
        int dx = 2 * x - (TS - 1), dy = 2 * y - (TS - 1);
        int r = (dx * dx + dy * dy) / 24;
        int ring = (r % 3 == 0) ? -28 : 0;
        if (x == 0 || y == 0 || x == TS - 1 || y == TS - 1)
            return rgba(95 + j / 2, 68 + j / 2, 40, 255);
        return rgba(180 + j / 2 + ring, 145 + j / 2 + ring, 92 + ring, 255);
    }
    case 7: /* leaves with see-through gaps */
        if ((noise(tile, x, y) & 7u) == 0) return rgba(0, 0, 0, 0);
        return rgba(48 + j, 118 + j, 40 + j / 2, 255);
    case 8: { /* planks: four boards with dark seams */
        int seam = (y % 4 == 3) || (x == ((y / 4) * 5 + 3) % TS);
        if (seam) return rgba(120, 86, 50, 255);
        return rgba(184 + j / 2, 142 + j / 2, 88 + j / 2, 255);
    }
    case 9: /* glass: light frame, clear centre */
        if (x == 0 || y == 0 || x == TS - 1 || y == TS - 1)
            return rgba(210, 235, 245, 255);
        if (x == y + 3 || x == y + 4) return rgba(235, 250, 255, 200);
        return rgba(0, 0, 0, 0);
    case 10: { /* brick */
        int row = y / 4;
        int off = (row & 1) ? 4 : 0;
        if (y % 4 == 3 || (x + off) % 8 == 7) return rgba(190 + j / 4, 186 + j / 4, 178, 255);
        return rgba(160 + j, 64 + j / 2, 48 + j / 2, 255);
    }
    case 11: /* water */
        return rgba(48 + j / 2, 92 + j / 2, 200 + j, 255);
    default:
        return rgba(255, 0, 255, 255);
    }
}

void textures_generate(unsigned int *atlas)
{
    int tx, ty, x, y;
    for (ty = 0; ty < ATLAS_TILES; ty++)
        for (tx = 0; tx < ATLAS_TILES; tx++) {
            int tile = ty * ATLAS_TILES + tx;
            for (y = 0; y < TS; y++)
                for (x = 0; x < TS; x++)
                    atlas[(ty * TS + y) * ATLAS_SIZE + tx * TS + x] = texel(tile, x, y);
        }
}
