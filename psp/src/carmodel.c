/* Car meshes built from a side profile: lower body, greenhouse, lights,
 * grille, plates, an opening driver door and spinning, steering wheels. */
#include "car.h"
#include "texgen.h"
#include "synth.h"

#include <string.h>

#define WHITE 0xFFFFFFFFu

/* Columns: name, nickname; length, width, height, wheelbase (to the middle of a rear tandem), track, wheel
 * radius, clearance; side profile (bumper, hood front, hood, windscreen base, roof front, roof rear, rear glass
 * base, trunk, rear); glass inset; round lights, chrome grille, spare wheel; mass, kW, km/h, grip, centre of
 * gravity, steering lock, suspension softness, drive; body, axles, rear doors, fixed paint, engine voice,
 * beacon, livery */
const CarSpec CAR_SPECS[CAR_TYPES] = {
    { "Кайзер 525",    "бумер",    4.72f, 1.75f, 1.41f, 2.76f, 1.47f, 0.32f, 0.14f, 0.45f, 0.72f, 0.86f, 1.55f, 2.25f, 3.45f, 3.95f, 0.95f, 0.80f, 0.08f, 1, 1, 0, 1500, 141, 230, 1.05f, 0.50f, 0.60f, 1.0f, DRIVE_RWD,
      BODY_SEDAN, 2, 1, 0, ENG_I6, BEACON_NONE, LIVERY_NONE },
    { "Ластівка 09",   "дев'ятка", 4.00f, 1.65f, 1.40f, 2.46f, 1.40f, 0.29f, 0.16f, 0.42f, 0.66f, 0.80f, 1.25f, 1.95f, 3.00f, 3.90f, 0.95f, 0.90f, 0.07f, 0, 0, 0,  950,  52, 155, 0.95f, 0.52f, 0.62f, 1.3f, DRIVE_FWD,
      BODY_HATCH, 2, 1, 0, ENG_I4, BEACON_NONE, LIVERY_NONE },
    { "Класика 07",    "сімка",    4.15f, 1.62f, 1.44f, 2.42f, 1.36f, 0.29f, 0.17f, 0.48f, 0.78f, 0.88f, 1.35f, 1.95f, 3.00f, 3.35f, 0.95f, 0.85f, 0.06f, 0, 1, 0, 1050,  57, 150, 0.90f, 0.55f, 0.60f, 1.5f, DRIVE_RWD,
      BODY_SEDAN, 2, 1, 0, ENG_I4_RASPY, BEACON_NONE, LIVERY_NONE },
    { "Пантера Гранд", "джип",     4.60f, 1.80f, 1.72f, 2.69f, 1.50f, 0.37f, 0.22f, 0.60f, 0.95f, 1.08f, 1.60f, 2.20f, 4.35f, 4.50f, 1.15f, 1.05f, 0.07f, 0, 1, 0, 1800, 160, 190, 0.95f, 0.70f, 0.60f, 1.4f, DRIVE_AWD,
      BODY_SUV, 2, 1, 0, ENG_V8, BEACON_NONE, LIVERY_NONE },
    { "Гелендер G",    "гелік",    4.40f, 1.76f, 1.93f, 2.85f, 1.45f, 0.38f, 0.24f, 0.62f, 1.05f, 1.12f, 1.45f, 1.65f, 4.25f, 4.30f, 1.20f, 1.20f, 0.04f, 1, 0, 1, 2200, 130, 170, 0.90f, 0.80f, 0.55f, 1.2f, DRIVE_AWD,
      BODY_SUV, 2, 1, 0xFF141414u, ENG_V8_BIG, BEACON_NONE, LIVERY_NONE },
    { "Поле 4x4",      "нива",     3.74f, 1.68f, 1.64f, 2.20f, 1.43f, 0.34f, 0.22f, 0.60f, 0.92f, 1.00f, 1.20f, 1.75f, 3.50f, 3.65f, 1.05f, 1.00f, 0.06f, 1, 0, 0, 1150,  59, 140, 0.95f, 0.65f, 0.62f, 1.5f, DRIVE_AWD,
      BODY_SUV, 2, 0, 0, ENG_I4, BEACON_NONE, LIVERY_NONE },
    { "Бобик 469",     "бобик",    4.03f, 1.78f, 2.02f, 2.38f, 1.44f, 0.39f, 0.30f, 0.66f, 1.08f, 1.14f, 1.22f, 1.30f, 3.92f, 3.98f, 1.16f, 1.16f, 0.05f, 1, 0, 1, 1650,  55, 115, 0.88f, 0.78f, 0.60f, 1.7f, DRIVE_AWD,
      BODY_SOFT, 2, 1, 0xFF34C4E4u, ENG_UAZ, BEACON_BLUE, LIVERY_POLICE },
    { "Буханка",       "таблетка", 4.36f, 1.94f, 2.06f, 2.30f, 1.44f, 0.39f, 0.22f, 0.56f, 1.02f, 1.16f, 0.34f, 0.80f, 4.22f, 4.32f, 1.30f, 1.30f, 0.06f, 1, 0, 0, 1750,  66, 110, 0.85f, 0.85f, 0.60f, 1.6f, DRIVE_AWD,
      BODY_VAN, 2, 1, 0xFFE4ECEEu, ENG_UAZ, BEACON_BLUE, LIVERY_AMBULANCE },
    { "Тайга 375",     "пожежка",  7.35f, 2.40f, 2.70f, 4.20f, 2.00f, 0.60f, 0.40f, 0.95f, 1.55f, 1.72f, 2.05f, 2.22f, 3.12f, 3.18f, 1.56f, 1.56f, 0.06f, 1, 0, 0, 6800, 150,  80, 0.85f, 1.20f, 0.55f, 1.1f, DRIVE_AWD,
      BODY_TRUCK, 3, 0, 0xFF2424C4u, ENG_TRUCK, BEACON_FIRE, LIVERY_FIRE },
    { "Столиця 412",   "таксі",    4.25f, 1.55f, 1.48f, 2.40f, 1.27f, 0.30f, 0.17f, 0.45f, 0.74f, 0.82f, 1.40f, 2.00f, 3.05f, 3.45f, 0.90f, 0.82f, 0.06f, 0, 1, 0, 1080,  55, 142, 0.90f, 0.55f, 0.60f, 1.5f, DRIVE_RWD,
      BODY_SEDAN, 2, 1, 0xFF22B4E8u, ENG_I4_RASPY, BEACON_TAXI, LIVERY_TAXI },
    { "Явір 638",      "ява",      2.12f, 0.76f, 1.15f, 1.40f, 0.12f, 0.31f, 0.20f, 0.30f, 0.60f, 0.80f, 0.70f, 0.75f, 1.40f, 1.50f, 0.78f, 0.70f, 0.00f, 1, 0, 0,  240,  20, 120, 1.10f, 0.62f, 0.55f, 1.0f, DRIVE_RWD,
      BODY_BIKE, 2, 0, 0xFF2830C0u, ENG_2STROKE, BEACON_NONE, LIVERY_NONE },
};

/* The driver/passenger doors are cut out of the body itself: the same
 * panels, texture coordinates and window glass, so a closed door is exactly
 * the body and an open one leaves an opening into the cabin. */
typedef struct {
    GMesh paint, detail, wheel, wheel_far;
    GMesh door_skin[2];  /* painted outer panel, in body space (0 left, 1 right) */
    GMesh door_glass[2]; /* window, stripes and signs on the door: move with it */
    GMesh door_trim[2];  /* door card and edge, drawn only while open */
    GMesh cabin;         /* floor, dashboard, wheel, seats, headlining... */
    GMesh wall_front[2], wall_rear[2]; /* inner side walls by the front door and behind it */
    GMesh fork;          /* motorcycle: bars, fork legs, lamp; turn with the steering */
    GMesh small;         /* plates, mirrors, wipers, exhausts...: left out on far cars */
    int naxles;
    float axle_z[3];
    float fork_z;
    CarLayout lay;
} CarModel;

static CarModel models[CAR_TYPES];

#define CHROME 0xFFE4E0DCu
#define BLACK 0xFF1E1C1Cu
#define RUBBER 0xFF161616u
#define AMBER 0xFF30A0F0u

static float zof(const CarSpec *s, float d) { return -s->length * 0.5f + d; }

/* Top of the body side at distance d from the front (piecewise linear contour) */
static float contour(const float *d, const float *y, int n, float x)
{
    int i;
    for (i = 0; i + 1 < n; i++)
        if (x <= d[i + 1]) return y[i] + (y[i + 1] - y[i]) * (x - d[i]) / (d[i + 1] - d[i]);
    return y[n - 1];
}

/* Where doors and seats are, from the spec alone (also used by the ride logic) */
void car_layout(int t, CarLayout *L)
{
    const CarSpec *s = &CAR_SPECS[t];
    float bpil = (s->roof_front + s->roof_rear) * 0.5f;
    if (s->body == BODY_VAN) bpil = s->ws_base + 1.15f;
    if (s->body == BODY_TRUCK) bpil = s->roof_rear - 0.04f;
    L->door_d0 = s->ws_base - 0.05f;
    L->door_d1 = bpil - 0.06f;
    L->bpillar = bpil;
    L->seat_d = (L->door_d0 + L->door_d1) * 0.5f + 0.12f;
    L->floor_y = s->body == BODY_TRUCK ? 1.2f : (s->body == BODY_VAN ? 0.72f : s->clearance + 0.12f);
    L->seat_x = -s->width * 0.22f;
    L->seat_y = fminf(L->floor_y - 0.25f, s->height - 0.1f - 1.53f);
    if (s->body == BODY_BIKE) {
        L->door_d0 = L->door_d1 = L->bpillar = L->seat_d = s->length * 0.58f;
        L->floor_y = 0.0f;
        L->seat_x = 0.0f;
        L->seat_y = 0.8f - 0.56f;
    }
}

/* ------------------------------------------------------------ primitives */

static UVRect uvw(void) { return uv_tile(T_WHITE); }

static void box(GMesh *m, V3 lo, V3 hi, UVRect uv, unsigned int c)
{
    gm_box(m, lo, hi, uv, uv, c, BOX_ALL);
}

/* standing on something: no bottom face to fight with what it stands on */
static void box_nb(GMesh *m, V3 lo, V3 hi, UVRect uv, unsigned int c)
{
    gm_box(m, lo, hi, uv, uv, c, BOX_NO_BOTTOM);
}

/* box whose -z (front) or +z (back) face shows its own picture */
static void box_face(GMesh *m, V3 lo, V3 hi, UVRect side, UVRect face, int back, unsigned int c)
{
    UVRect u[6];
    Xf id = xf_identity();
    int i;
    for (i = 0; i < 6; i++) u[i] = side;
    u[back ? 4 : 5] = face;
    gm_box_xf(m, &id, lo, hi, u, c, BOX_ALL);
}

/* box with a side (+x or -x) picture */
static void box_side(GMesh *m, V3 lo, V3 hi, UVRect side, UVRect face, int plus_x, unsigned int c)
{
    UVRect u[6];
    Xf id = xf_identity();
    int i;
    for (i = 0; i < 6; i++) u[i] = side;
    u[plus_x ? 0 : 1] = face;
    gm_box_xf(m, &id, lo, hi, u, c, BOX_ALL);
}

/* box turned by xf (seat backs, the steering wheel ring) */
static void box_xf(GMesh *m, const Xf *xf, V3 lo, V3 hi, UVRect uv, unsigned int c)
{
    UVRect u[6];
    int i;
    for (i = 0; i < 6; i++) u[i] = uv;
    gm_box_xf(m, xf, lo, hi, u, c, BOX_ALL);
}

/* cylinder along z from z0 to z1 (lamps, exhausts); cap at z0 shows `cap` */
static void cyl_z(GMesh *m, V3 c, float r, float z0, float z1, int seg, UVRect side, UVRect cap, unsigned int col, int caps)
{
    int i;
    float cu = (cap.u0 + cap.u1) * 0.5f, cv = (cap.v0 + cap.v1) * 0.5f, ru = (cap.u1 - cap.u0) * 0.5f, rv = (cap.v1 - cap.v0) * 0.5f;
    for (i = 0; i < seg; i++) {
        float a0 = i * 6.2831853f / seg, a1 = (i + 1) * 6.2831853f / seg;
        float x0 = c.x + cosf(a0) * r, y0 = c.y + sinf(a0) * r, x1 = c.x + cosf(a1) * r, y1 = c.y + sinf(a1) * r;
        unsigned int sh = gfx_shade(v3(cosf((a0 + a1) * 0.5f), sinf((a0 + a1) * 0.5f), 0), col);
        gm_tri(m, v3(x0, y0, z0), v3(x1, y1, z0), v3(x1, y1, z1), side.u0, side.v1, side.u1, side.v1, side.u1, side.v0, sh);
        gm_tri(m, v3(x0, y0, z0), v3(x1, y1, z1), v3(x0, y0, z1), side.u0, side.v1, side.u1, side.v0, side.u0, side.v0, sh);
        if (caps & 1)
            gm_tri(m, v3(c.x, c.y, z0), v3(x1, y1, z0), v3(x0, y0, z0), cu, cv, cu + cosf(a1) * ru, cv - sinf(a1) * rv,
                   cu + cosf(a0) * ru, cv - sinf(a0) * rv, gfx_shade(v3(0, 0, z0 < z1 ? -1.0f : 1.0f), col));
        if (caps & 2)
            gm_tri(m, v3(c.x, c.y, z1), v3(x0, y0, z1), v3(x1, y1, z1), cu, cv, cu + cosf(a0) * ru, cv - sinf(a0) * rv,
                   cu + cosf(a1) * ru, cv - sinf(a1) * rv, gfx_shade(v3(0, 0, z0 < z1 ? 1.0f : -1.0f), col));
    }
}

