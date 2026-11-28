/* sceGu renderer for UkraineCraft. */
#include <pspkernel.h>
#include <pspdisplay.h>
#include <pspge.h>
#include <pspgu.h>
#include <pspgum.h>

#include <malloc.h>
#include <math.h>
#include <string.h>

#include "render.h"
#include "city.h"
#include "texgen.h"

#define BUF_W 512
#define FB0_OFF 0x000000
#define FB1_OFF 0x088000
#define ZB_OFF 0x110000
#define FAR_PLANE 360.0f
#define NEAR_PLANE 0.3f
#define MAX_BATCH 65520 /* vertices per draw call, multiple of 3 */

typedef struct { unsigned int color; float x, y, z; } CVertex;

#define VT_F3D (GU_TEXTURE_32BITF | GU_COLOR_8888 | GU_VERTEX_32BITF | GU_TRANSFORM_3D)
#define VT_C3D (GU_COLOR_8888 | GU_VERTEX_32BITF | GU_TRANSFORM_3D)
#define VT_F2D (GU_TEXTURE_32BITF | GU_COLOR_8888 | GU_VERTEX_32BITF | GU_TRANSFORM_2D)
#define VT_C2D (GU_COLOR_8888 | GU_VERTEX_32BITF | GU_TRANSFORM_2D)

static unsigned int __attribute__((aligned(16))) gu_list[512 * 1024];
#define MIP_LEVELS 4   /* 512, 256, 128, 64 */
#define TEX_LOD_BIAS (-0.5f)

/* Triangles of the cells around the camera are cut to a cone 6x wider than
 * the view (see gm_cone_clip): the screen limit for the GE is about 9x. */
#define GUARD_K 6.0f
#define CLIP_CAP 48000
static GVertex *clip_buf;
static int clip_used;
static ViewCone cam_cone;

static unsigned int *atlas_mip[MIP_LEVELS];
static unsigned int *font_tex;
static int bound_tex;

/* ------------------------------------------------------------------ utils */

static void swizzle(unsigned char *out, const unsigned char *in, unsigned int width_bytes, unsigned int height)
{
    unsigned int bx, by, j;
    unsigned int width_blocks = width_bytes / 16, height_blocks = height / 8;
    unsigned int src_pitch = (width_bytes - 16) / 4, src_row = width_bytes * 8;
    const unsigned char *ysrc = in;
    unsigned int *dst = (unsigned int *)out;
    for (by = 0; by < height_blocks; by++) {
        const unsigned char *xsrc = ysrc;
        for (bx = 0; bx < width_blocks; bx++) {
            const unsigned int *src = (const unsigned int *)xsrc;
            for (j = 0; j < 8; j++) {
                *dst++ = *src++;
                *dst++ = *src++;
                *dst++ = *src++;
                *dst++ = *src++;
                src += src_pitch;
            }
            xsrc += 16;
        }
        ysrc += src_row;
    }
}

static void bind_atlas(void)
{
    if (bound_tex != 1) {
        int l;
        /* mip-mapped: far surfaces use smaller copies instead of shimmering */
        sceGuTexMode(GU_PSM_8888, MIP_LEVELS - 1, 0, 1);
        for (l = 0; l < MIP_LEVELS; l++)
            sceGuTexImage(l, TEX_SIZE >> l, TEX_SIZE >> l, TEX_SIZE >> l, atlas_mip[l]);
        /* Each level sampled nearest (crisp pixels, and no bleeding from the
         * neighbouring atlas tile that bilinear filtering of the small levels
         * caused: thin lines along every ground quad), blended between levels
         * so there are no visible bands. Bias -0.5: one step sharper than the
         * plain choice, which is mush on the ground at grazing angles. */
        sceGuTexLevelMode(GU_TEXTURE_AUTO, TEX_LOD_BIAS);
        sceGuTexFilter(GU_NEAREST_MIPMAP_LINEAR, GU_NEAREST);
        sceGuTexFlush();
        bound_tex = 1;
    }
    sceGuTexScale(1.0f, 1.0f);
    sceGuTexOffset(0.0f, 0.0f);
}

static void bind_font(void)
{
    if (bound_tex != 2) {
        sceGuTexMode(GU_PSM_8888, 0, 0, 0);
        sceGuTexImage(0, FONT_TEX_W, FONT_TEX_H, FONT_TEX_W, font_tex);
        sceGuTexFilter(GU_NEAREST, GU_NEAREST);
        sceGuTexFlush();
        bound_tex = 2;
    }
}

