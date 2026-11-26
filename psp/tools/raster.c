#include "raster.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <zlib.h>

void rs_init(Raster *r, int w, int h, const unsigned int *atlas)
{
    memset(r, 0, sizeof(*r));
    r->w = w;
    r->h = h;
    r->color = (unsigned int *)calloc((size_t)w * h, 4);
    r->depth = (float *)calloc((size_t)w * h, sizeof(float));
    r->tex = atlas;
    r->fov = 70.0f;
    r->fog_start = 1e9f;
    r->fog_end = 2e9f;
}

void rs_free(Raster *r)
{
    free(r->color);
    free(r->depth);
}

static unsigned int lerpc(unsigned int a, unsigned int b, float t)
{
    int i;
    unsigned int out = 0xFF000000u;
    for (i = 0; i < 24; i += 8) {
        float ca = (float)((a >> i) & 0xFF), cb = (float)((b >> i) & 0xFF);
        out |= (unsigned int)(ca + (cb - ca) * t) << i;
    }
    return out;
}

void rs_clear_sky(Raster *r, unsigned int top, unsigned int bottom)
{
    int x, y;
    for (y = 0; y < r->h; y++) {
        unsigned int c = lerpc(top, bottom, (float)y / r->h);
        for (x = 0; x < r->w; x++) {
            r->color[y * r->w + x] = c;
            r->depth[y * r->w + x] = 1e30f;
        }
    }
}

void rs_camera(Raster *r, V3 eye, float yaw, float pitch, float fov)
{
    r->eye = eye;
    r->yaw = yaw;
    r->pitch = pitch;
    r->fov = fov;
}

void rs_look_at(Raster *r, V3 eye, V3 target, float fov)
{
    V3 d = v3_sub(target, eye);
    float hz = sqrtf(d.x * d.x + d.z * d.z);
    rs_camera(r, eye, yaw_of(d.x, d.z), atan2f(d.y, hz), fov);
}

typedef struct { float x, y, z, u, v; float c[4]; } PV; /* view space */

static PV to_view(const Raster *r, const GVertex *g)
{
    PV p;
    float dx = g->x - r->eye.x, dy = g->y - r->eye.y, dz = g->z - r->eye.z;
    float cy = cosf(r->yaw), sy = sinf(r->yaw), cp = cosf(r->pitch), sp = sinf(r->pitch);
    /* inverse yaw: rotate by -yaw around y */
    float x1 = dx * cy - dz * sy, z1 = dx * sy + dz * cy;
    /* inverse pitch */
    float y2 = dy * cp + z1 * sp, z2 = -dy * sp + z1 * cp;
    p.x = x1;
    p.y = y2;
    p.z = z2;
    p.u = g->u;
    p.v = g->v;
    p.c[0] = (g->color & 0xFF) / 255.0f;
    p.c[1] = ((g->color >> 8) & 0xFF) / 255.0f;
    p.c[2] = ((g->color >> 16) & 0xFF) / 255.0f;
    p.c[3] = ((g->color >> 24) & 0xFF) / 255.0f;
    return p;
}

static PV lerp_pv(PV a, PV b, float t)
{
    PV p;
    int i;
    p.x = a.x + (b.x - a.x) * t;
    p.y = a.y + (b.y - a.y) * t;
    p.z = a.z + (b.z - a.z) * t;
    p.u = a.u + (b.u - a.u) * t;
    p.v = a.v + (b.v - a.v) * t;
    for (i = 0; i < 4; i++) p.c[i] = a.c[i] + (b.c[i] - a.c[i]) * t;
    return p;
}

