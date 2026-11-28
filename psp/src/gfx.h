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

#endif
