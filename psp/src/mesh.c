#include "mesh.h"
#include "world.h"

#include <stdlib.h>

/* Corner offsets per face, ordered bottom-left, top-left, top-right, bottom-right
 * as seen from outside the block, so textures stay upright on the sides. */
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

/* Simple directional shading, top brightest */
static const unsigned char FACE_SHADE[6] = { 200, 200, 255, 130, 170, 170 };

static const float CORNER_UV[4][2] = { { 0, 1 }, { 0, 0 }, { 1, 0 }, { 1, 1 } };
static const int TRI_ORDER[6] = { 0, 1, 2, 0, 2, 3 };

static int reserve(VertexBuf *b, int extra)
{
    if (b->count + extra > b->cap) {
        int ncap = b->cap ? b->cap * 2 : 4096;
        Vertex *nv;
        while (ncap < b->count + extra) ncap *= 2;
        nv = (Vertex *)realloc(b->v, (size_t)ncap * sizeof(Vertex));
        if (!nv) return 0;
        b->v = nv;
        b->cap = ncap;
    }
    return 1;
}

static int face_visible(int b, int n)
{
    if (b == B_WATER) return n == B_AIR;
    if (n == B_AIR || n == B_WATER) return 1;
    if (block_opaque(n)) return 0;
    /* neighbour is leaves or glass */
    return n != b || b == B_LEAVES;
}

static void emit_face(VertexBuf *buf, int x, int y, int z, int b, int face)
{
    const float tile = 1.0f / ATLAS_TILES;
    const float inset = 0.25f / ATLAS_SIZE; /* avoid bleeding from the next tile */
    int t = block_tile(b, face);
    float u0 = (t % ATLAS_TILES) * tile + inset;
    float v0 = (t / ATLAS_TILES) * tile + inset;
    float span = tile - 2.0f * inset;
    unsigned int s = FACE_SHADE[face];
    unsigned int alpha = (b == B_WATER) ? 0xB0u : 0xFFu;
    unsigned int color = (alpha << 24) | (s << 16) | (s << 8) | s;
    float top = (b == B_WATER && world_get(x, y + 1, z) != B_WATER) ? 0.875f : 1.0f;
    int i;

    if (!reserve(buf, 6)) return;
    for (i = 0; i < 6; i++) {
        int c = TRI_ORDER[i];
        Vertex *v = &buf->v[buf->count++];
        v->u = u0 + CORNER_UV[c][0] * span;
        v->v = v0 + CORNER_UV[c][1] * span;
        v->color = color;
        v->x = (float)(x + FACE_CORNERS[face][c][0]);
        v->y = (float)y + FACE_CORNERS[face][c][1] * top;
        v->z = (float)(z + FACE_CORNERS[face][c][2]);
    }
}

void mesh_build(ChunkMesh *m, int cx, int cz)
{
    int x, y, z, f;
    int x0 = cx * CHUNK, z0 = cz * CHUNK;

    m->solid.count = 0;
    m->water.count = 0;

    for (y = 0; y < WY; y++)
        for (z = z0; z < z0 + CHUNK; z++)
            for (x = x0; x < x0 + CHUNK; x++) {
                int b = world_get(x, y, z);
                if (b == B_AIR) continue;
                for (f = 0; f < 6; f++) {
                    int n = world_get(x + FACE_DIR[f][0], y + FACE_DIR[f][1], z + FACE_DIR[f][2]);
                    if (face_visible(b, n))
                        emit_face(b == B_WATER ? &m->water : &m->solid, x, y, z, b, f);
                }
            }
    m->dirty = 0;
}

void mesh_free(ChunkMesh *m)
{
    free(m->solid.v);
    free(m->water.v);
    m->solid.v = m->water.v = 0;
    m->solid.count = m->solid.cap = m->water.count = m->water.cap = 0;
}
