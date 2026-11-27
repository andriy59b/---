/* Procedural pixel-art texture atlas and HUD font (all generated in code). */
#include "texgen.h"

#include <math.h>
#include <string.h>

/* ------------------------------------------------------------------- font */

#define FONT_GLYPHS 94

/* 5x7 glyphs, one byte per row, bit 4 = leftmost pixel.
 * 0..63: ASCII 32..95 (lower case is drawn as upper case) */
static const unsigned char FONT[FONT_GLYPHS][7] = {
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
    /* 64.. Ukrainian letters that differ from Latin ones */
    {0x1F,0x10,0x10,0x1E,0x11,0x11,0x1E}, {0x1F,0x10,0x10,0x10,0x10,0x10,0x10}, /* Б Г */
    {0x01,0x1F,0x10,0x10,0x10,0x10,0x10}, {0x06,0x0A,0x0A,0x0A,0x0A,0x1F,0x11}, /* Ґ Д */
    {0x0E,0x11,0x10,0x1C,0x10,0x11,0x0E}, {0x15,0x15,0x15,0x0E,0x15,0x15,0x15}, /* Є Ж */
    {0x0E,0x11,0x01,0x06,0x01,0x11,0x0E}, {0x11,0x11,0x13,0x15,0x19,0x11,0x11}, /* З И */
    {0x0A,0x00,0x0E,0x04,0x04,0x04,0x0E}, {0x0A,0x04,0x11,0x13,0x15,0x19,0x11}, /* Ї Й */
    {0x07,0x09,0x09,0x09,0x09,0x09,0x11}, {0x1F,0x11,0x11,0x11,0x11,0x11,0x11}, /* Л П */
    {0x11,0x11,0x11,0x0F,0x01,0x11,0x0E}, {0x04,0x0E,0x15,0x15,0x15,0x0E,0x04}, /* У Ф */
    {0x12,0x12,0x12,0x12,0x12,0x1F,0x01}, {0x11,0x11,0x11,0x0F,0x01,0x01,0x01}, /* Ц Ч */
    {0x15,0x15,0x15,0x15,0x15,0x15,0x1F}, {0x15,0x15,0x15,0x15,0x15,0x1F,0x01}, /* Ш Щ */
    {0x10,0x10,0x10,0x1E,0x11,0x11,0x1E}, {0x12,0x15,0x15,0x1D,0x15,0x15,0x12}, /* Ь Ю */
    {0x0F,0x11,0x11,0x0F,0x05,0x09,0x11},                                       /* Я */
    /* 85.. symbols: hryvnia, arrows, PSP face buttons */
    {0x0E,0x01,0x1F,0x04,0x1F,0x10,0x0E}, {0x04,0x0E,0x15,0x04,0x04,0x04,0x04}, /* ₴ ↑ */
    {0x04,0x04,0x04,0x04,0x15,0x0E,0x04}, {0x00,0x04,0x08,0x1F,0x08,0x04,0x00}, /* ↓ ← */
    {0x00,0x04,0x02,0x1F,0x02,0x04,0x00}, {0x00,0x11,0x0A,0x04,0x0A,0x11,0x00}, /* → ✕ */
    {0x00,0x0E,0x11,0x11,0x11,0x0E,0x00}, {0x04,0x04,0x0A,0x0A,0x11,0x1F,0x00}, /* ○ △ */
    {0x00,0x1F,0x11,0x11,0x11,0x1F,0x00},                                       /* □ */
};

unsigned int utf8_next(const char **p)
{
    const unsigned char *s = (const unsigned char *)*p;
    unsigned int c = s[0];
    if (c < 0x80) { *p += 1; return c; }
    if ((c & 0xE0) == 0xC0 && s[1]) { *p += 2; return ((c & 0x1F) << 6) | (s[1] & 0x3F); }
    if ((c & 0xF0) == 0xE0 && s[1] && s[2]) {
        *p += 3;
        return ((c & 0x0F) << 12) | ((s[1] & 0x3F) << 6) | (s[2] & 0x3F);
    }
    *p += 1;
    return '?';
}

int font_glyph(unsigned int cp)
{
    static const unsigned short UKR[21] = {
        0x411, 0x413, 0x490, 0x414, 0x404, 0x416, 0x417, 0x418, 0x407, 0x419, 0x41B,
        0x41F, 0x423, 0x424, 0x426, 0x427, 0x428, 0x429, 0x42C, 0x42E, 0x42F,
    };
    static const unsigned short SYM[9] = { 0x20B4, 0x2191, 0x2193, 0x2190, 0x2192, 0x2715, 0x25CB, 0x25B3, 0x25A1 };
    int i;
    if (cp >= 'a' && cp <= 'z') cp -= 32;
    if (cp >= 0x430 && cp <= 0x44F) cp -= 0x20;      /* Cyrillic lower -> upper */
    else if (cp >= 0x450 && cp <= 0x45F) cp -= 0x50;
    else if (cp == 0x491) cp = 0x490;
    switch (cp) {                                     /* letters shared with Latin */
    case 0x410: cp = 'A'; break; case 0x412: cp = 'B'; break; case 0x415: case 0x401: cp = 'E'; break;
    case 0x41A: cp = 'K'; break; case 0x41C: cp = 'M'; break; case 0x41D: cp = 'H'; break;
    case 0x41E: cp = 'O'; break; case 0x420: cp = 'P'; break; case 0x421: cp = 'C'; break;
    case 0x422: cp = 'T'; break; case 0x425: cp = 'X'; break; case 0x406: cp = 'I'; break;
    case 0x2018: case 0x2019: case 0x2BC: cp = '\''; break;
    case 0x201C: case 0x201D: case 0xAB: case 0xBB: cp = '"'; break;
    case 0x2013: case 0x2014: cp = '-'; break;
    case 0x2026: cp = '.'; break;
    case 0xA0: cp = ' '; break;
    case 0x2022: case 0xB7: cp = '*'; break;
    default: break;
    }
    if (cp >= 32 && cp <= 95) return (int)cp - 32;
    for (i = 0; i < 21; i++) if (UKR[i] == cp) return 64 + i;
    for (i = 0; i < 9; i++) if (SYM[i] == cp) return 85 + i;
    return '?' - 32;
}

void font_generate(unsigned int *tex)
{
    int g, row, col;
    memset(tex, 0, FONT_TEX_W * FONT_TEX_H * 4);
    for (g = 0; g < FONT_GLYPHS; g++) {
        int ox = (g % 16) * 8, oy = (g / 16) * 8;
        for (row = 0; row < 7; row++)
            for (col = 0; col < 5; col++)
                if (FONT[g][row] & (0x10 >> col))
                    tex[(oy + row) * FONT_TEX_W + ox + col] = 0xFFFFFFFFu;
    }
}

/* Is pixel (x, y) of the text set, with text starting at (0, 0)? */
static int __attribute__((unused)) text_pixel(const char *text, int x, int y)
{
    const char *p = text;
    int i = 0;
    if (y < 0 || y >= 7 || x < 0) return 0;
    while (*p) {
        int g = font_glyph(utf8_next(&p));
        if (x >= i * 6 && x < i * 6 + 5) return (FONT[g][y] >> (4 - (x - i * 6))) & 1;
        i++;
    }
    return 0;
}

/* ------------------------------------------------------------- canvas */

static unsigned int *A;
static int ox, oy, cur_tile;

