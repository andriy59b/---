/* sceGu renderer: chunks, sky, clouds, held block, 2D HUD primitives. */
#include <pspkernel.h>
#include <pspdisplay.h>
#include <pspge.h>
#include <pspgu.h>
#include <pspgum.h>

#include <malloc.h>
#include <math.h>
#include <string.h>

#include "render.h"
#include "mesh.h"
#include "textures.h"
#include "world.h"

#define BUF_W 512
#define FB0_OFF 0x000000
#define FB1_OFF 0x088000
#define ZB_OFF 0x110000
#define ATLAS_OFF 0x154000 /* after the 16-bit depth buffer, 256 KiB */

#define FOCAL 194.2f /* (SCR_H / 2) / tan(35 deg) */
#define FAR_PLANE 220.0f

typedef struct { float u, v; unsigned int color; float x, y, z; } FVertex;
typedef struct { unsigned int color; float x, y, z; } CVertex;

#define VT_CHUNK (GU_TEXTURE_16BIT | GU_COLOR_8888 | GU_VERTEX_16BIT | GU_INDEX_16BIT | GU_TRANSFORM_3D)
#define VT_F3D (GU_TEXTURE_32BITF | GU_COLOR_8888 | GU_VERTEX_32BITF | GU_TRANSFORM_3D)
#define VT_C3D (GU_COLOR_8888 | GU_VERTEX_32BITF | GU_TRANSFORM_3D)
#define VT_F2D (GU_TEXTURE_32BITF | GU_COLOR_8888 | GU_VERTEX_32BITF | GU_TRANSFORM_2D)
#define VT_C2D (GU_COLOR_8888 | GU_VERTEX_32BITF | GU_TRANSFORM_2D)

#define QUADS_PER_BATCH 10920 /* 6 * 10920 indices < 65536 */

static unsigned short __attribute__((aligned(16))) quad_idx[QUADS_PER_BATCH * 6];
static unsigned int __attribute__((aligned(16))) gu_list[262144];
static unsigned int *atlas_tex;
static unsigned int *font_tex;
static int bound_tex; /* 0 none, 1 atlas, 2 font */

static const signed char CUBE_CORNERS[6][4][3] = {
    { {1,0,1}, {1,1,1}, {1,1,0}, {1,0,0} },
    { {0,0,0}, {0,1,0}, {0,1,1}, {0,0,1} },
    { {0,1,1}, {0,1,0}, {1,1,0}, {1,1,1} },
    { {0,0,0}, {0,0,1}, {1,0,1}, {1,0,0} },
    { {0,0,1}, {0,1,1}, {1,1,1}, {1,0,1} },
    { {1,0,0}, {1,1,0}, {0,1,0}, {0,0,0} },
};
static const float CUBE_SHADE[6] = { 0.80f, 0.80f, 1.00f, 0.50f, 0.65f, 0.65f };
static const float CORNER_UV[4][2] = { { 0, 1 }, { 0, 0 }, { 1, 0 }, { 1, 1 } };

/* ------------------------------------------------------------------ utils */

static unsigned int scale_color(unsigned int c, float s, unsigned int alpha)
{
    unsigned int r = (unsigned int)((c & 0xFF) * s), g = (unsigned int)(((c >> 8) & 0xFF) * s);
    unsigned int b = (unsigned int)(((c >> 16) & 0xFF) * s);
    if (r > 255) r = 255;
    if (g > 255) g = 255;
    if (b > 255) b = 255;
    return (alpha << 24) | (b << 16) | (g << 8) | r;
}

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

static void bind_atlas(float uv_scale)
{
    if (bound_tex != 1) {
        sceGuTexMode(GU_PSM_8888, 0, 0, 1);
        sceGuTexImage(0, ATLAS_SIZE, ATLAS_SIZE, ATLAS_SIZE, atlas_tex);
        sceGuTexFlush();
        bound_tex = 1;
    }
    sceGuTexScale(uv_scale, uv_scale);
    sceGuTexOffset(0.0f, 0.0f);
}