static void draw_verts(const GVertex *v, int n)
{
    while (n > 0) {
        int k = n > MAX_BATCH ? MAX_BATCH : n;
        sceGumDrawArray(GU_TRIANGLES, VT_F3D, k, 0, v);
        v += k;
        n -= k;
    }
}

static void draw_cut(const GVertex *v, int n)
{
    int m = clip_buf ? gm_cone_clip(v, n, &cam_cone, clip_buf + clip_used, CLIP_CAP - clip_used) : -1;
    if (m < 0) {
        draw_verts(v, n);
    } else if (m > 0) {
        sceKernelDcacheWritebackRange(clip_buf + clip_used, (unsigned int)(m * sizeof(GVertex)));
        draw_verts(clip_buf + clip_used, m);
        clip_used += m;
    }
}

/* Draws a mesh near the camera chunk by chunk: chunks inside the cone go
 * straight from the mesh (consecutive ones in one call), chunks outside it
 * (behind the camera) are skipped, only those on its edge are cut. */
static void draw_chunked(const GVertex *v, const GChunk *ch, int nch)
{
    int i, run_first = -1, run_end = 0;
    for (i = 0; i < nch; i++) {
        int k = view_cone_sphere(&cam_cone, ch[i].c, ch[i].r);
        if (k > 0) {
            if (run_first >= 0 && run_end == ch[i].first) {
                run_end += ch[i].count;
            } else {
                if (run_first >= 0) draw_verts(v + run_first, run_end - run_first);
                run_first = ch[i].first;
                run_end = run_first + ch[i].count;
            }
            continue;
        }
        if (run_first >= 0) draw_verts(v + run_first, run_end - run_first);
        run_first = -1;
        if (k == 0) draw_cut(v + ch[i].first, ch[i].count);
    }
    if (run_first >= 0) draw_verts(v + run_first, run_end - run_first);
}

#define DYN_CHUNKS 4096
static GChunk dyn_chunks[DYN_CHUNKS];

/* ------------------------------------------------------------------- init */

void r_init(void)
{
    unsigned int *linear;

    linear = (unsigned int *)memalign(16, TEX_SIZE * TEX_SIZE * 4);
    texgen_atlas(linear);
    {
        unsigned int *half = (unsigned int *)memalign(16, (TEX_SIZE / 2) * (TEX_SIZE / 2) * 4);
        int l, size = TEX_SIZE;
        for (l = 0; l < MIP_LEVELS; l++, size >>= 1) {
            if (l > 0) {
                /* alpha-weighted 2x2 average of the previous level (shared with the PC preview) */
                texgen_mip(linear, size * 2, half);
                memcpy(linear, half, (size_t)size * size * 4);
            }
            atlas_mip[l] = (unsigned int *)memalign(16, (size_t)size * size * 4);
            swizzle((unsigned char *)atlas_mip[l], (const unsigned char *)linear, (unsigned int)size * 4, (unsigned int)size);
        }
        free(half);
    }
    free(linear);

    clip_buf = (GVertex *)memalign(16, CLIP_CAP * sizeof(GVertex));

    font_tex = (unsigned int *)memalign(16, FONT_TEX_W * FONT_TEX_H * 4);
    font_generate(font_tex);
    sceKernelDcacheWritebackInvalidateAll();

    sceGuInit();
    sceGuStart(GU_DIRECT, gu_list);
    sceGuDrawBuffer(GU_PSM_8888, (void *)FB0_OFF, BUF_W);
    sceGuDispBuffer(SCR_W, SCR_H, (void *)FB1_OFF, BUF_W);
    sceGuDepthBuffer((void *)ZB_OFF, BUF_W);
    sceGuOffset(2048 - (SCR_W / 2), 2048 - (SCR_H / 2));
    sceGuViewport(2048, 2048, SCR_W, SCR_H);
    sceGuDepthRange(65535, 0);
    sceGuScissor(0, 0, SCR_W, SCR_H);
    sceGuEnable(GU_SCISSOR_TEST);
    sceGuDepthFunc(GU_GEQUAL);
    sceGuEnable(GU_DEPTH_TEST);
    sceGuDisable(GU_CULL_FACE);
    sceGuShadeModel(GU_SMOOTH);
    sceGuEnable(GU_CLIP_PLANES);
    sceGuEnable(GU_TEXTURE_2D);
    sceGuTexFunc(GU_TFX_MODULATE, GU_TCC_RGBA);
    sceGuTexFilter(GU_NEAREST_MIPMAP_LINEAR, GU_NEAREST);
    sceGuTexWrap(GU_CLAMP, GU_CLAMP);
    sceGuAlphaFunc(GU_GREATER, 0x80, 0xFF);
    sceGuEnable(GU_ALPHA_TEST);
    sceGuBlendFunc(GU_ADD, GU_SRC_ALPHA, GU_ONE_MINUS_SRC_ALPHA, 0, 0);
    sceGuFinish();
    sceGuSync(0, 0);
    sceDisplayWaitVblankStart();
    sceGuDisplay(GU_TRUE);
}

