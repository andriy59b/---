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
} Raster;

void rs_init(Raster *r, int w, int h, const unsigned int *atlas);
void rs_free(Raster *r);
void rs_clear_sky(Raster *r, unsigned int top, unsigned int bottom);
void rs_camera(Raster *r, V3 eye, float yaw, float pitch, float fov_deg);
void rs_look_at(Raster *r, V3 eye, V3 target, float fov_deg);
void rs_draw(Raster *r, const GVertex *v, int n);
int  rs_write_png(const Raster *r, const char *path);

#endif