/* cylinder along x (wheels, spare wheels on a side) */
static void cyl_x(GMesh *m, V3 c, float r, float x0, float x1, int seg, UVRect side, UVRect cap, unsigned int col, unsigned int capcol)
{
    int i;
    float cu = (cap.u0 + cap.u1) * 0.5f, cv = (cap.v0 + cap.v1) * 0.5f, ru = (cap.u1 - cap.u0) * 0.5f, rv = (cap.v1 - cap.v0) * 0.5f;
    for (i = 0; i < seg; i++) {
        float a0 = i * 6.2831853f / seg, a1 = (i + 1) * 6.2831853f / seg;
        float y0 = c.y + cosf(a0) * r, z0 = c.z + sinf(a0) * r, y1 = c.y + cosf(a1) * r, z1 = c.z + sinf(a1) * r;
        gm_quad(m, v3(x0, y0, z0), v3(x1, y0, z0), v3(x1, y1, z1), v3(x0, y1, z1), side, col);
        gm_tri(m, v3(x1, c.y, c.z), v3(x1, y0, z0), v3(x1, y1, z1), cu, cv, cu + cosf(a0) * ru, cv + sinf(a0) * rv,
               cu + cosf(a1) * ru, cv + sinf(a1) * rv, gfx_shade(v3(1, 0, 0), capcol));
        gm_tri(m, v3(x0, c.y, c.z), v3(x0, y1, z1), v3(x0, y0, z0), cu, cv, cu + cosf(a1) * ru, cv + sinf(a1) * rv,
               cu + cosf(a0) * ru, cv + sinf(a0) * rv, gfx_shade(v3(-1, 0, 0), capcol));
    }
}

/* vertical cylinder (beacons) */
static void cyl_y(GMesh *m, V3 c, float r, float y0, float y1, int seg, UVRect side, unsigned int col)
{
    int i;
    for (i = 0; i < seg; i++) {
        float a0 = i * 6.2831853f / seg, a1 = (i + 1) * 6.2831853f / seg;
        V3 p0 = v3(c.x + cosf(a0) * r, y0, c.z + sinf(a0) * r), p1 = v3(c.x + cosf(a1) * r, y0, c.z + sinf(a1) * r);
        gm_quad_flat(m, p0, v3(p0.x, y1, p0.z), v3(p1.x, y1, p1.z), p1, side, gfx_shade(v3(cosf(a0), 0.3f, sinf(a0)), col));
        gm_tri(m, v3(c.x, y1, c.z), v3(p1.x, y1, p1.z), v3(p0.x, y1, p0.z), side.u0, side.v0, side.u1, side.v0, side.u0, side.v0,
               gfx_shade(v3(0, 1, 0), col));
    }
}

/* flat picture facing -z (front) or +z (back) */
static void plate_z(GMesh *m, float x0, float x1, float y0, float y1, float z, UVRect uv, int back, unsigned int c)
{
    if (!back) gm_quad(m, v3(x0, y0, z), v3(x0, y1, z), v3(x1, y1, z), v3(x1, y0, z), uv, c);
    else gm_quad(m, v3(x1, y0, z), v3(x1, y1, z), v3(x0, y1, z), v3(x0, y0, z), uv, c);
}

/* flat picture on a side, facing -x (sx < 0) or +x, between distances z0..z1 (z0 at the front) */
static void plate_x(GMesh *m, float x, float z0, float z1, float y0, float y1, UVRect uv, unsigned int c)
{
    if (x < 0) gm_quad(m, v3(x, y0, z0), v3(x, y1, z0), v3(x, y1, z1), v3(x, y0, z1), uv, c);
    else gm_quad(m, v3(x, y0, z1), v3(x, y1, z1), v3(x, y1, z0), v3(x, y0, z0), uv, c);
}

/* ------------------------------------------------------------ body sides */

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

/* A stripe along the body (livery, plastic cladding): part of the side
 * panels themselves, not a decal over them, so nothing can flicker. */
typedef struct {
    float lo, hi;       /* heights; hi <= lo: none */
    UVRect uv;          /* stretched along the car */
    unsigned int color;
} Band;

/* Body side with wheel arches cut out, in columns between breakpoints, each
 * column split at the sill and the band. Door parts above the sill go to
 * door (painted) or door_band (unpainted stripe), the rest to body or
 * band. */
