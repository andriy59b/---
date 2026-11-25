/* Procedural block textures (32x32 tiles) and the HUD font. */
#include "textures.h"

#include <math.h>
#include <string.h>

#define TS TILE_PX

/* ----------------------------------------------------------------- helpers */

static unsigned int hsh(int tile, int salt, int x, int y)
{
    unsigned int h = (unsigned int)tile * 0x9E3779B9u ^ (unsigned int)salt * 0x85EBCA6Bu;
    h ^= (unsigned int)x * 0xC2B2AE35u;
    h = (h << 15) | (h >> 17);
    h ^= (unsigned int)y * 0x27D4EB2Fu;
    h ^= h >> 15;
    h *= 0x2C1B3C6Du;
    h ^= h >> 12;
    h *= 0x297A2D39u;
    h ^= h >> 15;
    return h;
}

static float rnd(int tile, int salt, int x, int y)
{
    return (hsh(tile, salt, x, y) & 0xFFFF) / 65535.0f;
}

static float sm(float t) { return t * t * (3.0f - 2.0f * t); }

/* Tileable value noise: px x py lattice cells across the tile. */
static float vnoise(int tile, int salt, int x, int y, int px, int py)
{
    float gx = (x + 0.5f) * px / TS, gy = (y + 0.5f) * py / TS;
    int ix = (int)floorf(gx), iy = (int)floorf(gy);
    float fx = sm(gx - ix), fy = sm(gy - iy);
    int x0 = ix % px, x1 = (ix + 1) % px, y0 = iy % py, y1 = (iy + 1) % py;
    float a = rnd(tile, salt, x0, y0), b = rnd(tile, salt, x1, y0);
    float c = rnd(tile, salt, x0, y1), d = rnd(tile, salt, x1, y1);
    return (a + (b - a) * fx) + ((c + (d - c) * fx) - (a + (b - a) * fx)) * fy;
}

static float fbm(int tile, int salt, int x, int y, int period, int octaves)
{
    float sum = 0.0f, amp = 0.5f, norm = 0.0f;
    int i;
    for (i = 0; i < octaves && period <= TS; i++) {
        sum += vnoise(tile, salt + i * 17, x, y, period, period) * amp;
        norm += amp;
        amp *= 0.5f;
        period *= 2;
    }
    return sum / norm;
}

/* Tileable Voronoi: distances to the nearest and second nearest feature point */
static void voronoi(int tile, int salt, int x, int y, int n, float *d1, float *d2, int *id)
{
    int i;
    float best = 1e9f, second = 1e9f;
    *id = 0;
    for (i = 0; i < n; i++) {
        float px = rnd(tile, salt, i, 0) * TS, py = rnd(tile, salt, i, 1) * TS;
        float dx = fabsf(x + 0.5f - px), dy = fabsf(y + 0.5f - py), d;
        if (dx > TS / 2) dx = TS - dx;
        if (dy > TS / 2) dy = TS - dy;
        d = sqrtf(dx * dx + dy * dy);
        if (d < best) {
            second = best;
            best = d;
            *id = i;
        } else if (d < second) {
            second = d;
        }
    }
    *d1 = best;
    *d2 = second;
}

static unsigned int c8(float v)
{
    int i = (int)(v + 0.5f);
    return (unsigned int)(i < 0 ? 0 : (i > 255 ? 255 : i));
}

static unsigned int rgba(float r, float g, float b, float a)
{
    return (c8(a) << 24) | (c8(b) << 16) | (c8(g) << 8) | c8(r);
}

static unsigned int shade(int r, int g, int b, float s)
{
    return rgba(r * s, g * s, b * s, 255);
}

#define CLEAR 0u

/* ----------------------------------------------------------------- tiles */

static unsigned int grass_px(int tile, int x, int y)
{
    float n = fbm(tile, 1, x, y, 4, 3), g = rnd(tile, 2, x, y);
    float s = 0.74f + 0.36f * n + 0.14f * (g - 0.5f);
    if (g > 0.94f) return rgba(118 * s * 1.15f, 186 * s * 1.15f, 70 * s, 255);
    return shade(92, 158, 56, s);
}

static unsigned int dirt_px(int tile, int x, int y)
{
    float n = fbm(tile, 3, x, y, 4, 3), g = rnd(tile, 4, x, y);
    float s = 0.72f + 0.38f * n;
    if (g > 0.95f) s *= 1.25f;
    else if (g < 0.05f) s *= 0.72f;
    return shade(134, 96, 67, s);
}