void r_flush_cache(void) { sceKernelDcacheWritebackInvalidateAll(); }

void r_begin_frame(unsigned int clear)
{
    sceGuStart(GU_DIRECT, gu_list);
    bound_tex = 0;
    clip_used = 0;
    sceGuDepthMask(GU_FALSE);
    sceGuClearColor(clear);
    sceGuClearDepth(0);
    sceGuClear(GU_COLOR_BUFFER_BIT | GU_DEPTH_BUFFER_BIT);
}

void r_end_frame(void)
{
    sceGuFinish();
    sceGuSync(0, 0);
    sceDisplayWaitVblankStart();
    sceGuSwapBuffers();
}

static V3 cam_eye;
static float cam_fov = 60.0f;

/* near_scale > 1 moves everything drawn after it slightly towards the
 * camera in depth only (x, y on screen are unchanged): a polygon offset for
 * decals such as lit windows, which sit a few cm in front of a wall. */
static void projection(float near_scale)
{
    sceGumMatrixMode(GU_PROJECTION);
    sceGumLoadIdentity();
    sceGumPerspective(cam_fov, (float)SCR_W / SCR_H, NEAR_PLANE * near_scale, FAR_PLANE);
    sceGumMatrixMode(GU_MODEL);
}

void r_camera(V3 eye, float yaw, float pitch, float fov)
{
    ScePspFVector3 t;
    cam_eye = eye;
    cam_fov = fov;
    view_cone(&cam_cone, eye, yaw, pitch, GUARD_K);
    projection(1.0f);
    sceGumMatrixMode(GU_VIEW);
    sceGumLoadIdentity();
    sceGumRotateX(-pitch);
    sceGumRotateY(-yaw);
    t.x = -eye.x;
    t.y = -eye.y;
    t.z = -eye.z;
    sceGumTranslate(&t);
    sceGumMatrixMode(GU_MODEL);
    sceGumLoadIdentity();
}

/* -------------------------------------------------------------------- sky */

static void quad2d_gradient(float y0, float y1, unsigned int top, unsigned int bottom)
{
    CVertex *v = (CVertex *)sceGuGetMemory(6 * sizeof(CVertex));
    v[0].color = top;    v[0].x = 0;     v[0].y = y0; v[0].z = 0;
    v[1].color = top;    v[1].x = SCR_W; v[1].y = y0; v[1].z = 0;
    v[2].color = bottom; v[2].x = SCR_W; v[2].y = y1; v[2].z = 0;
    v[3] = v[0];
    v[4] = v[2];
    v[5].color = bottom; v[5].x = 0;     v[5].y = y1; v[5].z = 0;
    sceGuDrawArray(GU_TRIANGLES, VT_C2D, 6, 0, v);
}

static void billboard(V3 c, V3 right, V3 up, float size, UVRect uv, unsigned int color)
{
    GVertex *v = (GVertex *)sceGuGetMemory(6 * sizeof(GVertex));
    static const float CS[4][2] = { { -1, -1 }, { -1, 1 }, { 1, 1 }, { 1, -1 } };
    static const int ORDER[6] = { 0, 1, 2, 0, 2, 3 };
    int i;
    for (i = 0; i < 6; i++) {
        int k = ORDER[i];
        v[i].u = k >= 2 ? uv.u1 : uv.u0;
        v[i].v = (k == 1 || k == 2) ? uv.v0 : uv.v1;
        v[i].color = color;
        v[i].x = c.x + (right.x * CS[k][0] + up.x * CS[k][1]) * size;
        v[i].y = c.y + (right.y * CS[k][0] + up.y * CS[k][1]) * size;
        v[i].z = c.z + (right.z * CS[k][0] + up.z * CS[k][1]) * size;
    }
    sceGumDrawArray(GU_TRIANGLES, VT_F3D, 6, 0, v);
}

