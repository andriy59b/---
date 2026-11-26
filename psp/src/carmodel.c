/* Car meshes built from a side profile: lower body, greenhouse, lights,
 * grille, plates, an opening driver door and spinning, steering wheels. */
#include "car.h"
#include "texgen.h"

#include <string.h>

#define WHITE 0xFFFFFFFFu

/*                 name            nickname        L     W     H     wb    track wr    clr   bump  hoodF hood  wsB   roofF roofR rgB   trunk rear  inset rnd chr spare mass  kW   kmh grip cg   steer soft drive */
const CarSpec CAR_SPECS[CAR_TYPES] = {
    { "Кайзер 525",    "бумер",        4.72f, 1.75f, 1.41f, 2.76f, 1.47f, 0.32f, 0.14f, 0.45f, 0.72f, 0.86f, 1.55f, 2.25f, 3.45f, 3.95f, 0.95f, 0.80f, 0.08f, 0, 1, 0, 1500, 141, 230, 1.05f, 0.50f, 0.60f, 1.0f, DRIVE_RWD },
    { "Ластівка 09",   "дев'ятка",     4.00f, 1.65f, 1.40f, 2.46f, 1.40f, 0.29f, 0.16f, 0.42f, 0.66f, 0.80f, 1.25f, 1.95f, 3.00f, 3.90f, 0.95f, 0.90f, 0.07f, 0, 0, 0,  950,  52, 155, 0.95f, 0.52f, 0.62f, 1.3f, DRIVE_FWD },
    { "Класика 07",    "сімка",        4.15f, 1.62f, 1.44f, 2.42f, 1.36f, 0.29f, 0.17f, 0.48f, 0.78f, 0.88f, 1.35f, 1.95f, 3.00f, 3.35f, 0.95f, 0.85f, 0.06f, 0, 1, 0, 1050,  57, 150, 0.90f, 0.55f, 0.60f, 1.5f, DRIVE_RWD },
    { "Пантера Гранд", "джип",         4.60f, 1.80f, 1.72f, 2.69f, 1.50f, 0.37f, 0.22f, 0.60f, 0.95f, 1.08f, 1.60f, 2.20f, 4.35f, 4.50f, 1.15f, 1.05f, 0.07f, 0, 1, 0, 1800, 160, 190, 0.95f, 0.70f, 0.60f, 1.4f, DRIVE_AWD },
    { "Гелендер G",    "гелік",        4.40f, 1.76f, 1.93f, 2.85f, 1.45f, 0.38f, 0.24f, 0.62f, 1.05f, 1.12f, 1.45f, 1.65f, 4.25f, 4.30f, 1.20f, 1.20f, 0.04f, 1, 0, 1, 2200, 130, 170, 0.90f, 0.80f, 0.55f, 1.2f, DRIVE_AWD },
    { "Поле 4x4",      "нива",         3.74f, 1.68f, 1.64f, 2.20f, 1.43f, 0.34f, 0.22f, 0.60f, 0.92f, 1.00f, 1.20f, 1.75f, 3.50f, 3.65f, 1.05f, 1.00f, 0.06f, 1, 0, 0, 1150,  59, 140, 0.95f, 0.65f, 0.62f, 1.5f, DRIVE_AWD },
};

typedef struct {
    GMesh paint, detail, door, wheel;
    float front_axle, rear_axle; /* z of axles in car space */
} CarModel;

static CarModel models[CAR_TYPES];

static float zof(const CarSpec *s, float d) { return -s->length * 0.5f + d; }

/* Top of the body side at distance d from the front (piecewise linear contour) */
static float contour(const float *d, const float *y, int n, float x)
{
    int i;
    for (i = 0; i + 1 < n; i++)
        if (x <= d[i + 1]) return y[i] + (y[i + 1] - y[i]) * (x - d[i]) / (d[i + 1] - d[i]);
    return y[n - 1];
}

