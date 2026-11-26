#include "gfx.h"

#include <stdlib.h>
#include <string.h>

void gm_init(GMesh *m)
{
    m->v = NULL;
    m->n = m->cap = 0;
}

void gm_free(GMesh *m)
{
    free(m->v);
    gm_init(m);
}

int gm_reserve(GMesh *m, int extra)
{
    if (m->n + extra > m->cap) {
        int cap = m->cap ? m->cap * 2 : 256;
        GVertex *nv;
        while (cap < m->n + extra) cap *= 2;
        nv = (GVertex *)realloc(m->v, (size_t)cap * sizeof(GVertex));
        if (!nv) return 0;
        m->v = nv;
        m->cap = cap;
    }
    return 1;
}

UVRect uv_sub(int tile, int x, int y, int w, int h)
{
    const float px = 1.0f / TEX_SIZE, inset = 0.35f * px;
    UVRect r;
    float tx = (float)((tile % TEX_TILES) * TEX_TILE), ty = (float)((tile / TEX_TILES) * TEX_TILE);
    r.u0 = (tx + x) * px + inset;
    r.v0 = (ty + y) * px + inset;
    r.u1 = (tx + x + w) * px - inset;
    r.v1 = (ty + y + h) * px - inset;
    return r;
}

UVRect uv_tile(int tile)
{
    return uv_sub(tile, 0, 0, TEX_TILE, TEX_TILE);
}

unsigned int gfx_rgba(int r, int g, int b, int a)
{
    r = r < 0 ? 0 : (r > 255 ? 255 : r);
    g = g < 0 ? 0 : (g > 255 ? 255 : g);
    b = b < 0 ? 0 : (b > 255 ? 255 : b);
    a = a < 0 ? 0 : (a > 255 ? 255 : a);
    return ((unsigned int)a << 24) | ((unsigned int)b << 16) | ((unsigned int)g << 8) | (unsigned int)r;
}

unsigned int gfx_mul(unsigned int c, float s)
{
    return gfx_rgba((int)((c & 0xFF) * s), (int)(((c >> 8) & 0xFF) * s), (int)(((c >> 16) & 0xFF) * s),
                    (int)(c >> 24));
}

unsigned int gfx_shade(V3 n, unsigned int tint)
{
    static const V3 SUN = { 0.42f, 0.78f, 0.46f };
    float d = v3_dot(v3_norm(n), SUN), s;
    if (d < 0.0f) d *= 0.25f;
    s = 0.62f + 0.40f * d + (n.y > 0.5f ? 0.06f : 0.0f);
    return gfx_mul(tint, s);
}

static void put(GMesh *m, V3 p, float u, float v, unsigned int c)
{
    GVertex *g = &m->v[m->n++];
    g->u = u;
    g->v = v;
    g->color = c;
    g->x = p.x;
    g->y = p.y;
    g->z = p.z;
}

void gm_tri(GMesh *m, V3 a, V3 b, V3 c, float ua, float va, float ub, float vb, float uc, float vc,
            unsigned int color)
{
    if (!gm_reserve(m, 3)) return;
    put(m, a, ua, va, color);
    put(m, b, ub, vb, color);
    put(m, c, uc, vc, color);
}

void gm_quad_flat(GMesh *m, V3 p0, V3 p1, V3 p2, V3 p3, UVRect uv, unsigned int color)
{
    if (!gm_reserve(m, 6)) return;
    put(m, p0, uv.u0, uv.v1, color);
    put(m, p1, uv.u0, uv.v0, color);
    put(m, p2, uv.u1, uv.v0, color);
    put(m, p0, uv.u0, uv.v1, color);
    put(m, p2, uv.u1, uv.v0, color);
    put(m, p3, uv.u1, uv.v1, color);
}

void gm_quad(GMesh *m, V3 p0, V3 p1, V3 p2, V3 p3, UVRect uv, unsigned int tint)
{
    V3 n = v3_cross(v3_sub(p1, p0), v3_sub(p3, p0));
    gm_quad_flat(m, p0, p1, p2, p3, uv, gfx_shade(v3_scale(n, -1.0f), tint));
}

/* face corners: bottom-left, top-left, top-right, bottom-right from outside */
static const signed char BOX_FACE[6][4][3] = {
    { {1,0,1}, {1,1,1}, {1,1,0}, {1,0,0} }, /* +X */
    { {0,0,0}, {0,1,0}, {0,1,1}, {0,0,1} }, /* -X */
    { {0,1,1}, {0,1,0}, {1,1,0}, {1,1,1} }, /* +Y */
    { {0,0,0}, {0,0,1}, {1,0,1}, {1,0,0} }, /* -Y */
    { {0,0,1}, {0,1,1}, {1,1,1}, {1,0,1} }, /* +Z */
    { {1,0,0}, {1,1,0}, {0,1,0}, {0,0,0} }, /* -Z */
};

static V3 corner(V3 lo, V3 hi, const signed char *c)
{
    return v3(c[0] ? hi.x : lo.x, c[1] ? hi.y : lo.y, c[2] ? hi.z : lo.z);
}

void gm_box(GMesh *m, V3 lo, V3 hi, UVRect side, UVRect top, unsigned int tint, int faces)
{
    int f;
    for (f = 0; f < 6; f++) {
        UVRect uv = (f == 2 || f == 3) ? top : side;
        if (!(faces & (1 << f))) continue;
        gm_quad(m, corner(lo, hi, BOX_FACE[f][0]), corner(lo, hi, BOX_FACE[f][1]),
                corner(lo, hi, BOX_FACE[f][2]), corner(lo, hi, BOX_FACE[f][3]), uv, tint);
    }
}

void gm_box_xf(GMesh *m, const Xf *xf, V3 lo, V3 hi, const UVRect uvs[6], unsigned int tint, int faces)
{
    int f;
    for (f = 0; f < 6; f++) {
        V3 p[4];
        int i;
        if (!(faces & (1 << f))) continue;
        for (i = 0; i < 4; i++) p[i] = xf_point(xf, corner(lo, hi, BOX_FACE[f][i]));
        gm_quad(m, p[0], p[1], p[2], p[3], uvs[f], tint);
    }
}

void gm_append_xf(GMesh *dst, const GMesh *src, const Xf *xf)
{
    int i;
    if (!gm_reserve(dst, src->n)) return;
    for (i = 0; i < src->n; i++) {
        GVertex g = src->v[i];
        V3 p = xf_point(xf, v3(g.x, g.y, g.z));
        g.x = p.x;
        g.y = p.y;
        g.z = p.z;
        dst->v[dst->n++] = g;
    }
}