static void bind_font(void)
{
    if (bound_tex != 2) {
        sceGuTexMode(GU_PSM_8888, 0, 0, 0);
        sceGuTexImage(0, FONT_TEX_W, FONT_TEX_H, FONT_TEX_W, font_tex);
        sceGuTexFlush();
        bound_tex = 2;
    }
}

/* ------------------------------------------------------------------- init */

void r_init(void)
{
    unsigned int *linear;
    unsigned int vram = (unsigned int)sceGeEdramGetAddr();
    int i;

    for (i = 0; i < QUADS_PER_BATCH; i++) {
        quad_idx[i * 6 + 0] = (unsigned short)(i * 4 + 0);
        quad_idx[i * 6 + 1] = (unsigned short)(i * 4 + 1);
        quad_idx[i * 6 + 2] = (unsigned short)(i * 4 + 2);
        quad_idx[i * 6 + 3] = (unsigned short)(i * 4 + 0);
        quad_idx[i * 6 + 4] = (unsigned short)(i * 4 + 2);
        quad_idx[i * 6 + 5] = (unsigned short)(i * 4 + 3);
    }

    /* block atlas: generated in RAM, stored swizzled in VRAM for fast sampling */
    linear = (unsigned int *)memalign(16, ATLAS_SIZE * ATLAS_SIZE * 4);
    textures_generate(linear);
    atlas_tex = (unsigned int *)(vram + ATLAS_OFF);
    swizzle((unsigned char *)((vram + ATLAS_OFF) | 0x40000000u), (const unsigned char *)linear,
            ATLAS_SIZE * 4, ATLAS_SIZE);
    free(linear);

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
    sceGuTexFilter(GU_NEAREST, GU_NEAREST);
    sceGuTexWrap(GU_REPEAT, GU_REPEAT);
    sceGuAlphaFunc(GU_GREATER, 0x80, 0xFF);
    sceGuBlendFunc(GU_ADD, GU_SRC_ALPHA, GU_ONE_MINUS_SRC_ALPHA, 0, 0);
    sceGuFinish();
    sceGuSync(0, 0);
    sceDisplayWaitVblankStart();
    sceGuDisplay(GU_TRUE);
}

void r_flush_cache(void)
{
    sceKernelDcacheWritebackInvalidateAll();
}