/* Body side with wheel arches cut out: columns between breakpoints */
static void side_quads(GMesh *m, const CarSpec *s, float x, const float *d, const float *y, int n, float top,
                       const float *arch, float arch_r)
{
    float bp[32];
    int nb = 0, i, k;
    UVRect full = uv_tile(T_CAR_SIDE);
    unsigned int c = gfx_shade(v3(x < 0 ? -1.0f : 1.0f, 0, 0), WHITE);
    for (i = 0; i < n; i++) bp[nb++] = d[i];
    for (k = 0; k < 2; k++) {
        int j;
        for (j = 0; j <= 6; j++) bp[nb++] = arch[k] - arch_r + arch_r * 2.0f * j / 6.0f;
    }
    /* sort breakpoints */
    for (i = 1; i < nb; i++) {
        float v = bp[i];
        int j = i - 1;
        while (j >= 0 && bp[j] > v) { bp[j + 1] = bp[j]; j--; }
        bp[j + 1] = v;
    }
    for (i = 0; i + 1 < nb; i++) {
        float d0 = bp[i], d1 = bp[i + 1], y0[2], y1[2], dd[2] = { d0, d1 };
        if (d1 - d0 < 0.005f) continue;
        for (k = 0; k < 2; k++) {
            float bottom = s->clearance, ax;
            int w;
            for (w = 0; w < 2; w++) {
                ax = (dd[k] - arch[w]) / arch_r;
                if (ax > -1.0f && ax < 1.0f) {
                    float h = s->wheel_r + sqrtf(1.0f - ax * ax) * arch_r * 0.95f;
                    if (h > bottom) bottom = h;
                }
            }
            y0[k] = bottom;
            y1[k] = contour(d, y, n, dd[k]);
            if (y0[k] > y1[k] - 0.05f) y0[k] = y1[k] - 0.05f;
        }
        {
            float u0 = full.u0 + (full.u1 - full.u0) * d0 / s->length, u1 = full.u0 + (full.u1 - full.u0) * d1 / s->length;
            float vb0 = full.v0 + (full.v1 - full.v0) * (1.0f - (y0[0] - s->clearance) / (top - s->clearance));
            float vb1 = full.v0 + (full.v1 - full.v0) * (1.0f - (y0[1] - s->clearance) / (top - s->clearance));
            float vt0 = full.v0 + (full.v1 - full.v0) * (1.0f - (y1[0] - s->clearance) / (top - s->clearance));
            float vt1 = full.v0 + (full.v1 - full.v0) * (1.0f - (y1[1] - s->clearance) / (top - s->clearance));
            V3 p0 = v3(x, y0[0], zof(s, d0)), p1 = v3(x, y1[0], zof(s, d0)), p2 = v3(x, y1[1], zof(s, d1)), p3 = v3(x, y0[1], zof(s, d1));
            gm_tri(m, p0, p1, p2, u0, vb0, u0, vt0, u1, vt1, c);
            gm_tri(m, p0, p2, p3, u0, vb0, u1, vt1, u1, vb1, c);
        }
    }
}

static void quad_x(GMesh *m, float x0, float x1, float y0, float y1, float z, UVRect uv, int facing_front)
{
    if (facing_front) gm_quad(m, v3(x0, y0, z), v3(x0, y1, z), v3(x1, y1, z), v3(x1, y0, z), uv, WHITE);
    else gm_quad(m, v3(x1, y0, z), v3(x1, y1, z), v3(x0, y1, z), v3(x0, y0, z), uv, WHITE);
}

