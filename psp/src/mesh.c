#include "mesh.h"

#include <stdlib.h>
#include <string.h>

#define PW (CH + 2)
#define PH (WH + 2)
#define MAX_SOLID_QUADS 16000
#define MAX_WATER_QUADS 6000

/* chunk plus a one-block border, y shifted by one (row 0 = below the world) */
static unsigned char pb[PH][PW][PW];
static unsigned char ph[PW][PW];
static Vertex scratch_solid[MAX_SOLID_QUADS * 4];
static Vertex scratch_water[MAX_WATER_QUADS * 4];

/* Corners ordered bottom-left, top-left, top-right, bottom-right seen from outside */
static const signed char FACE_CORNERS[6][4][3] = {
    { {1,0,1}, {1,1,1}, {1,1,0}, {1,0,0} }, /* +X */
    { {0,0,0}, {0,1,0}, {0,1,1}, {0,0,1} }, /* -X */
    { {0,1,1}, {0,1,0}, {1,1,0}, {1,1,1} }, /* +Y */
    { {0,0,0}, {0,0,1}, {1,0,1}, {1,0,0} }, /* -Y */
    { {0,0,1}, {0,1,1}, {1,1,1}, {1,0,1} }, /* +Z */
    { {1,0,0}, {1,1,0}, {0,1,0}, {0,0,0} }, /* -Z */
};
static const signed char FACE_DIR[6][3] = {
    { 1, 0, 0 }, { -1, 0, 0 }, { 0, 1, 0 }, { 0, -1, 0 }, { 0, 0, 1 }, { 0, 0, -1 },
};
static const int FACE_AXIS[6] = { 0, 0, 1, 1, 2, 2 };
static const float FACE_SHADE[6] = { 0.80f, 0.80f, 1.00f, 0.50f, 0.65f, 0.65f };
static const float AO_LEVEL[4] = { 1.00f, 0.78f, 0.62f, 0.48f };
static const unsigned char CORNER_UV[4][2] = { { 0, 1 }, { 0, 0 }, { 1, 0 }, { 1, 1 } };

static unsigned char opaque_tab[256], shape_tab[256];
static int tables_ready;
static int out_solid, out_water;

static void init_tables(void)
{
    int i;
    for (i = 0; i < 256; i++) {
        opaque_tab[i] = (unsigned char)block_opaque(i);
        shape_tab[i] = (unsigned char)block_shape(i);
    }
    tables_ready = 1;
}

#define opaque_at(x, y, z) opaque_tab[pb[y][z][x]]

/* Sky light of a padded cell, -1 for opaque cells (excluded from averages). */
static float light_at(int x, int y, int z)
{
    int h, d;
    float v;
    if (opaque_at(x, y, z)) return -1.0f;
    h = ph[z][x];
    d = h - (y - 1);
    if (d <= 0) return 1.0f;
    v = 0.80f - 0.09f * d;
    return v < 0.16f ? 0.16f : v;
}

static unsigned int gray(float v)
{
    int c = (int)(v * 255.0f + 0.5f);
    if (c > 255) c = 255;
    if (c < 0) c = 0;
    return 0xFF000000u | ((unsigned int)c << 16) | ((unsigned int)c << 8) | (unsigned int)c;
}

static void tile_uv(int tile, int corner, unsigned short *u, unsigned short *v)
{
    const int span = VUV_ONE / ATLAS_TILES;
    const int inset = VUV_ONE / ATLAS_SIZE / 2; /* half a texel against bleeding */
    int tu = (tile % ATLAS_TILES) * span, tv = (tile / ATLAS_TILES) * span;
    *u = (unsigned short)(tu + inset + CORNER_UV[corner][0] * (span - 2 * inset));
    *v = (unsigned short)(tv + inset + CORNER_UV[corner][1] * (span - 2 * inset));
}