static unsigned int stone_px(int tile, int x, int y, int r, int g, int b)
{
    float n = fbm(tile, 5, x, y, 4, 3), cr = vnoise(tile, 6, x, y, 4, 4);
    float s = 0.74f + 0.42f * n + 0.08f * (rnd(tile, 7, x, y) - 0.5f);
    if (fabsf(cr - 0.5f) < 0.025f) s *= 0.78f;
    return shade(r, g, b, s);
}

static unsigned int side_with_top(int tile, int x, int y, int is_snow)
{
    int depth = 5 + (int)(vnoise(tile, 8, x, 0, 8, 1) * 5.0f) + (rnd(tile, 9, x, 0) > 0.7f ? 2 : 0);
    if (y < depth) {
        if (is_snow) {
            float s = 0.9f + 0.1f * fbm(tile, 10, x, y, 4, 2);
            return shade(240, 245, 252, s);
        }
        return grass_px(tile, x, y);
    }
    if (y == depth) {
        unsigned int c = is_snow ? shade(200, 210, 222, 1.0f) : shade(70, 120, 42, 1.0f);
        return c;
    }
    return dirt_px(tile, x, y);
}

static unsigned int ore_px(int tile, int x, int y, int r, int g, int b)
{
    float d1, d2;
    int id;
    voronoi(tile, 11, x, y, 6, &d1, &d2, &id);
    if (d1 < 3.4f - rnd(tile, 12, id, 0) * 1.2f) {
        float s = 1.15f - d1 * 0.12f + 0.1f * (rnd(tile, 13, x, y) - 0.5f);
        if (x > 0 && y > 0 && rnd(tile, 14, x, y) > 0.8f) s *= 1.2f;
        return shade(r, g, b, s);
    }
    return stone_px(3, x, y, 125, 125, 125);
}

static unsigned int leaves_px(int tile, int x, int y, int r, int g, int b)
{
    float n = fbm(tile, 15, x, y, 8, 2), hole = vnoise(tile, 16, x, y, 8, 8) + 0.45f * rnd(tile, 17, x, y);
    if (hole < 0.42f) return CLEAR;
    return shade(r, g, b, 0.62f + 0.55f * n);
}

static unsigned int plank_px(int tile, int x, int y)
{
    int board = y / 8;
    int joint = (board * 13 + 5) % TS;
    float grain = vnoise(tile, 18 + board, x, y, 2, 16);
    float s = 0.78f + 0.3f * grain + 0.05f * (rnd(tile, 19, x, y) - 0.5f);
    if (y % 8 == 7) s = 0.52f;
    else if (y % 8 == 0) s *= 1.1f;
    if (x == joint && y % 8 != 7) s = 0.58f;
    if ((x == (joint + 3) % TS || x == (joint + TS - 3) % TS) && (y % 8 == 2 || y % 8 == 5)) s = 0.5f;
    return shade(166, 132, 80, s);
}

static unsigned int brick_px(int tile, int x, int y)
{
    int row = y / 8, off = (row & 1) ? 8 : 0, bx = (x + off) / 16;
    float g = rnd(tile, 20, x, y);
    if (y % 8 == 7 || (x + off) % 16 == 15)
        return shade(176, 170, 160, 0.85f + 0.2f * g);
    {
        float s = 0.85f + 0.25f * rnd(tile, 21, bx, row) + 0.12f * (g - 0.5f);
        if (y % 8 == 0) s *= 1.12f;
        if (y % 8 == 6) s *= 0.85f;
        return shade(150, 62, 48, s);
    }
}

static unsigned int stone_bricks_px(int tile, int x, int y)
{
    int row = y / 16, off = (row & 1) ? 8 : 0, lx = (x + off) % 16, ly = y % 16;
    float s = 0.82f + 0.3f * fbm(tile, 22, x, y, 4, 3);
    if (ly == 15 || lx == 15) return shade(70, 70, 72, 1.0f);
    if (ly == 0 || lx == 0) s *= 1.18f;
    if (ly == 14 || lx == 14) s *= 0.72f;
    if (fabsf(vnoise(tile, 23, x, y, 4, 4) - 0.5f) < 0.02f) s *= 0.8f;
    return shade(122, 122, 124, s);
}