static void build_wheel(GMesh *m, float r, float w)
{
    int i;
    UVRect cap = uv_tile(T_TIRE), tread = uv_sub(T_CAR_PARTS, CP_DARK);
    for (i = 0; i < 10; i++) {
        float a0 = i * 6.2831853f / 10, a1 = (i + 1) * 6.2831853f / 10;
        float y0 = cosf(a0) * r, z0 = sinf(a0) * r, y1 = cosf(a1) * r, z1 = sinf(a1) * r;
        float cu = (cap.u0 + cap.u1) * 0.5f, cv = (cap.v0 + cap.v1) * 0.5f;
        float ru = (cap.u1 - cap.u0) * 0.5f, rv = (cap.v1 - cap.v0) * 0.5f;
        unsigned int dark = gfx_rgba(40, 40, 42, 255);
        /* outer and inner caps */
        gm_tri(m, v3(w / 2, 0, 0), v3(w / 2, y0, z0), v3(w / 2, y1, z1), cu, cv, cu + cosf(a0) * ru,
               cv + sinf(a0) * rv, cu + cosf(a1) * ru, cv + sinf(a1) * rv, gfx_shade(v3(1, 0, 0), WHITE));
        gm_tri(m, v3(-w / 2, 0, 0), v3(-w / 2, y1, z1), v3(-w / 2, y0, z0), cu, cv, cu + cosf(a1) * ru,
               cv + sinf(a1) * rv, cu + cosf(a0) * ru, cv + sinf(a0) * rv, gfx_shade(v3(-1, 0, 0), WHITE));
        gm_quad(m, v3(-w / 2, y0, z0), v3(w / 2, y0, z0), v3(w / 2, y1, z1), v3(-w / 2, y1, z1), tread, dark);
    }
}