static unsigned int C(int r, int g, int b) { return gfx_rgba(r, g, b, 255); }
static unsigned int CA(int r, int g, int b, int a) { return gfx_rgba(r, g, b, a); }

static void begin(int tile)
{
    cur_tile = tile;
    ox = (tile % TEX_TILES) * TEX_TILE;
    oy = (tile / TEX_TILES) * TEX_TILE;
}

static void px(int x, int y, unsigned int c)
{
    if (x < 0 || y < 0 || x >= TEX_TILE || y >= TEX_TILE) return;
    A[(oy + y) * TEX_SIZE + ox + x] = c;
}

static unsigned int getpx(int x, int y)
{
    if (x < 0 || y < 0 || x >= TEX_TILE || y >= TEX_TILE) return 0;
    return A[(oy + y) * TEX_SIZE + ox + x];
}

static unsigned int hsh(int salt, int x, int y)
{
    unsigned int h = (unsigned int)cur_tile * 0x9E3779B9u ^ (unsigned int)salt * 0x85EBCA6Bu;
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

static float rn(int salt, int x, int y) { return (hsh(salt, x, y) & 0xFFFF) / 65535.0f; }

static float sm(float t) { return t * t * (3.0f - 2.0f * t); }

/* tileable value noise with `cells` lattice cells across the tile */
static float vn(int salt, int x, int y, int cells)
{
    float gx = (x + 0.5f) * cells / TEX_TILE, gy = (y + 0.5f) * cells / TEX_TILE;
    int ix = (int)gx, iy = (int)gy;
    float fx = sm(gx - ix), fy = sm(gy - iy);
    float a = rn(salt, ix % cells, iy % cells), b = rn(salt, (ix + 1) % cells, iy % cells);
    float c = rn(salt, ix % cells, (iy + 1) % cells), d = rn(salt, (ix + 1) % cells, (iy + 1) % cells);
    float top = a + (b - a) * fx, bot = c + (d - c) * fx;
    return top + (bot - top) * fy;
}

static float fbm(int salt, int x, int y, int cells)
{
    return vn(salt, x, y, cells) * 0.55f + vn(salt + 7, x, y, cells * 2) * 0.3f + vn(salt + 13, x, y, cells * 4) * 0.15f;
}

static unsigned int mul(unsigned int c, float s) { return gfx_mul(c, s); }

static unsigned int blend(unsigned int a, unsigned int b, float t)
{
    int r = (int)((a & 0xFF) + (((int)(b & 0xFF)) - (int)(a & 0xFF)) * t);
    int g = (int)(((a >> 8) & 0xFF) + (((int)((b >> 8) & 0xFF)) - (int)((a >> 8) & 0xFF)) * t);
    int bl = (int)(((a >> 16) & 0xFF) + (((int)((b >> 16) & 0xFF)) - (int)((a >> 16) & 0xFF)) * t);
    return gfx_rgba(r, g, bl, (int)(a >> 24));
}

static void rect(int x0, int y0, int x1, int y1, unsigned int c)
{
    int x, y;
    for (y = y0; y <= y1; y++)
        for (x = x0; x <= x1; x++) px(x, y, c);
}

static void frame(int x0, int y0, int x1, int y1, unsigned int c)
{
    int i;
    for (i = x0; i <= x1; i++) { px(i, y0, c); px(i, y1, c); }
    for (i = y0; i <= y1; i++) { px(x0, i, c); px(x1, i, c); }
}

/* fill a rect with a colour modulated by noise */
static void grain(int x0, int y0, int x1, int y1, unsigned int c, float amount, int salt, int cells)
{
    int x, y;
    for (y = y0; y <= y1; y++)
        for (x = x0; x <= x1; x++) {
            float n = cells ? fbm(salt, x, y, cells) : 0.5f;
            float s = 1.0f + (n - 0.5f) * amount + (rn(salt + 1, x, y) - 0.5f) * amount * 0.5f;
            px(x, y, mul(c, s));
        }
}

static void line(int x0, int y0, int x1, int y1, unsigned int c)
{
    int dx = x1 > x0 ? x1 - x0 : x0 - x1, dy = y1 > y0 ? y0 - y1 : y1 - y0;
    int sx = x0 < x1 ? 1 : -1, sy = y0 < y1 ? 1 : -1, err = dx + dy;
    for (;;) {
        int e2;
        px(x0, y0, c);
        if (x0 == x1 && y0 == y1) break;
        e2 = 2 * err;
        if (e2 >= dy) { err += dy; x0 += sx; }
        if (e2 <= dx) { err += dx; y0 += sy; }
    }
}

static void disc(float cx, float cy, float r, unsigned int c)
{
    int x, y;
    for (y = (int)(cy - r - 1); y <= (int)(cy + r + 1); y++)
        for (x = (int)(cx - r - 1); x <= (int)(cx + r + 1); x++) {
            float dx = x + 0.5f - cx, dy = y + 0.5f - cy;
            if (dx * dx + dy * dy <= r * r) px(x, y, c);
        }
}

static void text(int x, int y, const char *s, unsigned int c)
{
    const char *p = s;
    int i = 0, row, col;
    while (*p) {
        int g = font_glyph(utf8_next(&p));
        for (row = 0; row < 7; row++)
            for (col = 0; col < 5; col++)
                if (FONT[g][row] & (0x10 >> col)) px(x + i * 6 + col, y + row, c);
        i++;
    }
}

static int text_w(const char *s)
{
    const char *p = s;
    int n = 0;
    while (*p) { utf8_next(&p); n++; }
    return n * 6 - 1;
}

/* simple 3-step bevel for pixel-art edges */
static void bevel(int x0, int y0, int x1, int y1, float hi, float lo)
{
    int i;
    for (i = x0; i <= x1; i++) {
        px(i, y0, mul(getpx(i, y0), hi));
        px(i, y1, mul(getpx(i, y1), lo));
    }
    for (i = y0 + 1; i < y1; i++) {
        px(x0, i, mul(getpx(x0, i), hi));
        px(x1, i, mul(getpx(x1, i), lo));
    }
}

/* -------------------------------------------------------------- ground */

static void t_white(void) { grain(0, 0, 63, 63, C(238, 238, 238), 0.06f, 1, 4); }

static void t_asphalt(int salt)
{
    int x, y, i;
    grain(0, 0, 63, 63, C(78, 78, 82), 0.32f, salt, 8);
    for (y = 0; y < 64; y++)
        for (x = 0; x < 64; x++) {
            float r = rn(salt + 3, x, y);
            if (r > 0.985f) px(x, y, C(128, 126, 122));
            else if (r < 0.012f) px(x, y, C(44, 44, 46));
        }
    /* a crack and a patch */
    for (i = 0; i < 26; i++) {
        int cx = 10 + i + (int)(4 * sinf(i * 0.5f)), cy = 40 + (int)(6 * sinf(i * 0.27f + 1.0f));
        px(cx, cy, C(40, 40, 42));
        if (i % 5 == 0) px(cx, cy + 1, C(40, 40, 42));
    }
    for (y = 12; y < 26; y++)
        for (x = 40; x < 58; x++)
            if (fbm(salt + 5, x, y, 8) > 0.45f) px(x, y, mul(getpx(x, y), 0.82f));
}

static void t_road_line(void)
{
    int x, y;
    t_asphalt(11);
    for (y = 29; y <= 34; y++)
        for (x = 0; x < 40; x++)
            if (rn(12, x, y) > 0.1f) px(x, y, mul(C(236, 234, 220), 0.9f + 0.1f * rn(13, x, y)));
}

static void t_crosswalk(void)
{
    int x, y;
    t_asphalt(14);
    for (y = 0; y < 64; y++)
        for (x = 0; x < 64; x++)
            if ((x / 8) % 2 == 0 && rn(15, x, y) > 0.08f) px(x, y, mul(C(236, 234, 220), 0.88f + 0.12f * rn(16, x, y)));
}

static void t_sidewalk(void)
{
    int x, y;
    grain(0, 0, 63, 63, C(176, 172, 164), 0.18f, 20, 8);
    for (y = 0; y < 64; y++)
        for (x = 0; x < 64; x++) {
            int lx = x % 32, ly = y % 32;
            float tile = 0.92f + 0.12f * rn(21, x / 32, y / 32);
            px(x, y, mul(getpx(x, y), tile));
            if (lx == 31 || ly == 31) px(x, y, rn(22, x, y) > 0.6f ? C(92, 118, 64) : C(120, 118, 112));
            else if (lx == 0 || ly == 0) px(x, y, mul(getpx(x, y), 1.1f));
        }
    line(36, 6, 44, 20, C(120, 118, 112));
    line(44, 20, 52, 24, C(120, 118, 112));
}

static void t_curb(void)
{
    int x;
    grain(0, 0, 63, 63, C(190, 188, 182), 0.16f, 25, 8);
    rect(0, 0, 63, 5, C(214, 212, 206));
    for (x = 0; x < 64; x++) px(x, 6, C(150, 148, 142));
    for (x = 0; x < 64; x += 16) rect(x, 7, x, 63, C(140, 138, 132));
    rect(0, 56, 63, 63, C(110, 108, 104));
}

static void t_grass(int salt, int bare)
{
    int x, y;
    grain(0, 0, 63, 63, C(88, 140, 58), 0.4f, salt, 8);
    for (y = 0; y < 64; y++)
        for (x = 0; x < 64; x++) {
            float r = rn(salt + 2, x, y);
            if (bare && fbm(salt + 4, x, y, 4) > 0.58f) px(x, y, mul(C(128, 104, 74), 0.85f + 0.3f * rn(salt + 5, x, y)));
            else if (r > 0.93f) px(x, y, C(122, 178, 78));
            else if (r < 0.05f) px(x, y, C(60, 100, 40));
            else if (r > 0.995f) px(x, y, (x & 1) ? C(240, 220, 60) : C(240, 240, 240));
        }
}

static void t_dirt(void)
{
    int x, y;
    grain(0, 0, 63, 63, C(138, 110, 78), 0.35f, 30, 8);
    for (y = 0; y < 64; y++)
        for (x = 0; x < 64; x++)
            if (rn(31, x, y) > 0.96f) px(x, y, C(170, 150, 120));
}

/* ------------------------------------------------------------- facades */

static const unsigned int CURTAINS[4] = { 0xFF4060C8u, 0xFF30A0D8u, 0xFFC8B4A0u, 0xFF5080A0u };

static void window(int x0, int y0, int x1, int y1, unsigned int frame_c, int curtain, int salt)
{
    int x, y, mid = (x0 + x1) / 2;
    rect(x0, y0, x1, y1, frame_c);
    for (y = y0 + 2; y <= y1 - 2; y++)
        for (x = x0 + 2; x <= x1 - 2; x++) {
            float t = (float)(y - y0) / (float)(y1 - y0);
            unsigned int g = blend(C(70, 96, 130), C(30, 44, 64), t);
            if (x - y > x0 - y0 - 2 && x - y < x0 - y0 + 3) g = blend(g, C(200, 220, 240), 0.5f);
            px(x, y, g);
        }
    rect(mid, y0, mid, y1, frame_c);
    rect(x0, y0 + (y1 - y0) / 3, x1, y0 + (y1 - y0) / 3, frame_c);
    if (curtain >= 0) {
        unsigned int cc = CURTAINS[curtain & 3];
        for (y = y0 + 2; y <= y1 - 2; y++) {
            int w = 3 + (int)(2 * sinf(y * 0.7f + salt));
            for (x = x0 + 2; x < x0 + 2 + w; x++) px(x, y, mul(cc, 0.8f + 0.3f * ((x + y) & 1)));
            for (x = x1 - 1 - w; x <= x1 - 2; x++) px(x, y, mul(cc, 0.8f + 0.3f * ((x + y) & 1)));
        }
    }
    rect(x0 - 1, y1 + 1, x1 + 1, y1 + 2, C(150, 146, 138)); /* sill */
}

static void panel_base(int salt)
{
    grain(0, 0, 63, 63, C(186, 180, 166), 0.16f, salt, 8);
    rect(0, 0, 63, 0, C(132, 128, 120));
    rect(0, 0, 0, 63, C(132, 128, 120));
    rect(0, 63, 63, 63, C(150, 146, 136));
    {   /* rain streaks */
        int x, y;
        for (x = 0; x < 64; x++)
            if (rn(salt + 9, x, 0) > 0.8f)
                for (y = 40; y < 64; y++)
                    if (rn(salt + 10, x, y) > 0.3f) px(x, y, mul(getpx(x, y), 0.9f));
    }
}

static void t_panel_win(int variant)
{
    panel_base(40 + variant);
    window(14, 14, 49, 46, C(228, 226, 218), variant == 2 ? -1 : variant, variant);
    if (variant == 2) { /* flower pots and a cat on the sill */
        disc(22, 44, 3, C(60, 150, 60));
        rect(20, 45, 24, 47, C(180, 90, 50));
        disc(40, 43, 3, C(80, 80, 80));
        px(38, 39, C(80, 80, 80));
        px(42, 39, C(80, 80, 80));
    }
}

static void t_panel_blank(void)
{
    panel_base(45);
    rect(31, 0, 32, 63, C(150, 146, 136));
}

static void t_panel_door(void)
{
    panel_base(46);
    rect(16, 8, 47, 63, C(120, 116, 108));
    rect(19, 12, 44, 63, C(96, 62, 44));
    grain(20, 13, 43, 63, C(110, 70, 48), 0.2f, 47, 4);
    rect(26, 18, 37, 28, C(50, 60, 70));
    frame(26, 18, 37, 28, C(70, 44, 30));
    rect(38, 40, 41, 42, C(200, 190, 120));
    rect(25, 2, 38, 6, C(40, 70, 140));
    text(27, 0 + 1, "2", C(240, 240, 240));
    bevel(19, 12, 44, 63, 1.15f, 0.8f);
}

static void t_balcony(void)
{
    int x;
    panel_base(48);
    rect(0, 0, 63, 63, CA(0, 0, 0, 0));
    /* railing panel: ribbed concrete lower part */
    grain(0, 24, 63, 63, C(176, 170, 156), 0.15f, 49, 8);
    for (x = 0; x < 64; x += 4) rect(x, 26, x, 63, C(150, 144, 132));
    rect(0, 22, 63, 25, C(130, 126, 118));
    /* laundry on a line */
    line(0, 6, 63, 4, C(60, 60, 60));
    rect(8, 7, 15, 20, C(220, 220, 230));
    rect(20, 6, 27, 16, C(200, 60, 60));
    rect(34, 6, 40, 18, C(70, 110, 200));
    rect(48, 5, 56, 14, C(240, 200, 80));
}

static void t_brick(int with_window)
{
    int x, y;
    for (y = 0; y < 64; y++)
        for (x = 0; x < 64; x++) {
            int row = y / 6, off = (row & 1) ? 8 : 0, bx = (x + off) / 16;
            if (y % 6 == 5 || (x + off) % 16 == 15) px(x, y, mul(C(190, 182, 168), 0.85f + 0.2f * rn(51, x, y)));
            else px(x, y, mul(C(158, 66, 50), 0.82f + 0.25f * rn(52, bx, row) + 0.1f * (rn(53, x, y) - 0.5f)));
        }
    if (with_window) window(16, 14, 47, 46, C(236, 236, 230), 3, 2);
}

static void t_roof_flat(void)
{
    int x, y;
    grain(0, 0, 63, 63, C(64, 62, 60), 0.4f, 55, 4);
    for (y = 0; y < 64; y++)
        for (x = 0; x < 64; x++)
            if (fbm(56, x, y, 4) > 0.62f) px(x, y, C(48, 46, 46));
}

static void t_slate(void)
{
    int x, y;
    for (y = 0; y < 64; y++)
        for (x = 0; x < 64; x++) {
            float wave = 0.82f + 0.2f * sinf(x * 0.785f);
            int row = y / 16;
            float s = wave * (0.9f + 0.15f * rn(57, x / 32 + row * 3, row));
            if (y % 16 == 15) s *= 0.7f;
            if (fbm(58, x, y, 8) > 0.66f) s *= 0.85f; /* moss/dirt */
            px(x, y, mul(C(150, 152, 150), s));
        }
}

static void t_roof_red(void)
{
    int x, y;
    for (y = 0; y < 64; y++)
        for (x = 0; x < 64; x++) {
            int row = y / 8, off = (row & 1) ? 4 : 0, lx = (x + off) % 8, ly = y % 8;
            float d = (lx - 3.5f) * (lx - 3.5f) / 16.0f + ly / 8.0f;
            float s = 0.85f + 0.2f * rn(59, (x + off) / 8, row);
            s *= d > 1.0f || ly == 7 ? 0.65f : 1.1f - 0.25f * ly / 8.0f;
            px(x, y, mul(C(170, 76, 52), s));
        }
}

static void t_plaster(int with_window)
{
    int x, y;
    grain(0, 0, 63, 63, C(240, 238, 230), 0.08f, 60, 8);
    for (y = 52; y < 64; y++)
        for (x = 0; x < 64; x++) px(x, y, mul(C(70, 110, 180), 0.85f + 0.2f * rn(61, x, y)));
    rect(0, 51, 63, 51, C(50, 80, 140));
    line(4, 20, 9, 30, C(200, 196, 186));
    if (with_window) {
        rect(10, 12, 15, 42, C(50, 100, 170)); /* shutters */
        rect(48, 12, 53, 42, C(50, 100, 170));
        for (y = 14; y < 42; y += 4) { line(11, y, 14, y + 2, C(30, 70, 130)); line(49, y, 52, y + 2, C(30, 70, 130)); }
        window(17, 12, 46, 42, C(70, 120, 190), 2, 5);
        rect(24, 38, 28, 42, C(190, 90, 50));
        disc(26, 36, 3, C(220, 40, 40));
    }
}

static void t_fence(void)
{
    int x, y;
    rect(0, 0, 63, 63, CA(0, 0, 0, 0));
    for (x = 0; x < 64; x++) {
        int b = x / 8, lx = x % 8, top = 6 - (lx < 4 ? lx : 7 - lx) + (int)(rn(62, b, 0) * 3);
        if (lx == 7) continue;
        for (y = top; y < 64; y++) {
            float s = 0.85f + 0.2f * rn(63, x, y) - (lx == 0 ? 0.1f : 0.0f);
            px(x, y, mul(C(60, 130, 90), s));
        }
    }
    rect(0, 16, 63, 19, C(90, 70, 50));
    rect(0, 48, 63, 51, C(90, 70, 50));
}

static void t_garage_door(void)
{
    int x, y;
    grain(0, 0, 63, 63, C(76, 112, 84), 0.15f, 64, 8);
    frame(0, 0, 63, 63, C(60, 60, 58));
    frame(1, 1, 62, 62, C(60, 60, 58));
    rect(31, 2, 32, 61, C(40, 60, 46));
    for (y = 2; y < 62; y += 10) rect(2, y, 61, y, C(66, 98, 74));
    for (y = 0; y < 64; y++)
        for (x = 0; x < 64; x++)
            if (fbm(65, x, y, 8) > 0.64f) px(x, y, mul(C(150, 86, 44), 0.8f + 0.3f * rn(66, x, y)));
    rect(26, 30, 29, 34, C(40, 40, 40));
    rect(34, 30, 37, 34, C(40, 40, 40));
    rect(29, 31, 34, 33, C(160, 150, 120));
}

static void t_metal(void)
{
    int x, y;
    for (y = 0; y < 64; y++)
        for (x = 0; x < 64; x++) {
            float s = 0.82f + 0.2f * sinf(x * 0.785f) + 0.06f * (rn(67, x, y) - 0.5f);
            unsigned int c = mul(C(128, 136, 130), s);
            if (fbm(68, x, y, 8) > 0.63f) c = mul(C(146, 84, 44), 0.8f + 0.3f * rn(69, x, y));
            px(x, y, c);
        }
}

static void t_kiosk_front(void)
{
    int x, i;
    grain(0, 0, 63, 63, C(60, 100, 172), 0.12f, 70, 8);
    rect(4, 6, 59, 50, C(40, 50, 64));
    /* shelves with bottles and packs */
    for (i = 0; i < 3; i++) {
        int sy = 18 + i * 13;
        rect(5, sy, 58, sy, C(150, 130, 100));
        for (x = 6; x < 58; x += 4) {
            unsigned int c = i == 0 ? (x % 8 ? C(60, 130, 60) : C(140, 90, 40))
                           : i == 1 ? (x % 12 < 4 ? C(220, 40, 40) : x % 12 < 8 ? C(240, 240, 240) : C(40, 80, 200))
                           : C(200, 160, 40);
            rect(x, sy - (i == 0 ? 9 : 6), x + 2, sy - 1, c);
            if (i == 0) px(x + 1, sy - 11, C(60, 130, 60));
        }
    }
    for (x = 4; x < 60; x += 7) rect(x, 6, x, 50, C(150, 160, 170)); /* grille bars */
    rect(22, 40, 40, 50, C(230, 230, 220));                         /* window hatch */
    text(24, 42, "ЦІНА", C(40, 40, 40));
    rect(0, 52, 63, 63, C(48, 84, 150));
}

static void t_sign(const char *word, unsigned int bg, unsigned int fg)
{
    int w = text_w(word), x0 = (64 - w * 1) / 2;
    rect(0, 0, 63, 63, CA(0, 0, 0, 0));
    rect(0, 16, 63, 47, bg);
    frame(0, 16, 63, 47, mul(bg, 0.6f));
    frame(1, 17, 62, 46, mul(bg, 1.15f));
    text(x0, 28, word, fg);
    text(x0 + 1, 28, word, fg);
}

static void t_tent(void)
{
    int x, y;
    for (y = 0; y < 64; y++)
        for (x = 0; x < 64; x++) {
            unsigned int c = (x / 8) % 2 ? C(240, 240, 236) : C(200, 50, 50);
            px(x, y, mul(c, 0.85f + 0.15f * (1.0f - y / 64.0f) + 0.05f * rn(71, x, y)));
        }
    for (x = 0; x < 64; x++)
        if (x % 8 < 4) rect(x, 58, x, 63, CA(0, 0, 0, 0));
}

static void t_guardrail(void)
{
    int x, y;
    rect(0, 0, 63, 63, CA(0, 0, 0, 0));
    for (x = 0; x < 64; x += 32) rect(x + 2, 20, x + 7, 63, C(110, 112, 116));
    for (y = 10; y < 34; y++)
        for (x = 0; x < 64; x++) {
            float wave = (y < 18 || y > 25) ? 1.05f : 0.78f;
            px(x, y, mul(C(196, 200, 204), wave * (0.9f + 0.12f * rn(72, x, y))));
        }
    rect(0, 10, 63, 10, C(230, 232, 236));
    rect(0, 33, 63, 33, C(120, 122, 126));
    for (x = 12; x < 64; x += 32) rect(x, 14, x + 3, 16, C(240, 60, 40)); /* reflectors */
}

static void t_concrete(void) { grain(0, 0, 63, 63, C(156, 154, 148), 0.22f, 73, 8); }

/* ---------------------------------------------------------- vegetation */

static void foliage_blob(float cx, float cy, float r, unsigned int base, int salt)
{
    int x, y;
    for (y = (int)(cy - r); y <= (int)(cy + r); y++)
        for (x = (int)(cx - r); x <= (int)(cx + r); x++) {
            float dx = (x + 0.5f - cx) / r, dy = (y + 0.5f - cy) / r, d = dx * dx + dy * dy;
            float n = fbm(salt, x, y, 8);
            if (d + (n - 0.5f) * 0.9f < 1.0f && rn(salt + 1, x, y) > 0.08f) {
                float s = 0.7f + 0.45f * n - dy * 0.2f;
                px(x, y, mul(base, s));
            }
        }
}

static void t_birch(void)
{
    int y;
    rect(0, 0, 63, 63, CA(0, 0, 0, 0));
    for (y = 20; y < 64; y++) {
        rect(30, y, 33, y, C(232, 230, 222));
        if (rn(74, 0, y) > 0.7f) rect(30 + (y % 2), y, 31 + (y % 3), y, C(40, 40, 40));
    }
    line(31, 34, 22, 26, C(220, 218, 210));
    line(32, 30, 42, 22, C(220, 218, 210));
    foliage_blob(32, 22, 20, C(120, 170, 70), 75);
    foliage_blob(20, 30, 10, C(110, 160, 64), 76);
    foliage_blob(44, 28, 11, C(116, 166, 68), 77);
}

static void t_poplar(void)
{
    int y;
    rect(0, 0, 63, 63, CA(0, 0, 0, 0));
    for (y = 50; y < 64; y++) rect(30, y, 33, y, C(90, 80, 70));
    for (y = 0; y < 56; y++) {
        float w = 6.0f + 10.0f * sinf((y + 4) / 60.0f * 3.14159f);
        foliage_blob(32, (float)y, w * 0.5f + 2, C(64, 110, 52), 78 + y % 3);
    }
}

static void t_bush(void)
{
    rect(0, 0, 63, 63, CA(0, 0, 0, 0));
    foliage_blob(32, 44, 22, C(76, 128, 56), 80);
    foliage_blob(18, 50, 13, C(70, 120, 50), 81);
    foliage_blob(46, 50, 14, C(80, 132, 58), 82);
}

static void t_sunflowers(void)
{
    int i;
    rect(0, 0, 63, 63, CA(0, 0, 0, 0));
    for (i = 0; i < 4; i++) {
        int cx = 8 + i * 16, cy = 14 + (i & 1) * 8, y;
        for (y = cy; y < 64; y++) rect(cx, y, cx + 1, y, C(70, 120, 44));
        foliage_blob((float)cx - 4, (float)cy + 22, 5, C(80, 136, 50), 83 + i);
        foliage_blob((float)cx + 5, (float)cy + 30, 5, C(80, 136, 50), 87 + i);
        disc((float)cx + 1, (float)cy, 7, C(250, 200, 30));
        disc((float)cx + 1, (float)cy, 4, C(90, 56, 30));
    }
}

static void t_field(int sunflower)
{
    int x, y;
    if (!sunflower) {
        for (y = 0; y < 64; y++)
            for (x = 0; x < 64; x++) {
                float s = 0.8f + 0.25f * rn(90, x, y) - ((x % 8) == 0 ? 0.2f : 0.0f);
                px(x, y, mul(C(214, 178, 88), s));
            }
    } else {
        grain(0, 0, 63, 63, C(74, 118, 50), 0.35f, 91, 8);
        for (y = 4; y < 64; y += 8)
            for (x = 4; x < 64; x += 8) {
                disc((float)x + rn(92, x, y) * 2, (float)y, 2.6f, C(250, 200, 30));
                px(x, y, C(90, 56, 30));
            }
    }
}

static void t_hills(void)
{
    int x, y;
    rect(0, 0, 63, 63, CA(0, 0, 0, 0));
    for (x = 0; x < 64; x++) {
        int h = 30 + (int)(10 * sinf(x * 0.098f) + 6 * sinf(x * 0.29f + 1.0f));
        for (y = h; y < 64; y++) px(x, y, mul(C(96, 120, 92), 0.85f + 0.15f * (y - h) / 30.0f));
        if (rn(93, x, 0) > 0.55f)
            for (y = h - 4 - (int)(rn(94, x, 0) * 5); y < h; y++) px(x, y, C(70, 96, 70));
    }
}

static void t_window_lit(void)
{
    int x, y;
    rect(0, 0, 63, 63, CA(0, 0, 0, 0));
    for (y = 16; y < 45; y++)
        for (x = 16; x < 48; x++) px(x, y, CA(255, 214, 140, 230 - (y - 16) * 2));
}

/* --------------------------------------------------------------- cars */

/* Columns 0..47: plain body side (stretched along the car); columns 48..63:
 * one door panel with its seams and handle, mapped onto each real door. */
static void t_car_side(void)
{
    int x;
    grain(0, 0, 63, 63, C(242, 242, 242), 0.05f, 95, 4);
    rect(0, 26, 63, 26, C(150, 150, 150));     /* waist line */
    rect(48, 2, 48, 60, C(120, 120, 120));     /* door seams */
    rect(63, 2, 63, 60, C(120, 120, 120));
    rect(56, 30, 60, 31, C(110, 110, 110));    /* handle, at the rear edge */
    rect(0, 50, 63, 53, C(60, 60, 60));        /* rubbing strip */
    for (x = 0; x < 64; x++) px(x, 0, C(255, 255, 255));
}

static void t_car_parts(void)
{
    int x, y;
    rect(0, 0, 63, 63, C(40, 40, 40));
    /* chrome grille */
    for (y = 0; y < 12; y++)
        for (x = 0; x < 64; x++) px(x, y, (y % 3 == 0) ? C(90, 90, 96) : mul(C(214, 218, 224), 0.85f + 0.15f * (1.0f - y / 12.0f)));
    /* black plastic grille */
    for (y = 12; y < 24; y++)
        for (x = 0; x < 64; x++) px(x, y, ((x + y) % 4 == 0) ? C(60, 60, 60) : C(24, 24, 26));
    /* rectangular headlight */
    rect(0, 24, 23, 35, C(200, 204, 210));
    rect(1, 25, 22, 34, C(240, 244, 236));
    for (x = 2; x < 22; x += 3) rect(x, 26, x, 33, C(210, 214, 220));
    rect(16, 26, 21, 33, C(250, 180, 60));
    /* round headlight */
    disc(32, 32, 7.5f, C(190, 194, 200));
    disc(32, 32, 6, C(244, 246, 236));
    disc(30, 30, 2, C(255, 255, 255));
    /* tail light */
    rect(40, 24, 63, 33, C(170, 20, 20));
    rect(41, 25, 54, 32, C(220, 40, 30));
    rect(55, 25, 62, 32, C(240, 150, 40));
    /* plate: white with black text, blue strip */
    rect(0, 40, 31, 47, C(240, 240, 236));
    frame(0, 40, 31, 47, C(30, 30, 30));
    text(2, 40, "12-96", C(20, 20, 20));
    rect(40, 34, 47, 39, C(250, 150, 30));      /* indicator */
    for (y = 48; y < 54; y++)                    /* bumper */
        for (x = 0; x < 64; x++) px(x, y, mul(C(60, 60, 62), y == 48 ? 1.6f : 1.0f));
    rect(32, 40, 39, 47, C(20, 20, 22));
}

static void t_tire(void)
{
    int x, y;
    rect(0, 0, 63, 63, CA(0, 0, 0, 0));
    disc(32, 32, 31, C(28, 28, 30));
    disc(32, 32, 20, C(150, 152, 156));
    disc(32, 32, 17, C(110, 112, 116));
    disc(32, 32, 6, C(170, 172, 176));
    for (x = 0; x < 5; x++) {
        float a = x * 1.2566f;
        disc(32 + 11 * cosf(a), 32 + 11 * sinf(a), 2, C(60, 60, 64));
    }
    for (y = 0; y < 64; y++)
        for (x = 0; x < 64; x++) {
            float dx = x - 31.5f, dy = y - 31.5f, d = sqrtf(dx * dx + dy * dy);
            if (d > 27 && d < 30 && ((int)(atan2f(dy, dx) * 10) & 1)) px(x, y, C(48, 48, 50));
        }
}

static void t_car_glass(void)
{
    int x, y;
    for (y = 0; y < 64; y++)
        for (x = 0; x < 64; x++) {
            unsigned int c = blend(C(60, 80, 100), C(20, 28, 36), y / 64.0f);
            if (x - y > 4 && x - y < 14) c = blend(c, C(190, 210, 230), 0.45f);
            if (x - y > 20 && x - y < 24) c = blend(c, C(190, 210, 230), 0.3f);
            px(x, y, c);
        }
    frame(0, 0, 63, 63, C(20, 20, 20));
}

static void t_bus_side(void)
{
    int x, y;
    grain(0, 0, 63, 63, C(230, 200, 80), 0.08f, 96, 4);
    for (x = 2; x < 64; x += 16) {
        rect(x, 6, x + 13, 30, C(40, 50, 60));
        for (y = 7; y < 30; y++) if (y - (x % 9) > 10 && y - (x % 9) < 14) px(x + 4, y, C(150, 170, 190));
    }
    rect(0, 36, 63, 40, C(200, 60, 40));
}

static void t_lamp(void)
{
    rect(0, 0, 63, 63, CA(0, 0, 0, 0));
    rect(28, 8, 35, 63, C(160, 158, 152));
    rect(30, 8, 31, 63, C(190, 188, 182));
    rect(24, 4, 51, 9, C(80, 80, 84));
    rect(42, 9, 51, 12, C(240, 236, 200));
}

static void t_bench(void)
{
    rect(0, 0, 63, 63, CA(0, 0, 0, 0));
    rect(4, 20, 59, 25, C(160, 110, 60));
    rect(4, 28, 59, 33, C(150, 100, 56));
    rect(4, 38, 59, 42, C(160, 110, 60));
    rect(8, 42, 11, 63, C(60, 60, 60));
    rect(52, 42, 55, 63, C(60, 60, 60));
}

static void t_shadow(void)
{
    int x, y;
    for (y = 0; y < 64; y++)
        for (x = 0; x < 64; x++) {
            float dx = (x - 31.5f) / 32.0f, dy = (y - 31.5f) / 32.0f, d = sqrtf(dx * dx + dy * dy);
            float a = clampf(1.0f - d, 0.0f, 1.0f);
            px(x, y, CA(0, 0, 0, (int)(a * a * 170)));
        }
}

static void t_flare(void)
{
    int x, y;
    for (y = 0; y < 64; y++)
        for (x = 0; x < 64; x++) {
            float dx = (x - 31.5f) / 32.0f, dy = (y - 31.5f) / 32.0f, d = sqrtf(dx * dx + dy * dy);
            float a = clampf(1.0f - d, 0.0f, 1.0f);
            px(x, y, CA(255, 240, 200, (int)(a * a * a * 255)));
        }
}

/* --------------------------------------------------------- characters */

typedef struct {
    unsigned int skin, hair, eyes, brows, lips;
    int mustache, stubble, lipstick, glasses, bald, long_hair, scarf;
} FaceSpec;

static void face(int cx, int cy, const FaceSpec *f, int open)
{
    int x, y;
    /* skin with soft shading */
    for (y = 0; y < 32; y++)
        for (x = 0; x < 32; x++) {
            float s = 1.0f - fabsf(x - 15.5f) / 60.0f - (y > 26 ? 0.08f : 0.0f);
            px(cx + x, cy + y, mul(f->skin, s));
        }
    if (f->stubble)
        for (y = 20; y < 32; y++)
            for (x = 4; x < 28; x++)
                if (rn(100, x, y) > 0.55f) px(cx + x, cy + y, mul(f->skin, 0.78f));
    /* hair */
    if (!f->bald) {
        int top = f->long_hair ? 9 : 6;
        for (y = 0; y < top; y++)
            for (x = 0; x < 32; x++) px(cx + x, cy + y, mul(f->hair, 0.8f + 0.35f * rn(101, x, y)));
        for (x = 0; x < 32; x += 3) px(cx + x, cy + top, mul(f->hair, 0.9f));
        if (f->long_hair) { rect(cx, cy, cx + 3, cy + 31, f->hair); rect(cx + 28, cy, cx + 31, cy + 31, f->hair); }
    } else {
        for (x = 0; x < 32; x++) px(cx + x, cy, mul(f->skin, 0.9f));
    }
    if (f->scarf) {
        for (y = 0; y < 8; y++)
            for (x = 0; x < 32; x++) px(cx + x, cy + y, ((x / 3 + y / 3) & 1) ? C(200, 40, 40) : C(240, 210, 60));
        rect(cx, cy, cx + 2, cy + 31, C(200, 40, 40));
        rect(cx + 29, cy, cx + 31, cy + 31, C(200, 40, 40));
    }
    /* brows and eyes */
    rect(cx + 7, cy + 11, cx + 12, cy + 11, f->brows);
    rect(cx + 19, cy + 11, cx + 24, cy + 11, f->brows);
    rect(cx + 8, cy + 13, cx + 12, cy + 15, C(240, 240, 236));
    rect(cx + 19, cy + 13, cx + 23, cy + 15, C(240, 240, 236));
    rect(cx + 10, cy + 13, cx + 11, cy + 15, f->eyes);
    rect(cx + 20, cy + 13, cx + 21, cy + 15, f->eyes);
    px(cx + 10, cy + 13, C(250, 250, 250));
    px(cx + 20, cy + 13, C(250, 250, 250));
    if (f->glasses) {
        frame(cx + 6, cy + 12, cx + 13, cy + 17, C(30, 30, 30));
        frame(cx + 18, cy + 12, cx + 25, cy + 17, C(30, 30, 30));
        rect(cx + 14, cy + 13, cx + 17, cy + 13, C(30, 30, 30));
    }
    if (f->lipstick) { px(cx + 7, cy + 12, C(40, 30, 30)); px(cx + 24, cy + 12, C(40, 30, 30)); }
    /* nose */
    rect(cx + 15, cy + 15, cx + 16, cy + 20, mul(f->skin, 0.85f));
    rect(cx + 14, cy + 20, cx + 17, cy + 20, mul(f->skin, 0.72f));
    /* mustache */
    if (f->mustache) rect(cx + 11, cy + 22, cx + 20, cy + 23, f->hair);
    /* mouth */
    if (open) {
        rect(cx + 12, cy + 24, cx + 19, cy + 28, C(70, 20, 24));
        rect(cx + 13, cy + 24, cx + 18, cy + 24, C(240, 240, 236));
        rect(cx + 13, cy + 28, cx + 18, cy + 28, f->lips);
    } else {
        rect(cx + 12, cy + 25, cx + 19, cy + 25, f->lips);
        rect(cx + 13, cy + 26, cx + 18, cy + 26, mul(f->lips, 1.2f));
    }
    /* ears */
    rect(cx, cy + 13, cx + 1, cy + 19, mul(f->skin, 0.85f));
    rect(cx + 30, cy + 13, cx + 31, cy + 19, mul(f->skin, 0.85f));
}

static const FaceSpec FACES[FACE_COUNT] = {
    /* skin                hair               eyes               brows              lips */
    { 0xFF8CB0DCu, 0xFF203048u, 0xFF6A4A30u, 0xFF203048u, 0xFF5060A8u, 0, 0, 0, 0, 0, 0, 0 }, /* Andriy */
    { 0xFF7CA0D0u, 0xFF283850u, 0xFF404030u, 0xFF283850u, 0xFF4C5C98u, 1, 0, 0, 0, 0, 0, 0 }, /* Taras  */
    { 0xFF94B8E4u, 0xFF4060A0u, 0xFF603C20u, 0xFF203048u, 0xFF3030C0u, 0, 0, 1, 0, 0, 0, 1 }, /* Galya  */
    { 0xFF7098D0u, 0xFF303030u, 0xFF203020u, 0xFF202020u, 0xFF485890u, 0, 1, 0, 0, 1, 0, 0 }, /* Kaban  */
    { 0xFF88ACD8u, 0xFF505868u, 0xFF504030u, 0xFF404858u, 0xFF4858A0u, 0, 0, 0, 1, 0, 0, 0 }, /* Seller */
    { 0xFF80A4D4u, 0xFF181818u, 0xFF202020u, 0xFF181818u, 0xFF4858A0u, 0, 1, 0, 0, 0, 0, 0 }, /* Thug   */
};

static void t_faces(int first)
{
    int i;
    for (i = 0; i < 2; i++) {
        face(0, i * 32, &FACES[first + i], 0);
        face(32, i * 32, &FACES[first + i], 1);
    }
}

UVRect uv_face(int f, int open)
{
    int tile = T_FACES_A + f / 2;
    return uv_sub(tile, open ? 32 : 0, (f % 2) * 32, 32, 32);
}

static void t_hair(void) { grain(0, 0, 63, 63, C(200, 200, 200), 0.5f, 102, 16); }

static void t_cloth_track(void)
{
    int y;
    grain(0, 0, 63, 63, C(190, 190, 196), 0.12f, 103, 8);
    for (y = 0; y < 64; y++) { rect(4, y, 6, y, C(255, 255, 255)); rect(9, y, 11, y, C(255, 255, 255)); }
    for (y = 0; y < 64; y++) { rect(52, y, 54, y, C(255, 255, 255)); rect(57, y, 59, y, C(255, 255, 255)); }
    rect(31, 0, 32, 63, C(120, 120, 126)); /* zip */
}

static void t_cloth_leather(void)
{
    int x, y;
    for (y = 0; y < 64; y++)
        for (x = 0; x < 64; x++) {
            float s = 0.9f + 0.4f * fbm(104, x, y, 4);
            if (rn(105, x, y) > 0.97f) s = 1.8f;
            px(x, y, mul(C(36, 34, 34), s));
        }
    rect(31, 0, 32, 63, C(150, 150, 150));
    line(22, 0, 30, 14, C(20, 20, 20));
    line(41, 0, 33, 14, C(20, 20, 20));
}

static void t_cloth_blazer(void)
{
    int x, y;
    grain(0, 0, 63, 63, C(170, 30, 70), 0.18f, 106, 8);   /* raspberry */
    /* open front: shirt, chain, lapels */
    for (y = 0; y < 40; y++) {
        int w = 10 - y / 5;
        if (w < 2) w = 2;
        rect(16 - w, y, 16 + w, y, C(240, 240, 236));
    }
    for (x = 8; x < 25; x++) {
        int y2 = 12 + (int)(0.06f * (x - 16) * (x - 16) * 3);
        px(x, y2, C(250, 210, 60));
        px(x, y2 + 1, C(200, 150, 30));
    }
    line(4, 0, 12, 30, C(120, 20, 50));
    line(28, 0, 20, 30, C(120, 20, 50));
    rect(15, 44, 17, 46, C(230, 190, 60));
    rect(15, 54, 17, 56, C(230, 190, 60));
}

static void t_cloth_dress(void)
{
    int x, y;
    grain(0, 0, 63, 63, C(60, 80, 160), 0.1f, 107, 8);
    for (y = 3; y < 64; y += 9)
        for (x = (y / 9 % 2) * 5 + 2; x < 64; x += 10) {
            disc((float)x, (float)y, 2, C(240, 200, 60));
            px(x, y, C(220, 60, 60));
        }
    rect(18, 20, 45, 63, C(236, 236, 230));   /* apron */
    frame(18, 20, 45, 63, C(200, 200, 196));
}

static void t_cloth_overalls(void)
{
    int x, y;
    grain(0, 0, 63, 63, C(52, 84, 140), 0.18f, 108, 8);
    rect(20, 20, 43, 34, C(44, 72, 124));
    frame(20, 20, 43, 34, C(200, 180, 80));
    for (y = 0; y < 64; y++)
        for (x = 0; x < 64; x++)
            if (fbm(109, x, y, 8) > 0.68f) px(x, y, mul(getpx(x, y), 0.6f)); /* oil stains */
    rect(8, 0, 11, 63, C(200, 180, 80));
    rect(52, 0, 55, 63, C(200, 180, 80));
}

static void t_cloth_denim(void)
{
    int y;
    grain(0, 0, 63, 63, C(70, 100, 150), 0.25f, 110, 16);
    for (y = 0; y < 64; y++) { px(31, y, C(200, 160, 90)); px(32, y, C(50, 70, 110)); }
}

static void t_cloth_army(void)
{
    int x, y;
    for (y = 0; y < 64; y++)
        for (x = 0; x < 64; x++) {
            float n = fbm(111, x, y, 4);
            px(x, y, n > 0.6f ? C(70, 80, 50) : n > 0.45f ? C(100, 110, 70) : C(124, 128, 86));
        }
    /* telnyashka (striped vest) in the open collar */
    for (y = 0; y < 24; y++) {
        int w = 7 - y / 4;
        if (w < 0) continue;
        rect(32 - w, y, 32 + w, y, (y / 2) % 2 ? C(40, 60, 150) : C(240, 240, 240));
    }
    rect(10, 30, 22, 40, C(90, 100, 60));
    frame(10, 30, 22, 40, C(60, 66, 40));
    rect(42, 30, 54, 40, C(90, 100, 60));
    frame(42, 30, 54, 40, C(60, 66, 40));
}

static void t_cloth_shirt(void) { grain(0, 0, 63, 63, C(226, 226, 230), 0.1f, 112, 8); }

static void t_shoe(void)
{
    grain(0, 0, 63, 63, C(230, 230, 230), 0.08f, 113, 4);
    rect(0, 48, 63, 63, C(60, 60, 60));
    rect(10, 20, 54, 22, C(40, 70, 160));
}

static void t_headscarf(void)
{
    int x, y;
    for (y = 0; y < 64; y++)
        for (x = 0; x < 64; x++) px(x, y, ((x / 6 + y / 6) & 1) ? C(200, 40, 40) : C(150, 20, 30));
    for (y = 4; y < 64; y += 12)
        for (x = 4; x < 64; x += 12) disc((float)x, (float)y, 3, C(240, 210, 60));
}

/* ----------------------------------------------------------- assemble */

void texgen_atlas(unsigned int *atlas)
{
    int t;
    A = atlas;
    memset(atlas, 0, TEX_SIZE * TEX_SIZE * 4);
    for (t = 0; t < T_COUNT; t++) {
        begin(t);
        switch (t) {
        case T_WHITE: t_white(); break;
        case T_ASPHALT: t_asphalt(10); break;
        case T_ROAD_LINE: t_road_line(); break;
        case T_CROSSWALK: t_crosswalk(); break;
        case T_SIDEWALK: t_sidewalk(); break;
        case T_CURB: t_curb(); break;
        case T_GRASS: t_grass(23, 0); break;
        case T_YARD: t_grass(24, 1); break;
        case T_DIRT: t_dirt(); break;
        case T_PANEL_WIN1: t_panel_win(0); break;
        case T_PANEL_WIN2: t_panel_win(1); break;
        case T_PANEL_WIN3: t_panel_win(2); break;
        case T_PANEL_BLANK: t_panel_blank(); break;
        case T_PANEL_DOOR: t_panel_door(); break;
        case T_BALCONY: t_balcony(); break;
        case T_BRICK_WIN: t_brick(1); break;
        case T_BRICK: t_brick(0); break;
        case T_ROOF_FLAT: t_roof_flat(); break;
        case T_SLATE: t_slate(); break;
        case T_ROOF_RED: t_roof_red(); break;
        case T_PLASTER_WIN: t_plaster(1); break;
        case T_PLASTER: t_plaster(0); break;
        case T_FENCE: t_fence(); break;
        case T_GARAGE_DOOR: t_garage_door(); break;
        case T_METAL: t_metal(); break;
        case T_KIOSK_FRONT: t_kiosk_front(); break;
        case T_SIGN_PIVO: t_sign("ПИВО", C(250, 214, 60), C(196, 30, 30)); break;
        case T_SIGN_KAVA: t_sign("КАВА", C(90, 50, 30), C(250, 230, 200)); break;
        case T_SIGN_TSYHARKY: t_sign("ЦИГАРКИ", C(40, 80, 160), C(250, 250, 250)); break;
        case T_SIGN_HAZETY: t_sign("ГАЗЕТИ", C(240, 240, 236), C(30, 60, 140)); break;
        case T_TENT: t_tent(); break;
        case T_GUARDRAIL: t_guardrail(); break;
        case T_CONCRETE: t_concrete(); break;
        case T_BIRCH: t_birch(); break;
        case T_POPLAR: t_poplar(); break;
        case T_BUSH: t_bush(); break;
        case T_SUNFLOWERS: t_sunflowers(); break;
        case T_FIELD_WHEAT: t_field(0); break;
        case T_FIELD_SUNFLOWER: t_field(1); break;
        case T_HILLS: t_hills(); break;
        case T_WINDOW_LIT: t_window_lit(); break;
        case T_CAR_SIDE: t_car_side(); break;
        case T_CAR_PARTS: t_car_parts(); break;
        case T_TIRE: t_tire(); break;
        case T_CAR_GLASS: t_car_glass(); break;
        case T_FACES_A: t_faces(0); break;
        case T_FACES_B: t_faces(2); break;
        case T_FACES_C: t_faces(4); break;
        case T_HAIR: t_hair(); break;
        case T_CLOTH_TRACK: t_cloth_track(); break;
        case T_CLOTH_LEATHER: t_cloth_leather(); break;
        case T_CLOTH_BLAZER: t_cloth_blazer(); break;
        case T_CLOTH_DRESS: t_cloth_dress(); break;
        case T_CLOTH_OVERALLS: t_cloth_overalls(); break;
        case T_CLOTH_DENIM: t_cloth_denim(); break;
        case T_CLOTH_ARMY: t_cloth_army(); break;
        case T_CLOTH_SHIRT: t_cloth_shirt(); break;
        case T_SHOE: t_shoe(); break;
        case T_HEADSCARF: t_headscarf(); break;
        case T_BUS_SIDE: t_bus_side(); break;
        case T_LAMP: t_lamp(); break;
        case T_BENCH: t_bench(); break;
        case T_SHADOW: t_shadow(); break;
        case T_FLARE: t_flare(); break;
        default: break;
        }
    }
}

/* Next mip level (half size) of the atlas. Colours are averaged weighted by
 * alpha, so cut-out sprites (trees, fences) do not get dark fringes from the
 * transparent black around them. Tiles stay separate at every level since
 * TEX_TILE is a multiple of 2^levels. */
void texgen_mip(const unsigned int *src, int src_size, unsigned int *dst)
{
    int size = src_size / 2, x, y, k;
    for (y = 0; y < size; y++)
        for (x = 0; x < size; x++) {
            unsigned int px[4], wsum = 0, c[3] = { 0, 0, 0 }, a = 0;
            px[0] = src[(2 * y) * src_size + 2 * x];
            px[1] = src[(2 * y) * src_size + 2 * x + 1];
            px[2] = src[(2 * y + 1) * src_size + 2 * x];
            px[3] = src[(2 * y + 1) * src_size + 2 * x + 1];
            for (k = 0; k < 4; k++) {
                unsigned int al = px[k] >> 24;
                a += al;
                wsum += al;
                c[0] += (px[k] & 0xFF) * al;
                c[1] += ((px[k] >> 8) & 0xFF) * al;
                c[2] += ((px[k] >> 16) & 0xFF) * al;
            }
            if (wsum) {
                for (k = 0; k < 3; k++) c[k] = (c[k] + wsum / 2) / wsum;
            }
            /* keep cut-outs solid while at least half the texels are opaque */
            a = (a + 2) / 4;
            dst[y * size + x] = (a << 24) | (c[2] << 16) | (c[1] << 8) | c[0];
        }
}