void r_begin_frame(unsigned int clear)
{
    sceGuStart(GU_DIRECT, gu_list);
    bound_tex = 0;
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

/* ----------------------------------------------------------------- camera */

void r_camera(const float eye[3], float yaw, float pitch)
{
    ScePspFVector3 t;
    sceGumMatrixMode(GU_PROJECTION);
    sceGumLoadIdentity();
    sceGumPerspective(70.0f, (float)SCR_W / SCR_H, 0.1f, FAR_PLANE);
    sceGumMatrixMode(GU_VIEW);
    sceGumLoadIdentity();
    sceGumRotateX(-pitch);
    sceGumRotateY(-yaw);
    t.x = -eye[0];
    t.y = -eye[1];
    t.z = -eye[2];
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

static void celestial(const float dir[3], float size, int tile, unsigned int color)
{
    const float dist = 150.0f;
    float u[3], w[3], c[3], len;
    float tu = (tile % ATLAS_TILES) / (float)ATLAS_TILES, tv = (tile / ATLAS_TILES) / (float)ATLAS_TILES;
    float span = 1.0f / ATLAS_TILES;
    FVertex *v = (FVertex *)sceGuGetMemory(6 * sizeof(FVertex));
    static const int ORDER[6] = { 0, 1, 2, 0, 2, 3 };
    float cs[4][2] = { { -1, -1 }, { -1, 1 }, { 1, 1 }, { 1, -1 } };
    int i;

    /* u = dir x Z, w = u x dir */
    u[0] = dir[1]; u[1] = -dir[0]; u[2] = 0.0f;
    len = sqrtf(u[0] * u[0] + u[1] * u[1]);
    if (len < 1e-4f) { u[0] = 1.0f; u[1] = 0.0f; len = 1.0f; }
    u[0] /= len; u[1] /= len;
    w[0] = u[1] * dir[2] - u[2] * dir[1];
    w[1] = u[2] * dir[0] - u[0] * dir[2];
    w[2] = u[0] * dir[1] - u[1] * dir[0];
    c[0] = dir[0] * dist; c[1] = dir[1] * dist; c[2] = dir[2] * dist;

    for (i = 0; i < 6; i++) {
        int k = ORDER[i];
        v[i].u = tu + (cs[k][0] * 0.5f + 0.5f) * span;
        v[i].v = tv + (cs[k][1] * 0.5f + 0.5f) * span;
        v[i].color = color;
        v[i].x = c[0] + (u[0] * cs[k][0] + w[0] * cs[k][1]) * size;
        v[i].y = c[1] + (u[1] * cs[k][0] + w[1] * cs[k][1]) * size;
        v[i].z = c[2] + (u[2] * cs[k][0] + w[2] * cs[k][1]) * size;
    }
    sceGumDrawArray(GU_TRIANGLES, VT_F3D, 6, 0, v);
}

void r_sky(const SkyState *s, float yaw, float pitch)
{
    float hy = SCR_H * 0.5f + tanf(pitch) * FOCAL;
    float sun[3], len, sh = sinf(s->sun_angle);

    sceGuDisable(GU_DEPTH_TEST);
    sceGuDisable(GU_TEXTURE_2D);
    sceGuDisable(GU_FOG);
    sceGuDisable(GU_ALPHA_TEST);
    sceGuDisable(GU_BLEND);

    if (s->underwater) {
        quad2d_gradient(0, SCR_H, s->fog, s->fog);
    } else {
        if (hy > 0) quad2d_gradient(hy - 230.0f, hy, s->zenith, s->horizon);
        if (hy < SCR_H) quad2d_gradient(hy, SCR_H, s->horizon, s->horizon);

        /* sun and moon follow the camera rotation only */
        sceGumMatrixMode(GU_PROJECTION);
        sceGumLoadIdentity();
        sceGumPerspective(70.0f, (float)SCR_W / SCR_H, 0.1f, FAR_PLANE);
        sceGumMatrixMode(GU_VIEW);
        sceGumLoadIdentity();
        sceGumRotateX(-pitch);
        sceGumRotateY(-yaw);

        sun[0] = cosf(s->sun_angle);
        sun[1] = sh;
        sun[2] = 0.25f;
        len = sqrtf(sun[0] * sun[0] + sun[1] * sun[1] + sun[2] * sun[2]);
        sun[0] /= len; sun[1] /= len; sun[2] /= len;

        sceGuEnable(GU_TEXTURE_2D);
        sceGuEnable(GU_BLEND);
        bind_atlas(1.0f);
        sceGumMatrixMode(GU_MODEL);
        sceGumLoadIdentity();
        if (sh > -0.15f) celestial(sun, 16.0f, TILE_SUN, 0xFFFFFFFFu);
        if (sh < 0.15f) {
            float moon[3] = { -sun[0], -sun[1], -sun[2] };
            celestial(moon, 13.0f, TILE_MOON, 0xFFFFFFFFu);
        }
        sceGuDisable(GU_BLEND);
    }

    sceGuEnable(GU_TEXTURE_2D);
    sceGuEnable(GU_ALPHA_TEST);
    sceGuEnable(GU_DEPTH_TEST);
}

/* ------------------------------------------------------------------ world */

static void draw_quads(const Vertex *v, int n)
{
    while (n > 0) {
        int k = n > QUADS_PER_BATCH ? QUADS_PER_BATCH : n;
        sceGumDrawArray(GU_TRIANGLES, VT_CHUNK, k * 6, quad_idx, v);
        v += k * 4;
        n -= k;
    }
}

static int chunk_in_view(const Chunk *c, const float eye[3], float fx, float fz, float pitch)
{
    float vx = c->cx * CH + CH * 0.5f - eye[0], vz = c->cz * CH + CH * 0.5f - eye[2];
    float d = sqrtf(vx * vx + vz * vz), half, r;
    if (d < 24.0f || fabsf(pitch) > 1.1f) return 1;
    r = 11.4f / d;
    half = 0.92f + fabsf(pitch) * 0.6f + asinf(r > 1.0f ? 1.0f : r);
    if (half >= 3.14159f) return 1;
    return (vx * fx + vz * fz) / d >= cosf(half);
}

static void set_chunk_matrix(const Chunk *c)
{
    ScePspFVector3 t, s;
    t.x = (float)(c->cx * CH);
    t.y = 0.0f;
    t.z = (float)(c->cz * CH);
    s.x = s.y = s.z = 32768.0f / VPOS_SCALE;
    sceGumMatrixMode(GU_MODEL);
    sceGumLoadIdentity();
    sceGumTranslate(&t);
    sceGumScale(&s);
}

void r_world(const float eye[3], float yaw, float pitch, int pcx, int pcz, int radius, const SkyState *s)
{
    float fx = -sinf(yaw), fz = -cosf(yaw);
    int dx, dz, pass;

    bind_atlas(2.0f);
    sceGuEnable(GU_TEXTURE_2D);
    sceGuEnable(GU_DEPTH_TEST);
    sceGuDepthFunc(GU_GEQUAL);
    sceGuDepthMask(GU_FALSE);
    sceGuEnable(GU_FOG);
    sceGuFog(s->fog_start, s->fog_end, s->fog);

    for (pass = 0; pass < 2; pass++) {
        if (pass == 0) {
            sceGuEnable(GU_ALPHA_TEST);
            sceGuDisable(GU_BLEND);
        } else {
            sceGuDisable(GU_ALPHA_TEST);
            sceGuEnable(GU_BLEND);
        }
        for (dz = -radius; dz <= radius; dz++)
            for (dx = -radius; dx <= radius; dx++) {
                Chunk *c = world_chunk(pcx + dx, pcz + dz);
                if (!c || !c->meshed) continue;
                if (pass == 0 ? !c->nsolid : !c->nwater) continue;
                if (!chunk_in_view(c, eye, fx, fz, pitch)) continue;
                set_chunk_matrix(c);
                if (pass == 0) draw_quads(c->solid, c->nsolid);
                else draw_quads(c->water, c->nwater);
            }
    }

    sceGuDisable(GU_BLEND);
    sceGuEnable(GU_ALPHA_TEST);
    sceGuDisable(GU_FOG);
    sceGumMatrixMode(GU_MODEL);
    sceGumLoadIdentity();
}

void r_outline(const int b[3])
{
    static const signed char E[12][2][3] = {
        {{0,0,0},{1,0,0}}, {{1,0,0},{1,0,1}}, {{1,0,1},{0,0,1}}, {{0,0,1},{0,0,0}},
        {{0,1,0},{1,1,0}}, {{1,1,0},{1,1,1}}, {{1,1,1},{0,1,1}}, {{0,1,1},{0,1,0}},
        {{0,0,0},{0,1,0}}, {{1,0,0},{1,1,0}}, {{1,0,1},{1,1,1}}, {{0,0,1},{0,1,1}},
    };
    const float g = 0.004f;
    CVertex *v = (CVertex *)sceGuGetMemory(24 * sizeof(CVertex));
    int e, k;
    for (e = 0; e < 12; e++)
        for (k = 0; k < 2; k++) {
            CVertex *cv = &v[e * 2 + k];
            cv->color = 0xC0000000u;
            cv->x = b[0] + (E[e][k][0] ? 1.0f + g : -g);
            cv->y = b[1] + (E[e][k][1] ? 1.0f + g : -g);
            cv->z = b[2] + (E[e][k][2] ? 1.0f + g : -g);
        }
    sceGumMatrixMode(GU_MODEL);
    sceGumLoadIdentity();
    sceGuDisable(GU_TEXTURE_2D);
    sceGuEnable(GU_BLEND);
    sceGumDrawArray(GU_LINES, VT_C3D, 24, 0, v);
    sceGuDisable(GU_BLEND);
    sceGuEnable(GU_TEXTURE_2D);
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

void r_clouds(const float eye[3], const SkyState *s)
{
    const float cell = 12.0f, y = WH + 44.0f, reach = 11.0f * 12.0f;
    const int n = 22;
    float off = s->time_s * 1.5f;
    int ox = (int)floorf((eye[0] + off) / cell), oz = (int)floorf(eye[2] / cell);
    CVertex *v = (CVertex *)sceGuGetMemory(n * n * 6 * sizeof(CVertex));
    int i, j, count = 0;

    if (s->underwater) return;
    for (j = -n / 2; j < n / 2; j++)
        for (i = -n / 2; i < n / 2; i++) {
            int cx = ox + i, cz = oz + j;
            float x0 = cx * cell - off, z0 = cz * cell, dxc, dzc, d, a;
            unsigned int col;
            if (cloud_noise(cx * 0.31f, cz * 0.31f) < 0.6f) continue;
            dxc = x0 + cell * 0.5f - eye[0];
            dzc = z0 + cell * 0.5f - eye[2];
            d = sqrtf(dxc * dxc + dzc * dzc);
            a = 0.82f * (1.0f - d / reach);
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

/* Darkens only pixels covered by terrain (depth buffer written), not the sky. */
void r_night_overlay(const SkyState *s)
{
    CVertex *v;
    unsigned int a;
    if (s->night <= 0.01f) return;
    a = (unsigned int)(s->night * 0.72f * 255.0f);
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

/* -------------------------------------------------------------- held block */

static void tile_uv_f(int tile, int corner, float *u, float *v)
{
    float span = 1.0f / ATLAS_TILES, inset = 0.5f / ATLAS_SIZE;
    *u = (tile % ATLAS_TILES) * span + inset + CORNER_UV[corner][0] * (span - 2 * inset);
    *v = (tile / ATLAS_TILES) * span + inset + CORNER_UV[corner][1] * (span - 2 * inset);
}

void r_held_block(int block, float swing, float bob, float brightness)
{
    static const int ORDER[6] = { 0, 1, 2, 0, 2, 3 };
    ScePspFVector3 t, sc;
    FVertex *v;
    int f, i, n = 0, cross = block_shape(block) == SHAPE_CROSS;

    if (block <= B_AIR || block >= B_COUNT) return;

    sceGuClear(GU_DEPTH_BUFFER_BIT);
    sceGumMatrixMode(GU_VIEW);
    sceGumLoadIdentity();
    sceGumMatrixMode(GU_MODEL);
    sceGumLoadIdentity();
    t.x = 0.66f + sinf(bob) * 0.025f - swing * 0.15f;
    t.y = -0.58f - fabsf(cosf(bob)) * 0.025f - swing * 0.18f;
    t.z = -1.05f + swing * 0.12f;
    sceGumTranslate(&t);
    sceGumRotateY(0.72f);
    sceGumRotateX(-swing * 0.9f);
    sc.x = sc.y = sc.z = 0.46f;
    sceGumScale(&sc);

    v = (FVertex *)sceGuGetMemory(36 * sizeof(FVertex));
    if (cross) {
        int tile = block_tile(block, 0);
        for (i = 0; i < 6; i++) {
            int k = ORDER[i];
            tile_uv_f(tile, k, &v[i].u, &v[i].v);
            v[i].color = scale_color(0xFFFFFFu, brightness, 0xFF);
            v[i].x = (k >= 2 ? 0.5f : -0.5f);
            v[i].y = (k == 1 || k == 2) ? 0.5f : -0.5f;
            v[i].z = 0.0f;
        }
        n = 6;
    } else {
        for (f = 0; f < 6; f++) {
            int tile = block_tile(block, f);
            unsigned int col = scale_color(0xFFFFFFu, CUBE_SHADE[f] * brightness, 0xFF);
            for (i = 0; i < 6; i++) {
                int k = ORDER[i];
                const signed char *cc = CUBE_CORNERS[f][k];
                FVertex *p = &v[n++];
                tile_uv_f(tile, k, &p->u, &p->v);
                p->color = col;
                p->x = cc[0] - 0.5f;
                p->y = cc[1] - 0.5f;
                p->z = cc[2] - 0.5f;
            }
        }
    }

    bind_atlas(1.0f);
    sceGuEnable(GU_TEXTURE_2D);
    sceGuEnable(GU_DEPTH_TEST);
    sceGuEnable(GU_ALPHA_TEST);
    sceGuDisable(GU_FOG);
    sceGuDisable(GU_BLEND);
    sceGumDrawArray(GU_TRIANGLES, VT_F3D, n, 0, v);
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

int r_text_width(int scale, const char *s)
{
    int n = (int)strlen(s);
    return n ? (n * 6 - 1) * scale : 0;
}

void r_text(float x, float y, int scale, unsigned int color, const char *s)
{
    int n = (int)strlen(s), i, pass;
    FVertex *v;
    if (!n) return;
    bind_font();
    sceGuEnable(GU_TEXTURE_2D);
    for (pass = 0; pass < 2; pass++) {
        unsigned int col = pass == 0 ? (color & 0xFF000000u) | 0x202020u : color;
        float off = pass == 0 ? (float)scale : 0.0f;
        v = (FVertex *)sceGuGetMemory((unsigned int)n * 2 * sizeof(FVertex));
        for (i = 0; i < n; i++) {
            int ch = (unsigned char)s[i], g;
            float gx, gy;
            if (ch >= 'a' && ch <= 'z') ch -= 32;
            if (ch < 32 || ch > 95) ch = '?';
            g = ch - 32;
            gx = (float)((g % 16) * 8);
            gy = (float)((g / 16) * 8);
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

static void icon_face(FVertex *v, const float p[4][2], int tile, float shade)
{
    static const int ORDER[6] = { 0, 1, 2, 0, 2, 3 };
    static const float UV[4][2] = { { 0, 0 }, { 1, 0 }, { 1, 1 }, { 0, 1 } };
    float tu = (float)((tile % ATLAS_TILES) * TILE_PX), tv = (float)((tile / ATLAS_TILES) * TILE_PX);
    unsigned int col = scale_color(0xFFFFFFu, shade, 0xFF);
    int i;
    for (i = 0; i < 6; i++) {
        int k = ORDER[i];
        v[i].u = tu + UV[k][0] * TILE_PX;
        v[i].v = tv + UV[k][1] * TILE_PX;
        v[i].color = col;
        v[i].x = p[k][0];
        v[i].y = p[k][1];
        v[i].z = 0.0f;
    }
}

void r_block_icon(float x, float y, float s, int block)
{
    FVertex *v;
    if (block <= B_AIR || block >= B_COUNT) return;
    bind_atlas(1.0f);
    sceGuEnable(GU_TEXTURE_2D);
    if (block_shape(block) == SHAPE_CROSS) {
        int tile = block_tile(block, 0);
        float tu = (float)((tile % ATLAS_TILES) * TILE_PX), tv = (float)((tile / ATLAS_TILES) * TILE_PX);
        v = (FVertex *)sceGuGetMemory(2 * sizeof(FVertex));
        v[0].u = tu;           v[0].v = tv;           v[0].color = 0xFFFFFFFFu; v[0].x = x;     v[0].y = y;     v[0].z = 0;
        v[1].u = tu + TILE_PX; v[1].v = tv + TILE_PX; v[1].color = 0xFFFFFFFFu; v[1].x = x + s; v[1].y = y + s; v[1].z = 0;
        sceGuDrawArray(GU_SPRITES, VT_F2D, 2, 0, v);
        return;
    }
    {
        float cx = x + s * 0.5f;
        float top[4][2] = { { cx, y }, { x + s, y + s * 0.25f }, { cx, y + s * 0.5f }, { x, y + s * 0.25f } };
        float left[4][2] = { { x, y + s * 0.25f }, { cx, y + s * 0.5f }, { cx, y + s }, { x, y + s * 0.75f } };
        float right[4][2] = { { cx, y + s * 0.5f }, { x + s, y + s * 0.25f }, { x + s, y + s * 0.75f }, { cx, y + s } };
        v = (FVertex *)sceGuGetMemory(18 * sizeof(FVertex));
        icon_face(v, top, block_tile(block, F_PY), 1.0f);
        icon_face(v + 6, left, block_tile(block, F_PZ), 0.78f);
        icon_face(v + 12, right, block_tile(block, F_PX), 0.58f);
        sceGuDrawArray(GU_TRIANGLES, VT_F2D, 18, 0, v);
    }
}
