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

/* ------------------------------------------------------------ view cone */

void view_cone(ViewCone *c, V3 eye, float yaw, float pitch, float k)
{
    float cy = cosf(yaw), sy = sinf(yaw), cp = cosf(pitch), sp = sinf(pitch);
    c->eye = eye;
    c->fwd = v3(-sy * cp, sp, -cy * cp);
    c->right = v3(cy, 0.0f, -sy);
    c->up = v3(sy * sp, cp, cy * sp);
    c->k = k;
}

/* the four side planes: k*d - x, k*d + x, k*d - y, k*d + y (inside >= 0) */
static void cone_dist(const ViewCone *c, V3 p, float out[4])
{
    V3 r = v3_sub(p, c->eye);
    float d = v3_dot(r, c->fwd) * c->k, x = v3_dot(r, c->right), y = v3_dot(r, c->up);
    out[0] = d - x;
    out[1] = d + x;
    out[2] = d - y;
    out[3] = d + y;
}

int view_cone_contains(const ViewCone *c, V3 lo, V3 hi)
{
    int i, k;
    for (i = 0; i < 8; i++) {
        float e[4];
        cone_dist(c, v3(i & 1 ? hi.x : lo.x, i & 2 ? hi.y : lo.y, i & 4 ? hi.z : lo.z), e);
        for (k = 0; k < 4; k++)
            if (e[k] < 0.0f) return 0;
    }
    return 1;
}

static GVertex lerp_vertex(const GVertex *a, const GVertex *b, float t)
{
    GVertex g;
    int s;
    g.u = a->u + (b->u - a->u) * t;
    g.v = a->v + (b->v - a->v) * t;
    g.x = a->x + (b->x - a->x) * t;
    g.y = a->y + (b->y - a->y) * t;
    g.z = a->z + (b->z - a->z) * t;
    g.color = 0;
    for (s = 0; s < 32; s += 8) {
        float ca = (float)((a->color >> s) & 0xFF), cb = (float)((b->color >> s) & 0xFF);
        g.color |= (unsigned int)(ca + (cb - ca) * t + 0.5f) << s;
    }
    return g;
}

int gm_cone_clip(const GVertex *v, int n, const ViewCone *c, GVertex *out, int cap)
{
    int t, count = 0;
    for (t = 0; t + 2 < n; t += 3) {
        float e[3][4];
        int i, k, inside = 1;
        for (i = 0; i < 3; i++) {
            cone_dist(c, v3(v[t + i].x, v[t + i].y, v[t + i].z), e[i]);
            for (k = 0; k < 4; k++)
                if (e[i][k] < 0.0f) inside = 0;
        }
        if (inside) {
            if (count + 3 > cap) return -1;
            out[count++] = v[t];
            out[count++] = v[t + 1];
            out[count++] = v[t + 2];
            continue;
        }
        {
            /* Sutherland-Hodgman against the four planes */
            GVertex poly[2][8];
            float dist[2][8][4];
            int np = 3, cur = 0, pl;
            for (i = 0; i < 3; i++) {
                poly[0][i] = v[t + i];
                for (k = 0; k < 4; k++) dist[0][i][k] = e[i][k];
            }
            for (pl = 0; pl < 4 && np >= 3; pl++) {
                int nn = 0, nx = cur ^ 1;
                for (i = 0; i < np; i++) {
                    int j = (i + 1) % np;
                    float da = dist[cur][i][pl], db = dist[cur][j][pl];
                    if (da >= 0.0f) {
                        poly[nx][nn] = poly[cur][i];
                        for (k = 0; k < 4; k++) dist[nx][nn][k] = dist[cur][i][k];
                        nn++;
                    }
                    if ((da >= 0.0f) != (db >= 0.0f) && nn < 8) {
                        float s = da / (da - db);
                        poly[nx][nn] = lerp_vertex(&poly[cur][i], &poly[cur][j], s);
                        for (k = 0; k < 4; k++)
                            dist[nx][nn][k] = dist[cur][i][k] + (dist[cur][j][k] - dist[cur][i][k]) * s;
                        nn++;
                    }
                }
                np = nn;
                cur = nx;
            }
            if (np < 3) continue;
            if (count + (np - 2) * 3 > cap) return -1;
            for (i = 1; i + 1 < np; i++) {
                out[count++] = poly[cur][0];
                out[count++] = poly[cur][i];
                out[count++] = poly[cur][i + 1];
            }
        }
    }
    return count;
}

/* ---------------------------------------------------------------- chunks */

#define CHUNK_TRIS 48
#define CHUNK_SIZE 8.0f

static void chunk_close(GChunk *ch, V3 lo, V3 hi)
{
    ch->c = v3_scale(v3_add(lo, hi), 0.5f);
    ch->r = v3_len(v3_sub(hi, lo)) * 0.5f + 0.01f;
}

int gm_chunks(const GVertex *v, int n, GChunk *out, int cap)
{
    int t, i, nc = 0;
    V3 lo = v3(0, 0, 0), hi = v3(0, 0, 0);
    for (t = 0; t + 2 < n; t += 3) {
        V3 tlo = v3(v[t].x, v[t].y, v[t].z), thi = tlo;
        for (i = 1; i < 3; i++) {
            const GVertex *g = &v[t + i];
            tlo = v3(fminf(tlo.x, g->x), fminf(tlo.y, g->y), fminf(tlo.z, g->z));
            thi = v3(fmaxf(thi.x, g->x), fmaxf(thi.y, g->y), fmaxf(thi.z, g->z));
        }
        if (nc > 0 && out[nc - 1].count < CHUNK_TRIS * 3) {
            V3 a = v3(fminf(lo.x, tlo.x), fminf(lo.y, tlo.y), fminf(lo.z, tlo.z));
            V3 b = v3(fmaxf(hi.x, thi.x), fmaxf(hi.y, thi.y), fmaxf(hi.z, thi.z));
            if (b.x - a.x <= CHUNK_SIZE && b.y - a.y <= CHUNK_SIZE && b.z - a.z <= CHUNK_SIZE) {
                out[nc - 1].count += 3;
                lo = a;
                hi = b;
                continue;
            }
        }
        if (nc > 0) chunk_close(&out[nc - 1], lo, hi);
        if (nc >= cap) return -1;
        out[nc].first = t;
        out[nc].count = 3;
        lo = tlo;
        hi = thi;
        nc++;
    }
    if (nc > 0) chunk_close(&out[nc - 1], lo, hi);
    return nc;
}

int view_cone_sphere(const ViewCone *c, V3 centre, float r)
{
    float e[4], m = r * sqrtf(c->k * c->k + 1.0f);
    int k, inside = 1;
    cone_dist(c, centre, e);
    for (k = 0; k < 4; k++) {
        if (e[k] < -m) return -1;
        if (e[k] < m) inside = 0;
    }
    return inside;
}