static unsigned int cobble_px(int tile, int x, int y)
{
    float d1, d2;
    int id;
    voronoi(tile, 24, x, y, 10, &d1, &d2, &id);
    if (d2 - d1 < 1.6f) return shade(62, 62, 64, 0.9f + 0.2f * rnd(tile, 25, x, y));
    {
        float s = 0.72f + 0.38f * rnd(tile, 26, id, 0) - d1 * 0.02f + 0.1f * (rnd(tile, 27, x, y) - 0.5f);
        return shade(132, 132, 132, s);
    }
}

static unsigned int gravel_px(int tile, int x, int y)
{
    float d1, d2;
    int id;
    voronoi(tile, 28, x, y, 26, &d1, &d2, &id);
    if (d2 - d1 < 1.0f) return shade(80, 76, 72, 0.9f);
    {
        float k = rnd(tile, 29, id, 0), s = 0.75f + 0.45f * rnd(tile, 30, id, 1) - d1 * 0.03f;
        if (k < 0.3f) return shade(150, 130, 115, s);
        return shade(140, 138, 136, s);
    }
}

static unsigned int log_side_px(int tile, int x, int y, int birch)
{
    if (birch) {
        float m = vnoise(tile, 31, x, y, 4, 16), s = 0.9f + 0.12f * fbm(tile, 32, x, y, 4, 2);
        if (m < 0.27f) return shade(52, 50, 46, 1.0f);
        return shade(220, 216, 204, s);
    } else {
        float n = vnoise(tile, 33, x, y, 8, 2), d = vnoise(tile, 34, x, y, 16, 4);
        float s = 0.68f + 0.42f * n;
        if (d < 0.25f) s *= 0.7f;
        return shade(106, 80, 50, s);
    }
}

static unsigned int log_top_px(int tile, int x, int y)
{
    float dx = x + 0.5f - TS / 2.0f, dy = y + 0.5f - TS / 2.0f;
    float r = sqrtf(dx * dx + dy * dy) + 1.5f * fbm(tile, 35, x, y, 4, 2);
    if (r > 14.5f) return shade(98, 74, 46, 0.8f + 0.3f * rnd(tile, 36, x, y));
    {
        float ring = 0.5f + 0.5f * sinf(r * 1.7f);
        return shade(182, 148, 96, 0.78f + 0.24f * ring);
    }
}

static unsigned int water_px(int tile, int x, int y)
{
    float n = vnoise(tile, 37, x, y, 4, 8) * 0.6f + fbm(tile, 38, x, y, 4, 2) * 0.4f;
    float s = 0.78f + 0.4f * n;
    return rgba(44 * s, 96 * s, 205 * s, 0xA8);
}

static unsigned int glass_px(int x, int y)
{
    if (x == 0 || y == 0 || x == TS - 1 || y == TS - 1) return shade(196, 220, 228, 1.0f);
    if (x == 1 || y == 1 || x == TS - 2 || y == TS - 2) return shade(232, 244, 248, 1.0f);
    if ((x - y == 6 || x - y == 7 || x - y == 9) && x < 26) return rgba(240, 250, 255, 190);
    if (x - y == -14 && y < 26) return rgba(240, 250, 255, 190);
    return CLEAR;
}

static unsigned int cactus_side_px(int tile, int x, int y)
{
    float s = 0.85f + 0.25f * vnoise(tile, 39, x, y, 4, 8);
    int k = x % 8;
    if (k == 0) s *= 0.68f;
    else if (k == 4) s *= 1.15f;
    if (k == 2 && rnd(tile, 40, x, y / 4) > 0.6f && y % 4 == 0) return shade(236, 228, 180, 1.0f);
    return shade(68, 132, 48, s);
}

static unsigned int cactus_top_px(int tile, int x, int y)
{
    float dx = x + 0.5f - TS / 2.0f, dy = y + 0.5f - TS / 2.0f, r = sqrtf(dx * dx + dy * dy);
    float s = 0.9f + 0.2f * fbm(tile, 41, x, y, 4, 2);
    if (r > 14.0f) s *= 0.7f;
    else if (r < 3.0f) s *= 1.2f;
    return shade(86, 150, 60, s);
}