static void raster_tri(Raster *r, const PV *a, const PV *b, const PV *c)
{
    float f = 1.0f / tanf(r->fov * 0.5f * PI_F / 180.0f), aspect = (float)r->w / r->h;
    float sx[3], sy[3], iz[3];
    const PV *p[3] = { a, b, c };
    int i, x, y, minx, maxx, miny, maxy;
    float area;
    for (i = 0; i < 3; i++) {
        float w = -p[i]->z;
        sx[i] = (p[i]->x * f / aspect / w * 0.5f + 0.5f) * r->w;
        sy[i] = (0.5f - p[i]->y * f / w * 0.5f) * r->h;
        iz[i] = 1.0f / w;
    }
    area = (sx[1] - sx[0]) * (sy[2] - sy[0]) - (sx[2] - sx[0]) * (sy[1] - sy[0]);
    if (fabsf(area) < 1e-6f) return;
    minx = (int)floorf(fminf(sx[0], fminf(sx[1], sx[2])));
    maxx = (int)ceilf(fmaxf(sx[0], fmaxf(sx[1], sx[2])));
    miny = (int)floorf(fminf(sy[0], fminf(sy[1], sy[2])));
    maxy = (int)ceilf(fmaxf(sy[0], fmaxf(sy[1], sy[2])));
    if (minx < 0) minx = 0;
    if (miny < 0) miny = 0;
    if (maxx > r->w - 1) maxx = r->w - 1;
    if (maxy > r->h - 1) maxy = r->h - 1;
    for (y = miny; y <= maxy; y++)
        for (x = minx; x <= maxx; x++) {
            float px = x + 0.5f, py = y + 0.5f;
            float w0 = ((sx[1] - px) * (sy[2] - py) - (sx[2] - px) * (sy[1] - py)) / area;
            float w1 = ((sx[2] - px) * (sy[0] - py) - (sx[0] - px) * (sy[2] - py)) / area;
            float w2 = 1.0f - w0 - w1, z, u, v, col[4], depth;
            unsigned int t, out;
            int k, tx, ty;
            if (w0 < 0 || w1 < 0 || w2 < 0) continue;
            z = w0 * iz[0] + w1 * iz[1] + w2 * iz[2];
            depth = 1.0f / z;
            if (depth >= r->depth[y * r->w + x]) continue;
            u = (w0 * p[0]->u * iz[0] + w1 * p[1]->u * iz[1] + w2 * p[2]->u * iz[2]) / z;
            v = (w0 * p[0]->v * iz[0] + w1 * p[1]->v * iz[1] + w2 * p[2]->v * iz[2]) / z;
            for (k = 0; k < 4; k++)
                col[k] = (w0 * p[0]->c[k] * iz[0] + w1 * p[1]->c[k] * iz[1] + w2 * p[2]->c[k] * iz[2]) / z;
            tx = (int)(u * TEX_SIZE) & (TEX_SIZE - 1);
            ty = (int)(v * TEX_SIZE) & (TEX_SIZE - 1);
            t = r->tex[ty * TEX_SIZE + tx];
            {
                float tr = (t & 0xFF) / 255.0f * col[0], tg = ((t >> 8) & 0xFF) / 255.0f * col[1];
                float tb = ((t >> 16) & 0xFF) / 255.0f * col[2], ta = ((t >> 24) & 0xFF) / 255.0f * col[3];
                float fog = clampf((depth - r->fog_start) / (r->fog_end - r->fog_start), 0.0f, 1.0f);
                unsigned int dst = r->color[y * r->w + x];
                if (!r->blend && ta < 0.5f) continue;
                if (ta < 0.01f) continue;
                tr = tr + ((r->fog_color & 0xFF) / 255.0f - tr) * fog;
                tg = tg + (((r->fog_color >> 8) & 0xFF) / 255.0f - tg) * fog;
                tb = tb + (((r->fog_color >> 16) & 0xFF) / 255.0f - tb) * fog;
                if (r->blend) {
                    tr = tr * ta + (dst & 0xFF) / 255.0f * (1 - ta);
                    tg = tg * ta + ((dst >> 8) & 0xFF) / 255.0f * (1 - ta);
                    tb = tb * ta + ((dst >> 16) & 0xFF) / 255.0f * (1 - ta);
                } else {
                    r->depth[y * r->w + x] = depth;
                }
                out = 0xFF000000u | ((unsigned int)(clampf(tb, 0, 1) * 255) << 16) |
                      ((unsigned int)(clampf(tg, 0, 1) * 255) << 8) | (unsigned int)(clampf(tr, 0, 1) * 255);
                r->color[y * r->w + x] = out;
            }
        }
}

