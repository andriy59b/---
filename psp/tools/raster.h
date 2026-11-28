/* Tiny software rasterizer for previewing game geometry on a PC
 * (perspective, z-buffer, near clipping, atlas texturing, alpha test, fog). */
#ifndef UC_RASTER_H
#define UC_RASTER_H

#include "../src/gfx.h"

typedef struct {
    int w, h;
    unsigned int *color; /* ABGR */
    float *depth;
    const unsigned int *tex;
    V3 eye;
    float yaw, pitch, fov;
    float fog_start, fog_end;
    unsigned int fog_color;
    int blend;           /* 1: alpha blend instead of alpha test */
    /* texture filtering, mirroring the PSP GE: level 0 is tex */
    const unsigned int *mip[8];
    int levels;          /* 1 = no mipmaps */
    int filter;          /* RS_FILTER_* used when minified */
    float lod_bias;
    float near_z;        /* near clip distance (default 0.1) */
    int tw, th;          /* level 0 size of the bound texture (default the atlas) */
    int wrap_u;          /* 1: repeat along u (the ground texture) */
    int guard_cull;      /* 1: drop triangles like the PSP GE does when a vertex in
                            front of the camera lands outside the 4096x4096
                            drawing area (PPSSPP emulates this too) */
} Raster;

enum { RS_NEAREST = 0, RS_LINEAR_MIPMAP_LINEAR, RS_NEAREST_MIPMAP_LINEAR, RS_NEAREST_MIPMAP_NEAREST };

void rs_init(Raster *r, int w, int h, const unsigned int *atlas);
void rs_free(Raster *r);
void rs_clear_sky(Raster *r, unsigned int top, unsigned int bottom);
void rs_camera(Raster *r, V3 eye, float yaw, float pitch, float fov_deg);
void rs_look_at(Raster *r, V3 eye, V3 target, float fov_deg);
void rs_draw(Raster *r, const GVertex *v, int n);
int  rs_write_png(const Raster *r, const char *path);

#endif
