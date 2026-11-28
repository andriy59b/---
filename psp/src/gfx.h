/* Triangle meshes in the PSP float vertex format (pure C, shared by the PSP
 * renderer and the host preview tool). */
#ifndef UC_GFX_H
#define UC_GFX_H

#include "mathx.h"

/* Matches GU_TEXTURE_32BITF | GU_COLOR_8888 | GU_VERTEX_32BITF */
typedef struct {
    float u, v;
    unsigned int color; /* ABGR */
    float x, y, z;
} GVertex;

typedef struct {
    GVertex *v;
    int n, cap;
} GMesh;

typedef struct { float u0, v0, u1, v1; } UVRect;

#define TEX_SIZE 512
#define TEX_TILE 64
#define TEX_TILES (TEX_SIZE / TEX_TILE)

void gm_init(GMesh *m);
void gm_free(GMesh *m);
int  gm_reserve(GMesh *m, int extra);

/* UV rect of a whole atlas tile, or of a pixel sub-rectangle of it. */
UVRect uv_tile(int tile);
UVRect uv_sub(int tile, int x, int y, int w, int h);
/* Repeat a rect n times along u (clamped to the tile; used for scaled quads). */

/* Light a colour by a surface normal (fixed sun direction). */
unsigned int gfx_shade(V3 normal, unsigned int tint);
unsigned int gfx_rgba(int r, int g, int b, int a);
unsigned int gfx_mul(unsigned int c, float s);

/* Quad p0..p3 = bottom-left, top-left, top-right, bottom-right seen from the
 * front. uv maps (u0,v1) to p0 and (u1,v0) to p2. Lit by its normal. */
void gm_quad(GMesh *m, V3 p0, V3 p1, V3 p2, V3 p3, UVRect uv, unsigned int tint);
/* Same but unlit (colour used as is) */
void gm_quad_flat(GMesh *m, V3 p0, V3 p1, V3 p2, V3 p3, UVRect uv, unsigned int color);
void gm_tri(GMesh *m, V3 a, V3 b, V3 c, float ua, float va, float ub, float vb, float uc, float vc,
            unsigned int color);

/* Axis-aligned box. faces bitmask: 1 +X, 2 -X, 4 +Y, 8 -Y, 16 +Z, 32 -Z */
#define BOX_ALL 63
#define BOX_NO_BOTTOM 55
void gm_box(GMesh *m, V3 lo, V3 hi, UVRect side, UVRect top, unsigned int tint, int faces);
/* Box transformed by a matrix (for parts of characters and cars). */
void gm_box_xf(GMesh *m, const Xf *xf, V3 lo, V3 hi, const UVRect uvs[6], unsigned int tint, int faces);

/* Appends src transformed by xf (positions only, colours kept). */
void gm_append_xf(GMesh *dst, const GMesh *src, const Xf *xf);

/* Vertex with a normal, lit by the GE: matches GU_TEXTURE_32BITF |
 * GU_COLOR_8888 | GU_NORMAL_32BITF | GU_VERTEX_32BITF. color is the
 * unlit material colour. */
typedef struct {
    float u, v;
    unsigned int color;
    float nx, ny, nz;
    float x, y, z;
} LVertex;

typedef struct {
    LVertex *v;
    int n, cap;
} LMesh;

void lm_init(LMesh *m);
void lm_free(LMesh *m);
int  lm_reserve(LMesh *m, int extra);
/* adds a vertex transformed by xf (point and normal) */
void lm_vertex(LMesh *m, const Xf *xf, V3 p, V3 n, float u, float v, unsigned int color);
/* lit box (faces as in gm_box) */
void lm_box(LMesh *m, const Xf *xf, V3 lo, V3 hi, const UVRect uvs[6], unsigned int color, int faces);

/* Things that move are not rebuilt vertex by vertex every frame: their
 * meshes are built once and each frame only (mesh, matrix) pairs are
 * listed. The PSP draws them with the GE transforming (and lighting) the
 * vertices; the PC preview and the tests flatten the list on the CPU. */
typedef struct {
    const void *v;      /* GVertex (lit = 0) or LVertex (lit = 1) */
    int n;
    int lit;
    Xf xf;
} DrawItem;

typedef struct {
    DrawItem *items;
    int n, cap;
    int verts;          /* total vertices listed (for budgets) */
} DrawList;

void dl_init(DrawList *d);
void dl_free(DrawList *d);
void dl_clear(DrawList *d);
void dl_add(DrawList *d, const GVertex *v, int n, const Xf *xf);
void dl_add_lit(DrawList *d, const LVertex *v, int n, const Xf *xf);
/* world-space, CPU-lit copy of everything listed */
void dl_flatten(const DrawList *d, GMesh *out);

/* The PSP GE drops a whole triangle when a vertex in front of the camera
 * projects outside its 4096x4096 drawing area (PPSSPP emulates this): big
 * ground quads next to the camera vanish and the sky shows through. The fix
 * is to cut such triangles on the CPU to a cone a few times wider than the
 * screen (|x|, |y| <= k * depth), which keeps every vertex inside. */
typedef struct {
    V3 eye, right, up, fwd;
    float k;
} ViewCone;

void view_cone(ViewCone *c, V3 eye, float yaw, float pitch, float k);
/* 1 if every point of the box is inside the cone (draw it as it is) */
int  view_cone_contains(const ViewCone *c, V3 lo, V3 hi);
/* Copies triangles to out, cutting the ones that reach outside the cone.
 * Returns the number of vertices written, or -1 if out is too small. */
int  gm_cone_clip(const GVertex *v, int n, const ViewCone *c, GVertex *out, int cap);

/* Runs of consecutive triangles that lie close together, with a bounding
 * sphere: against the cone most runs are wholly inside (drawn straight from
 * the mesh) or wholly outside (skipped), only the few on its edge are cut. */
typedef struct {
    V3 c;
    float r;
    int first, count;   /* vertices */
} GChunk;

int  gm_chunks(const GVertex *v, int n, GChunk *out, int cap);  /* returns the chunk count, -1 if cap too small */
/* 1 sphere wholly inside the cone, -1 wholly outside, 0 on its edge */
int  view_cone_sphere(const ViewCone *c, V3 centre, float r);

#endif