static void build_model(int t)
{
    const CarSpec *s = &CAR_SPECS[t];
    CarModel *cm = &models[t];
    const float hw = s->width * 0.5f;
    float d[4] = { 0.0f, s->ws_base, s->rear_glass_base, s->length };
    float y[4] = { s->hood_front_h, s->hood_h, s->trunk_h, s->rear_h };
    float top = s->hood_h > s->trunk_h ? s->hood_h : s->trunk_h;
    float zf = zof(s, 0.0f), zr = zof(s, s->length);
    float gi = hw - s->glass_inset, overhang = (s->length - s->wheelbase) * 0.47f;
    UVRect white = uv_tile(T_WHITE), glass = uv_tile(T_CAR_GLASS);
    GMesh *P = &cm->paint, *D = &cm->detail;

    if (top < s->rear_h) top = s->rear_h;
    if (top < s->hood_front_h) top = s->hood_front_h;
    gm_init(P);
    gm_init(D);
    gm_init(&cm->door);
    gm_init(&cm->wheel);

    /* axles and wheel arches */
    cm->front_axle = zof(s, overhang);
    cm->rear_axle = zof(s, overhang + s->wheelbase);
    {
        float arch[2] = { overhang, overhang + s->wheelbase }, ar = s->wheel_r * 1.18f;
        int w;
        side_quads(P, s, -hw, d, y, 4, top, arch, ar);
        side_quads(P, s, hw, d, y, 4, top, arch, ar);
        /* dark wheel wells so the arches are not see-through */
        for (w = 0; w < 2; w++) {
            float zc = zof(s, arch[w]);
            UVRect dk = uv_sub(T_CAR_PARTS, CP_DARK);
            unsigned int c = gfx_rgba(24, 24, 26, 255);
            gm_quad(D, v3(-hw + 0.3f, s->clearance, zc + ar), v3(-hw + 0.3f, s->wheel_r + ar, zc + ar),
                    v3(-hw + 0.3f, s->wheel_r + ar, zc - ar), v3(-hw + 0.3f, s->clearance, zc - ar), dk, c);
            gm_quad(D, v3(hw - 0.3f, s->clearance, zc - ar), v3(hw - 0.3f, s->wheel_r + ar, zc - ar),
                    v3(hw - 0.3f, s->wheel_r + ar, zc + ar), v3(hw - 0.3f, s->clearance, zc + ar), dk, c);
            gm_quad(D, v3(-hw, s->wheel_r + ar * 0.95f, zc - ar), v3(-hw, s->wheel_r + ar * 0.95f, zc + ar),
                    v3(hw, s->wheel_r + ar * 0.95f, zc + ar), v3(hw, s->wheel_r + ar * 0.95f, zc - ar), dk, c);
        }
    }
    gm_quad(P, v3(-hw, s->hood_front_h, zf), v3(-hw, s->hood_h, zof(s, s->ws_base)), v3(hw, s->hood_h, zof(s, s->ws_base)),
            v3(hw, s->hood_front_h, zf), white, WHITE);                                   /* hood */
    gm_quad(P, v3(-hw, s->trunk_h, zof(s, s->rear_glass_base)), v3(-hw, s->rear_h, zr), v3(hw, s->rear_h, zr),
            v3(hw, s->trunk_h, zof(s, s->rear_glass_base)), white, WHITE);               /* trunk */
    quad_x(P, -hw, hw, s->clearance, s->hood_front_h, zf, white, 1);                     /* front panel */
    quad_x(P, -hw, hw, s->clearance, s->rear_h, zr, white, 0);                           /* rear panel */
    gm_quad(D, v3(-hw, s->clearance, zr), v3(-hw, s->clearance, zf), v3(hw, s->clearance, zf), v3(hw, s->clearance, zr),
            uv_sub(T_CAR_PARTS, CP_DARK), gfx_rgba(30, 30, 30, 255));                    /* underside */

    /* greenhouse */
    {
        float zb0 = zof(s, s->ws_base), zt0 = zof(s, s->roof_front), zt1 = zof(s, s->roof_rear), zb1 = zof(s, s->rear_glass_base);
        float yb0 = s->hood_h, yb1 = s->trunk_h, H = s->height, bx = hw - 0.02f, mid = (zt0 + zt1) * 0.5f;
        gm_quad(D, v3(-bx, yb0, zb0), v3(-gi, H, zt0), v3(gi, H, zt0), v3(bx, yb0, zb0), glass, WHITE);   /* windscreen */
        gm_quad(D, v3(bx, yb1, zb1), v3(gi, H, zt1), v3(-gi, H, zt1), v3(-bx, yb1, zb1), glass, WHITE);   /* rear glass */
        gm_quad(P, v3(-gi, H, zt1), v3(-gi, H, zt0), v3(gi, H, zt0), v3(gi, H, zt1), white, WHITE);       /* roof */
        /* side windows */
        gm_quad(D, v3(-bx, yb1, zb1), v3(-gi, H, zt1), v3(-gi, H, zt0), v3(-bx, yb0, zb0), glass, WHITE);
        gm_quad(D, v3(bx, yb0, zb0), v3(gi, H, zt0), v3(gi, H, zt1), v3(bx, yb1, zb1), glass, WHITE);
        /* B pillars and roof edges in body colour */
        gm_quad(P, v3(-bx - 0.005f, (yb0 + yb1) * 0.5f, mid + 0.06f), v3(-gi - 0.005f, H, mid + 0.06f),
                v3(-gi - 0.005f, H, mid - 0.06f), v3(-bx - 0.005f, (yb0 + yb1) * 0.5f, mid - 0.06f), white, WHITE);
        gm_quad(P, v3(bx + 0.005f, (yb0 + yb1) * 0.5f, mid - 0.06f), v3(gi + 0.005f, H, mid - 0.06f),
                v3(gi + 0.005f, H, mid + 0.06f), v3(bx + 0.005f, (yb0 + yb1) * 0.5f, mid + 0.06f), white, WHITE);
    }

    /* front details */
    {
        float zd = zf - 0.006f, gy0 = s->bumper_h + 0.03f, gy1 = s->hood_front_h - 0.04f;
        float lw = hw * 0.36f;
        quad_x(D, -hw * 0.42f, hw * 0.42f, gy0, gy1, zd,
               s->chrome_grille ? uv_sub(T_CAR_PARTS, CP_GRILLE_CHROME) : uv_sub(T_CAR_PARTS, CP_GRILLE_BLACK), 1);
        if (s->round_lights) {
            float r = (gy1 - gy0) * 0.5f, cy = (gy0 + gy1) * 0.5f;
            quad_x(D, -hw * 0.86f, -hw * 0.86f + 2 * r, cy - r, cy + r, zd, uv_sub(T_CAR_PARTS, CP_HEAD_ROUND), 1);
            quad_x(D, hw * 0.86f - 2 * r, hw * 0.86f, cy - r, cy + r, zd, uv_sub(T_CAR_PARTS, CP_HEAD_ROUND), 1);
        } else {
            quad_x(D, -hw * 0.95f, -hw * 0.95f + lw * 1.4f, gy0, gy1, zd, uv_sub(T_CAR_PARTS, CP_HEAD_RECT), 1);
            quad_x(D, hw * 0.95f - lw * 1.4f, hw * 0.95f, gy0, gy1, zd, uv_sub(T_CAR_PARTS, CP_HEAD_RECT), 1);
        }
        quad_x(D, -hw - 0.02f, hw + 0.02f, s->clearance + 0.04f, s->bumper_h, zd - 0.03f, uv_sub(T_CAR_PARTS, CP_BUMPER), 1);
        quad_x(D, -0.26f, 0.26f, s->clearance + 0.1f, s->clearance + 0.21f, zd - 0.04f, uv_sub(T_CAR_PARTS, CP_PLATE), 1);
        quad_x(D, -hw * 0.98f, -hw * 0.8f, s->bumper_h - 0.08f, s->bumper_h - 0.01f, zd - 0.035f, uv_sub(T_CAR_PARTS, CP_INDICATOR), 1);
        quad_x(D, hw * 0.8f, hw * 0.98f, s->bumper_h - 0.08f, s->bumper_h - 0.01f, zd - 0.035f, uv_sub(T_CAR_PARTS, CP_INDICATOR), 1);
    }
    /* rear details */
    {
        float zd = zr + 0.006f, ty0 = s->bumper_h + 0.04f, ty1 = s->rear_h - 0.06f;
        quad_x(D, -hw * 0.96f, -hw * 0.5f, ty0, ty1, zd, uv_sub(T_CAR_PARTS, CP_TAIL), 0);
        quad_x(D, hw * 0.5f, hw * 0.96f, ty0, ty1, zd, uv_sub(T_CAR_PARTS, CP_TAIL), 0);
        quad_x(D, -hw - 0.02f, hw + 0.02f, s->clearance + 0.04f, s->bumper_h, zd + 0.03f, uv_sub(T_CAR_PARTS, CP_BUMPER), 0);
        quad_x(D, -0.26f, 0.26f, s->bumper_h + 0.02f, s->bumper_h + 0.13f, zd + 0.01f, uv_sub(T_CAR_PARTS, CP_PLATE), 0);
        if (s->spare_wheel) {
            float r = 0.36f, cy = (s->bumper_h + s->rear_h) * 0.5f + 0.1f;
            quad_x(D, -r, r, cy - r, cy + r, zd + 0.12f, uv_tile(T_TIRE), 0);
        }
    }

    /* driver door, built around its hinge (front edge) */
    {
        float len = s->length * 0.26f, y0 = s->clearance + 0.08f, y1 = s->hood_h - 0.02f;
        UVRect side = uv_tile(T_CAR_SIDE);
        UVRect sub = { side.u0 + (side.u1 - side.u0) * 0.36f, side.v0 + (side.v1 - side.v0) * 0.2f,
                       side.u0 + (side.u1 - side.u0) * 0.66f, side.v1 };
        gm_quad(&cm->door, v3(-0.01f, y0, 0), v3(-0.01f, y1, 0), v3(-0.01f, y1, len), v3(-0.01f, y0, len), sub, WHITE);
        gm_quad(&cm->door, v3(0.0f, y0, len), v3(0.0f, y1, len), v3(0.0f, y1, 0), v3(0.0f, y0, 0),
                uv_sub(T_CAR_PARTS, CP_DARK), gfx_rgba(90, 90, 90, 255));
    }

    build_wheel(&cm->wheel, s->wheel_r, 0.22f);
}