void r_sky(const SkyState *s, float yaw, float pitch, float fov)
{
    float focal = (SCR_H * 0.5f) / tanf(fov * 0.5f * PI_F / 180.0f);
    float hy = SCR_H * 0.5f + tanf(pitch) * focal, sh = sinf(s->sun_angle);
    V3 sun = v3_norm(v3(cosf(s->sun_angle), sh, 0.3f));

    sceGuDisable(GU_DEPTH_TEST);
    sceGuDisable(GU_TEXTURE_2D);
    sceGuDisable(GU_FOG);
    sceGuDisable(GU_ALPHA_TEST);
    sceGuDisable(GU_BLEND);
    if (hy > 0) quad2d_gradient(hy - 230.0f, hy, s->zenith, s->horizon);
    if (hy < SCR_H) quad2d_gradient(hy, SCR_H, s->horizon, s->horizon);

    /* sun and moon: rotation-only camera */
    sceGumMatrixMode(GU_PROJECTION);
    sceGumLoadIdentity();
    sceGumPerspective(fov, (float)SCR_W / SCR_H, 0.1f, FAR_PLANE);
    sceGumMatrixMode(GU_VIEW);
    sceGumLoadIdentity();
    sceGumRotateX(-pitch);
    sceGumRotateY(-yaw);
    sceGumMatrixMode(GU_MODEL);
    sceGumLoadIdentity();
    sceGuEnable(GU_TEXTURE_2D);
    sceGuEnable(GU_BLEND);
    sceGuBlendFunc(GU_ADD, GU_SRC_ALPHA, GU_FIX, 0, 0xFFFFFFFF);
    bind_atlas();
    {
        V3 r = v3_norm(v3_cross(sun, v3(0, 0, 1))), u = v3_cross(r, sun);
        if (sh > -0.15f) billboard(v3_scale(sun, 200.0f), r, u, 34.0f, uv_tile(T_FLARE), 0xFFB0F0FFu);
        if (sh < 0.15f) billboard(v3_scale(sun, -200.0f), r, u, 16.0f, uv_tile(T_FLARE), 0xFFFFF0E0u);
    }
    sceGuBlendFunc(GU_ADD, GU_SRC_ALPHA, GU_ONE_MINUS_SRC_ALPHA, 0, 0);
    sceGuDisable(GU_BLEND);
    sceGuEnable(GU_ALPHA_TEST);
    sceGuEnable(GU_DEPTH_TEST);
}

/* ------------------------------------------------------------------- city */

static int cell_visible(const CityCell *c, V3 eye, float fx, float fz, float pitch, float radius)
{
    float vx = c->cx - eye.x, vz = c->cz - eye.z, d = sqrtf(vx * vx + vz * vz), half, r;
    if (d > radius + 46.0f) return 0;
    if (d < 70.0f || fabsf(pitch) > 1.0f) return 1;
    r = 46.0f / d;
    half = 0.95f + fabsf(pitch) * 0.5f + asinf(r > 1.0f ? 1.0f : r);
    if (half >= 3.1f) return 1;
    return (vx * fx + vz * fz) / d >= cosf(half);
}

void r_city(V3 eye, float yaw, float pitch, float radius, const SkyState *s)
{
    float fx = -sinf(yaw), fz = -cosf(yaw);
    int i, j;
    bind_atlas();
    sceGuEnable(GU_TEXTURE_2D);
    sceGuEnable(GU_DEPTH_TEST);
    sceGuEnable(GU_ALPHA_TEST);
    sceGuDisable(GU_BLEND);
    sceGuEnable(GU_FOG);
    sceGuFog(s->fog_start, s->fog_end, s->fog);
    sceGumMatrixMode(GU_MODEL);
    sceGumLoadIdentity();
    for (j = CELL_MIN; j < CELL_MIN + CELLS; j++)
        for (i = CELL_MIN; i < CELL_MIN + CELLS; i++) {
            CityCell *c = city_cell(i, j);
            if (!c->solid.n || !cell_visible(c, eye, fx, fz, pitch, radius)) continue;
            if (view_cone_contains(&cam_cone, v3(c->cx - LOT * 0.5f, -1.0f, c->cz - LOT * 0.5f),
                                   v3(c->cx + LOT * 0.5f, 60.0f, c->cz + LOT * 0.5f)))
                draw_verts(c->solid.v, c->solid.n);
            else if (c->nchunks)
                draw_chunked(c->solid.v, c->chunks, c->nchunks); /* near the camera */
            else
                draw_cut(c->solid.v, c->solid.n);
        }
    sceGuDisable(GU_FOG);
}