static void side_quads(GMesh *body, GMesh *band_mesh, GMesh *door, GMesh *door_band, const CarSpec *s, float x,
                       const float *d, const float *y, int n, float top, const float *arch, int narch, float arch_r,
                       const DoorCut *dc, const Band *bd)
{
    float bp[64];
    int nb = 0, i, k;
    UVRect full = uv_tile(T_CAR_SIDE);
    unsigned int c = gfx_shade(v3(x < 0 ? -1.0f : 1.0f, 0, 0), WHITE), cb = gfx_shade(v3(x < 0 ? -1.0f : 1.0f, 0, 0), bd->color);
    for (i = 0; i < n; i++) bp[nb++] = d[i];
    for (k = 0; k < narch; k++) {
        int j;
        for (j = 0; j <= 6; j++) bp[nb++] = arch[k] - arch_r + arch_r * 2.0f * j / 6.0f;
    }
    bp[nb++] = dc->f0;
    bp[nb++] = dc->f1;
    if (dc->r1 > dc->r0) {
        bp[nb++] = dc->r0;
        bp[nb++] = dc->r1;
    }
    for (i = 1; i < nb; i++) {
        float v = bp[i];
        int j = i - 1;
        while (j >= 0 && bp[j] > v) { bp[j + 1] = bp[j]; j--; }
        bp[j + 1] = v;
    }
    for (i = 0; i + 1 < nb; i++) {
        float d0 = bp[i], d1 = bp[i + 1], y0[2], y1[2], dd[2] = { d0, d1 }, dm = (d0 + d1) * 0.5f, u0, u1;
        float lv[6];
        int front = dm > dc->f0 && dm < dc->f1, rear = dm > dc->r0 && dm < dc->r1, nl = 0, seg;
        if (d1 - d0 < 0.005f) continue;
        for (k = 0; k < 2; k++) {
            float bottom = s->clearance, ax;
            int w;
            for (w = 0; w < narch; w++) {
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
        /* levels that split this column */
        lv[nl++] = -10.0f;
        if (front) lv[nl++] = dc->sill;
        if (bd->hi > bd->lo) {
            lv[nl++] = bd->lo;
            lv[nl++] = bd->hi;
        }
        lv[nl++] = 10.0f;
        for (k = 1; k < nl; k++) {
            float v = lv[k];
            int j = k - 1;
            while (j >= 0 && lv[j] > v) { lv[j + 1] = lv[j]; j--; }
            lv[j + 1] = v;
        }
        for (seg = 0; seg + 1 < nl; seg++) {
            float la = lv[seg], lb = lv[seg + 1], mid = (la + lb) * 0.5f;
            float a0 = clampf(la, y0[0], y1[0]), a1 = clampf(la, y0[1], y1[1]);
            float b0 = clampf(lb, y0[0], y1[0]), b1 = clampf(lb, y0[1], y1[1]);
            int in_band = bd->hi > bd->lo && mid > bd->lo && mid < bd->hi, on_door = front && mid > dc->sill;
            GMesh *m = in_band ? (on_door ? door_band : band_mesh) : (on_door ? door : body);
            if (b0 - a0 < 0.004f && b1 - a1 < 0.004f) continue;
            if (in_band) {
                UVRect bu = bd->uv;
                float bu0 = bu.u0 + (bu.u1 - bu.u0) * d0 / s->length, bu1 = bu.u0 + (bu.u1 - bu.u0) * d1 / s->length;
                float vv[4];
                vv[0] = bu.v1 + (bu.v0 - bu.v1) * (a0 - bd->lo) / (bd->hi - bd->lo);
                vv[1] = bu.v1 + (bu.v0 - bu.v1) * (b0 - bd->lo) / (bd->hi - bd->lo);
                vv[2] = bu.v1 + (bu.v0 - bu.v1) * (b1 - bd->lo) / (bd->hi - bd->lo);
                vv[3] = bu.v1 + (bu.v0 - bu.v1) * (a1 - bd->lo) / (bd->hi - bd->lo);
                gm_tri(m, v3(x, a0, zof(s, d0)), v3(x, b0, zof(s, d0)), v3(x, b1, zof(s, d1)), bu0, vv[0], bu0, vv[1], bu1, vv[2], cb);
                gm_tri(m, v3(x, a0, zof(s, d0)), v3(x, b1, zof(s, d1)), v3(x, a1, zof(s, d1)), bu0, vv[0], bu1, vv[2], bu1, vv[3], cb);
            } else {
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

/* Side window between distances da..db from the front: the part of the
 * greenhouse side (belt line at x = bx, roof edge at x = gi) in that range. */
static void side_glass(GMesh *m, const CarSpec *s, float sx, float da, float db, float bx, float gi, UVRect g, unsigned int col)
{
    float ba = da > s->ws_base ? da : s->ws_base, bb = db < s->rear_glass_base ? db : s->rear_glass_base;
    float ta = da > s->roof_front ? da : s->roof_front, tb = db < s->roof_rear ? db : s->roof_rear;
    float span = s->rear_glass_base - s->ws_base;
    float ya = s->hood_h + (s->trunk_h - s->hood_h) * (ba - s->ws_base) / span;
    float yb = s->hood_h + (s->trunk_h - s->hood_h) * (bb - s->ws_base) / span;
    float ua = g.u0 + (g.u1 - g.u0) * (da - s->ws_base) / span, ub = g.u0 + (g.u1 - g.u0) * (db - s->ws_base) / span;
    unsigned int c = gfx_shade(v3(sx, 0.4f, 0), col);
    UVRect r;
    r.u0 = ua < g.u0 ? g.u0 : ua;
    r.u1 = ub > g.u1 ? g.u1 : ub;
    r.v0 = g.v0;
    r.v1 = g.v1;
    if (bb - ba < 0.01f || tb - ta < 0.01f) return;
    gm_quad_flat(m, v3(sx * bx, ya, zof(s, ba)), v3(sx * gi, s->height, zof(s, ta)), v3(sx * gi, s->height, zof(s, tb)),
                 v3(sx * bx, yb, zof(s, bb)), r, c);
}

/* ---------------------------------------------------------------- wheels */

static void build_wheel(GMesh *m, float r, float w, int seg, UVRect rim, unsigned int rim_col)
{
    int i;
    UVRect tread = uv_sub(T_CAR_PARTS, CP_DARK), tire = uv_tile(T_TIRE);
    float tu = (tire.u0 + tire.u1) * 0.5f, tv = (tire.v0 + tire.v1) * 0.5f, tru = (tire.u1 - tire.u0) * 0.5f, trv = (tire.v1 - tire.v0) * 0.5f;
    float cu = (rim.u0 + rim.u1) * 0.5f, cv = (rim.v0 + rim.v1) * 0.5f, ru = (rim.u1 - rim.u0) * 0.5f, rv = (rim.v1 - rim.v0) * 0.5f;
    const float ri = r * 0.62f;
    for (i = 0; i < seg; i++) {
        float a0 = i * 6.2831853f / seg, a1 = (i + 1) * 6.2831853f / seg;
        float c0 = cosf(a0), s0 = sinf(a0), c1 = cosf(a1), s1 = sinf(a1);
        int side;
        gm_quad(m, v3(-w / 2, c0 * r, s0 * r), v3(w / 2, c0 * r, s0 * r), v3(w / 2, c1 * r, s1 * r), v3(-w / 2, c1 * r, s1 * r),
                tread, RUBBER);
        for (side = 0; side < 2; side++) {
            float x = side ? w / 2 : -w / 2, xr = side ? w / 2 - 0.025f : -w / 2 + 0.025f;  /* the rim sits a little in */
            unsigned int sh = gfx_shade(v3(side ? 1.0f : -1.0f, 0, 0), WHITE), shr = gfx_shade(v3(side ? 1.0f : -1.0f, 0, 0), rim_col);
            V3 o0 = v3(x, c0 * r, s0 * r), o1 = v3(x, c1 * r, s1 * r), i0 = v3(xr, c0 * ri, s0 * ri), i1 = v3(xr, c1 * ri, s1 * ri);
            float tu0 = tu + c0 * tru, tv0 = tv + s0 * trv, tu1 = tu + c1 * tru, tv1 = tv + s1 * trv;
            float iu0 = tu + c0 * tru * 0.62f, iv0 = tv + s0 * trv * 0.62f, iu1 = tu + c1 * tru * 0.62f, iv1 = tv + s1 * trv * 0.62f;
            /* sidewall ring (tyre picture), then the rim disc */
            if (side) {
                gm_tri(m, o0, o1, i1, tu0, tv0, tu1, tv1, iu1, iv1, sh);
                gm_tri(m, o0, i1, i0, tu0, tv0, iu1, iv1, iu0, iv0, sh);
                gm_tri(m, v3(xr, 0, 0), i0, i1, cu, cv, cu + c0 * ru, cv + s0 * rv, cu + c1 * ru, cv + s1 * rv, shr);
            } else {
                gm_tri(m, o1, o0, i0, tu1, tv1, tu0, tv0, iu0, iv0, sh);
                gm_tri(m, o1, i0, i1, tu1, tv1, iu0, iv0, iu1, iv1, sh);
                gm_tri(m, v3(xr, 0, 0), i1, i0, cu, cv, cu + c1 * ru, cv + s1 * rv, cu + c0 * ru, cv + s0 * rv, shr);
            }
        }
    }
}

/* ----------------------------------------------------------- the body */

typedef struct {
    const CarSpec *s;
    CarModel *cm;
    GMesh *P, *D, *S;        /* painted, unpainted, small unpainted details */
    float hw, zf, zr, top, gi, bx;
    float zb0, zt0, zt1, zb1; /* windscreen base, roof front and rear, rear glass base */
    float arch[3], ar;
    int narch;
    DoorCut dc;
} Ctx;

static float hood_at(const Ctx *x, float d)
{
    const CarSpec *s = x->s;
    if (d <= s->ws_base) return s->hood_front_h + (s->hood_h - s->hood_front_h) * d / s->ws_base;
    return s->hood_h;
}

/* both door mirrors: arm, housing and glass, at the base of the A pillar */
static void mirrors(Ctx *x, float w, float h, unsigned int housing, int painted)
{
    const CarSpec *s = x->s;
    int k;
    for (k = 0; k < 2; k++) {
        float sx = k ? 1.0f : -1.0f, z = x->zb0 + 0.12f, y = s->hood_h + 0.12f;
        GMesh *m = painted ? x->P : x->S;
        box(x->S, v3(sx > 0 ? x->hw - 0.02f : -x->hw - 0.06f, y - 0.03f, z - 0.03f), v3(sx > 0 ? x->hw + 0.06f : -x->hw + 0.02f, y + 0.02f, z + 0.03f),
            uv_sub(T_CAR_PARTS, CP_BLACK_PLASTIC), BLACK);
        box(m, v3(sx > 0 ? x->hw + 0.04f : -x->hw - 0.04f - w, y - h * 0.5f, z - 0.05f),
            v3(sx > 0 ? x->hw + 0.04f + w : -x->hw - 0.04f, y + h * 0.5f, z + 0.05f), painted ? uvw() : uv_sub(T_CAR_PARTS, CP_BLACK_PLASTIC), housing);
        plate_z(x->S, sx > 0 ? x->hw + 0.05f : -x->hw - 0.03f - w, sx > 0 ? x->hw + 0.03f + w : -x->hw - 0.05f, y - h * 0.4f, y + h * 0.4f,
                z + 0.057f, uv_sub(T_CAR_PARTS2, C2_MIRROR), 1, WHITE);
    }
}

static void wipers(Ctx *x, float spread)
{
    const CarSpec *s = x->s;
    int k;
    for (k = 0; k < 2; k++) {
        float cx = (k ? 0.12f : -0.38f) * spread;
        box(x->S, v3(cx - 0.01f, s->hood_h + 0.005f, x->zb0 - 0.02f), v3(cx + 0.34f * spread, s->hood_h + 0.025f, x->zb0 + 0.02f),
            uv_sub(T_CAR_PARTS, CP_DARK), BLACK);
    }
}

static void plates(Ctx *x, float yf, float yr, float zf_off, float zr_off)
{
    plate_z(x->S, -0.26f, 0.26f, yf, yf + 0.11f, x->zf - zf_off, uv_sub(T_CAR_PARTS, CP_PLATE), 0, WHITE);
    plate_z(x->S, -0.26f, 0.26f, yr, yr + 0.11f, x->zr + zr_off, uv_sub(T_CAR_PARTS, CP_PLATE), 1, WHITE);
}

static void exhaust(Ctx *x, float side, float y, int twin)
{
    int k;
    for (k = 0; k < (twin ? 2 : 1); k++) {
        float cx = side * (x->hw * 0.55f + k * 0.1f);
        cyl_z(x->S, v3(cx, y, 0), 0.03f, x->zr - 0.25f, x->zr + 0.06f, 6, uv_sub(T_CAR_PARTS, CP_CHROME), uv_sub(T_CAR_PARTS, CP_DARK),
              0xFF8C8C90u, 2);
    }
}

/* round lamp: housing ring and lens, facing the front (back = 0) or rear */
static void round_lamp(GMesh *m, float cx, float cy, float z, float r, int back, UVRect lens, unsigned int ring)
{
    float z1 = back ? z + 0.04f : z - 0.04f;
    cyl_z(m, v3(cx, cy, 0), r, z, z1, 8, uv_sub(T_CAR_PARTS, CP_CHROME), lens, ring, 2);
}

/* the generic body: sides with arches, hood, trunk, panels, glass, roof */
static void build_body(Ctx *x)
{
    const CarSpec *s = x->s;
    CarModel *cm = x->cm;
    float d[4] = { 0.0f, s->ws_base, s->rear_glass_base, s->length };
    float y[4] = { s->hood_front_h, s->hood_h, s->trunk_h, s->rear_h };
    UVRect white = uvw(), glass = uv_tile(T_CAR_GLASS), canvas = uv_sub(T_CAR_PARTS2, C2_CANVAS);
    const unsigned int canvas_col = 0xFF4C5A54u;
    Band band;
    int k, w;

    band.lo = band.hi = 0.0f;
    band.uv = uv_sub(T_WHITE, 8, 8, 16, 16);
    band.color = WHITE;
    switch (s->livery) {
    case LIVERY_POLICE:    band.lo = s->clearance + 0.38f; band.hi = band.lo + 0.13f; break;
    case LIVERY_AMBULANCE: band.lo = s->hood_h - 0.2f; band.hi = band.lo + 0.14f; band.color = 0xFF2C2CD0u; break;
    case LIVERY_FIRE:      band.lo = s->clearance + 0.55f; band.hi = band.lo + 0.12f; break;
    case LIVERY_TAXI:      band.lo = s->clearance + 0.36f; band.hi = band.lo + 0.08f; band.uv = uv_sub(T_LIVERY, LV_CHECKER); break;
    default:
        if (s->body == BODY_SUV && s->sound == ENG_V8) {   /* plastic cladding on the big off-roader */
            band.lo = s->clearance + 0.02f;
            band.hi = band.lo + 0.26f;
            band.uv = uv_sub(T_CAR_PARTS, CP_BLACK_PLASTIC);
            band.color = 0xFF404040u;
        }
        break;
    }
    if (s->livery == LIVERY_POLICE) band.color = 0xFFB03C1Eu;   /* blue */
    if (s->livery == LIVERY_FIRE) band.color = 0xFFF0F0F0u;

    for (k = 0; k < 2; k++)
        side_quads(x->P, x->D, &cm->door_skin[k], &cm->door_glass[k], s, k ? x->hw : -x->hw, d, y, 4, x->top, x->arch, x->narch,
                   x->ar, &x->dc, &band);
    /* dark wheel wells so the arches are not see-through */
    for (w = 0; w < x->narch; w++) {
        float zc = zof(s, x->arch[w]), ar = x->ar;
        UVRect dk = uv_sub(T_CAR_PARTS, CP_DARK);
        unsigned int c = 0xFF1A1818u;
        gm_quad(x->D, v3(-x->hw + 0.3f, s->clearance, zc + ar), v3(-x->hw + 0.3f, s->wheel_r + ar, zc + ar),
                v3(-x->hw + 0.3f, s->wheel_r + ar, zc - ar), v3(-x->hw + 0.3f, s->clearance, zc - ar), dk, c);
        gm_quad(x->D, v3(x->hw - 0.3f, s->clearance, zc - ar), v3(x->hw - 0.3f, s->wheel_r + ar, zc - ar),
                v3(x->hw - 0.3f, s->wheel_r + ar, zc + ar), v3(x->hw - 0.3f, s->clearance, zc + ar), dk, c);
        gm_quad(x->D, v3(-x->hw, s->wheel_r + ar * 0.95f, zc - ar), v3(-x->hw, s->wheel_r + ar * 0.95f, zc + ar),
                v3(x->hw, s->wheel_r + ar * 0.95f, zc + ar), v3(x->hw, s->wheel_r + ar * 0.95f, zc - ar), dk, c);
    }
    gm_quad(x->P, v3(-x->hw, s->hood_front_h, x->zf), v3(-x->hw, s->hood_h, x->zb0), v3(x->hw, s->hood_h, x->zb0),
            v3(x->hw, s->hood_front_h, x->zf), white, WHITE);                                         /* hood */
    gm_quad(x->P, v3(-x->hw, s->trunk_h, x->zb1), v3(-x->hw, s->rear_h, x->zr), v3(x->hw, s->rear_h, x->zr),
            v3(x->hw, s->trunk_h, x->zb1), white, WHITE);                                             /* trunk / deck */
    plate_z(x->P, -x->hw, x->hw, s->clearance, s->hood_front_h, x->zf, white, 0, WHITE);             /* front panel */
    plate_z(x->P, -x->hw, x->hw, s->clearance, s->rear_h, x->zr, white, 1, WHITE);                   /* rear panel */
    gm_quad(x->D, v3(-x->hw, s->clearance, x->zr), v3(-x->hw, s->clearance, x->zf), v3(x->hw, s->clearance, x->zf),
            v3(x->hw, s->clearance, x->zr), uv_sub(T_CAR_PARTS, CP_DARK), 0xFF1E1E1Eu);               /* underside */

    /* greenhouse */
    {
        float yb0 = s->hood_h, yb1 = s->trunk_h, H = s->height, bx = x->bx, gi = x->gi;
        float bz = zof(s, cm->lay.bpillar);
        int soft = s->body == BODY_SOFT, truck = s->body == BODY_TRUCK;
        gm_quad(x->D, v3(-bx, yb0, x->zb0), v3(-gi, H, x->zt0), v3(gi, H, x->zt0), v3(bx, yb0, x->zb0), glass, WHITE);   /* windscreen */
        if (truck) {
            /* cab back: painted wall with a small window */
            plate_z(x->P, -bx, bx, yb1, H, x->zb1, white, 1, WHITE);
            plate_z(x->D, -0.35f, 0.35f, H - 0.55f, H - 0.2f, x->zb1 + 0.015f, glass, 1, WHITE);
        } else if (soft) {
            plate_z(x->D, -bx, bx, yb1, H, x->zb1, canvas, 1, canvas_col);                              /* canvas back */
            plate_z(x->D, -0.45f, 0.45f, H - 0.42f, H - 0.12f, x->zb1 + 0.02f, glass, 1, WHITE);         /* its little window */
        } else {
            gm_quad(x->D, v3(bx, yb1, x->zb1), v3(gi, H, x->zt1), v3(-gi, H, x->zt1), v3(-bx, yb1, x->zb1), glass, WHITE); /* rear glass */
        }
        if (soft) gm_quad(x->D, v3(-gi, H, x->zt1), v3(-gi, H, x->zt0), v3(gi, H, x->zt0), v3(gi, H, x->zt1), canvas, canvas_col);
        else gm_quad(x->P, v3(-gi, H, x->zt1), v3(-gi, H, x->zt0), v3(gi, H, x->zt0), v3(gi, H, x->zt1), white, WHITE);   /* roof */
        /* side windows: the front part belongs to the door; behind it glass, or canvas on the soft top */
        for (k = 0; k < 2; k++) {
            float sx = k ? 1.0f : -1.0f;
            side_glass(&cm->door_glass[k], s, sx, s->ws_base, x->dc.f1, bx, gi, glass, WHITE);
            if (soft) side_glass(x->D, s, sx, x->dc.f1, s->rear_glass_base, bx, gi, canvas, canvas_col);
            else if (s->livery == LIVERY_AMBULANCE) side_glass(x->D, s, sx, x->dc.f1, s->rear_glass_base, bx, gi, uv_sub(T_WHITE, 8, 8, 16, 16), 0xFFDCE4E4u);
            else side_glass(x->D, s, sx, x->dc.f1, s->rear_glass_base, bx, gi, glass, WHITE);
        }
        /* B pillars in body colour */
        if (!soft) {
            gm_quad(x->P, v3(-bx - 0.015f, (yb0 + yb1) * 0.5f, bz + 0.06f), v3(-gi - 0.015f, H, bz + 0.06f),
                    v3(-gi - 0.015f, H, bz - 0.06f), v3(-bx - 0.015f, (yb0 + yb1) * 0.5f, bz - 0.06f), white, WHITE);
            gm_quad(x->P, v3(bx + 0.015f, (yb0 + yb1) * 0.5f, bz - 0.06f), v3(gi + 0.015f, H, bz - 0.06f),
                    v3(gi + 0.015f, H, bz + 0.06f), v3(bx + 0.015f, (yb0 + yb1) * 0.5f, bz + 0.06f), white, WHITE);
        }
    }
}

/* ------------------------------------------------- each model's own face */

#define UV2(r) uv_sub(T_CAR_PARTS2, r)
#define UVP(r) uv_sub(T_CAR_PARTS, r)

static void add_beacon(CarModel *cm, V3 p, unsigned int col)
{
    if (cm->lay.nbeacons < 4) {
        cm->lay.beacon[cm->lay.nbeacons] = p;
        cm->lay.beacon_col[cm->lay.nbeacons] = col;
        cm->lay.nbeacons++;
    }
}

/* a roof bar with two lenses (police, fire) */
static void beacon_bar(Ctx *x, float z, float w, UVRect left, UVRect right, unsigned int lc, unsigned int rc)
{
    const float H = x->s->height;
    box(x->D, v3(-w, H + 0.01f, z - 0.1f), v3(w, H + 0.06f, z + 0.1f), UVP(CP_DARK), 0xFF303030u);
    box_nb(x->D, v3(-w + 0.04f, H + 0.06f, z - 0.08f), v3(-0.06f, H + 0.17f, z + 0.08f), left, WHITE);
    box_nb(x->D, v3(0.06f, H + 0.06f, z - 0.08f), v3(w - 0.04f, H + 0.17f, z + 0.08f), right, WHITE);
    add_beacon(x->cm, v3(-w * 0.5f, H + 0.12f, z), lc);
    add_beacon(x->cm, v3(w * 0.5f, H + 0.12f, z), rc);
}

/* a sign on both front doors (moves with them) */
static void door_signs(Ctx *x, float y0, float y1, UVRect uv)
{
    int k;
    float d0 = x->dc.f0 + (x->dc.f1 - x->dc.f0) * 0.12f, d1 = x->dc.f1 - (x->dc.f1 - x->dc.f0) * 0.12f;
    for (k = 0; k < 2; k++) {
        float sx = k ? 1.0f : -1.0f;
        UVRect u = uv;
        if (k) { float t = u.u0; u.u0 = u.u1; u.u1 = t; }  /* reads left to right on both sides */
        plate_x(&x->cm->door_glass[k], sx * (x->hw + 0.022f), zof(x->s, d0), zof(x->s, d1), y0, y1, u, WHITE);
    }
}

static void model_kaiser(Ctx *x)
{
    const float zf = x->zf, zr = x->zr, hw = x->hw;
    int k;
    for (k = 0; k < 2; k++) {
        float sx = k ? 1.0f : -1.0f;
        /* two round lamps each side in the black band, the kidneys in between */
        box_face(x->D, v3(k ? 0.22f : -0.82f, 0.5f, zf - 0.03f), v3(k ? 0.82f : -0.22f, 0.65f, zf + 0.01f), UVP(CP_DARK),
                 uv_sub(T_CAR_PARTS2, k * 16, 24, 16, 8), 0, WHITE);
        round_lamp(x->D, sx * 0.56f, 0.24f, zf - 0.105f, 0.045f, 0, UVP(CP_HEAD_ROUND), BLACK);           /* fog lamps */
        plate_z(x->D, k ? 0.68f : -0.84f, k ? 0.84f : -0.68f, 0.37f, 0.42f, zf - 0.105f, UVP(CP_INDICATOR), 0, WHITE);
        box_face(x->D, v3(k ? 0.3f : -0.84f, 0.62f, zr - 0.01f), v3(k ? 0.84f : -0.3f, 0.76f, zr + 0.03f), UVP(CP_DARK), UVP(CP_TAIL), 1, WHITE);
        /* chrome window line */
        box(x->D, v3(sx > 0 ? hw - 0.004f : -hw - 0.012f, x->s->hood_h - 0.02f, x->zb0 + 0.05f),
            v3(sx > 0 ? hw + 0.012f : -hw + 0.004f, x->s->hood_h - 0.004f, x->zb1), UVP(CP_CHROME), CHROME);
    }
    box_face(x->D, v3(-0.2f, 0.47f, zf - 0.055f), v3(0.2f, 0.68f, zf + 0.01f), UVP(CP_CHROME), UV2(C2_KIDNEYS), 0, WHITE);
    box(x->P, v3(-hw - 0.01f, 0.18f, zf - 0.1f), v3(hw + 0.01f, 0.44f, zf + 0.02f), uvw(), WHITE);          /* bumpers in body colour */
    box(x->D, v3(-hw - 0.02f, 0.3f, zf - 0.115f), v3(hw + 0.02f, 0.35f, zf - 0.05f), UVP(CP_BLACK_PLASTIC), BLACK);
    box(x->P, v3(-hw - 0.01f, 0.18f, zr - 0.02f), v3(hw + 0.01f, 0.44f, zr + 0.1f), uvw(), WHITE);
    box(x->D, v3(-hw - 0.02f, 0.3f, zr + 0.05f), v3(hw + 0.02f, 0.35f, zr + 0.115f), UVP(CP_BLACK_PLASTIC), BLACK);
    plate_z(x->S, -0.26f, 0.26f, 0.19f, 0.3f, zf - 0.12f, UVP(CP_PLATE), 0, WHITE);
    plate_z(x->S, -0.26f, 0.26f, 0.64f, 0.75f, zr + 0.02f, UVP(CP_PLATE), 1, WHITE);
    exhaust(x, -1.0f, 0.22f, 1);
    box(x->D, v3(hw * 0.7f, x->s->trunk_h, zr - 0.45f), v3(hw * 0.7f + 0.008f, x->s->trunk_h + 0.55f, zr - 0.442f), UVP(CP_DARK), BLACK); /* aerial */
    mirrors(x, 0.12f, 0.08f, WHITE, 1);
    wipers(x, 1.0f);
}

static void model_lastivka(Ctx *x)
{
    const float zf = x->zf, zr = x->zr, hw = x->hw;
    int k;
    box(x->D, v3(-hw - 0.02f, 0.14f, zf - 0.09f), v3(hw + 0.02f, 0.4f, zf + 0.03f), UVP(CP_BLACK_PLASTIC), 0xFF303030u);
    box(x->D, v3(-hw - 0.02f, 0.14f, zr - 0.03f), v3(hw + 0.02f, 0.42f, zr + 0.09f), UVP(CP_BLACK_PLASTIC), 0xFF303030u);
    box(x->D, v3(-hw * 0.8f, 0.1f, zf - 0.06f), v3(hw * 0.8f, 0.135f, zf + 0.06f), UVP(CP_BLACK_PLASTIC), BLACK);  /* lip */
    for (k = 0; k < 2; k++) {
        float sx = k ? 1.0f : -1.0f;
        UVRect l = UV2(C2_RECT_SLANT);
        if (k) { float t = l.u0; l.u0 = l.u1; l.u1 = t; }
        box_face(x->D, v3(k ? 0.25f : -0.78f, 0.47f, zf - 0.02f), v3(k ? 0.78f : -0.25f, 0.6f, zf + 0.01f), UVP(CP_DARK), l, 0, WHITE);
        plate_z(x->D, sx * 0.62f - 0.08f, sx * 0.62f + 0.08f, 0.32f, 0.37f, zf - 0.095f, UVP(CP_INDICATOR), 0, WHITE);
    }
    box_face(x->D, v3(-0.24f, 0.49f, zf - 0.015f), v3(0.24f, 0.58f, zf + 0.01f), UVP(CP_DARK), UVP(CP_GRILLE_BLACK), 0, WHITE);
    box_face(x->D, v3(-0.8f, 0.66f, zr - 0.01f), v3(0.8f, 0.8f, zr + 0.03f), UVP(CP_DARK), UVP(CP_TAIL), 1, WHITE);
    box(x->D, v3(-0.02f, x->s->trunk_h + 0.05f, x->zb1 + 0.02f), v3(0.32f, x->s->trunk_h + 0.07f, x->zb1 + 0.05f), UVP(CP_DARK), BLACK); /* rear wiper */
    plates(x, 0.18f, 0.2f, 0.095f, 0.095f);
    exhaust(x, -1.0f, 0.17f, 0);
    mirrors(x, 0.1f, 0.08f, 0xFF303030u, 0);
    wipers(x, 0.95f);
}

static void model_klasyka(Ctx *x)
{
    const float zf = x->zf, zr = x->zr, hw = x->hw, H = x->s->height;
    int k;
    box_face(x->D, v3(-0.36f, 0.48f, zf - 0.05f), v3(0.36f, 0.72f, zf + 0.01f), UVP(CP_CHROME), UVP(CP_GRILLE_CHROME), 0, WHITE);
    box_face(x->D, v3(-0.07f, 0.46f, zf - 0.075f), v3(0.07f, 0.74f, zf - 0.02f), UVP(CP_CHROME), uv_sub(T_CAR_PARTS2, 36, 1, 8, 10), 0, WHITE);
    for (k = 0; k < 2; k++) {
        float sx = k ? 1.0f : -1.0f;
        box(x->D, v3(k ? 0.38f : -0.8f, 0.5f, zf - 0.035f), v3(k ? 0.8f : -0.38f, 0.7f, zf + 0.005f), UVP(CP_CHROME), CHROME);
        plate_z(x->D, k ? 0.4f : -0.58f, k ? 0.58f : -0.4f, 0.52f, 0.68f, zf - 0.038f, UVP(CP_HEAD_RECT), 0, WHITE);
        plate_z(x->D, k ? 0.6f : -0.78f, k ? 0.78f : -0.6f, 0.52f, 0.68f, zf - 0.038f, UVP(CP_HEAD_RECT), 0, WHITE);
        box(x->D, v3(sx * 0.35f - 0.04f, 0.28f, zf - 0.11f), v3(sx * 0.35f + 0.04f, 0.45f, zf - 0.07f), UVP(CP_BLACK_PLASTIC), BLACK); /* overriders */
        box(x->D, v3(sx * 0.35f - 0.04f, 0.28f, zr + 0.07f), v3(sx * 0.35f + 0.04f, 0.45f, zr + 0.11f), UVP(CP_BLACK_PLASTIC), BLACK);
        plate_z(x->D, sx * 0.6f - 0.07f, sx * 0.6f + 0.07f, 0.24f, 0.3f, zf - 0.03f, UVP(CP_INDICATOR), 0, WHITE);
        box_face(x->D, v3(k ? 0.32f : -0.78f, 0.5f, zr - 0.01f), v3(k ? 0.78f : -0.32f, 0.7f, zr + 0.03f), UVP(CP_DARK), UVP(CP_TAIL), 1, WHITE);
        /* chrome rain gutters along the roof */
        box(x->D, v3(sx > 0 ? x->gi - 0.005f : -x->gi - 0.015f, H - 0.012f, x->zt0), v3(sx > 0 ? x->gi + 0.015f : -x->gi + 0.005f, H + 0.012f, x->zt1),
            UVP(CP_CHROME), CHROME);
        /* quarter-light frame on the door window */
        box(&x->cm->door_glass[k], v3(sx * x->bx - 0.012f, x->s->hood_h, zof(x->s, x->s->ws_base + 0.32f)),
            v3(sx * x->bx + 0.012f, x->s->hood_h + 0.36f, zof(x->s, x->s->ws_base + 0.34f)), UVP(CP_CHROME), CHROME);
    }
    box(x->D, v3(-hw - 0.03f, 0.32f, zf - 0.09f), v3(hw + 0.03f, 0.42f, zf + 0.02f), UVP(CP_CHROME), CHROME);
    box(x->D, v3(-hw - 0.03f, 0.32f, zr - 0.02f), v3(hw + 0.03f, 0.42f, zr + 0.09f), UVP(CP_CHROME), CHROME);
    plate_z(x->S, -0.26f, 0.26f, 0.19f, 0.3f, zf - 0.03f, UVP(CP_PLATE), 0, WHITE);
    plate_z(x->S, -0.26f, 0.26f, 0.53f, 0.64f, zr + 0.02f, UVP(CP_PLATE), 1, WHITE);
    exhaust(x, -1.0f, 0.2f, 0);
    mirrors(x, 0.09f, 0.07f, CHROME, 0);
    wipers(x, 0.9f);
}

static void model_pantera(Ctx *x)
{
    const float zf = x->zf, zr = x->zr, hw = x->hw, H = x->s->height;
    int k;
    box_face(x->D, v3(-0.32f, 0.68f, zf - 0.045f), v3(0.32f, 0.92f, zf + 0.01f), UVP(CP_CHROME), UV2(C2_SLOTS7), 0, WHITE);
    box(x->P, v3(-hw - 0.02f, 0.4f, zf - 0.1f), v3(hw + 0.02f, 0.64f, zf + 0.02f), uvw(), WHITE);
    box(x->D, v3(-hw, 0.3f, zf - 0.08f), v3(hw, 0.395f, zf + 0.02f), UVP(CP_BLACK_PLASTIC), BLACK);
    box(x->P, v3(-hw - 0.02f, 0.4f, zr - 0.02f), v3(hw + 0.02f, 0.62f, zr + 0.1f), uvw(), WHITE);
    box(x->D, v3(-0.04f, 0.36f, zr + 0.06f), v3(0.04f, 0.42f, zr + 0.22f), UVP(CP_DARK), BLACK);              /* tow hitch */
    for (k = 0; k < 2; k++) {
        float sx = k ? 1.0f : -1.0f;
        box_face(x->D, v3(k ? 0.34f : -0.8f, 0.72f, zf - 0.03f), v3(k ? 0.8f : -0.34f, 0.88f, zf + 0.005f), UVP(CP_DARK), UVP(CP_HEAD_RECT), 0, WHITE);
        round_lamp(x->D, sx * 0.62f, 0.5f, zf - 0.1f, 0.05f, 0, UVP(CP_HEAD_ROUND), BLACK);
        plate_z(x->D, sx * 0.78f - 0.07f, sx * 0.78f + 0.07f, 0.55f, 0.61f, zf - 0.103f, UVP(CP_INDICATOR), 0, WHITE);
        box_face(x->D, v3(k ? 0.72f : -0.88f, 0.75f, zr - 0.01f), v3(k ? 0.88f : -0.72f, 1.15f, zr + 0.03f), UVP(CP_DARK), UVP(CP_TAIL), 1, WHITE);
        /* roof rails on risers */
        box(x->D, v3(sx * (x->gi - 0.1f) - 0.025f, H + 0.05f, x->zt0 + 0.25f), v3(sx * (x->gi - 0.1f) + 0.025f, H + 0.08f, x->zt1 - 0.1f),
            UVP(CP_BLACK_PLASTIC), BLACK);
        box_nb(x->D, v3(sx * (x->gi - 0.1f) - 0.015f, H, x->zt0 + 0.26f), v3(sx * (x->gi - 0.1f) + 0.015f, H + 0.046f, x->zt0 + 0.33f), UVP(CP_DARK), BLACK);
        box_nb(x->D, v3(sx * (x->gi - 0.1f) - 0.015f, H, x->zt1 - 0.18f), v3(sx * (x->gi - 0.1f) + 0.015f, H + 0.046f, x->zt1 - 0.11f), UVP(CP_DARK), BLACK);
    }
    box(x->D, v3(-0.02f, x->s->trunk_h + 0.05f, x->zb1 + 0.02f), v3(0.36f, x->s->trunk_h + 0.07f, x->zb1 + 0.05f), UVP(CP_DARK), BLACK);
    plate_z(x->S, -0.26f, 0.26f, 0.43f, 0.54f, zf - 0.105f, UVP(CP_PLATE), 0, WHITE);
    plate_z(x->S, -0.26f, 0.26f, 0.72f, 0.83f, zr + 0.02f, UVP(CP_PLATE), 1, WHITE);
    exhaust(x, -1.0f, 0.32f, 0);
    mirrors(x, 0.15f, 0.11f, WHITE, 1);
    wipers(x, 1.05f);
}

static void model_gelender(Ctx *x)
{
    const CarSpec *s = x->s;
    const float zf = x->zf, zr = x->zr, hw = x->hw, H = s->height;
    int k;
    box_face(x->D, v3(-0.4f, 0.72f, zf - 0.03f), v3(0.4f, 1.0f, zf + 0.01f), UVP(CP_DARK), UVP(CP_GRILLE_BLACK), 0, WHITE);
    box(x->D, v3(-hw - 0.03f, 0.42f, zf - 0.12f), v3(hw + 0.03f, 0.66f, zf + 0.02f), UVP(CP_BLACK_PLASTIC), 0xFF282828u);
    box(x->D, v3(-hw - 0.03f, 0.42f, zr - 0.02f), v3(hw + 0.03f, 0.64f, zr + 0.12f), UVP(CP_BLACK_PLASTIC), 0xFF282828u);
    for (k = 0; k < 2; k++) {
        float sx = k ? 1.0f : -1.0f;
        round_lamp(x->D, sx * 0.62f, 0.86f, zf, 0.11f, 0, UVP(CP_HEAD_ROUND), CHROME);
        /* the indicators sit on top of the front wings */
        box(x->D, v3(sx * 0.77f - 0.07f, s->hood_front_h, zf + 0.02f), v3(sx * 0.77f + 0.07f, s->hood_front_h + 0.06f, zf + 0.17f), uvw(), AMBER);
        round_lamp(x->D, sx * 0.6f, 0.52f, zf - 0.12f, 0.045f, 0, UVP(CP_HEAD_ROUND), BLACK);
        box_face(x->D, v3(k ? 0.66f : -0.86f, 0.7f, zr - 0.01f), v3(k ? 0.86f : -0.66f, 0.95f, zr + 0.03f), UVP(CP_DARK), UVP(CP_TAIL), 1, WHITE);
        /* running boards */
        box(x->D, v3(sx > 0 ? hw - 0.02f : -hw - 0.09f, s->clearance + 0.12f, zof(s, x->arch[0] + x->ar + 0.05f)),
            v3(sx > 0 ? hw + 0.09f : -hw + 0.02f, s->clearance + 0.17f, zof(s, x->arch[1] - x->ar - 0.05f)), UVP(CP_BLACK_PLASTIC), BLACK);
        /* external door hinges and roof gutters */
        box(&x->cm->door_glass[k], v3(sx * (hw + 0.005f) - 0.015f, 0.72f, zof(s, x->dc.f0 + 0.03f)), v3(sx * (hw + 0.005f) + 0.015f, 0.8f, zof(s, x->dc.f0 + 0.1f)),
            UVP(CP_CHROME), 0xFF606060u);
        box(&x->cm->door_glass[k], v3(sx * (hw + 0.005f) - 0.015f, 1.0f, zof(s, x->dc.f0 + 0.03f)), v3(sx * (hw + 0.005f) + 0.015f, 1.08f, zof(s, x->dc.f0 + 0.1f)),
            UVP(CP_CHROME), 0xFF606060u);
        box(x->D, v3(sx > 0 ? x->gi - 0.005f : -x->gi - 0.025f, H - 0.015f, x->zt0), v3(sx > 0 ? x->gi + 0.025f : -x->gi + 0.005f, H + 0.015f, x->zt1),
            UVP(CP_DARK), BLACK);
    }
    /* spare wheel on the back door, under a cover */
    cyl_z(x->D, v3(0.18f, 1.2f, 0), 0.37f, zr + 0.02f, zr + 0.26f, 10, UVP(CP_DARK), UVP(CP_DARK), 0xFF262626u, 2);
    cyl_z(x->D, v3(0.18f, 1.2f, 0), 0.12f, zr + 0.26f, zr + 0.27f, 8, UVP(CP_CHROME), UVP(CP_CHROME), CHROME, 2);
    plate_z(x->S, -0.26f, 0.26f, 0.46f, 0.57f, zf - 0.125f, UVP(CP_PLATE), 0, WHITE);
    plate_z(x->S, -0.26f, 0.26f, 0.46f, 0.57f, zr + 0.125f, UVP(CP_PLATE), 1, WHITE);
    exhaust(x, 1.0f, 0.36f, 1);
    mirrors(x, 0.13f, 0.11f, BLACK, 0);
    wipers(x, 1.0f);
}

static void model_pole(Ctx *x)
{
    const float zf = x->zf, zr = x->zr, hw = x->hw, H = x->s->height;
    int k;
    box_face(x->D, v3(-0.4f, 0.66f, zf - 0.03f), v3(0.4f, 0.88f, zf + 0.01f), UVP(CP_DARK), UV2(C2_MOSK), 0, WHITE);
    box(x->D, v3(-hw - 0.02f, 0.4f, zf - 0.08f), v3(hw + 0.02f, 0.56f, zf + 0.02f), UVP(CP_BLACK_PLASTIC), 0xFF2C2C2Cu);
    box(x->D, v3(-hw - 0.02f, 0.4f, zr - 0.02f), v3(hw + 0.02f, 0.56f, zr + 0.08f), UVP(CP_BLACK_PLASTIC), 0xFF2C2C2Cu);
    for (k = 0; k < 2; k++) {
        float sx = k ? 1.0f : -1.0f;
        box(x->D, v3(k ? 0.42f : -0.74f, 0.62f, zf - 0.035f), v3(k ? 0.74f : -0.42f, 0.9f, zf + 0.005f), UVP(CP_CHROME), CHROME);
        round_lamp(x->D, sx * 0.58f, 0.76f, zf - 0.035f, 0.11f, 0, UVP(CP_HEAD_ROUND), 0xFF505050u);
        box_face(x->D, v3(k ? 0.62f : -0.8f, 0.7f, zr - 0.01f), v3(k ? 0.8f : -0.62f, 0.95f, zr + 0.03f), UVP(CP_DARK), UVP(CP_TAIL), 1, WHITE);
        /* mud flaps behind the wheels */
        box(x->D, v3(sx > 0 ? hw - 0.21f : -hw + 0.01f, x->s->clearance - 0.1f, zof(x->s, x->arch[1] + x->ar) + 0.01f),
            v3(sx > 0 ? hw - 0.01f : -hw + 0.21f, x->s->clearance + 0.25f, zof(x->s, x->arch[1] + x->ar) + 0.02f), UVP(CP_DARK), BLACK);
    }
    /* roof rack */
    for (k = 0; k < 2; k++) {
        float sx = k ? 1.0f : -1.0f;
        box(x->D, v3(sx * (x->gi - 0.08f) - 0.02f, H + 0.08f, x->zt0 + 0.2f), v3(sx * (x->gi - 0.08f) + 0.02f, H + 0.11f, x->zt1 - 0.15f), UVP(CP_DARK), BLACK);
        box_nb(x->D, v3(sx * (x->gi - 0.08f) - 0.012f, H, x->zt0 + 0.22f), v3(sx * (x->gi - 0.08f) + 0.012f, H + 0.075f, x->zt0 + 0.27f), UVP(CP_DARK), BLACK);
        box_nb(x->D, v3(sx * (x->gi - 0.08f) - 0.012f, H, x->zt1 - 0.22f), v3(sx * (x->gi - 0.08f) + 0.012f, H + 0.075f, x->zt1 - 0.17f), UVP(CP_DARK), BLACK);
    }
    for (k = 0; k < 4; k++) {
        float z = x->zt0 + 0.2f + (x->zt1 - x->zt0 - 0.35f) * k / 3.0f;
        box(x->D, v3(-x->gi + 0.07f, H + 0.07f, z - 0.02f), v3(x->gi - 0.07f, H + 0.1f, z + 0.02f), UVP(CP_DARK), BLACK);
    }
    plate_z(x->S, -0.26f, 0.26f, 0.42f, 0.53f, zf - 0.085f, UVP(CP_PLATE), 0, WHITE);
    plate_z(x->S, -0.26f, 0.26f, 0.62f, 0.73f, zr + 0.02f, UVP(CP_PLATE), 1, WHITE);
    exhaust(x, -1.0f, 0.3f, 0);
    mirrors(x, 0.1f, 0.08f, BLACK, 0);
    wipers(x, 0.95f);
}

static void model_bobik(Ctx *x)
{
    const CarSpec *s = x->s;
    const float zf = x->zf, zr = x->zr, hw = x->hw, H = s->height;
    int k;
    box_face(x->P, v3(-0.42f, 0.7f, zf - 0.02f), v3(0.42f, 1.02f, zf + 0.01f), uvw(), UV2(C2_UAZ), 0, WHITE);      /* slatted panel */
    box(x->D, v3(-hw - 0.01f, 0.55f, zf - 0.1f), v3(hw + 0.01f, 0.66f, zf + 0.02f), UVP(CP_DARK), 0xFF2E342Eu);    /* steel bumpers */
    box(x->D, v3(-hw - 0.01f, 0.55f, zr - 0.02f), v3(hw + 0.01f, 0.66f, zr + 0.1f), UVP(CP_DARK), 0xFF2E342Eu);
    for (k = 0; k < 2; k++) {
        float sx = k ? 1.0f : -1.0f;
        round_lamp(x->D, sx * 0.71f, 0.88f, zf, 0.1f, 0, UVP(CP_HEAD_ROUND), 0xFF404040u);
        /* the wings stand out over the front wheels */
        box(x->P, v3(sx > 0 ? hw - 0.06f : -hw - 0.07f, 0.84f, zf + 0.04f), v3(sx > 0 ? hw + 0.07f : -hw + 0.06f, 0.95f, zof(s, x->arch[0] + x->ar + 0.02f)),
            uvw(), WHITE);
        box(x->P, v3(sx > 0 ? hw - 0.06f : -hw - 0.07f, 0.84f, zof(s, x->arch[1] - x->ar - 0.02f)), v3(sx > 0 ? hw + 0.07f : -hw + 0.06f, 0.95f, zr - 0.06f),
            uvw(), WHITE);
        box(x->D, v3(sx * 0.6f - 0.04f, 0.5f, zf - 0.14f), v3(sx * 0.6f + 0.04f, 0.56f, zf - 0.08f), UVP(CP_DARK), 0xFF2020A0u);  /* tow hooks */
        round_lamp(x->D, sx * 0.78f, 0.78f, zr + 0.01f, 0.05f, 1, UVP(CP_TAIL), BLACK);
        plate_z(x->D, sx * 0.71f - 0.05f, sx * 0.71f + 0.05f, 0.7f, 0.75f, zf - 0.005f, UVP(CP_INDICATOR), 0, WHITE);
    }
    /* windscreen frame */
    box(x->P, v3(-x->bx - 0.02f, s->hood_h, x->zb0 - 0.03f), v3(x->bx + 0.02f, s->hood_h + 0.05f, x->zb0 + 0.03f), uvw(), WHITE);
    box(x->P, v3(-x->gi - 0.02f, H - 0.05f, x->zt0 - 0.03f), v3(x->gi + 0.02f, H + 0.01f, x->zt0 + 0.03f), uvw(), WHITE);
    /* spare wheel and jerrycan on the back */
    cyl_z(x->D, v3(0.4f, 1.08f, 0), s->wheel_r, zr + 0.02f, zr + 0.25f, 10, UVP(CP_DARK), UV2(C2_RIM_TRUCK), 0xFF262626u, 2);
    box(x->D, v3(-0.68f, 0.72f, zr + 0.01f), v3(-0.44f, 1.14f, zr + 0.16f), UVP(CP_DARK), 0xFF3A5A3Au);
    beacon_bar(x, x->zt0 + 0.25f, 0.5f, UV2(C2_BEACON_BLUE), UV2(C2_BEACON_BLUE), 0xFFFF6020u, 0xFFFF6020u);
    door_signs(x, s->clearance + 0.53f, s->clearance + 0.65f, uv_sub(T_LIVERY, LV_POLICE));
    plate_z(x->D, -0.42f, 0.42f, 1.02f, 1.1f, zf - 0.025f, uv_sub(T_LIVERY, LV_POLICE), 0, WHITE);
    plate_z(x->S, -0.26f, 0.26f, 0.42f, 0.53f, zf - 0.02f, UVP(CP_PLATE), 0, WHITE);
    plate_z(x->S, -0.26f, 0.26f, 0.7f, 0.81f, zr + 0.02f, UVP(CP_PLATE), 1, WHITE);
    exhaust(x, 1.0f, 0.36f, 0);
    mirrors(x, 0.11f, 0.11f, BLACK, 0);
    wipers(x, 1.0f);
}

static void model_bukhanka(Ctx *x)
{
    const CarSpec *s = x->s;
    const float zf = x->zf, zr = x->zr, hw = x->hw, H = s->height;
    int k;
    box_face(x->P, v3(-0.25f, 0.68f, zf - 0.02f), v3(0.25f, 0.98f, zf + 0.01f), uvw(), UV2(C2_UAZ), 0, WHITE);
    box(x->D, v3(-hw - 0.01f, 0.44f, zf - 0.1f), v3(hw + 0.01f, 0.55f, zf + 0.02f), UVP(CP_DARK), 0xFF303030u);
    box(x->D, v3(-hw - 0.01f, 0.44f, zr - 0.02f), v3(hw + 0.01f, 0.55f, zr + 0.1f), UVP(CP_DARK), 0xFF303030u);
    for (k = 0; k < 2; k++) {
        float sx = k ? 1.0f : -1.0f;
        round_lamp(x->D, sx * 0.7f, 0.85f, zf, 0.1f, 0, UVP(CP_HEAD_ROUND), 0xFF505050u);
        plate_z(x->D, sx * 0.7f - 0.06f, sx * 0.7f + 0.06f, 0.66f, 0.72f, zf - 0.005f, UVP(CP_INDICATOR), 0, WHITE);
        box_face(x->D, v3(k ? 0.76f : -0.9f, 0.62f, zr - 0.01f), v3(k ? 0.9f : -0.76f, 0.9f, zr + 0.03f), UVP(CP_DARK), UVP(CP_TAIL), 1, WHITE);
        /* red cross on both sides of the back, signs on the cab doors */
        plate_x(x->D, sx * (hw + 0.025f), zof(s, 3.0f), zof(s, 3.55f), s->hood_h - 0.02f + 0.18f, s->hood_h + 0.71f, uv_sub(T_LIVERY, LV_CROSS), WHITE);
    }
    plate_z(x->D, -0.012f, 0.012f, 0.56f, 1.95f, zr + 0.012f, UVP(CP_DARK), 1, BLACK);         /* split of the back doors */
    plate_z(x->D, -0.45f, 0.45f, 1.0f, 1.13f, zr + 0.025f, uv_sub(T_LIVERY, LV_AMBULANCE), 1, WHITE);
    door_signs(x, s->hood_h - 0.48f, s->hood_h - 0.3f, uv_sub(T_LIVERY, LV_03));
    cyl_y(x->D, v3(0, 0, x->zt0 + 0.25f), 0.1f, H + 0.02f, H + 0.2f, 8, UV2(C2_BEACON_BLUE), WHITE);
    box_nb(x->D, v3(-0.14f, H, x->zt0 + 0.11f), v3(0.14f, H + 0.015f, x->zt0 + 0.39f), UVP(CP_DARK), 0xFF303030u);
    add_beacon(x->cm, v3(0, H + 0.12f, x->zt0 + 0.25f), 0xFFFF6020u);
    plate_z(x->S, -0.26f, 0.26f, 0.3f, 0.41f, zf - 0.02f, UVP(CP_PLATE), 0, WHITE);
    plate_z(x->S, -0.26f, 0.26f, 0.58f, 0.69f, zr + 0.02f, UVP(CP_PLATE), 1, WHITE);
    exhaust(x, 1.0f, 0.3f, 0);
    mirrors(x, 0.12f, 0.14f, BLACK, 0);
    wipers(x, 1.05f);
}

static void model_taiga(Ctx *x)
{
    const CarSpec *s = x->s;
    const float zf = x->zf, zr = x->zr, hw = x->hw;
    const float b0 = zof(s, 3.3f), b1 = zof(s, 7.28f), by0 = s->trunk_h, by1 = 2.82f;
    int k, i;
    box_face(x->D, v3(-0.5f, 0.95f, zf - 0.03f), v3(0.5f, 1.5f, zf + 0.01f), UVP(CP_DARK), UV2(C2_TRUCK), 0, WHITE);
    box(x->D, v3(-hw - 0.02f, 0.8f, zf - 0.16f), v3(hw + 0.02f, 0.98f, zf + 0.02f), UVP(CP_DARK), 0xFF262626u);
    /* the fire body: lockers behind roller shutters, a ladder on top, the hose reel at the back */
    box_nb(x->P, v3(-hw + 0.05f, by0, b0), v3(hw - 0.05f, by1, b1), uvw(), WHITE);
    for (k = 0; k < 2; k++) {
        float sx = k ? 1.0f : -1.0f, xo = sx * (hw - 0.05f);
        round_lamp(x->D, sx * 0.95f, 1.2f, zf, 0.12f, 0, UVP(CP_HEAD_ROUND), 0xFF505050u);
        box(x->D, v3(sx * 0.62f - 0.05f, 0.74f, zf - 0.22f), v3(sx * 0.62f + 0.05f, 0.82f, zf - 0.14f), UVP(CP_DARK), 0xFF2020B0u);
        for (i = 0; i < 3; i++) {
            float z0 = zof(s, 3.45f + i * 1.28f), z1 = zof(s, 4.6f + i * 1.28f);
            box_side(x->D, v3(sx > 0 ? xo + 0.004f : xo - 0.034f, by0 + 0.14f, z0), v3(sx > 0 ? xo + 0.034f : xo - 0.004f, by1 - 0.22f, z1),
                     uvw(), uv_tile(T_METAL), sx > 0, 0xFFD8D8D8u);
        }
        plate_x(x->D, sx * (hw - 0.02f), zof(s, 3.6f), zof(s, 5.6f), by1 - 0.2f, by1 - 0.04f, uv_sub(T_LIVERY, LV_FIRE), WHITE);
        box(x->D, v3(sx * 0.45f - 0.03f, by1 + 0.03f, b0 + 0.1f), v3(sx * 0.45f + 0.03f, by1 + 0.08f, b1 + 0.05f), UVP(CP_CHROME), 0xFFC8C8C8u);
        box_face(x->D, v3(k ? 0.85f : -1.1f, 0.95f, zr - 0.01f), v3(k ? 1.1f : -0.85f, 1.15f, zr + 0.03f), UVP(CP_DARK), UVP(CP_TAIL), 1, WHITE);
        box_nb(x->D, v3(sx * 0.4f - 0.03f, by1, b0 + 0.15f), v3(sx * 0.4f + 0.03f, by1 + 0.026f, b0 + 0.25f), UVP(CP_DARK), BLACK);
        box_nb(x->D, v3(sx * 0.4f - 0.03f, by1, b1 - 0.25f), v3(sx * 0.4f + 0.03f, by1 + 0.026f, b1 - 0.15f), UVP(CP_DARK), BLACK);
    }
    for (i = 0; i < 12; i++) {
        float z = b0 + 0.2f + i * (b1 - b0 - 0.3f) / 11.0f;
        box(x->S, v3(-0.45f, by1 + 0.04f, z - 0.015f), v3(0.45f, by1 + 0.07f, z + 0.015f), UVP(CP_CHROME), 0xFFC0C0C0u);
    }
    cyl_x(x->D, v3(0, 2.15f, zr + 0.2f), 0.26f, -0.55f, 0.55f, 10, uvw(), uvw(), 0xFF404050u, 0xFF2828B8u);      /* hose reel */
    cyl_x(x->D, v3(0, 2.15f, zr + 0.2f), 0.06f, -0.62f, 0.62f, 6, uvw(), uvw(), 0xFF808080u, 0xFF808080u);
    beacon_bar(x, x->zt0 + 0.2f, 0.62f, UV2(C2_BEACON_BLUE), UV2(C2_BEACON_RED), 0xFFFF6020u, 0xFF2020FFu);
    door_signs(x, 1.85f, 2.05f, uv_sub(T_LIVERY, LV_01));
    plate_z(x->S, -0.26f, 0.26f, 0.8f, 0.91f, zf - 0.165f, UVP(CP_PLATE), 0, WHITE);
    plate_z(x->S, -0.26f, 0.26f, 0.9f, 1.01f, zr + 0.02f, UVP(CP_PLATE), 1, WHITE);
    mirrors(x, 0.16f, 0.24f, BLACK, 0);
    wipers(x, 1.25f);
}

static void model_stolytsia(Ctx *x)
{
    const float zf = x->zf, zr = x->zr, hw = x->hw, H = x->s->height;
    UVRect u[6];
    Xf id = xf_identity();
    int k;
    box_face(x->D, v3(-0.44f, 0.5f, zf - 0.03f), v3(0.44f, 0.68f, zf + 0.01f), UVP(CP_CHROME), UV2(C2_MOSK), 0, WHITE);
    box(x->D, v3(-hw - 0.03f, 0.3f, zf - 0.09f), v3(hw + 0.03f, 0.4f, zf + 0.02f), UVP(CP_CHROME), CHROME);
    box(x->D, v3(-hw - 0.03f, 0.3f, zr - 0.02f), v3(hw + 0.03f, 0.4f, zr + 0.09f), UVP(CP_CHROME), CHROME);
    for (k = 0; k < 2; k++) {
        float sx = k ? 1.0f : -1.0f;
        plate_z(x->D, k ? 0.46f : -0.7f, k ? 0.7f : -0.46f, 0.52f, 0.66f, zf - 0.035f, UVP(CP_HEAD_RECT), 0, WHITE);
        box(x->D, v3(sx * 0.3f - 0.035f, 0.27f, zf - 0.11f), v3(sx * 0.3f + 0.035f, 0.43f, zf - 0.07f), UVP(CP_BLACK_PLASTIC), BLACK);
        plate_z(x->D, sx * 0.55f - 0.06f, sx * 0.55f + 0.06f, 0.22f, 0.28f, zf - 0.03f, UVP(CP_INDICATOR), 0, WHITE);
        box_face(x->D, v3(k ? 0.3f : -0.72f, 0.56f, zr - 0.01f), v3(k ? 0.72f : -0.3f, 0.7f, zr + 0.03f), UVP(CP_DARK), UVP(CP_TAIL), 1, WHITE);
        box(x->D, v3(sx > 0 ? x->gi - 0.005f : -x->gi - 0.015f, H - 0.012f, x->zt0), v3(sx > 0 ? x->gi + 0.015f : -x->gi + 0.005f, H + 0.012f, x->zt1),
            UVP(CP_CHROME), CHROME);
    }
    /* the taxi sign on the roof, with the green "free" lamp */
    for (k = 0; k < 6; k++) u[k] = uv_sub(T_LIVERY, 2, 22, 4, 4);
    u[4] = u[5] = uv_sub(T_LIVERY, LV_TAXI);
    gm_box_xf(x->D, &id, v3(-0.28f, H + 0.03f, x->zt0 + 0.35f), v3(0.28f, H + 0.19f, x->zt0 + 0.47f), u, WHITE, BOX_NO_BOTTOM);
    box_nb(x->D, v3(-0.2f, H, x->zt0 + 0.38f), v3(0.2f, H + 0.025f, x->zt0 + 0.44f), UVP(CP_DARK), BLACK);
    box_nb(x->D, v3(0.13f, H + 0.19f, x->zt0 + 0.38f), v3(0.23f, H + 0.25f, x->zt0 + 0.44f), uvw(), 0xFF40E040u);
    add_beacon(x->cm, v3(0.18f, H + 0.22f, x->zt0 + 0.41f), 0xFF40FF40u);
    plate_z(x->S, -0.26f, 0.26f, 0.18f, 0.29f, zf - 0.03f, UVP(CP_PLATE), 0, WHITE);
    plate_z(x->S, -0.26f, 0.26f, 0.57f, 0.68f, zr + 0.02f, UVP(CP_PLATE), 1, WHITE);
    exhaust(x, -1.0f, 0.2f, 0);
    mirrors(x, 0.09f, 0.07f, CHROME, 0);
    wipers(x, 0.9f);
}

/* ----------------------------------------------------------------- cabins */

typedef struct {
    UVRect seat_uv;
    unsigned int seat, dash, trim, carpet;
    int headrests, wood, rear_bench, shelf, headlining, meter;
} Cabin;

static void cabin_style(int t, Cabin *c)
{
    memset(c, 0, sizeof(*c));
    c->seat_uv = uv_sub(T_CAR_INTERIOR, IN_VELOUR);
    c->seat = 0xFF7C7874u;
    c->dash = 0xFF262626u;
    c->trim = 0xFF706C68u;
    c->carpet = 0xFF606060u;
    c->rear_bench = c->shelf = c->headlining = 1;
    switch (t) {
    case CAR_KAISER:   c->seat_uv = uv_sub(T_CAR_INTERIOR, IN_LEATHER); c->seat = 0xFF8CB0C8u; c->trim = 0xFF7898B0u; c->headrests = c->wood = 1; break;
    case CAR_LASTIVKA: c->seat = 0xFF6C7078u; break;
    case CAR_KLASYKA:  c->seat_uv = uv_sub(T_CAR_INTERIOR, IN_VINYL); c->seat = 0xFF38408Cu; c->trim = 0xFF303878u; break;
    case CAR_PANTERA:  c->seat_uv = uv_sub(T_CAR_INTERIOR, IN_LEATHER); c->seat = 0xFFA0A4A8u; c->headrests = c->wood = 1; c->shelf = 0; break;
    case CAR_GELENDER: c->seat_uv = uv_sub(T_CAR_INTERIOR, IN_LEATHER); c->seat = 0xFF383634u; c->trim = 0xFF403C38u; c->headrests = c->wood = 1; c->shelf = 0; break;
    case CAR_POLE:     c->seat = 0xFF5C6870u; c->shelf = 0; break;
    case CAR_BOBIK:    c->seat_uv = uv_sub(T_CAR_INTERIOR, IN_VINYL); c->seat = 0xFF3A403Au; c->dash = 0xFF34C4E4u; c->trim = 0xFF3C4440u;
                       c->shelf = c->headlining = 0; break;
    case CAR_BUKHANKA: c->seat_uv = uv_sub(T_CAR_INTERIOR, IN_VINYL); c->seat = 0xFF404040u; c->dash = 0xFF505050u; c->trim = 0xFFC8D0D0u;
                       c->rear_bench = c->shelf = 0; break;
    case CAR_TAIGA:    c->seat_uv = uv_sub(T_CAR_INTERIOR, IN_VINYL); c->seat = 0xFF303434u; c->dash = 0xFF2424A0u; c->trim = 0xFF303434u;
                       c->rear_bench = c->shelf = 0; break;
    case CAR_STOLYTSIA: c->seat_uv = uv_sub(T_CAR_INTERIOR, IN_VINYL); c->seat = 0xFF3038A0u; c->trim = 0xFF30348Cu; c->meter = 1; break;
    default: break;
    }
}

static void build_cabin(Ctx *x, int t)
{
    const CarSpec *s = x->s;
    CarModel *cm = x->cm;
    const CarLayout *L = &cm->lay;
    Cabin st;
    GMesh *C = &cm->cabin;
    const float hw = x->hw, xw = hw - 0.07f, yf = L->floor_y, belt = s->hood_h - 0.02f;
    float zf = zof(s, s->ws_base) - 0.05f, seat = zof(s, L->seat_d);
    float zrear = zof(s, s->body == BODY_TRUCK ? s->rear_glass_base - 0.06f : (s->body == BODY_VAN ? s->length - 0.25f : s->rear_glass_base + 0.15f));
    float rear_arch = zof(s, x->arch[x->narch - 1] - x->ar - 0.04f), dash_end, ys = yf + 0.3f;
    UVRect W = uvw(), carpet = uv_sub(T_CAR_INTERIOR, IN_CARPET), plastic = UVP(CP_BLACK_PLASTIC);
    int k;

    cabin_style(t, &st);
    if (zrear > rear_arch && s->body != BODY_VAN) zrear = rear_arch;
    /* floor */
    gm_quad_flat(C, v3(-xw, yf, zrear), v3(-xw, yf, zf), v3(xw, yf, zf), v3(xw, yf, zrear), carpet, st.carpet);
    /* dashboard: body, top, instrument cluster in front of the driver, console in the middle */
    dash_end = fminf(seat - 0.6f, zf + 0.45f);
    if (dash_end < zf + 0.1f) dash_end = zf + 0.1f;
    box(C, v3(-xw + 0.02f, yf + 0.2f, zf), v3(xw - 0.02f, belt, dash_end), plastic, st.dash);
    plate_z(C, L->seat_x - 0.17f, L->seat_x + 0.17f, belt - 0.16f, belt - 0.03f, dash_end + 0.003f, uv_sub(T_CAR_INTERIOR, IN_DASH), 1, WHITE);
    plate_z(C, -0.16f, 0.16f, belt - 0.22f, belt - 0.06f, dash_end + 0.003f, uv_sub(T_CAR_INTERIOR, IN_CONSOLE), 1, WHITE);
    if (st.wood) plate_z(C, 0.2f, xw - 0.06f, belt - 0.1f, belt - 0.06f, dash_end + 0.003f, uv_sub(T_CAR_INTERIOR, IN_WOOD), 1, WHITE);
    if (st.meter) box_nb(C, v3(0.1f, belt, dash_end - 0.12f), v3(0.24f, belt + 0.09f, dash_end - 0.02f), plastic, 0xFF202020u);   /* taxi meter */
    /* steering wheel: column, hub and an eight-piece rim, tilted towards the driver */
    {
        Xf t0 = xf_translate(v3(L->seat_x, belt - 0.02f, dash_end + 0.14f)), r0 = xf_rot_x(-0.45f), w0 = xf_mul(&t0, &r0);
        float R = s->body == BODY_TRUCK ? 0.25f : 0.19f;
        int i;
        box_xf(C, &w0, v3(-0.025f, -0.025f, -0.18f), v3(0.025f, 0.025f, 0.0f), plastic, 0xFF202020u);
        box_xf(C, &w0, v3(-0.05f, -0.04f, -0.01f), v3(0.05f, 0.04f, 0.02f), plastic, 0xFF202020u);
        for (i = 0; i < 8; i++) {
            float a = i * PI_F / 4.0f;
            Xf rz = xf_rot_z(a), seg = xf_mul(&w0, &rz);
            box_xf(C, &seg, v3(-R * 0.42f, R - 0.015f, -0.015f), v3(R * 0.42f, R + 0.015f, 0.015f), plastic, 0xFF181818u);
        }
        box_xf(C, &w0, v3(-R, -0.012f, -0.01f), v3(R, 0.012f, 0.01f), plastic, 0xFF202020u);    /* spokes */
    }
    /* pedals, gear lever, handbrake */
    for (k = 0; k < 3; k++)
        box(C, v3(L->seat_x - 0.13f + k * 0.11f, yf + 0.05f, dash_end + 0.02f), v3(L->seat_x - 0.08f + k * 0.11f, yf + 0.15f, dash_end + 0.05f),
            plastic, 0xFF303030u);
    box_nb(C, v3(-0.1f, yf, dash_end), v3(0.1f, yf + 0.22f, seat + 0.1f), plastic, st.dash);                   /* tunnel and console */
    box_nb(C, v3(-0.012f, yf + 0.22f, seat - 0.35f), v3(0.012f, yf + 0.5f, seat - 0.33f), W, 0xFF303030u);       /* gear lever */
    box(C, v3(-0.03f, yf + 0.48f, seat - 0.37f), v3(0.03f, yf + 0.54f, seat - 0.31f), plastic, 0xFF101010u);
    box_nb(C, v3(-0.03f, yf + 0.22f, seat - 0.1f), v3(0.03f, yf + 0.28f, seat + 0.12f), plastic, 0xFF202020u);    /* handbrake */
    /* front seats: cushion, reclined back, headrest */
    for (k = 0; k < 2; k++) {
        float sx = k ? 1.0f : -1.0f, cx = sx * fabsf(L->seat_x), w = fminf(s->body == BODY_TRUCK ? 0.28f : 0.24f, fabsf(L->seat_x) - 0.125f);
        Xf t0 = xf_translate(v3(cx, ys, seat + 0.24f)), r0 = xf_rot_x(-0.22f), back = xf_mul(&t0, &r0);
        UVRect u[6];
        int i;
        for (i = 0; i < 6; i++) u[i] = st.seat_uv;
        gm_box(C, v3(cx - w, yf + 0.06f, seat - 0.26f), v3(cx + w, ys, seat + 0.24f), st.seat_uv, st.seat_uv, st.seat, BOX_ALL & ~8);
        gm_box_xf(C, &back, v3(-w, 0.0f, 0.0f), v3(w, 0.62f, 0.12f), u, gfx_mul(st.seat, 0.92f), BOX_ALL);
        if (st.headrests) gm_box_xf(C, &back, v3(-w * 0.55f, 0.66f, 0.01f), v3(w * 0.55f, 0.84f, 0.11f), u, st.seat, BOX_ALL);
        box_nb(C, v3(cx - w + 0.03f, yf, seat - 0.2f), v3(cx + w - 0.03f, yf + 0.06f, seat + 0.2f), W, 0xFF282828u);   /* runners */
    }
    /* back seat and parcel shelf */
    if (st.rear_bench && zrear - (seat + 0.55f) > 0.45f) {
        float zb = zrear - 0.08f, zc = zb - 0.5f;
        gm_box(C, v3(-xw + 0.03f, yf + 0.04f, zc), v3(xw - 0.03f, ys - 0.02f, zb - 0.12f), st.seat_uv, st.seat_uv, st.seat, BOX_ALL & ~8);
        gm_box(C, v3(-xw + 0.03f, yf + 0.04f, zb - 0.14f), v3(xw - 0.03f, belt + 0.12f, zb), st.seat_uv, st.seat_uv, gfx_mul(st.seat, 0.92f), BOX_ALL & ~8);
        if (st.shelf) box(C, v3(-xw, belt + 0.12f, zb), v3(xw, belt + 0.15f, zrear + 0.25f), carpet, 0xFF303030u);
    } else if (t == CAR_BUKHANKA) {
        /* the ambulance back: a stretcher and a cabinet */
        box(C, v3(0.1f, yf + 0.3f, seat + 0.7f), v3(0.65f, yf + 0.38f, zrear - 0.1f), W, 0xFFE8E8E8u);
        box_nb(C, v3(0.12f, yf, seat + 0.75f), v3(0.18f, yf + 0.295f, seat + 0.85f), W, 0xFF808080u);
        box_nb(C, v3(0.57f, yf, zrear - 0.3f), v3(0.63f, yf + 0.295f, zrear - 0.2f), W, 0xFF808080u);
        box_nb(C, v3(-xw + 0.02f, yf, seat + 0.8f), v3(-0.3f, s->height - 0.2f, seat + 1.4f), W, 0xFFD8DCDCu);
        box(C, v3(-xw + 0.04f, yf + 0.9f, seat + 0.82f), v3(-0.32f, yf + 0.95f, seat + 1.38f), plastic, 0xFF404040u);
    }
    /* headlining just under the roof, rear-view mirror, sun visors */
    if (st.headlining) {
        float yc = s->height - 0.03f;
        gm_quad_flat(C, v3(-x->gi + 0.03f, yc, x->zt0 + 0.02f), v3(-x->gi + 0.03f, yc, x->zt1 - 0.02f), v3(x->gi - 0.03f, yc, x->zt1 - 0.02f),
                     v3(x->gi - 0.03f, yc, x->zt0 + 0.02f), uv_sub(T_CAR_INTERIOR, IN_VELOUR), 0xFF9AA0A4u);
        for (k = 0; k < 2; k++) {
            float sx = k ? 1.0f : -1.0f;
            box(C, v3(sx * 0.42f - 0.17f, yc - 0.025f, x->zt0 + 0.04f), v3(sx * 0.42f + 0.17f, yc - 0.006f, x->zt0 + 0.22f), W, 0xFF8C9094u);
        }
    }
    box(C, v3(-0.1f, s->height - 0.12f, x->zt0 + 0.04f), v3(0.1f, s->height - 0.06f, x->zt0 + 0.06f), plastic, 0xFF202020u);
    /* inner side walls: by the front door, and behind it */
    for (k = 0; k < 2; k++) {
        float sx = k ? 1.0f : -1.0f, xi = sx * xw, zd = fminf(zof(s, x->dc.f1), zrear);
        if (zrear - zd < 0.05f) {
            /* no room behind the door (three-door): only the wall by it */
            if (k) gm_quad_flat(&cm->wall_front[k], v3(xi, yf, zd), v3(xi, belt, zd), v3(xi, belt, zf), v3(xi, yf, zf), carpet, st.trim);
            else gm_quad_flat(&cm->wall_front[k], v3(xi, yf, zf), v3(xi, belt, zf), v3(xi, belt, zd), v3(xi, yf, zd), carpet, st.trim);
        } else if (k) {
            gm_quad_flat(&cm->wall_front[k], v3(xi, yf, zd), v3(xi, belt, zd), v3(xi, belt, zf), v3(xi, yf, zf), carpet, st.trim);
            gm_quad_flat(&cm->wall_rear[k], v3(xi, yf, zrear), v3(xi, belt, zrear), v3(xi, belt, zd), v3(xi, yf, zd), uv_sub(T_CAR_INTERIOR, IN_DOOR), st.trim);
        } else {
            gm_quad_flat(&cm->wall_front[k], v3(xi, yf, zf), v3(xi, belt, zf), v3(xi, belt, zd), v3(xi, yf, zd), carpet, st.trim);
            gm_quad_flat(&cm->wall_rear[k], v3(xi, yf, zd), v3(xi, belt, zd), v3(xi, belt, zrear), v3(xi, yf, zrear), uv_sub(T_CAR_INTERIOR, IN_DOOR), st.trim);
        }
    }
    /* door cards: trim with armrest and handle, and the edge */
    for (k = 0; k < 2; k++) {
        float sx = k ? 1.0f : -1.0f, xo = sx * hw, xi = sx * (hw - 0.05f);
        float z0 = zof(s, x->dc.f0), z1 = zof(s, x->dc.f1), b0 = hood_at(x, x->dc.f0), b1 = s->body == BODY_TRUCK ? s->hood_h : hood_at(x, x->dc.f1);
        UVRect door = uv_sub(T_CAR_INTERIOR, IN_DOOR);
        gm_quad_flat(&cm->door_trim[k], v3(xi, x->dc.sill, z0), v3(xi, b0, z0), v3(xi, b1, z1), v3(xi, x->dc.sill, z1), door, st.trim);
        gm_quad_flat(&cm->door_trim[k], v3(xo, x->dc.sill, z1), v3(xo, b1, z1), v3(xi, b1, z1), v3(xi, x->dc.sill, z1), W, 0xFF505054u);
        gm_quad_flat(&cm->door_trim[k], v3(xo, b0, z0), v3(xo, b1, z1), v3(xi, b1, z1), v3(xi, b0, z0), W, 0xFF505054u);
    }
}

/* ------------------------------------------------------------ motorcycle */

static void build_bike(CarModel *cm, const CarSpec *s)
{
    GMesh *P = &cm->paint, *D = &cm->detail, *F = &cm->fork;
    const float zf = zof(s, 0.0f), wr = s->wheel_r, a0 = zof(s, 0.36f), a1 = zof(s, 0.36f + s->wheelbase);
    UVRect W = uvw(), chrome = UVP(CP_CHROME), dark = UVP(CP_DARK), seat = uv_sub(T_CAR_INTERIOR, IN_VINYL);
    int k;
    cm->naxles = 2;
    cm->axle_z[0] = a0;
    cm->axle_z[1] = a1;
    cm->fork_z = zf + 0.5f;
    /* tank with its chrome flash, seat, side covers, rear mudguard */
    box_side(P, v3(-0.13f, 0.7f, zf + 0.62f), v3(0.13f, 0.9f, zf + 1.02f), W, UV2(C2_TANK), 1, WHITE);
    box_side(P, v3(-0.13f, 0.7f, zf + 0.62f), v3(-0.129f, 0.9f, zf + 1.02f), W, UV2(C2_TANK), 0, WHITE);
    box(D, v3(-0.12f, 0.78f, zf + 1.0f), v3(0.12f, 0.86f, zf + 1.6f), seat, 0xFF202020u);
    box(D, v3(-0.13f, 0.86f, zf + 1.58f), v3(0.13f, 0.88f, zf + 1.62f), chrome, CHROME);    /* grab rail */
    box(P, v3(-0.14f, 0.56f, zf + 1.05f), v3(0.14f, 0.76f, zf + 1.4f), W, WHITE);
    box(P, v3(-0.08f, 0.6f, a1 - 0.25f), v3(0.08f, 0.64f, a1 + 0.38f), W, WHITE);
    box(D, v3(-0.06f, 0.66f, a1 + 0.36f), v3(0.06f, 0.74f, a1 + 0.4f), dark, 0xFF2020C0u);  /* tail lamp */
    plate_z(D, -0.1f, 0.1f, 0.5f, 0.6f, a1 + 0.41f, UVP(CP_PLATE), 1, WHITE);
    /* frame: down tube, top tube, swing arm */
    {
        Xf t0 = xf_translate(v3(0, 0.9f, zf + 0.55f)), r0 = xf_rot_x(-0.55f), down = xf_mul(&t0, &r0);
        Xf t1 = xf_translate(v3(0, 0.42f, zf + 1.2f)), r1 = xf_rot_x(0.25f), arm = xf_mul(&t1, &r1);
        box_xf(D, &down, v3(-0.025f, -0.6f, -0.025f), v3(0.025f, 0.0f, 0.025f), dark, BLACK);
        box(D, v3(-0.025f, 0.82f, zf + 0.55f), v3(0.025f, 0.86f, zf + 1.3f), dark, BLACK);
        for (k = 0; k < 2; k++) {
            float sx = k ? 1.0f : -1.0f;
            box_xf(D, &arm, v3(sx * 0.1f - 0.02f, -0.02f, 0.0f), v3(sx * 0.1f + 0.02f, 0.02f, 0.55f), dark, BLACK);
            /* rear shocks with chrome springs */
            box(D, v3(sx * 0.12f - 0.022f, 0.33f, a1 - 0.08f), v3(sx * 0.12f + 0.022f, 0.74f, a1 - 0.03f), chrome, CHROME);
            /* twin exhausts */
            cyl_z(D, v3(sx * 0.16f, 0.3f, 0), 0.04f, zf + 0.7f, a1 + 0.42f, 6, chrome, dark, CHROME, 2);
            /* footpegs */
            box(D, v3(sx > 0 ? 0.12f : -0.24f, 0.3f, zf + 1.1f), v3(sx > 0 ? 0.24f : -0.12f, 0.33f, zf + 1.14f), dark, BLACK);
        }
    }
    /* engine: crankcase, finned cylinder, kick starter */
    box(D, v3(-0.15f, 0.24f, zf + 0.75f), v3(0.15f, 0.48f, zf + 1.15f), chrome, 0xFFB4B4B0u);
    for (k = 0; k < 5; k++)
        box(D, v3(-0.12f, 0.48f + k * 0.035f, zf + 0.72f), v3(0.12f, 0.5f + k * 0.035f, zf + 0.98f), dark, 0xFF606064u);
    box(D, v3(0.15f, 0.3f, zf + 1.05f), v3(0.18f, 0.5f, zf + 1.08f), dark, BLACK);
    /* fork, bars, lamp and the front mudguard turn with the steering */
    {
        Xf t0 = xf_translate(v3(0, 0.95f, cm->fork_z)), r0 = xf_rot_x(0.24f), leg = xf_mul(&t0, &r0);
        for (k = 0; k < 2; k++) {
            float sx = k ? 1.0f : -1.0f;
            box_xf(F, &leg, v3(sx * 0.08f - 0.022f, -0.68f, -0.022f), v3(sx * 0.08f + 0.022f, 0.0f, 0.022f), chrome, CHROME);
            box(F, v3(sx > 0 ? 0.08f : -0.36f, 1.04f, cm->fork_z + 0.02f), v3(sx > 0 ? 0.36f : -0.08f, 1.07f, cm->fork_z + 0.05f), chrome, CHROME);
            box(F, v3(sx > 0 ? 0.28f : -0.38f, 1.03f, cm->fork_z + 0.01f), v3(sx > 0 ? 0.38f : -0.28f, 1.08f, cm->fork_z + 0.06f), dark, RUBBER);
            box(F, v3(sx * 0.2f - 0.02f, 1.07f, cm->fork_z + 0.02f), v3(sx * 0.2f + 0.02f, 1.2f, cm->fork_z + 0.03f), chrome, CHROME); /* mirror stems */
            plate_z(F, sx * 0.2f - 0.05f, sx * 0.2f + 0.05f, 1.18f, 1.25f, cm->fork_z + 0.035f, UV2(C2_MIRROR), 1, WHITE);
        }
        box(F, v3(-0.1f, 0.93f, cm->fork_z - 0.02f), v3(0.1f, 1.03f, cm->fork_z + 0.06f), dark, BLACK);         /* headstock and speedo */
        round_lamp(F, 0.0f, 0.88f, cm->fork_z - 0.1f, 0.09f, 0, UVP(CP_HEAD_ROUND), CHROME);
        box(F, v3(-0.07f, wr + 0.3f, a0 - 0.3f), v3(0.07f, wr + 0.33f, a0 + 0.22f), chrome, CHROME);
    }
    build_wheel(&cm->wheel, wr, 0.1f, 10, UV2(C2_RIM_SPOKES), WHITE);
    build_wheel(&cm->wheel_far, wr, 0.1f, 6, UV2(C2_RIM_SPOKES), WHITE);
}

/* ------------------------------------------------------------- assembly */

static void build_model(int t)
{
    const CarSpec *s = &CAR_SPECS[t];
    CarModel *cm = &models[t];
    Ctx x;
    UVRect rim = UV2(C2_RIM_STEEL);
    unsigned int rim_col = 0xFFB8B8B8u;
    float overhang = (s->length - s->wheelbase) * 0.47f, ww = s->body == BODY_TRUCK ? 0.36f : 0.22f;
    int k;

    gm_init(&cm->paint);
    gm_init(&cm->detail);
    gm_init(&cm->wheel);
    gm_init(&cm->wheel_far);
    gm_init(&cm->cabin);
    gm_init(&cm->fork);
    gm_init(&cm->small);
    for (k = 0; k < 2; k++) {
        gm_init(&cm->door_skin[k]);
        gm_init(&cm->door_glass[k]);
        gm_init(&cm->door_trim[k]);
        gm_init(&cm->wall_front[k]);
        gm_init(&cm->wall_rear[k]);
    }
    car_layout(t, &cm->lay);
    cm->lay.nbeacons = 0;
    if (s->body == BODY_BIKE) {
        build_bike(cm, s);
        return;
    }

    memset(&x, 0, sizeof(x));
    x.s = s;
    x.cm = cm;
    x.P = &cm->paint;
    x.D = &cm->detail;
    x.S = &cm->small;
    x.hw = s->width * 0.5f;
    x.zf = zof(s, 0.0f);
    x.zr = zof(s, s->length);
    x.top = fmaxf(fmaxf(s->hood_h, s->trunk_h), fmaxf(s->rear_h, s->hood_front_h));
    x.gi = x.hw - s->glass_inset;
    x.bx = x.hw - 0.02f;
    x.zb0 = zof(s, s->ws_base);
    x.zt0 = zof(s, s->roof_front);
    x.zt1 = zof(s, s->roof_rear);
    x.zb1 = zof(s, s->rear_glass_base);
    x.ar = s->wheel_r * 1.18f;
    if (s->axles == 3) {
        x.narch = 3;
        x.arch[0] = overhang;
        x.arch[1] = overhang + s->wheelbase - 0.7f;
        x.arch[2] = overhang + s->wheelbase + 0.7f;
    } else {
        x.narch = 2;
        x.arch[0] = overhang;
        x.arch[1] = overhang + s->wheelbase;
    }
    cm->naxles = x.narch;
    for (k = 0; k < x.narch; k++) cm->axle_z[k] = zof(s, x.arch[k]);
    x.dc.f0 = cm->lay.door_d0;
    x.dc.f1 = cm->lay.door_d1;
    x.dc.r0 = cm->lay.bpillar + 0.06f;
    x.dc.r1 = s->rear_doors ? s->rear_glass_base - 0.05f : x.dc.r0;
    if (x.dc.r1 > x.dc.r0 + (x.dc.f1 - x.dc.f0)) x.dc.r1 = x.dc.r0 + (x.dc.f1 - x.dc.f0);
    x.dc.sill = s->clearance + 0.1f;

    build_body(&x);
    switch (t) {
    case CAR_KAISER:    model_kaiser(&x); rim = UV2(C2_RIM_ALLOY); rim_col = WHITE; break;
    case CAR_LASTIVKA:  model_lastivka(&x); break;
    case CAR_KLASYKA:   model_klasyka(&x); rim_col = CHROME; break;
    case CAR_PANTERA:   model_pantera(&x); rim = UV2(C2_RIM_ALLOY); rim_col = WHITE; break;
    case CAR_GELENDER:  model_gelender(&x); rim = UV2(C2_RIM_ALLOY); rim_col = 0xFF707070u; break;
    case CAR_POLE:      model_pole(&x); break;
    case CAR_BOBIK:     model_bobik(&x); rim = UV2(C2_RIM_TRUCK); rim_col = 0xFF6A8A6Au; break;
    case CAR_BUKHANKA:  model_bukhanka(&x); rim_col = 0xFFE0E0E0u; break;
    case CAR_TAIGA:     model_taiga(&x); rim = UV2(C2_RIM_TRUCK); rim_col = 0xFF3838B8u; break;
    case CAR_STOLYTSIA: model_stolytsia(&x); rim_col = CHROME; break;
    default: break;
    }
    build_cabin(&x, t);
    build_wheel(&cm->wheel, s->wheel_r, ww, 9, rim, rim_col);
    build_wheel(&cm->wheel_far, s->wheel_r, ww, 6, rim, rim_col);
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
    if (s->body == BODY_BIKE) {
        x = 0.0f;
        y = i < 2 ? 0.88f : 0.7f;
    }
    return xf_point(&b, v3(x, i < 2 ? y : s->bumper_h + 0.12f, z));
}

V3 car_rider_pos(const Car *c)
{
    const CarSpec *s = &CAR_SPECS[c->type];
    Xf b = body_xf(c);
    CarLayout L;
    car_layout(c->type, &L);
    return xf_point(&b, v3(0.0f, L.seat_y, -s->length * 0.5f + L.seat_d));
}

int car_beacons(const Car *c, V3 *pos, unsigned int *col)
{
    const CarModel *cm = &models[c->type];
    Xf b = body_xf(c);
    int i;
    for (i = 0; i < cm->lay.nbeacons; i++) {
        pos[i] = xf_point(&b, cm->lay.beacon[i]);
        col[i] = cm->lay.beacon_col[i];
    }
    return cm->lay.nbeacons;
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
    GMesh door[2];  /* painted door skin + its window and signs */
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

void car_draw(DrawList *dl, const Car *c, int lod)
{
    int near = lod >= 2;
    const CarSpec *s = &CAR_SPECS[c->type];
    const CarModel *cm = &models[c->type];
    const CarInstance *ci = instance(c);
    const GMesh *wheel = near ? &cm->wheel : &cm->wheel_far;
    Xf body = body_xf(c);
    int w, k;
    dl_add(dl, ci->body.v, ci->body.n, &body);
    if (near) dl_add(dl, cm->small.v, cm->small.n, &body);
    if (s->body == BODY_BIKE) {
        /* fork and front wheel turn about the steering head; both wheels lean with the bike */
        Xf h = xf_translate(v3(0, 0, cm->fork_z)), r = xf_rot_y(c->steer), hi = xf_translate(v3(0, 0, -cm->fork_z));
        Xf a1 = xf_mul(&body, &h), a2 = xf_mul(&a1, &r), fork = xf_mul(&a2, &hi);
        Xf spin = xf_rot_x(-c->wheel_spin);
        Xf hub0 = xf_translate(v3(0, s->wheel_r, cm->axle_z[0])), hub1 = xf_translate(v3(0, s->wheel_r, cm->axle_z[1]));
        Xf f1 = xf_mul(&fork, &hub0), f2 = xf_mul(&f1, &spin), r1 = xf_mul(&body, &hub1), r2 = xf_mul(&r1, &spin);
        dl_add(dl, cm->fork.v, cm->fork.n, &fork);
        dl_add(dl, wheel->v, wheel->n, &f2);
        dl_add(dl, wheel->v, wheel->n, &r2);
        return;
    }
    for (k = 0; k < 2; k++) {
        if (c->door > 0.02f && c->door_side == k) {
            /* swing about the vertical hinge line at the door's front edge */
            float sx = k ? 1.0f : -1.0f, hz = zof(s, cm->lay.door_d0);
            Xf h = xf_translate(v3(sx * s->width * 0.5f, 0, hz)), r = xf_rot_y(sx * c->door * 1.15f);
            Xf hi = xf_translate(v3(-sx * s->width * 0.5f, 0, -hz)), a1 = xf_mul(&body, &h), a2 = xf_mul(&a1, &r);
            Xf m = xf_mul(&a2, &hi);
            dl_add(dl, ci->door[k].v, ci->door[k].n, &m);
            dl_add(dl, cm->door_trim[k].v, cm->door_trim[k].n, &m);
            dl_add(dl, cm->cabin.v, cm->cabin.n, &body);
            dl_add(dl, cm->wall_front[1 - k].v, cm->wall_front[1 - k].n, &body);
            dl_add(dl, cm->wall_rear[1 - k].v, cm->wall_rear[1 - k].n, &body);
            dl_add(dl, cm->wall_rear[k].v, cm->wall_rear[k].n, &body);
        } else {
            dl_add(dl, ci->door[k].v, ci->door[k].n, &body);
        }
    }
    /* wheels follow the ground, not the body roll; the front axle steers */
    for (w = 0; w < (lod > 0 ? cm->naxles * 2 : 0); w++) {
        float x = (w & 1 ? 1.0f : -1.0f) * (s->width * 0.5f - (s->body == BODY_TRUCK ? 0.2f : 0.09f)), z = cm->axle_z[w / 2];
        Xf base = xf_translate(v3(c->pos.x, c->pos.y + c->body_y * 0.3f, c->pos.z));
        Xf yaw = xf_rot_y(c->yaw), hub = xf_translate(v3(x, s->wheel_r, z));
        Xf steer = xf_rot_y(w < 2 ? c->steer : 0.0f), spin = xf_rot_x(-c->wheel_spin);
        Xf m1 = xf_mul(&base, &yaw), m2 = xf_mul(&m1, &hub), m3 = xf_mul(&m2, &steer), m4 = xf_mul(&m3, &spin);
        dl_add(dl, wheel->v, wheel->n, &m4);
    }
}

void car_append_mesh(GMesh *out, const Car *c)
{
    DrawList dl;
    dl_init(&dl);
    car_draw(&dl, c, 2);
    dl_flatten(&dl, out);
    dl_free(&dl);
}
