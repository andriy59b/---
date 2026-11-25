#ifndef KUBIK_RENDER_H
#define KUBIK_RENDER_H

#define SCR_W 480
#define SCR_H 272

typedef struct {
    unsigned int zenith;   /* sky colour at the top (ABGR) */
    unsigned int horizon;  /* sky colour at the horizon */
    unsigned int fog;      /* fog colour used for terrain */
    unsigned int cloud;    /* cloud tint */
    float fog_start, fog_end;
    float night;           /* 0 day .. 1 night: strength of the terrain darkening */
    float sun_angle;       /* radians, 0 = sunrise, pi/2 = noon */
    float time_s;          /* seconds, animates clouds */
    int underwater;
} SkyState;

void r_init(void);
void r_flush_cache(void);
void r_begin_frame(unsigned int clear);
void r_end_frame(void);

void r_camera(const float eye[3], float yaw, float pitch);
/* Sky gradient, sun and moon. Call before r_camera (it sets its own view). */
void r_sky(const SkyState *s, float yaw, float pitch);
void r_world(const float eye[3], float yaw, float pitch, int pcx, int pcz, int radius, const SkyState *s);
void r_outline(const int b[3]);
void r_clouds(const float eye[3], const SkyState *s);
void r_night_overlay(const SkyState *s);
void r_held_block(int block, float swing, float bob, float brightness);

/* 2D (screen pixel coordinates) */
void r_begin_2d(void);
void r_rect(float x, float y, float w, float h, unsigned int color);
void r_text(float x, float y, int scale, unsigned int color, const char *s);
int  r_text_width(int scale, const char *s);
void r_block_icon(float x, float y, float size, int block);

#endif