void r_glow(V3 eye, float radius, float night)
{
    unsigned int a = (unsigned int)(clampf(night * 1.4f - 0.3f, 0.0f, 1.0f) * 255.0f);
    int i, j;
    if (a < 8) return;
    bind_atlas();
    sceGuEnable(GU_BLEND);
    sceGuDisable(GU_ALPHA_TEST);
    sceGuDepthMask(GU_TRUE);
    /* additive, scaled by how dark it is */
    sceGuBlendFunc(GU_ADD, GU_FIX, GU_FIX, (a << 16) | (a << 8) | a, 0xFFFFFF);
    projection(1.015f); /* the lit panes are 8 cm in front of the facade */
    for (j = CELL_MIN; j < CELL_MIN + CELLS; j++)
        for (i = CELL_MIN; i < CELL_MIN + CELLS; i++) {
            CityCell *c = city_cell(i, j);
            float dx = c->cx - eye.x, dz = c->cz - eye.z;
            if (!c->glow.n || dx * dx + dz * dz > (radius + 46.0f) * (radius + 46.0f)) continue;
            draw_verts(c->glow.v, c->glow.n);
        }
    projection(1.0f);
    sceGuBlendFunc(GU_ADD, GU_SRC_ALPHA, GU_ONE_MINUS_SRC_ALPHA, 0, 0);
    sceGuDepthMask(GU_FALSE);
    sceGuEnable(GU_ALPHA_TEST);
    sceGuDisable(GU_BLEND);
}

void r_mesh(const GMesh *m, const SkyState *s)
{
    if (!m->n) return;
    sceKernelDcacheWritebackRange(m->v, (unsigned int)(m->n * sizeof(GVertex)));
    bind_atlas();
    sceGuEnable(GU_TEXTURE_2D);
    sceGuEnable(GU_DEPTH_TEST);
    sceGuEnable(GU_ALPHA_TEST);
    sceGuDisable(GU_BLEND);
    sceGuEnable(GU_FOG);
    sceGuFog(s->fog_start, s->fog_end, s->fog);
    sceGumMatrixMode(GU_MODEL);
    sceGumLoadIdentity();
    {
        int nc = gm_chunks(m->v, m->n, dyn_chunks, DYN_CHUNKS);
        if (nc > 0) draw_chunked(m->v, dyn_chunks, nc);
        else draw_cut(m->v, m->n);
    }
    sceGuDisable(GU_FOG);
}

void r_shadow(V3 p, float hw, float hl, float yaw, float alpha)
{
    GVertex *v = (GVertex *)sceGuGetMemory(6 * sizeof(GVertex));
    V3 f = yaw_fwd(yaw), r = yaw_right(yaw);
    UVRect uv = uv_tile(T_SHADOW);
    V3 c[4];
    static const int ORDER[6] = { 0, 1, 2, 0, 2, 3 };
    float u[4] = { uv.u0, uv.u0, uv.u1, uv.u1 }, w[4] = { uv.v1, uv.v0, uv.v0, uv.v1 };
    float dx = p.x - cam_eye.x, dz = p.z - cam_eye.z, dist = sqrtf(dx * dx + dz * dz);
    unsigned int col;
    int i;
    /* a shadow 6 cm above the ground loses the depth test against it far
     * away (16-bit depth buffer): fade it out instead of letting it flicker */
    alpha *= clampf((50.0f - dist) / 15.0f, 0.0f, 1.0f);
    if (alpha <= 0.01f) return;
    col = ((unsigned int)(clampf(alpha, 0, 1) * 255.0f) << 24) | 0xFFFFFF;
    c[0] = v3_add(p, v3_add(v3_scale(r, -hw), v3_scale(f, -hl)));
    c[1] = v3_add(p, v3_add(v3_scale(r, -hw), v3_scale(f, hl)));
    c[2] = v3_add(p, v3_add(v3_scale(r, hw), v3_scale(f, hl)));
    c[3] = v3_add(p, v3_add(v3_scale(r, hw), v3_scale(f, -hl)));
    for (i = 0; i < 6; i++) {
        int k = ORDER[i];
        v[i].u = u[k];
        v[i].v = w[k];
        v[i].color = col;
        v[i].x = c[k].x;
        v[i].y = p.y + 0.06f;
        v[i].z = c[k].z;
    }
    bind_atlas();
    sceGumMatrixMode(GU_MODEL);
    sceGumLoadIdentity();
    sceGuEnable(GU_TEXTURE_2D);
    sceGuDisable(GU_ALPHA_TEST);
    sceGuEnable(GU_BLEND);
    sceGuDepthMask(GU_TRUE);
    projection(1.01f);
    sceGumDrawArray(GU_TRIANGLES, VT_F3D, 6, 0, v);
    projection(1.0f);
    sceGuDepthMask(GU_FALSE);
    sceGuDisable(GU_BLEND);
    sceGuEnable(GU_ALPHA_TEST);
}