void rs_draw(Raster *r, const GVertex *v, int n)
{
    const float nearz = -0.1f;
    int t;
    for (t = 0; t + 2 < n; t += 3) {
        PV in[3], out[4];
        int i, k = 0;
        for (i = 0; i < 3; i++) in[i] = to_view(r, &v[t + i]);
        /* clip against the near plane (z < nearz is visible) */
        for (i = 0; i < 3; i++) {
            PV a = in[i], b = in[(i + 1) % 3];
            int ain = a.z < nearz, bin = b.z < nearz;
            if (ain) out[k++] = a;
            if (ain != bin) out[k++] = lerp_pv(a, b, (nearz - a.z) / (b.z - a.z));
        }
        if (k >= 3) raster_tri(r, &out[0], &out[1], &out[2]);
        if (k == 4) raster_tri(r, &out[0], &out[2], &out[3]);
    }
}

int rs_write_png(const Raster *r, const char *path)
{
    FILE *f = fopen(path, "wb");
    unsigned char *raw;
    unsigned long zlen;
    unsigned char *z;
    int x, y;
    static const unsigned char sig[8] = { 137, 80, 78, 71, 13, 10, 26, 10 };
    if (!f) return 0;
    raw = (unsigned char *)malloc((size_t)(r->w * 3 + 1) * r->h);
    for (y = 0; y < r->h; y++) {
        raw[y * (r->w * 3 + 1)] = 0;
        for (x = 0; x < r->w; x++) {
            unsigned int c = r->color[y * r->w + x];
            unsigned char *p = &raw[y * (r->w * 3 + 1) + 1 + x * 3];
            p[0] = c & 0xFF;
            p[1] = (c >> 8) & 0xFF;
            p[2] = (c >> 16) & 0xFF;
        }
    }
    zlen = compressBound((unsigned long)(r->w * 3 + 1) * r->h);
    z = (unsigned char *)malloc(zlen);
    compress(z, &zlen, raw, (unsigned long)(r->w * 3 + 1) * r->h);
    fwrite(sig, 1, 8, f);
    {
        unsigned char hdr[13];
        unsigned int crc;
        unsigned char len[4], crcb[4];
        hdr[0] = (unsigned char)(r->w >> 24); hdr[1] = (unsigned char)(r->w >> 16); hdr[2] = (unsigned char)(r->w >> 8); hdr[3] = (unsigned char)r->w;
        hdr[4] = (unsigned char)(r->h >> 24); hdr[5] = (unsigned char)(r->h >> 16); hdr[6] = (unsigned char)(r->h >> 8); hdr[7] = (unsigned char)r->h;
        hdr[8] = 8; hdr[9] = 2; hdr[10] = 0; hdr[11] = 0; hdr[12] = 0;
        len[0] = 0; len[1] = 0; len[2] = 0; len[3] = 13;
        fwrite(len, 1, 4, f);
        fwrite("IHDR", 1, 4, f);
        fwrite(hdr, 1, 13, f);
        crc = (unsigned int)crc32(crc32(0, (const unsigned char *)"IHDR", 4), hdr, 13);
        crcb[0] = (unsigned char)(crc >> 24); crcb[1] = (unsigned char)(crc >> 16); crcb[2] = (unsigned char)(crc >> 8); crcb[3] = (unsigned char)crc;
        fwrite(crcb, 1, 4, f);
        len[0] = (unsigned char)(zlen >> 24); len[1] = (unsigned char)(zlen >> 16); len[2] = (unsigned char)(zlen >> 8); len[3] = (unsigned char)zlen;
        fwrite(len, 1, 4, f);
        fwrite("IDAT", 1, 4, f);
        fwrite(z, 1, zlen, f);
        crc = (unsigned int)crc32(crc32(0, (const unsigned char *)"IDAT", 4), z, (unsigned int)zlen);
        crcb[0] = (unsigned char)(crc >> 24); crcb[1] = (unsigned char)(crc >> 16); crcb[2] = (unsigned char)(crc >> 8); crcb[3] = (unsigned char)crc;
        fwrite(crcb, 1, 4, f);
        len[0] = len[1] = len[2] = len[3] = 0;
        fwrite(len, 1, 4, f);
        fwrite("IEND", 1, 4, f);
        crc = (unsigned int)crc32(0, (const unsigned char *)"IEND", 4);
        crcb[0] = (unsigned char)(crc >> 24); crcb[1] = (unsigned char)(crc >> 16); crcb[2] = (unsigned char)(crc >> 8); crcb[3] = (unsigned char)crc;
        fwrite(crcb, 1, 4, f);
    }
    fclose(f);
    free(raw);
    free(z);
    return 1;
}