void mesh_gather(Chunk *c)
{
    int x, z, y;
    if (!tables_ready) init_tables();
    c->mesh_dirty = 0;
    for (z = -1; z <= CH; z++)
        for (x = -1; x <= CH; x++) {
            int ox = x < 0 ? -1 : (x >= CH ? 1 : 0), oz = z < 0 ? -1 : (z >= CH ? 1 : 0);
            const Chunk *n = world_chunk(c->cx + ox, c->cz + oz);
            int lx = x - ox * CH, lz = z - oz * CH;
            pb[0][z + 1][x + 1] = B_BEDROCK;
            pb[PH - 1][z + 1][x + 1] = B_AIR;
            if (!n) {
                for (y = 0; y < WH; y++) pb[y + 1][z + 1][x + 1] = B_STONE;
                ph[z + 1][x + 1] = WH;
                continue;
            }
            for (y = 0; y < WH; y++) pb[y + 1][z + 1][x + 1] = n->blocks[BIDX(lx, y, lz)];
            ph[z + 1][x + 1] = n->height[lz * CH + lx];
        }
}

static int face_visible(int b, int n)
{
    if (shape_tab[b] == SHAPE_LIQUID) return n != b && !opaque_tab[n];
    if (opaque_tab[n]) return 0;
    return n != b || b == B_LEAVES || b == B_BIRCH_LEAVES;
}

static void emit_cube_face(Vertex *out, int x, int y, int z, int b, int f)
{
    /* padded coordinates of the cell in front of the face */
    int fx = x + 1 + FACE_DIR[f][0], fy = y + 1 + FACE_DIR[f][1], fz = z + 1 + FACE_DIR[f][2];
    int axis = FACE_AXIS[f], t1 = (axis + 1) % 3, t2 = (axis + 2) % 3;
    int tile = block_tile(b, f);
    float bright[4];
    int order[4] = { 0, 1, 2, 3 }, i;
    float front = light_at(fx, fy, fz);
    if (front < 0.0f) front = 0.16f;

    for (i = 0; i < 4; i++) {
        const signed char *cc = FACE_CORNERS[f][i];
        int p1[3] = { fx, fy, fz }, p2[3] = { fx, fy, fz }, p3[3] = { fx, fy, fz };
        int o1 = cc[t1] ? 1 : -1, o2 = cc[t2] ? 1 : -1, s1, s2, sc, ao;
        float l, sum = front;
        int n = 1;
        p1[t1] += o1;
        p2[t2] += o2;
        p3[t1] += o1;
        p3[t2] += o2;
        s1 = opaque_at(p1[0], p1[1], p1[2]);
        s2 = opaque_at(p2[0], p2[1], p2[2]);
        sc = opaque_at(p3[0], p3[1], p3[2]);
        ao = (s1 && s2) ? 3 : s1 + s2 + sc;
        if (!s1) { l = light_at(p1[0], p1[1], p1[2]); sum += l; n++; }
        if (!s2) { l = light_at(p2[0], p2[1], p2[2]); sum += l; n++; }
        if (!sc && !(s1 && s2)) { l = light_at(p3[0], p3[1], p3[2]); sum += l; n++; }
        bright[i] = FACE_SHADE[f] * AO_LEVEL[ao] * (sum / n);
    }

    /* split the quad along the brighter diagonal to avoid AO artefacts */
    if (bright[0] + bright[2] < bright[1] + bright[3]) {
        order[0] = 1; order[1] = 2; order[2] = 3; order[3] = 0;
    }
    for (i = 0; i < 4; i++) {
        int k = order[i];
        const signed char *cc = FACE_CORNERS[f][k];
        Vertex *v = &out[i];
        tile_uv(tile, k, &v->u, &v->v);
        v->color = gray(bright[k]);
        v->x = (short)((x + cc[0]) * VPOS_SCALE);
        v->y = (short)((y + cc[1]) * VPOS_SCALE);
        v->z = (short)((z + cc[2]) * VPOS_SCALE);
        v->pad = 0;
    }
}

