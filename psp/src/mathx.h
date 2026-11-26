/* Small vector / matrix helpers (pure C, header only). */
#ifndef UC_MATHX_H
#define UC_MATHX_H

#include <math.h>

#define PI_F 3.14159265f

typedef struct { float x, y, z; } V3;
/* 3x4 affine transform: columns are the basis vectors, t the translation */
typedef struct { V3 ax, ay, az, t; } Xf;

static inline V3 v3(float x, float y, float z) { V3 r = { x, y, z }; return r; }
static inline V3 v3_add(V3 a, V3 b) { return v3(a.x + b.x, a.y + b.y, a.z + b.z); }
static inline V3 v3_sub(V3 a, V3 b) { return v3(a.x - b.x, a.y - b.y, a.z - b.z); }
static inline V3 v3_scale(V3 a, float s) { return v3(a.x * s, a.y * s, a.z * s); }
static inline float v3_dot(V3 a, V3 b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
static inline V3 v3_cross(V3 a, V3 b)
{
    return v3(a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x);
}
static inline float v3_len(V3 a) { return sqrtf(v3_dot(a, a)); }
static inline V3 v3_norm(V3 a)
{
    float l = v3_len(a);
    return l > 1e-6f ? v3_scale(a, 1.0f / l) : v3(0, 1, 0);
}
static inline V3 v3_lerp(V3 a, V3 b, float t) { return v3_add(a, v3_scale(v3_sub(b, a), t)); }

static inline float clampf(float v, float lo, float hi) { return v < lo ? lo : (v > hi ? hi : v); }
static inline float lerpf(float a, float b, float t) { return a + (b - a) * t; }
static inline float wrap_pi(float a)
{
    while (a > PI_F) a -= 2.0f * PI_F;
    while (a < -PI_F) a += 2.0f * PI_F;
    return a;
}
static inline float approach(float v, float target, float step)
{
    if (v < target) return v + step > target ? target : v + step;
    return v - step < target ? target : v - step;
}
static inline float smooth01(float t)
{
    t = clampf(t, 0.0f, 1.0f);
    return t * t * (3.0f - 2.0f * t);
}

/* Heading convention: yaw 0 looks along -Z, forward = (-sin, 0, -cos),
 * right = (cos, 0, -sin). Positive yaw turns left. */
static inline V3 yaw_fwd(float yaw) { return v3(-sinf(yaw), 0.0f, -cosf(yaw)); }
static inline V3 yaw_right(float yaw) { return v3(cosf(yaw), 0.0f, -sinf(yaw)); }
static inline float yaw_of(float dx, float dz) { return atan2f(-dx, -dz); }

static inline Xf xf_identity(void)
{
    Xf m = { { 1, 0, 0 }, { 0, 1, 0 }, { 0, 0, 1 }, { 0, 0, 0 } };
    return m;
}
static inline V3 xf_point(const Xf *m, V3 p)
{
    return v3(m->t.x + m->ax.x * p.x + m->ay.x * p.y + m->az.x * p.z,
              m->t.y + m->ax.y * p.x + m->ay.y * p.y + m->az.y * p.z,
              m->t.z + m->ax.z * p.x + m->ay.z * p.y + m->az.z * p.z);
}
static inline V3 xf_dir(const Xf *m, V3 d)
{
    return v3(m->ax.x * d.x + m->ay.x * d.y + m->az.x * d.z,
              m->ax.y * d.x + m->ay.y * d.y + m->az.y * d.z,
              m->ax.z * d.x + m->ay.z * d.y + m->az.z * d.z);
}
/* a * b (apply b first) */
static inline Xf xf_mul(const Xf *a, const Xf *b)
{
    Xf r;
    r.ax = xf_dir(a, b->ax);
    r.ay = xf_dir(a, b->ay);
    r.az = xf_dir(a, b->az);
    r.t = xf_point(a, b->t);
    return r;
}
static inline Xf xf_translate(V3 t)
{
    Xf m = xf_identity();
    m.t = t;
    return m;
}
static inline Xf xf_rot_x(float a)
{
    Xf m = xf_identity();
    float c = cosf(a), s = sinf(a);
    m.ay = v3(0, c, s);
    m.az = v3(0, -s, c);
    return m;
}
static inline Xf xf_rot_y(float a)
{
    Xf m = xf_identity();
    float c = cosf(a), s = sinf(a);
    m.ax = v3(c, 0, -s);
    m.az = v3(s, 0, c);
    return m;
}
static inline Xf xf_rot_z(float a)
{
    Xf m = xf_identity();
    float c = cosf(a), s = sinf(a);
    m.ax = v3(c, s, 0);
    m.ay = v3(-s, c, 0);
    return m;
}
/* Rotation Y * X * Z (yaw, then pitch, then roll in local space) */
static inline Xf xf_euler(float pitch, float yaw, float roll)
{
    Xf y = xf_rot_y(yaw), x = xf_rot_x(pitch), z = xf_rot_z(roll), yx = xf_mul(&y, &x);
    return xf_mul(&yx, &z);
}

#endif