static unsigned int tall_grass_px(int tile, int x, int y)
{
    int i;
    for (i = 0; i < 9; i++) {
        float base = 3.0f + rnd(tile, 42, i, 0) * 26.0f;
        float h = 12.0f + rnd(tile, 43, i, 0) * 18.0f;
        float lean = (rnd(tile, 44, i, 0) - 0.5f) * 0.5f;
        float t = (TS - 1 - y) / h;
        float cx = base + lean * (TS - 1 - y);
        float w = 1.6f * (1.0f - t) + 0.4f;
        if (t >= 0.0f && t <= 1.0f && fabsf(x + 0.5f - cx) < w) {
            float s = 0.62f + 0.5f * t + 0.1f * rnd(tile, 45, x, y);
            return shade(88, 150, 52, s);
        }
    }
    return CLEAR;
}

static unsigned int flower_px(int tile, int x, int y, int r, int g, int b, int cr, int cg, int cb)
{
    float dx = x + 0.5f - 16.0f, dy = y + 0.5f - 10.0f, d = sqrtf(dx * dx + dy * dy);
    if (d < 2.2f) return shade(cr, cg, cb, 1.0f);
    if (d < 6.0f) {
        float ang = atan2f(dy, dx);
        float petal = 4.6f + 1.4f * cosf(ang * 5.0f);
        if (d < petal) return shade(r, g, b, 0.8f + 0.25f * (1.0f - d / 6.0f) + 0.1f * rnd(tile, 46, x, y));
    }
    if ((x == 15 || x == 16) && y > 13) return shade(60, 120, 40, x == 15 ? 1.0f : 0.8f);
    if (y > 20 && y < 25) {
        int lx = 16 - (y - 20) * 2, rx = 16 + (y - 20) * 2;
        if ((x >= lx - 3 && x < lx && y > 21) || (x > rx - 1 && x <= rx + 2 && y < 24))
            return shade(72, 140, 48, 0.9f);
    }
    return CLEAR;
}

static unsigned int sandstone_px(int tile, int x, int y, int top)
{
    float n = fbm(tile, 47, x, y, 4, 3), s;
    if (top) return shade(220, 206, 152, 0.9f + 0.12f * n);
    s = 0.88f + 0.06f * sinf(y * 0.9f) + 0.08f * n;
    if (y < 4) s *= 1.06f;
    if (y > 27) s *= 0.86f;
    if (y == 4 || y == 27) s *= 0.9f;
    return shade(216, 201, 148, s);
}

static unsigned int sun_px(int x, int y)
{
    float dx = x + 0.5f - 16.0f, dy = y + 0.5f - 16.0f, d = sqrtf(dx * dx + dy * dy);
    if (d < 9.0f) return rgba(255, 252, 220, 255);
    if (d < 15.0f) return rgba(255, 236, 150, 255.0f * (1.0f - (d - 9.0f) / 6.0f) * 0.9f);
    return CLEAR;
}

static unsigned int moon_px(int tile, int x, int y)
{
    float dx = x + 0.5f - 16.0f, dy = y + 0.5f - 16.0f, d = sqrtf(dx * dx + dy * dy);
    if (d < 9.0f) {
        float s = 0.85f + 0.15f * fbm(tile, 48, x, y, 4, 2);
        float cx = x - 12.0f, cy = y - 13.0f, cx2 = x - 19.0f, cy2 = y - 19.0f;
        if (cx * cx + cy * cy < 6.0f || cx2 * cx2 + cy2 * cy2 < 4.0f) s *= 0.78f;
        return shade(224, 228, 238, s);
    }
    if (d < 12.0f) return rgba(200, 210, 240, 120.0f * (1.0f - (d - 9.0f) / 3.0f));
    return CLEAR;
}