/* camera-facing glow sprite, additive */
static V3 cam_right = { 1, 0, 0 }, cam_up = { 0, 1, 0 };

void r_flare(V3 pos, float size, unsigned int color)
{
    bind_atlas();
    sceGumMatrixMode(GU_MODEL);
    sceGumLoadIdentity();
    sceGuEnable(GU_TEXTURE_2D);
    sceGuDisable(GU_ALPHA_TEST);
    sceGuDisable(GU_FOG);
    sceGuEnable(GU_BLEND);
    sceGuDepthMask(GU_TRUE);
    sceGuBlendFunc(GU_ADD, GU_SRC_ALPHA, GU_FIX, 0, 0xFFFFFFFF);
    billboard(pos, cam_right, cam_up, size, uv_tile(T_FLARE), color);
    sceGuBlendFunc(GU_ADD, GU_SRC_ALPHA, GU_ONE_MINUS_SRC_ALPHA, 0, 0);
    sceGuDepthMask(GU_FALSE);
    sceGuDisable(GU_BLEND);
    sceGuEnable(GU_ALPHA_TEST);
}

/* ----------------------------------------------------------------- clouds */

static float cloud_lat(int x, int z)
{
    unsigned int h = (unsigned int)x * 0x8DA6B343u ^ (unsigned int)z * 0xD8163841u;
    h ^= h >> 13;
    h *= 0x5BD1E995u;
    h ^= h >> 15;
    return (h & 0xFFFF) / 65535.0f;
}

static float cloud_noise(float x, float z)
{
    int ix = (int)floorf(x), iz = (int)floorf(z);
    float fx = x - ix, fz = z - iz, a, b;
    fx = fx * fx * (3 - 2 * fx);
    fz = fz * fz * (3 - 2 * fz);
    a = cloud_lat(ix, iz) + (cloud_lat(ix + 1, iz) - cloud_lat(ix, iz)) * fx;
    b = cloud_lat(ix, iz + 1) + (cloud_lat(ix + 1, iz + 1) - cloud_lat(ix, iz + 1)) * fx;
    return a + (b - a) * fz;
}

void r_clouds(V3 eye, const SkyState *s)
{
    const float cell = 24.0f, y = 120.0f, reach = 12.0f * 24.0f;
    const int n = 24;
    float off = s->time_s * 2.0f;
    int ox = (int)floorf((eye.x + off) / cell), oz = (int)floorf(eye.z / cell);
    CVertex *v = (CVertex *)sceGuGetMemory(n * n * 6 * sizeof(CVertex));
    int i, j, count = 0;
    for (j = -n / 2; j < n / 2; j++)
        for (i = -n / 2; i < n / 2; i++) {
            int cx = ox + i, cz = oz + j;
            float x0 = cx * cell - off, z0 = cz * cell, dxc, dzc, d, a;
            unsigned int col;
            if (cloud_noise(cx * 0.31f, cz * 0.31f) < 0.6f) continue;
            dxc = x0 + cell * 0.5f - eye.x;
            dzc = z0 + cell * 0.5f - eye.z;
            d = sqrtf(dxc * dxc + dzc * dzc);
            a = 0.8f * (1.0f - d / reach);
            if (a <= 0.02f) continue;
            col = (s->cloud & 0x00FFFFFFu) | ((unsigned int)(a * 255.0f) << 24);
            v[count + 0].color = col; v[count + 0].x = x0;        v[count + 0].y = y; v[count + 0].z = z0;
            v[count + 1].color = col; v[count + 1].x = x0 + cell; v[count + 1].y = y; v[count + 1].z = z0;
            v[count + 2].color = col; v[count + 2].x = x0 + cell; v[count + 2].y = y; v[count + 2].z = z0 + cell;
            v[count + 3] = v[count + 0];
            v[count + 4] = v[count + 2];
            v[count + 5].color = col; v[count + 5].x = x0;        v[count + 5].y = y; v[count + 5].z = z0 + cell;
            count += 6;
        }
    if (!count) return;
    sceGumMatrixMode(GU_MODEL);
    sceGumLoadIdentity();
    sceGuDisable(GU_TEXTURE_2D);
    sceGuDisable(GU_ALPHA_TEST);
    sceGuEnable(GU_BLEND);
    sceGuDepthMask(GU_TRUE);
    sceGumDrawArray(GU_TRIANGLES, VT_C3D, count, 0, v);
    sceGuDepthMask(GU_FALSE);
    sceGuDisable(GU_BLEND);
    sceGuEnable(GU_ALPHA_TEST);
    sceGuEnable(GU_TEXTURE_2D);
}

