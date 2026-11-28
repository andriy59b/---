/* sceGu renderer: sky, city cells, dynamic meshes, shadows, flares, HUD 2D. */
#ifndef UC_RENDER_H
#define UC_RENDER_H

#include "gfx.h"

#define SCR_W 480
#define SCR_H 272

typedef struct {
    unsigned int zenith, horizon, fog, cloud;
    float fog_start, fog_end;
    float night;       /* 0 day .. 1 night */
    float sun_angle;
    float time_s;
} SkyState;

void r_init(void);
void r_flush_cache(void);
void r_begin_frame(unsigned int clear);
void r_end_frame(void);

void r_camera(V3 eye, float yaw, float pitch, float fov);
void r_sky(const SkyState *s, float yaw, float pitch, float fov);
void r_city(V3 eye, float yaw, float pitch, float radius, const SkyState *s);
void r_glow(V3 eye, float radius, float night);
void r_draw_list(const DrawList *d, const SkyState *s); /* moving things, transformed (and lit) by the GE */
void r_shadow(V3 pos, float half_w, float half_l, float yaw, float alpha);
void r_flare(V3 pos, float size, unsigned int color);
void r_clouds(V3 eye, const SkyState *s);
void r_night_overlay(const SkyState *s);
void r_marker(V3 pos, float t);
void r_set_billboard_basis(V3 right, V3 up);

/* 2D (screen pixel coordinates) */
void r_begin_2d(void);
void r_rect(float x, float y, float w, float h, unsigned int color);
void r_text(float x, float y, int scale, unsigned int color, const char *s);
int  r_text_width(int scale, const char *s);
void r_arrow(float cx, float cy, float angle, float size, unsigned int color);
void r_gradient(float x, float y, float w, float h, unsigned int top, unsigned int bottom);

#endif
