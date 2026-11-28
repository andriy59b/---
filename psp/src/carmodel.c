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

/* Cars with rear doors (the others are three-door) */
static const unsigned char REAR_DOORS[CAR_TYPES] = { 1, 1, 1, 1, 1, 0 };

/* The driver/passenger doors are cut out of the body itself: the same
 * panels, texture coordinates and window glass, so a closed door is exactly
 * the body and an open one leaves an opening into a simple cabin. */
typedef struct {
    GMesh paint, detail, wheel, wheel_far;
    GMesh door_skin[2];  /* painted outer panel, in body space (0 left, 1 right) */
    GMesh door_glass[2]; /* window, moves with the door */
    GMesh door_trim[2];  /* inner panel and edge, drawn only while open */
    GMesh cabin;         /* floor, dashboard, seats, ceiling, rear wall */
    GMesh cabin_wall[2]; /* lower inner side walls */
    float front_axle, rear_axle; /* z of axles in car space */
    float door_d0, door_d1;      /* front door from/to, distance from the car's front */
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

/* Side panel texture: columns 0..47 of T_CAR_SIDE run along the whole body,
 * columns 48..63 hold one door panel (seams, handle) put onto each door. */
static float side_u(float px)
{
    UVRect t = uv_tile(T_CAR_SIDE);
    return t.u0 + (t.u1 - t.u0) * px / 64.0f;
}

typedef struct {
    float f0, f1;   /* front door */
    float r0, r1;   /* rear door (r1 <= r0: none) */
    float sill;     /* height of the door's lower edge */
} DoorCut;

/* Body side with wheel arches cut out: columns between breakpoints. The
 * front door part above the sill goes to `door`, the rest to `body`. */
static void side_quads(GMesh *body, GMesh *door, const CarSpec *s, float x, const float *d, const float *y, int n,
                       float top, const float *arch, float arch_r, const DoorCut *dc)
{
    float bp[40];
    int nb = 0, i, k;
    UVRect full = uv_tile(T_CAR_SIDE);
    unsigned int c = gfx_shade(v3(x < 0 ? -1.0f : 1.0f, 0, 0), WHITE);
    for (i = 0; i < n; i++) bp[nb++] = d[i];
    for (k = 0; k < 2; k++) {
        int j;
        for (j = 0; j <= 6; j++) bp[nb++] = arch[k] - arch_r + arch_r * 2.0f * j / 6.0f;
    }
    bp[nb++] = dc->f0;
    bp[nb++] = dc->f1;
    if (dc->r1 > dc->r0) {
        bp[nb++] = dc->r0;
        bp[nb++] = dc->r1;
    }
    /* sort breakpoints */
    for (i = 1; i < nb; i++) {
        float v = bp[i];
        int j = i - 1;
        while (j >= 0 && bp[j] > v) { bp[j + 1] = bp[j]; j--; }
        bp[j + 1] = v;
    }
    for (i = 0; i + 1 < nb; i++) {
        float d0 = bp[i], d1 = bp[i + 1], y0[2], y1[2], dd[2] = { d0, d1 }, dm = (d0 + d1) * 0.5f, u0, u1;
        int front = dm > dc->f0 && dm < dc->f1, rear = dm > dc->r0 && dm < dc->r1;
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
        if (front) {
            u0 = side_u(48.35f + 15.3f * (d0 - dc->f0) / (dc->f1 - dc->f0));
            u1 = side_u(48.35f + 15.3f * (d1 - dc->f0) / (dc->f1 - dc->f0));
        } else if (rear) {
            u0 = side_u(48.35f + 15.3f * (d0 - dc->r0) / (dc->r1 - dc->r0));
            u1 = side_u(48.35f + 15.3f * (d1 - dc->r0) / (dc->r1 - dc->r0));
        } else {
            u0 = side_u(0.35f + 47.3f * d0 / s->length);
            u1 = side_u(0.35f + 47.3f * d1 / s->length);
        }
        {
            /* one or two strips: below the sill stays on the body */
            float cut = front ? dc->sill : -1.0f;
            int part;
            for (part = 0; part < 2; part++) {
                float a0 = y0[0], a1 = y0[1], b0 = y1[0], b1 = y1[1];
                GMesh *m = body;
                if (cut > 0.0f) {
                    if (part == 0) {
                        /* where a wheel arch starts the bottom can be above the sill */
                        b0 = cut < b0 ? (cut > a0 ? cut : a0) : b0;
                        b1 = cut < b1 ? (cut > a1 ? cut : a1) : b1;
                    } else {
                        a0 = cut > a0 ? cut : a0;
                        a1 = cut > a1 ? cut : a1;
                        m = door;
                    }
                } else if (part == 1) {
                    break;
                }
                if (b0 - a0 < 0.005f && b1 - a1 < 0.005f) continue;
                {
                    float vb0 = full.v0 + (full.v1 - full.v0) * (1.0f - (a0 - s->clearance) / (top - s->clearance));
                    float vb1 = full.v0 + (full.v1 - full.v0) * (1.0f - (a1 - s->clearance) / (top - s->clearance));
                    float vt0 = full.v0 + (full.v1 - full.v0) * (1.0f - (b0 - s->clearance) / (top - s->clearance));
                    float vt1 = full.v0 + (full.v1 - full.v0) * (1.0f - (b1 - s->clearance) / (top - s->clearance));
                    V3 p0 = v3(x, a0, zof(s, d0)), p1 = v3(x, b0, zof(s, d0)), p2 = v3(x, b1, zof(s, d1)), p3 = v3(x, a1, zof(s, d1));
                    gm_tri(m, p0, p1, p2, u0, vb0, u0, vt0, u1, vt1, c);
                    gm_tri(m, p0, p2, p3, u0, vb0, u1, vt1, u1, vb1, c);
                }
            }
        }
    }
}

/* Side window between distances da..db from the front: the part of the
 * greenhouse side (belt line at x = bx, roof edge at x = gi) in that range. */
static void side_glass(GMesh *m, const CarSpec *s, float sx, float da, float db, float bx, float gi)
{
    UVRect g = uv_tile(T_CAR_GLASS);
    float ba = da > s->ws_base ? da : s->ws_base, bb = db < s->rear_glass_base ? db : s->rear_glass_base;
    float ta = da > s->roof_front ? da : s->roof_front, tb = db < s->roof_rear ? db : s->roof_rear;
    float span = s->rear_glass_base - s->ws_base;
    float ya = s->hood_h + (s->trunk_h - s->hood_h) * (ba - s->ws_base) / span;
    float yb = s->hood_h + (s->trunk_h - s->hood_h) * (bb - s->ws_base) / span;
    float ua = g.u0 + (g.u1 - g.u0) * (da - s->ws_base) / span, ub = g.u0 + (g.u1 - g.u0) * (db - s->ws_base) / span;
    unsigned int c = gfx_shade(v3(sx, 0.4f, 0), WHITE);
    UVRect r;
    r.u0 = ua < g.u0 ? g.u0 : ua;
    r.u1 = ub > g.u1 ? g.u1 : ub;
    r.v0 = g.v0;
    r.v1 = g.v1;
    if (bb - ba < 0.01f || tb - ta < 0.01f) return;
    gm_quad_flat(m, v3(sx * bx, ya, zof(s, ba)), v3(sx * gi, s->height, zof(s, ta)), v3(sx * gi, s->height, zof(s, tb)),
                 v3(sx * bx, yb, zof(s, bb)), r, c);
}

static void quad_x(GMesh *m, float x0, float x1, float y0, float y1, float z, UVRect uv, int facing_front)
{
    if (facing_front) gm_quad(m, v3(x0, y0, z), v3(x0, y1, z), v3(x1, y1, z), v3(x1, y0, z), uv, WHITE);
    else gm_quad(m, v3(x1, y0, z), v3(x1, y1, z), v3(x0, y1, z), v3(x0, y0, z), uv, WHITE);
}

static void build_wheel(GMesh *m, float r, float w, int seg)
{
    int i;
    UVRect cap = uv_tile(T_TIRE), tread = uv_sub(T_CAR_PARTS, CP_DARK);
    for (i = 0; i < seg; i++) {
        float a0 = i * 6.2831853f / seg, a1 = (i + 1) * 6.2831853f / seg;
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
    DoorCut dc;
    int k;

    gm_init(P);
    gm_init(D);
    gm_init(&cm->wheel);
    gm_init(&cm->wheel_far);
    gm_init(&cm->cabin);
    for (k = 0; k < 2; k++) {
        gm_init(&cm->door_skin[k]);
        gm_init(&cm->door_glass[k]);
        gm_init(&cm->door_trim[k]);
        gm_init(&cm->cabin_wall[k]);
    }

    /* front door: from just ahead of the windscreen base to the B pillar */
    {
        float bpil = (s->roof_front + s->roof_rear) * 0.5f;
        dc.f0 = s->ws_base - 0.05f;
        dc.f1 = bpil - 0.06f;
        dc.r0 = bpil + 0.06f;
        dc.r1 = REAR_DOORS[t] ? s->rear_glass_base - 0.05f : dc.r0;
        if (dc.r1 > dc.r0 + (dc.f1 - dc.f0)) dc.r1 = dc.r0 + (dc.f1 - dc.f0);
        dc.sill = s->clearance + 0.1f;
        cm->door_d0 = dc.f0;
        cm->door_d1 = dc.f1;
    }

    /* axles and wheel arches */
    cm->front_axle = zof(s, overhang);
    cm->rear_axle = zof(s, overhang + s->wheelbase);
    {
        float arch[2] = { overhang, overhang + s->wheelbase }, ar = s->wheel_r * 1.18f;
        int w;
        side_quads(P, &cm->door_skin[0], s, -hw, d, y, 4, top, arch, ar, &dc);
        side_quads(P, &cm->door_skin[1], s, hw, d, y, 4, top, arch, ar, &dc);
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
        /* side windows: the front part belongs to the door */
        for (k = 0; k < 2; k++) {
            float sx = k ? 1.0f : -1.0f;
            side_glass(&cm->door_glass[k], s, sx, s->ws_base, dc.f1, bx, gi);
            side_glass(D, s, sx, dc.f1, s->rear_glass_base, bx, gi);
        }
        /* B pillars and roof edges in body colour */
        gm_quad(P, v3(-bx - 0.015f, (yb0 + yb1) * 0.5f, mid + 0.06f), v3(-gi - 0.015f, H, mid + 0.06f),
                v3(-gi - 0.015f, H, mid - 0.06f), v3(-bx - 0.015f, (yb0 + yb1) * 0.5f, mid - 0.06f), white, WHITE);
        gm_quad(P, v3(bx + 0.015f, (yb0 + yb1) * 0.5f, mid - 0.06f), v3(gi + 0.015f, H, mid - 0.06f),
                v3(gi + 0.015f, H, mid + 0.06f), v3(bx + 0.015f, (yb0 + yb1) * 0.5f, mid + 0.06f), white, WHITE);
    }

    /* front details */
    {
        float zd = zf - 0.025f, gy0 = s->bumper_h + 0.03f, gy1 = s->hood_front_h - 0.04f;
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
        /* plate and indicators stand clear of the bumper face: 5 mm would
         * flicker against it in the PSP's 16-bit depth buffer */
        quad_x(D, -0.26f, 0.26f, s->clearance + 0.1f, s->clearance + 0.21f, zd - 0.06f, uv_sub(T_CAR_PARTS, CP_PLATE), 1);
        quad_x(D, -hw * 0.98f, -hw * 0.8f, s->bumper_h - 0.08f, s->bumper_h - 0.01f, zd - 0.055f, uv_sub(T_CAR_PARTS, CP_INDICATOR), 1);
        quad_x(D, hw * 0.8f, hw * 0.98f, s->bumper_h - 0.08f, s->bumper_h - 0.01f, zd - 0.055f, uv_sub(T_CAR_PARTS, CP_INDICATOR), 1);
    }
    /* rear details */
    {
        float zd = zr + 0.025f, ty0 = s->bumper_h + 0.04f, ty1 = s->rear_h - 0.06f;
        quad_x(D, -hw * 0.96f, -hw * 0.5f, ty0, ty1, zd, uv_sub(T_CAR_PARTS, CP_TAIL), 0);
        quad_x(D, hw * 0.5f, hw * 0.96f, ty0, ty1, zd, uv_sub(T_CAR_PARTS, CP_TAIL), 0);
        quad_x(D, -hw - 0.02f, hw + 0.02f, s->clearance + 0.04f, s->bumper_h, zd + 0.03f, uv_sub(T_CAR_PARTS, CP_BUMPER), 0);
        quad_x(D, -0.26f, 0.26f, s->bumper_h + 0.02f, s->bumper_h + 0.13f, zd + 0.01f, uv_sub(T_CAR_PARTS, CP_PLATE), 0);
        if (s->spare_wheel) {
            float r = 0.36f, cy = (s->bumper_h + s->rear_h) * 0.5f + 0.1f;
            quad_x(D, -r, r, cy - r, cy + r, zd + 0.12f, uv_tile(T_TIRE), 0);
        }
    }

    /* inside of the doors: trim panel 5 cm in from the skin, and the rear edge */
    for (k = 0; k < 2; k++) {
        float sx = k ? 1.0f : -1.0f, xo = sx * hw, xi = sx * (hw - 0.05f);
        float z0 = zof(s, dc.f0), z1 = zof(s, dc.f1);
        float b0 = contour(d, y, 4, dc.f0), b1 = contour(d, y, 4, dc.f1);
        UVRect dk = uv_tile(T_WHITE);
        unsigned int trim = gfx_rgba(128, 118, 108, 255), edge = gfx_rgba(80, 80, 84, 255);
        gm_quad_flat(&cm->door_trim[k], v3(xi, dc.sill, z0), v3(xi, b0, z0), v3(xi, b1, z1), v3(xi, dc.sill, z1), dk, trim);
        gm_quad_flat(&cm->door_trim[k], v3(xo, dc.sill, z1), v3(xo, b1, z1), v3(xi, b1, z1), v3(xi, dc.sill, z1), dk, edge);
        gm_quad_flat(&cm->door_trim[k], v3(xo, b0, z0), v3(xo, b1, z1), v3(xi, b1, z1), v3(xi, b0, z0), dk, edge);
    }

    /* cabin, seen through an open door */
    {
        float overhang_r = (s->length - s->wheelbase) * 0.47f + s->wheelbase - s->wheel_r * 1.18f - 0.02f;
        float back = (s->roof_front + s->roof_rear) * 0.5f + 0.12f;
        float zf = zof(s, s->ws_base), zb = zof(s, back < overhang_r ? back : overhang_r);
        float yf = s->clearance + 0.12f, yc = s->height - 0.1f, xw = hw - 0.06f, belt = s->hood_h - 0.02f;
        float seat = zof(s, s->ws_base + s->length * 0.15f), ys = yf + 0.28f, yback = yf + 0.95f;
        UVRect dk = uv_tile(T_WHITE), wt = dk;
        unsigned int floor_c = gfx_rgba(40, 38, 36, 255), wall = gfx_rgba(58, 54, 50, 255), roof_c = gfx_rgba(150, 146, 136, 255);
        unsigned int seat_c = gfx_rgba(96, 64, 52, 255), dash = gfx_rgba(34, 34, 36, 255);
        GMesh *C = &cm->cabin;
        if (yback > yc - 0.08f) yback = yc - 0.08f;
        gm_quad_flat(C, v3(-xw, yf, zb), v3(-xw, yf, zf), v3(xw, yf, zf), v3(xw, yf, zb), dk, floor_c);           /* floor */
        gm_quad_flat(C, v3(-gi + 0.05f, yc, zf + 0.3f), v3(-gi + 0.05f, yc, zb), v3(gi - 0.05f, yc, zb),
                     v3(gi - 0.05f, yc, zf + 0.3f), dk, roof_c);                                                  /* headlining */
        gm_quad_flat(C, v3(-xw, yf, zb), v3(-xw, yc, zb), v3(xw, yc, zb), v3(xw, yf, zb), dk, wall);              /* behind the seats */
        /* dashboard ends leaving leg room in front of the seats */
        float dash_end = seat - 0.26f - 0.3f < zf + 0.42f ? seat - 0.26f - 0.3f : zf + 0.42f;
        if (dash_end < zf + 0.05f) dash_end = zf + 0.05f;
        gm_box(C, v3(-xw + 0.03f, yf, zf - 0.05f), v3(xw - 0.03f, belt, dash_end), wt, wt, dash, BOX_NO_BOTTOM);
        gm_box(C, v3(-hw * 0.44f - 0.17f, belt - 0.02f, dash_end + 0.04f), v3(-hw * 0.44f + 0.17f, belt + 0.26f, dash_end + 0.08f),
               wt, wt, gfx_rgba(20, 20, 20, 255), BOX_ALL);                                                       /* steering wheel */
        for (k = 0; k < 2; k++) {
            float sx = k ? 1.0f : -1.0f, xa = sx * hw * 0.12f, xb = sx * (hw - 0.12f);
            float lo = xa < xb ? xa : xb, hi = xa < xb ? xb : xa;
            /* no faces against the floor or against each other */
            gm_box(C, v3(lo, yf, seat - 0.26f), v3(hi, ys, seat + 0.24f), wt, wt, seat_c, BOX_NO_BOTTOM & ~16);   /* cushion */
            gm_box(C, v3(lo, yf, seat + 0.24f), v3(hi, yback, seat + 0.36f), wt, wt, gfx_mul(seat_c, 0.9f), BOX_NO_BOTTOM); /* back */
            gm_quad_flat(&cm->cabin_wall[k], v3(sx * xw, yf, zb), v3(sx * xw, belt, zb), v3(sx * xw, belt, zf),
                         v3(sx * xw, yf, zf), dk, wall);
        }
    }

    build_wheel(&cm->wheel, s->wheel_r, 0.22f, 10);
    build_wheel(&cm->wheel_far, s->wheel_r, 0.22f, 6);
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

/* Per car slot: the model with this car's paint baked in, built once when
 * the slot gets a new car; every frame only matrices change. */
typedef struct {
    int valid, type;
    unsigned int paint;
    GMesh body;     /* painted panels + details */
    GMesh door[2];  /* painted door skin + its window */
} CarInstance;

static CarInstance inst[MAX_CARS + 1];

static const CarInstance *instance(const Car *c)
{
    int slot = (c >= cars && c < cars + MAX_CARS) ? (int)(c - cars) : MAX_CARS;
    CarInstance *ci = &inst[slot];
    const CarModel *cm = &models[c->type];
    Xf id = xf_identity();
    int k;
    if (ci->valid && ci->type == c->type && ci->paint == c->paint && slot < MAX_CARS) return ci;
    gm_free(&ci->body);
    append_painted(&ci->body, &cm->paint, &id, c->paint);
    gm_append_xf(&ci->body, &cm->detail, &id);
    for (k = 0; k < 2; k++) {
        gm_free(&ci->door[k]);
        append_painted(&ci->door[k], &cm->door_skin[k], &id, c->paint);
        gm_append_xf(&ci->door[k], &cm->door_glass[k], &id);
    }
    ci->valid = 1;
    ci->type = c->type;
    ci->paint = c->paint;
    return ci;
}

void car_draw(DrawList *dl, const Car *c, int near)
{
    const CarSpec *s = &CAR_SPECS[c->type];
    const CarModel *cm = &models[c->type];
    const CarInstance *ci = instance(c);
    Xf body = body_xf(c);
    int w, k;
    dl_add(dl, ci->body.v, ci->body.n, &body);
    for (k = 0; k < 2; k++) {
        if (c->door > 0.02f && c->door_side == k) {
            /* swing about the vertical hinge line at the door's front edge */
            float sx = k ? 1.0f : -1.0f, hz = zof(s, cm->door_d0);
            Xf h = xf_translate(v3(sx * s->width * 0.5f, 0, hz)), r = xf_rot_y(sx * c->door * 1.15f);
            Xf hi = xf_translate(v3(-sx * s->width * 0.5f, 0, -hz)), a1 = xf_mul(&body, &h), a2 = xf_mul(&a1, &r);
            Xf m = xf_mul(&a2, &hi);
            dl_add(dl, ci->door[k].v, ci->door[k].n, &m);
            dl_add(dl, cm->door_trim[k].v, cm->door_trim[k].n, &m);
            dl_add(dl, cm->cabin.v, cm->cabin.n, &body);
            dl_add(dl, cm->cabin_wall[1 - k].v, cm->cabin_wall[1 - k].n, &body);
        } else {
            dl_add(dl, ci->door[k].v, ci->door[k].n, &body);
        }
    }
    /* wheels follow the ground, not the body roll */
    for (w = 0; w < 4; w++) {
        float x = (w & 1 ? 1.0f : -1.0f) * (s->width * 0.5f - 0.09f), z = w < 2 ? cm->front_axle : cm->rear_axle;
        Xf base = xf_translate(v3(c->pos.x, c->pos.y + c->body_y * 0.3f, c->pos.z));
        Xf yaw = xf_rot_y(c->yaw), hub = xf_translate(v3(x, s->wheel_r, z));
        Xf steer = xf_rot_y(w < 2 ? c->steer : 0.0f), spin = xf_rot_x(-c->wheel_spin);
        Xf m1 = xf_mul(&base, &yaw), m2 = xf_mul(&m1, &hub), m3 = xf_mul(&m2, &steer), m4 = xf_mul(&m3, &spin);
        if (near) dl_add(dl, cm->wheel.v, cm->wheel.n, &m4);
        else dl_add(dl, cm->wheel_far.v, cm->wheel_far.n, &m4);
    }
}

void car_append_mesh(GMesh *out, const Car *c)
{
    DrawList dl;
    dl_init(&dl);
    car_draw(&dl, c, 1);
    dl_flatten(&dl, out);
    dl_free(&dl);
}