/* Darkens pixels covered by geometry (depth written), not the sky. */
void r_night_overlay(const SkyState *s)
{
    CVertex *v;
    unsigned int a;
    if (s->night <= 0.01f) return;
    a = (unsigned int)(s->night * 0.7f * 255.0f);
    v = (CVertex *)sceGuGetMemory(2 * sizeof(CVertex));
    v[0].color = (a << 24) | 0x1E0A08u; v[0].x = 0;     v[0].y = 0;     v[0].z = 1.0f;
    v[1].color = v[0].color;            v[1].x = SCR_W; v[1].y = SCR_H; v[1].z = 1.0f;
    sceGuDisable(GU_TEXTURE_2D);
    sceGuDisable(GU_ALPHA_TEST);
    sceGuEnable(GU_BLEND);
    sceGuEnable(GU_DEPTH_TEST);
    sceGuDepthFunc(GU_LESS);
    sceGuDepthMask(GU_TRUE);
    sceGuDrawArray(GU_SPRITES, VT_C2D, 2, 0, v);
    sceGuDepthMask(GU_FALSE);
    sceGuDepthFunc(GU_GEQUAL);
    sceGuDisable(GU_BLEND);
    sceGuEnable(GU_ALPHA_TEST);
    sceGuEnable(GU_TEXTURE_2D);
}

void r_marker(V3 pos, float t)
{
    const int sides = 12;
    const float r = 0.75f, h = 2.6f;
    CVertex *v = (CVertex *)sceGuGetMemory(sides * 6 * sizeof(CVertex));
    float bob = 0.12f * sinf(t * 3.0f);
    int i, n = 0;
    for (i = 0; i < sides; i++) {
        float a0 = i * 6.2831853f / sides, a1 = (i + 1) * 6.2831853f / sides;
        float x0 = pos.x + cosf(a0) * r, z0 = pos.z + sinf(a0) * r;
        float x1 = pos.x + cosf(a1) * r, z1 = pos.z + sinf(a1) * r;
        unsigned int lo = 0xA020D8FFu, hi = 0x0020D8FFu;
        CVertex q0 = { lo, x0, pos.y + bob, z0 }, q1 = { hi, x0, pos.y + h + bob, z0 };
        CVertex q2 = { hi, x1, pos.y + h + bob, z1 }, q3 = { lo, x1, pos.y + bob, z1 };
        v[n++] = q0; v[n++] = q1; v[n++] = q2;
        v[n++] = q0; v[n++] = q2; v[n++] = q3;
    }
    sceGumMatrixMode(GU_MODEL);
    sceGumLoadIdentity();
    sceGuDisable(GU_TEXTURE_2D);
    sceGuDisable(GU_ALPHA_TEST);
    sceGuDisable(GU_FOG);
    sceGuEnable(GU_BLEND);
    sceGuDepthMask(GU_TRUE);
    sceGumDrawArray(GU_TRIANGLES, VT_C3D, n, 0, v);
    sceGuDepthMask(GU_FALSE);
    sceGuDisable(GU_BLEND);
    sceGuEnable(GU_ALPHA_TEST);
    sceGuEnable(GU_TEXTURE_2D);
}

/* --------------------------------------------------------------------- 2D */