void carmodels_build(void)
{
    int t;
    for (t = 0; t < CAR_TYPES; t++) build_model(t);
}

static Xf body_xf(const Car *c)
{
    Xf t = xf_translate(v3(c->pos.x, c->pos.y + c->body_y, c->pos.z));
    Xf r = xf_euler(c->body_pitch + c->curb_pitch, c->yaw, c->body_roll + c->curb_roll);
    return xf_mul(&t, &r);
}

V3 car_local(const Car *c, V3 local)
{
    Xf t = xf_translate(c->pos), r = xf_rot_y(c->yaw), m = xf_mul(&t, &r);
    return xf_point(&m, local);
}

V3 car_light_pos(const Car *c, int i)
{
    const CarSpec *s = &CAR_SPECS[c->type];
    float x = (i & 1 ? 1.0f : -1.0f) * s->width * 0.36f;
    float y = (s->bumper_h + s->hood_front_h) * 0.5f;
    float z = i < 2 ? -s->length * 0.5f - 0.08f : s->length * 0.5f + 0.06f;
    Xf b = body_xf(c);
    return xf_point(&b, v3(x, i < 2 ? y : s->bumper_h + 0.12f, z));
}

static void append_painted(GMesh *out, const GMesh *src, const Xf *xf, unsigned int paint)
{
    int i, start = out->n;
    float pr = (paint & 0xFF) / 255.0f, pg = ((paint >> 8) & 0xFF) / 255.0f, pb = ((paint >> 16) & 0xFF) / 255.0f;
    gm_append_xf(out, src, xf);
    for (i = start; i < out->n; i++) {
        unsigned int c = out->v[i].color;
        out->v[i].color = gfx_rgba((int)((c & 0xFF) * pr), (int)(((c >> 8) & 0xFF) * pg), (int)(((c >> 16) & 0xFF) * pb), 255);
    }
}