static unsigned int texel(int tile, int x, int y)
{
    switch (tile) {
    case 0:  return grass_px(tile, x, y);
    case 1:  return side_with_top(tile, x, y, 0);
    case 2:  return dirt_px(tile, x, y);
    case 3:  return stone_px(tile, x, y, 125, 125, 125);
    case 4: {
        float n = fbm(tile, 49, x, y, 4, 3);
        float s = 0.9f + 0.12f * n + 0.06f * rnd(tile, 50, x, y) + 0.035f * sinf(y * 0.8f + n * 6.0f);
        return shade(219, 207, 163, s);
    }
    case 5:  return log_side_px(tile, x, y, 0);
    case 6:  return log_top_px(tile, x, y);
    case 7:  return leaves_px(tile, x, y, 56, 118, 40);
    case 8:  return plank_px(tile, x, y);
    case 9:  return glass_px(x, y);
    case 10: return brick_px(tile, x, y);
    case 11: return water_px(tile, x, y);
    case 12: return cobble_px(tile, x, y);
    case 13: {
        float n = fbm(tile, 51, x, y, 8, 2);
        return shade(110, 110, 112, 0.3f + 0.9f * n + 0.15f * rnd(tile, 52, x, y));
    }
    case 14: return gravel_px(tile, x, y);
    case 15: return shade(240, 245, 252, 0.9f + 0.1f * fbm(tile, 53, x, y, 4, 2));
    case 16: return side_with_top(tile, x, y, 1);
    case 17: {
        float s = 0.9f + 0.12f * fbm(tile, 54, x, y, 4, 2);
        if (fabsf(vnoise(tile, 55, x, y, 4, 4) - 0.5f) < 0.02f) return shade(232, 242, 255, 1.0f);
        return shade(150, 192, 246, s);
    }
    case 18: return cactus_side_px(tile, x, y);
    case 19: return cactus_top_px(tile, x, y);
    case 20: return ore_px(tile, x, y, 44, 44, 46);
    case 21: return ore_px(tile, x, y, 216, 176, 148);
    case 22: return ore_px(tile, x, y, 250, 214, 60);
    case 23: return ore_px(tile, x, y, 92, 228, 228);
    case 24: return tall_grass_px(tile, x, y);
    case 25: return flower_px(tile, x, y, 212, 40, 44, 250, 220, 110);
    case 26: return flower_px(tile, x, y, 244, 210, 40, 200, 120, 30);
    case 27: return log_side_px(tile, x, y, 1);
    case 28: return leaves_px(tile, x, y, 98, 150, 62);
    case 29: return sandstone_px(tile, x, y, 0);
    case 30: return sandstone_px(tile, x, y, 1);
    case 31: return stone_bricks_px(tile, x, y);
    case TILE_SUN:  return sun_px(x, y);
    case TILE_MOON: return moon_px(tile, x, y);
    default: return ((x / 4 + y / 4) & 1) ? rgba(255, 0, 255, 255) : rgba(0, 0, 0, 255);
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

/* ------------------------------------------------------------------- font */

/* 5x7 glyphs for ASCII 32..95, one byte per row, bit 4 = leftmost pixel */
static const unsigned char FONT[64][7] = {
    {0x00,0x00,0x00,0x00,0x00,0x00,0x00}, {0x04,0x04,0x04,0x04,0x04,0x00,0x04}, /* space ! */
    {0x0A,0x0A,0x00,0x00,0x00,0x00,0x00}, {0x0A,0x1F,0x0A,0x0A,0x1F,0x0A,0x00}, /* " # */
    {0x04,0x0F,0x14,0x0E,0x05,0x1E,0x04}, {0x18,0x19,0x02,0x04,0x08,0x13,0x03}, /* $ % */
    {0x08,0x14,0x14,0x08,0x15,0x12,0x0D}, {0x04,0x04,0x00,0x00,0x00,0x00,0x00}, /* & ' */
    {0x02,0x04,0x08,0x08,0x08,0x04,0x02}, {0x08,0x04,0x02,0x02,0x02,0x04,0x08}, /* ( ) */
    {0x00,0x04,0x15,0x0E,0x15,0x04,0x00}, {0x00,0x04,0x04,0x1F,0x04,0x04,0x00}, /* * + */
    {0x00,0x00,0x00,0x00,0x06,0x04,0x08}, {0x00,0x00,0x00,0x1F,0x00,0x00,0x00}, /* , - */
    {0x00,0x00,0x00,0x00,0x00,0x0C,0x0C}, {0x01,0x01,0x02,0x04,0x08,0x10,0x10}, /* . / */
    {0x0E,0x11,0x13,0x15,0x19,0x11,0x0E}, {0x04,0x0C,0x04,0x04,0x04,0x04,0x0E}, /* 0 1 */
    {0x0E,0x11,0x01,0x02,0x04,0x08,0x1F}, {0x1F,0x02,0x04,0x02,0x01,0x11,0x0E}, /* 2 3 */
    {0x02,0x06,0x0A,0x12,0x1F,0x02,0x02}, {0x1F,0x10,0x1E,0x01,0x01,0x11,0x0E}, /* 4 5 */
    {0x06,0x08,0x10,0x1E,0x11,0x11,0x0E}, {0x1F,0x01,0x02,0x04,0x08,0x08,0x08}, /* 6 7 */
    {0x0E,0x11,0x11,0x0E,0x11,0x11,0x0E}, {0x0E,0x11,0x11,0x0F,0x01,0x02,0x0C}, /* 8 9 */
    {0x00,0x0C,0x0C,0x00,0x0C,0x0C,0x00}, {0x00,0x0C,0x0C,0x00,0x0C,0x04,0x08}, /* : ; */
    {0x02,0x04,0x08,0x10,0x08,0x04,0x02}, {0x00,0x00,0x1F,0x00,0x1F,0x00,0x00}, /* < = */
    {0x08,0x04,0x02,0x01,0x02,0x04,0x08}, {0x0E,0x11,0x01,0x02,0x04,0x00,0x04}, /* > ? */
    {0x0E,0x11,0x01,0x0D,0x15,0x15,0x0E}, {0x0E,0x11,0x11,0x11,0x1F,0x11,0x11}, /* @ A */
    {0x1E,0x11,0x11,0x1E,0x11,0x11,0x1E}, {0x0E,0x11,0x10,0x10,0x10,0x11,0x0E}, /* B C */
    {0x1C,0x12,0x11,0x11,0x11,0x12,0x1C}, {0x1F,0x10,0x10,0x1E,0x10,0x10,0x1F}, /* D E */
    {0x1F,0x10,0x10,0x1E,0x10,0x10,0x10}, {0x0E,0x11,0x10,0x17,0x11,0x11,0x0F}, /* F G */
    {0x11,0x11,0x11,0x1F,0x11,0x11,0x11}, {0x0E,0x04,0x04,0x04,0x04,0x04,0x0E}, /* H I */
    {0x07,0x02,0x02,0x02,0x02,0x12,0x0C}, {0x11,0x12,0x14,0x18,0x14,0x12,0x11}, /* J K */
    {0x10,0x10,0x10,0x10,0x10,0x10,0x1F}, {0x11,0x1B,0x15,0x15,0x11,0x11,0x11}, /* L M */
    {0x11,0x11,0x19,0x15,0x13,0x11,0x11}, {0x0E,0x11,0x11,0x11,0x11,0x11,0x0E}, /* N O */
    {0x1E,0x11,0x11,0x1E,0x10,0x10,0x10}, {0x0E,0x11,0x11,0x11,0x15,0x12,0x0D}, /* P Q */
    {0x1E,0x11,0x11,0x1E,0x14,0x12,0x11}, {0x0F,0x10,0x10,0x0E,0x01,0x01,0x1E}, /* R S */
    {0x1F,0x04,0x04,0x04,0x04,0x04,0x04}, {0x11,0x11,0x11,0x11,0x11,0x11,0x0E}, /* T U */
    {0x11,0x11,0x11,0x11,0x11,0x0A,0x04}, {0x11,0x11,0x11,0x15,0x15,0x15,0x0A}, /* V W */
    {0x11,0x11,0x0A,0x04,0x0A,0x11,0x11}, {0x11,0x11,0x11,0x0A,0x04,0x04,0x04}, /* X Y */
    {0x1F,0x01,0x02,0x04,0x08,0x10,0x1F}, {0x0E,0x08,0x08,0x08,0x08,0x08,0x0E}, /* Z [ */
    {0x10,0x10,0x08,0x04,0x02,0x01,0x01}, {0x0E,0x02,0x02,0x02,0x02,0x02,0x0E}, /* \ ] */
    {0x04,0x0A,0x11,0x00,0x00,0x00,0x00}, {0x00,0x00,0x00,0x00,0x00,0x00,0x1F}, /* ^ _ */
};

void font_generate(unsigned int *tex)
{
    int g, row, col;
    memset(tex, 0, FONT_TEX_W * FONT_TEX_H * 4);
    for (g = 0; g < 64; g++) {
        int ox = (g % 16) * 8, oy = (g / 16) * 8;
        for (row = 0; row < 7; row++)
            for (col = 0; col < 5; col++)
                if (FONT[g][row] & (0x10 >> col))
                    tex[(oy + row) * FONT_TEX_W + ox + col] = 0xFFFFFFFFu;
    }
}