static void emit_water_face(Vertex *out, int x, int y, int z, int b, int f)
{
    int above = pb[y + 2][z + 1][x + 1];
    int top = (block_shape(above) == SHAPE_LIQUID) ? VPOS_SCALE : VPOS_SCALE * 7 / 8;
    float l = light_at(x + 1 + FACE_DIR[f][0], y + 1 + FACE_DIR[f][1], z + 1 + FACE_DIR[f][2]);
    unsigned int color;
    int i;
    if (l < 0.0f) l = 0.16f;
    color = gray(FACE_SHADE[f] * l);
    for (i = 0; i < 4; i++) {
        const signed char *cc = FACE_CORNERS[f][i];
        Vertex *v = &out[i];
        tile_uv(block_tile(b, f), i, &v->u, &v->v);
        v->color = color;
        v->x = (short)((x + cc[0]) * VPOS_SCALE);
        v->y = (short)(y * VPOS_SCALE + cc[1] * top);
        v->z = (short)((z + cc[2]) * VPOS_SCALE);
        v->pad = 0;
    }
}

static int emit_cross(Vertex *out, int x, int y, int z, int b)
{
    static const short D[2][2][2] = { { { 38, 38 }, { 218, 218 } }, { { 38, 218 }, { 218, 38 } } };
    float l = light_at(x + 1, y + 1, z + 1);
    unsigned int color;
    int q, i;
    if (l < 0.0f) l = 0.16f;
    color = gray(l * 0.92f);
    for (q = 0; q < 2; q++)
        for (i = 0; i < 4; i++) {
            /* corners: bottom-left, top-left, top-right, bottom-right */
            int side = (i == 0 || i == 1) ? 0 : 1, up = (i == 1 || i == 2);
            Vertex *v = &out[q * 4 + i];
            tile_uv(block_tile(b, 0), i, &v->u, &v->v);
            v->color = color;
            v->x = (short)(x * VPOS_SCALE + D[q][side][0]);
            v->y = (short)((y + up) * VPOS_SCALE);
            v->z = (short)(z * VPOS_SCALE + D[q][side][1]);
            v->pad = 0;
        }
    return 2;
}

static Vertex *copy_out(const Vertex *src, int quads)
{
    Vertex *v;
    if (quads == 0) return NULL;
    v = (Vertex *)malloc((size_t)quads * 4 * sizeof(Vertex));
    if (v) memcpy(v, src, (size_t)quads * 4 * sizeof(Vertex));
    return v;
}

void mesh_compute(void)
{
    int x, y, z, f, ns = 0, nw = 0;

    for (y = 0; y < WH; y++)
        for (z = 0; z < CH; z++)
            for (x = 0; x < CH; x++) {
                int b = pb[y + 1][z + 1][x + 1], shape = shape_tab[b];
                if (shape == SHAPE_NONE) continue;
                if (shape == SHAPE_CROSS) {
                    if (ns + 2 <= MAX_SOLID_QUADS)
                        ns += emit_cross(&scratch_solid[ns * 4], x, y, z, b);
                    continue;
                }
                for (f = 0; f < 6; f++) {
                    int n = pb[y + 1 + FACE_DIR[f][1]][z + 1 + FACE_DIR[f][2]][x + 1 + FACE_DIR[f][0]];
                    if (!face_visible(b, n)) continue;
                    if (shape == SHAPE_LIQUID) {
                        if (nw < MAX_WATER_QUADS) emit_water_face(&scratch_water[nw++ * 4], x, y, z, b, f);
                    } else if (ns < MAX_SOLID_QUADS) {
                        emit_cube_face(&scratch_solid[ns++ * 4], x, y, z, b, f);
                    }
                }
            }
    out_solid = ns;
    out_water = nw;
}

int mesh_commit(Chunk *c)
{
    free(c->solid);
    free(c->water);
    c->solid = copy_out(scratch_solid, out_solid);
    c->water = copy_out(scratch_water, out_water);
    c->nsolid = c->solid ? out_solid : 0;
    c->nwater = c->water ? out_water : 0;
    c->meshed = 1;
    return (out_solid == 0 || c->solid) && (out_water == 0 || c->water);
}

int mesh_build(Chunk *c)
{
    mesh_gather(c);
    mesh_compute();
    return mesh_commit(c);
}