void r_begin_2d(void)
{
    sceGuDisable(GU_DEPTH_TEST);
    sceGuDisable(GU_FOG);
    sceGuDisable(GU_ALPHA_TEST);
    sceGuEnable(GU_BLEND);
}

void r_rect(float x, float y, float w, float h, unsigned int color)
{
    CVertex *v = (CVertex *)sceGuGetMemory(2 * sizeof(CVertex));
    v[0].color = color; v[0].x = x;     v[0].y = y;     v[0].z = 0.0f;
    v[1].color = color; v[1].x = x + w; v[1].y = y + h; v[1].z = 0.0f;
    sceGuDisable(GU_TEXTURE_2D);
    sceGuDrawArray(GU_SPRITES, VT_C2D, 2, 0, v);
    sceGuEnable(GU_TEXTURE_2D);
}

static int utf8_len(const char *s)
{
    int n = 0;
    while (*s) {
        utf8_next(&s);
        n++;
    }
    return n;
}

int r_text_width(int scale, const char *s)
{
    int n = utf8_len(s);
    return n ? (n * 6 - 1) * scale : 0;
}

void r_text(float x, float y, int scale, unsigned int color, const char *s)
{
    int n = utf8_len(s), i, pass;
    if (!n) return;
    bind_font();
    sceGuEnable(GU_TEXTURE_2D);
    for (pass = 0; pass < 2; pass++) {
        unsigned int col = pass == 0 ? (color & 0xFF000000u) | 0x202020u : color;
        float off = pass == 0 ? (float)scale : 0.0f;
        const char *p = s;
        GVertex *v = (GVertex *)sceGuGetMemory((unsigned int)n * 2 * sizeof(GVertex));
        for (i = 0; i < n; i++) {
            int g = font_glyph(utf8_next(&p));
            float gx = (float)((g % 16) * 8), gy = (float)((g / 16) * 8);
            v[i * 2].u = gx;        v[i * 2].v = gy;
            v[i * 2].color = col;
            v[i * 2].x = x + off + i * 6 * scale; v[i * 2].y = y + off; v[i * 2].z = 0;
            v[i * 2 + 1].u = gx + 8; v[i * 2 + 1].v = gy + 8;
            v[i * 2 + 1].color = col;
            v[i * 2 + 1].x = x + off + i * 6 * scale + 8 * scale; v[i * 2 + 1].y = y + off + 8 * scale;
            v[i * 2 + 1].z = 0;
        }
        sceGuDrawArray(GU_SPRITES, VT_F2D, n * 2, 0, v);
    }
}

void r_arrow(float cx, float cy, float angle, float size, unsigned int color)
{
    CVertex *v = (CVertex *)sceGuGetMemory(3 * sizeof(CVertex));
    float s = sinf(angle), c = cosf(angle);
    float pts[3][2] = { { 0.0f, -size }, { size * 0.7f, size * 0.7f }, { -size * 0.7f, size * 0.7f } };
    int i;
    for (i = 0; i < 3; i++) {
        v[i].color = color;
        v[i].x = cx + pts[i][0] * c - pts[i][1] * s;
        v[i].y = cy + pts[i][0] * s + pts[i][1] * c;
        v[i].z = 0.0f;
    }
    sceGuDisable(GU_TEXTURE_2D);
    sceGuDrawArray(GU_TRIANGLES, VT_C2D, 3, 0, v);
    sceGuEnable(GU_TEXTURE_2D);
}

void r_gradient(float x, float y, float w, float h, unsigned int top, unsigned int bottom)
{
    CVertex *v = (CVertex *)sceGuGetMemory(6 * sizeof(CVertex));
    v[0].color = top;    v[0].x = x;     v[0].y = y;     v[0].z = 0;
    v[1].color = top;    v[1].x = x + w; v[1].y = y;     v[1].z = 0;
    v[2].color = bottom; v[2].x = x + w; v[2].y = y + h; v[2].z = 0;
    v[3] = v[0];
    v[4] = v[2];
    v[5].color = bottom; v[5].x = x;     v[5].y = y + h; v[5].z = 0;
    sceGuDisable(GU_TEXTURE_2D);
    sceGuDrawArray(GU_TRIANGLES, VT_C2D, 6, 0, v);
    sceGuEnable(GU_TEXTURE_2D);
}

/* camera basis for flares, set by the game each frame */
void r_set_billboard_basis(V3 right, V3 up)
{
    cam_right = right;
    cam_up = up;
}