void car_append_mesh(GMesh *out, const Car *c)
{
    const CarSpec *s = &CAR_SPECS[c->type];
    const CarModel *cm = &models[c->type];
    Xf body = body_xf(c);
    int w;
    append_painted(out, &cm->paint, &body, c->paint);
    gm_append_xf(out, &cm->detail, &body);
    {
        Xf hinge = xf_translate(v3(-s->width * 0.5f - 0.005f, 0, zof(s, s->ws_base - 0.05f)));
        Xf rot = xf_rot_y(-c->door * 1.1f), a = xf_mul(&body, &hinge), m = xf_mul(&a, &rot);
        append_painted(out, &cm->door, &m, c->paint);
    }
    /* wheels follow the ground, not the body roll */
    for (w = 0; w < 4; w++) {
        float x = (w & 1 ? 1.0f : -1.0f) * (s->width * 0.5f - 0.09f), z = w < 2 ? cm->front_axle : cm->rear_axle;
        Xf base = xf_translate(v3(c->pos.x, c->pos.y + c->body_y * 0.3f, c->pos.z));
        Xf yaw = xf_rot_y(c->yaw), hub = xf_translate(v3(x, s->wheel_r, z));
        Xf steer = xf_rot_y(w < 2 ? c->steer : 0.0f), spin = xf_rot_x(-c->wheel_spin);
        Xf m1 = xf_mul(&base, &yaw), m2 = xf_mul(&m1, &hub), m3 = xf_mul(&m2, &steer), m4 = xf_mul(&m3, &spin);
        gm_append_xf(out, &cm->wheel, &m4);
    }
}
