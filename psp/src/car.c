/* Car physics (bicycle model with slip-angle tyres, weight transfer, gears),
 * collisions with the city and other cars, traffic and chaser drivers. */
#include "car.h"
#include "city.h"

#include <string.h>

#define G 9.81f
#define SUBSTEP (1.0f / 120.0f)
#define TRAFFIC_MAX 9
#define TRAFFIC_SPEED 11.0f

Car cars[MAX_CARS];

static const float GEARS[5] = { 3.5f, 2.1f, 1.45f, 1.05f, 0.82f };
#define FINAL_DRIVE 3.9f
#define REV_GEAR 3.3f
#define IDLE_RPM 850.0f
#define RED_RPM 6200.0f

static unsigned int rng = 0x9E3779B9u;
static unsigned int rnd(void)
{
    rng ^= rng << 13;
    rng ^= rng >> 17;
    rng ^= rng << 5;
    return rng;
}
static float frnd(void) { return (rnd() & 0xFFFF) / 65535.0f; }

static const unsigned int PAINTS[] = {
    0xFFE8E8E8u, /* white */
    0xFF9CC4D8u, /* beige */
    0xFF3A2A9Au, /* cherry */
    0xFF5A2A5Au, /* aubergine */
    0xFF6A6A70u, /* wet asphalt */
    0xFFC09060u, /* sky blue */
    0xFF708A3Cu, /* sea wave */
    0xFF2A6AC0u, /* orange-red */
};

void cars_clear(void) { memset(cars, 0, sizeof(cars)); }

int car_spawn(int kind, int type, V3 pos, float yaw, unsigned int paint)
{
    int i;
    for (i = 0; i < MAX_CARS; i++)
        if (cars[i].kind == CK_FREE) {
            Car *c = &cars[i];
            memset(c, 0, sizeof(*c));
            c->kind = kind;
            c->type = type;
            c->pos = pos;
            c->pos.y = city_ground(pos.x, pos.z);
            c->yaw = yaw;
            c->paint = CAR_SPECS[type].fixed_paint ? CAR_SPECS[type].fixed_paint : paint;  /* the G is always black */
            c->gear = 1;
            c->rpm = IDLE_RPM;
            c->band_done = -999;
            return i;
        }
    return -1;
}

int car_find_kind(int kind)
{
    int i;
    for (i = 0; i < MAX_CARS; i++)
        if (cars[i].kind == kind) return i;
    return -1;
}

int car_lod(float dist) { return dist < CAR_NEAR ? 2 : (dist < CAR_MID ? 1 : 0); }

float car_speed(const Car *c) { return v3_dot(c->vel, yaw_fwd(c->yaw)); }

V3 car_door_point(const Car *c, int side)
{
    const CarSpec *s = &CAR_SPECS[c->type];
    CarLayout L;
    float x;
    car_layout(c->type, &L);
    if (s->body == BODY_BIKE) return car_local(c, v3(side ? 0.6f : -0.6f, 0, -s->length * 0.5f + L.seat_d));
    x = (s->width * 0.5f + 0.55f) * (side ? 1.0f : -1.0f);
    return car_local(c, v3(x, 0, -s->length * 0.5f + (L.door_d0 + L.door_d1) * 0.5f + 0.1f));
}

V3 car_seat_point(const Car *c)
{
    const CarSpec *s = &CAR_SPECS[c->type];
    CarLayout L;
    car_layout(c->type, &L);
    return car_local(c, v3(L.seat_x, 0, -s->length * 0.5f + L.seat_d));
}

int car_nearest(V3 pos, float maxd, int *side)
{
    int i, best = -1, sd;
    float bd = maxd * maxd;
    for (i = 0; i < MAX_CARS; i++) {
        V3 d;
        float dd;
        if (cars[i].kind == CK_FREE || cars[i].kind == CK_CHASER || cars[i].kind == CK_SCRIPT) continue;
        for (sd = 0; sd < 2; sd++) {
            d = v3_sub(car_door_point(&cars[i], sd), pos);
            dd = d.x * d.x + d.z * d.z;
            if (dd < bd) {
                bd = dd;
                best = i;
                if (side) *side = sd;
            }
        }
    }
    return best;
}

/* ------------------------------------------------------------ collision */

static void car_corners(const Car *c, V3 out[4])
{
    const CarSpec *s = &CAR_SPECS[c->type];
    V3 f = yaw_fwd(c->yaw), r = yaw_right(c->yaw);
    float hl = s->length * 0.5f, hw = s->width * 0.5f;
    out[0] = v3_add(c->pos, v3_add(v3_scale(f, hl), v3_scale(r, hw)));
    out[1] = v3_add(c->pos, v3_add(v3_scale(f, hl), v3_scale(r, -hw)));
    out[2] = v3_add(c->pos, v3_add(v3_scale(f, -hl), v3_scale(r, -hw)));
    out[3] = v3_add(c->pos, v3_add(v3_scale(f, -hl), v3_scale(r, hw)));
}

/* Separating axis test of two convex quads in XZ. Returns penetration depth
 * and the normal pointing from b to a. */
static int sat(const V3 a[4], const V3 b[4], V3 *normal, float *depth)
{
    V3 axes[4];
    int i, k;
    float best = 1e9f;
    axes[0] = v3_norm(v3(a[1].x - a[0].x, 0, a[1].z - a[0].z));
    axes[1] = v3_norm(v3(a[3].x - a[0].x, 0, a[3].z - a[0].z));
    axes[2] = v3_norm(v3(b[1].x - b[0].x, 0, b[1].z - b[0].z));
    axes[3] = v3_norm(v3(b[3].x - b[0].x, 0, b[3].z - b[0].z));
    for (k = 0; k < 4; k++) {
        float amin = 1e9f, amax = -1e9f, bmin = 1e9f, bmax = -1e9f, o;
        for (i = 0; i < 4; i++) {
            float pa = a[i].x * axes[k].x + a[i].z * axes[k].z, pb = b[i].x * axes[k].x + b[i].z * axes[k].z;
            amin = pa < amin ? pa : amin; amax = pa > amax ? pa : amax;
            bmin = pb < bmin ? pb : bmin; bmax = pb > bmax ? pb : bmax;
        }
        o = (amax < bmax ? amax : bmax) - (amin > bmin ? amin : bmin);
        if (o <= 0.0f) return 0;
        if (o < best) {
            float ca = (amin + amax) * 0.5f, cb = (bmin + bmax) * 0.5f;
            best = o;
            *normal = ca >= cb ? axes[k] : v3_scale(axes[k], -1.0f);
        }
    }
    *depth = best;
    return 1;
}

static V3 deepest_point(const V3 a[4], V3 n)
{
    int i, k = 0;
    float best = 1e9f;
    for (i = 0; i < 4; i++) {
        float d = a[i].x * n.x + a[i].z * n.z;
        if (d < best) { best = d; k = i; }
    }
    return a[k];
}

static float inertia(const Car *c)
{
    const CarSpec *s = &CAR_SPECS[c->type];
    return s->mass * (s->length * s->length + s->width * s->width) / 12.0f;
}

static void apply_impulse(Car *c, V3 p, V3 j)
{
    const CarSpec *s = &CAR_SPECS[c->type];
    V3 r = v3_sub(p, c->pos);
    c->vel = v3_add(c->vel, v3_scale(j, 1.0f / s->mass));
    c->yaw_rate += (r.z * j.x - r.x * j.z) / inertia(c);
}

static V3 point_vel(const Car *c, V3 p)
{
    V3 r = v3_sub(p, c->pos);
    return v3(c->vel.x + c->yaw_rate * r.z, 0, c->vel.z - c->yaw_rate * r.x);
}

static void collide_static(Car *c)
{
    const Box *hits[24];
    V3 corners[4];
    int n, k;
    float ext = CAR_SPECS[c->type].length * 0.6f;
    n = city_boxes(c->pos.x - ext, c->pos.z - ext, c->pos.x + ext, c->pos.z + ext, hits, 24);
    for (k = 0; k < n; k++) {
        const Box *b = hits[k];
        V3 bq[4], nrm, p, vp;
        float depth, vn;
        if (b->y1 < c->pos.y + 0.35f || b->y0 > c->pos.y + 1.3f) continue;
        car_corners(c, corners);
        bq[0] = v3(b->x0, 0, b->z0); bq[1] = v3(b->x1, 0, b->z0); bq[2] = v3(b->x1, 0, b->z1); bq[3] = v3(b->x0, 0, b->z1);
        if (!sat(corners, bq, &nrm, &depth)) continue;
        c->pos.x += nrm.x * depth;
        c->pos.z += nrm.z * depth;
        p = deepest_point(corners, nrm);
        vp = point_vel(c, p);
        vn = vp.x * nrm.x + vp.z * nrm.z;
        if (vn < 0.0f) {
            V3 r = v3_sub(p, c->pos), t;
            float rn = r.z * nrm.x - r.x * nrm.z;
            float jn = -(1.0f + 0.25f) * vn / (1.0f / CAR_SPECS[c->type].mass + rn * rn / inertia(c));
            float vt, rt, jt;
            apply_impulse(c, p, v3_scale(nrm, jn));
            t = v3(-nrm.z, 0, nrm.x);
            vp = point_vel(c, p);
            vt = vp.x * t.x + vp.z * t.z;
            rt = r.z * t.x - r.x * t.z;
            jt = -vt / (1.0f / CAR_SPECS[c->type].mass + rt * rt / inertia(c));
            jt = clampf(jt, -0.4f * jn, 0.4f * jn);
            apply_impulse(c, p, v3_scale(t, jt));
            if (-vn > c->impact) c->impact = -vn;
        }
    }
}

static void collide_cars(Car *a, Car *b)
{
    V3 ca[4], cb[4], n, p;
    float depth, ma, mb, vn;
    if (fabsf(a->pos.x - b->pos.x) > 6.0f || fabsf(a->pos.z - b->pos.z) > 6.0f) return;
    car_corners(a, ca);
    car_corners(b, cb);
    if (!sat(ca, cb, &n, &depth)) return;
    ma = CAR_SPECS[a->type].mass;
    mb = CAR_SPECS[b->type].mass;
    a->pos.x += n.x * depth * mb / (ma + mb);
    a->pos.z += n.z * depth * mb / (ma + mb);
    b->pos.x -= n.x * depth * ma / (ma + mb);
    b->pos.z -= n.z * depth * ma / (ma + mb);
    p = deepest_point(ca, n);
    {
        V3 va = point_vel(a, p), vb = point_vel(b, p), ra = v3_sub(p, a->pos), rb = v3_sub(p, b->pos);
        float rna = ra.z * n.x - ra.x * n.z, rnb = rb.z * n.x - rb.x * n.z, j;
        vn = (va.x - vb.x) * n.x + (va.z - vb.z) * n.z;
        if (vn >= 0.0f) return;
        j = -(1.0f + 0.3f) * vn / (1.0f / ma + 1.0f / mb + rna * rna / inertia(a) + rnb * rnb / inertia(b));
        apply_impulse(a, p, v3_scale(n, j));
        apply_impulse(b, p, v3_scale(n, -j));
        if (-vn > a->impact) a->impact = -vn;
        if (-vn > b->impact) b->impact = -vn;
    }
}

float car_push_point(V3 *p, float radius)
{
    int i;
    float hit = 0.0f;
    for (i = 0; i < MAX_CARS; i++) {
        Car *c = &cars[i];
        const CarSpec *s;
        V3 d, f, r;
        float lf, lr, hl, hw;
        if (c->kind == CK_FREE) continue;
        s = &CAR_SPECS[c->type];
        if (p->y > c->pos.y + s->height || p->y + 1.8f < c->pos.y) continue;
        d = v3_sub(*p, c->pos);
        f = yaw_fwd(c->yaw);
        r = yaw_right(c->yaw);
        lf = d.x * f.x + d.z * f.z;
        lr = d.x * r.x + d.z * r.z;
        hl = s->length * 0.5f + radius;
        hw = s->width * 0.5f + radius;
        if (fabsf(lf) >= hl || fabsf(lr) >= hw) continue;
        {
            float speed = v3_len(point_vel(c, *p));
            if (speed > hit) hit = speed;
        }
        if (hw - fabsf(lr) < hl - fabsf(lf)) lr = lr < 0 ? -hw : hw;
        else lf = lf < 0 ? -hl : hl;
        p->x = c->pos.x + f.x * lf + r.x * lr;
        p->z = c->pos.z + f.z * lf + r.z * lr;
    }
    return hit;
}

/* -------------------------------------------------------------- physics */

/* big wheels get a shorter final drive: the same engine speed at the same road speed */
static float final_drive(const CarSpec *s) { return s->wheel_r > 0.45f ? FINAL_DRIVE * s->wheel_r / 0.34f : FINAL_DRIVE; }

static float engine_torque(const CarSpec *s, float rpm)
{
    float peak = 4200.0f, t = (rpm - peak) / 3500.0f;
    float tmax = s->power_kw * 1000.0f / (peak * 6.2831853f / 60.0f) * 1.15f;
    float k = 1.0f - t * t;
    return tmax * (k < 0.45f ? 0.45f : k);
}

static void physics_substep(Car *c, float dt)
{
    const CarSpec *s = &CAR_SPECS[c->type];
    const float m = s->mass, L = s->wheelbase, a = L * 0.5f, b = L * 0.5f, I = inertia(c);
    V3 f = yaw_fwd(c->yaw), r = yaw_right(c->yaw);
    float u = v3_dot(c->vel, f), speed = v3_len(c->vel);
    float vmax = s->top_kmh / 3.6f, cd = s->power_kw * 1000.0f / (vmax * vmax * vmax);
    float steer_target, ax_prev, Fzf, Fzr, mu = s->grip * 1.05f, mu_r;
    V3 Ff = v3(0, 0, 0), Fr = v3(0, 0, 0), F, wf, wr, vfa, vra;
    float uf, sf, ur, sr, af, ar, Cf, Cr, latf, latr, drive = 0.0f, brk = 0.0f;
    float throttle = c->in_throttle;

    /* steering: less lock at speed, smooth wheel movement */
    steer_target = c->in_steer * s->max_steer / (1.0f + fabsf(u) / 18.0f);
    c->steer = approach(c->steer, steer_target, 2.6f * dt);

    /* gearbox: pressing against the direction of travel brakes first,
     * then selects first gear / reverse once the car has (almost) stopped */
    if (throttle > 0.05f) {
        if (u < -0.6f) brk = throttle * m * 9.0f;
        else if (c->gear < 1) c->gear = 1;
    } else if (throttle < -0.05f) {
        if (u > 0.6f) brk = -throttle * m * 9.0f;
        else c->gear = -1;
    }
    {
        float ratio = c->gear < 0 ? REV_GEAR : GEARS[c->gear - 1];
        c->rpm = fabsf(u) / s->wheel_r * ratio * final_drive(s) * 60.0f / 6.2831853f;
        if (c->rpm < IDLE_RPM + 2600.0f * fabsf(throttle) && (c->gear == 1 || c->gear < 0))
            c->rpm = IDLE_RPM + 2600.0f * fabsf(throttle); /* clutch slip when pulling away */
        if (c->rpm < IDLE_RPM) c->rpm = IDLE_RPM;
        if (c->gear > 0 && c->rpm > RED_RPM * 0.88f && c->gear < 5) c->gear++;
        if (c->gear > 1 && c->rpm < RED_RPM * 0.42f) c->gear--;
        if (c->rpm > RED_RPM) c->rpm = RED_RPM;
        if (brk == 0.0f) {
            if (c->gear > 0 && throttle > 0.0f)
                drive = throttle * engine_torque(s, c->rpm) * GEARS[c->gear - 1] * final_drive(s) / s->wheel_r * 0.85f;
            else if (c->gear < 0 && throttle < 0.0f)
                drive = throttle * engine_torque(s, c->rpm) * REV_GEAR * final_drive(s) / s->wheel_r * 0.6f;
        }
    }
    if (c->rpm >= RED_RPM - 1.0f) drive *= 0.2f;

    /* weight transfer from last step's longitudinal acceleration */
    ax_prev = c->ax;
    Fzf = m * G * b / L - m * ax_prev * s->cg_h / L;
    Fzr = m * G * a / L + m * ax_prev * s->cg_h / L;
    Fzf = clampf(Fzf, m * G * 0.2f, m * G * 0.8f);
    Fzr = clampf(Fzr, m * G * 0.2f, m * G * 0.8f);
    mu_r = c->in_handbrake ? mu * 0.38f : mu;

    /* tyre velocities */
    wf = yaw_fwd(c->yaw + c->steer);
    wr = yaw_right(c->yaw + c->steer);
    vfa = v3_sub(c->vel, v3_scale(r, a * c->yaw_rate));
    vra = v3_add(c->vel, v3_scale(r, b * c->yaw_rate));
    uf = v3_dot(vfa, wf);
    sf = v3_dot(vfa, wr);
    ur = v3_dot(vra, f);
    sr = v3_dot(vra, r);
    af = atan2f(sf, fabsf(uf) + 0.6f);
    ar = atan2f(sr, fabsf(ur) + 0.6f);
    Cf = Fzf * 7.5f;
    Cr = Fzr * 8.0f;
    latf = clampf(-Cf * af, -mu * Fzf, mu * Fzf);
    latr = clampf(-Cr * ar, -mu_r * Fzr, mu_r * Fzr);
    if (speed < 1.5f) {
        /* slow speed: plain damping avoids jitter */
        float k = speed / 1.5f;
        latf = latf * k - sf * m * 3.0f * (1.0f - k) * 0.5f;
        latr = latr * k - sr * m * 3.0f * (1.0f - k) * 0.5f;
    }

    /* longitudinal forces per axle */
    {
        float df = s->drive == DRIVE_FWD ? 1.0f : s->drive == DRIVE_AWD ? 0.4f : 0.0f;
        float lonf = drive * df, lonr = drive * (1.0f - df);
        float sign_f = uf > 0.05f ? 1.0f : uf < -0.05f ? -1.0f : 0.0f, sign_r = ur > 0.05f ? 1.0f : ur < -0.05f ? -1.0f : 0.0f;
        float lim;
        lonf -= sign_f * brk * 0.6f;
        lonr -= sign_r * brk * 0.4f;
        if (c->in_handbrake) lonr -= sign_r * m * 5.0f;
        lonf -= uf * m * 0.012f;  /* rolling resistance */
        lonr -= ur * m * 0.012f;
        /* traction circle */
        lim = mu * Fzf;
        if (sqrtf(lonf * lonf + latf * latf) > lim) {
            float k = lim / sqrtf(lonf * lonf + latf * latf);
            lonf *= k;
            latf *= k;
        }
        lim = mu_r * Fzr;
        if (sqrtf(lonr * lonr + latr * latr) > lim) {
            float k = lim / sqrtf(lonr * lonr + latr * latr);
            lonr *= k;
            latr *= k;
        }
        Ff = v3_add(v3_scale(wf, lonf), v3_scale(wr, latf));
        Fr = v3_add(v3_scale(f, lonr), v3_scale(r, latr));
        c->skid = clampf((fabsf(ar) - 0.12f) * 3.0f + (c->in_handbrake && speed > 4 ? 0.5f : 0.0f), 0.0f, 1.0f);
        if (speed < 3.0f) c->skid = 0.0f;
    }

    F = v3_add(Ff, Fr);
    F = v3_sub(F, v3_scale(c->vel, cd * speed));
    {
        V3 pf = v3_scale(f, a), pr = v3_scale(f, -b);
        float torque = (pf.z * Ff.x - pf.x * Ff.z) + (pr.z * Fr.x - pr.x * Fr.z);
        c->yaw_rate += torque / I * dt;
        c->yaw_rate *= 1.0f - 0.6f * dt;
    }
    c->vel = v3_add(c->vel, v3_scale(F, dt / m));
    c->vel.y = 0.0f;
    if (fabsf(throttle) < 0.01f && speed < 0.25f && !c->in_handbrake) c->vel = v3_scale(c->vel, 0.8f);
    if (c->kind == CK_PARKED && speed < 0.3f) {
        c->vel = v3(0, 0, 0);
        c->yaw_rate = 0.0f;
    }
    c->pos = v3_add(c->pos, v3_scale(c->vel, dt));
    c->yaw = wrap_pi(c->yaw + c->yaw_rate * dt);

    /* body motion: pitch from acceleration, roll from lateral acceleration */
    {
        float axl = v3_dot(F, f) / m, ayl = v3_dot(F, r) / m;
        float tp = clampf(axl * 0.012f * s->soft, -0.08f, 0.08f), tr = clampf(ayl * 0.014f * s->soft, -0.09f, 0.09f);
        c->ax = clampf(axl, -12.0f, 12.0f);
        c->pitch_v += ((tp - c->body_pitch) * 90.0f - c->pitch_v * 9.0f) * dt;
        c->roll_v += ((tr - c->body_roll) * 80.0f - c->roll_v * 8.0f) * dt;
        c->body_pitch += c->pitch_v * dt;
        c->body_roll += c->roll_v * dt;
    }
}

/* Motorcycle: a bicycle model (no sideways slip), leaning into the turn at
 * the angle that balances it; four gears for the engine note. */
static void bike_substep(Car *c, float dt)
{
    const CarSpec *s = &CAR_SPECS[c->type];
    const float m = s->mass, L = s->wheelbase;
    V3 f = yaw_fwd(c->yaw);
    float u = v3_dot(c->vel, f), vmax = s->top_kmh / 3.6f, cd = s->power_kw * 1000.0f / (vmax * vmax * vmax);
    float throttle = c->in_throttle, F = 0.0f, steer_target, lean_target;
    /* steering: little lock at speed */
    steer_target = c->in_steer * s->max_steer / (1.0f + fabsf(u) / 5.0f);
    c->steer = approach(c->steer, steer_target, 3.0f * dt);
    /* gearbox and engine */
    if (c->gear < 1) c->gear = 1;
    c->rpm = fabsf(u) / s->wheel_r * GEARS[c->gear - 1] * FINAL_DRIVE * 1.25f * 60.0f / 6.2831853f;
    if (c->rpm < IDLE_RPM + 3200.0f * (throttle > 0 ? throttle : 0.0f) && c->gear == 1) c->rpm = IDLE_RPM + 3200.0f * (throttle > 0 ? throttle : 0.0f);
    if (c->rpm < IDLE_RPM) c->rpm = IDLE_RPM;
    if (c->rpm > RED_RPM * 0.9f && c->gear < 4) c->gear++;
    if (c->gear > 1 && c->rpm < RED_RPM * 0.4f) c->gear--;
    if (c->rpm > RED_RPM) c->rpm = RED_RPM;
    if (throttle > 0.05f) {
        if (u < -0.3f) F = throttle * m * 6.0f;                                   /* stop rolling back */
        else F = throttle * engine_torque(s, c->rpm) * GEARS[c->gear - 1] * FINAL_DRIVE * 1.25f / s->wheel_r * 0.85f;
    } else if (throttle < -0.05f) {
        if (u > 0.4f) F = throttle * m * 7.5f;                                     /* both brakes */
        else if (u > -1.4f) F = throttle * m * 1.2f;                               /* paddle backwards */
    }
    if (c->in_handbrake && fabsf(u) > 0.2f) F -= (u > 0 ? 1.0f : -1.0f) * m * 3.5f; /* rear brake */
    F -= cd * u * fabsf(u) + u * m * 0.015f;
    if (fabsf(throttle) < 0.02f && fabsf(u) < 0.3f) u *= 0.9f;
    u += F / m * dt;
    if (c->kind == CK_PARKED && fabsf(u) < 0.3f) u = 0.0f;
    c->yaw_rate = u * tanf(c->steer) / L;
    c->vel = v3_scale(f, u);
    c->pos = v3_add(c->pos, v3_scale(c->vel, dt));
    c->yaw = wrap_pi(c->yaw + c->yaw_rate * dt);
    /* lean: balance angle while riding, the stand when parked, lying down after a fall */
    if (c->kind == CK_PARKED) lean_target = fabsf(c->lean) > 0.5f ? (c->lean > 0 ? 1.35f : -1.35f) : 0.12f;
    else lean_target = clampf(atanf(u * c->yaw_rate / G), -0.75f, 0.75f);
    c->lean = approach(c->lean, lean_target, 3.0f * dt);
    c->body_roll = c->lean;
    c->body_pitch = 0.0f;
    c->ax = F / m;
    c->skid = c->in_handbrake && fabsf(u) > 5.0f ? 0.6f : 0.0f;
}

void car_step(Car *c, float dt)
{
    const CarSpec *s = &CAR_SPECS[c->type];
    float t = 0.0f, gy;
    c->impact = 0.0f;
    if (s->body == BODY_BIKE) {
        while (t < dt - 1e-5f) {
            float h = dt - t < SUBSTEP ? dt - t : SUBSTEP;
            bike_substep(c, h);
            collide_static(c);
            t += h;
        }
        c->pos.y = city_ground(c->pos.x, c->pos.z);
        c->curb_pitch = c->curb_roll = 0.0f;
        c->wheel_spin = fmodf(c->wheel_spin + car_speed(c) / s->wheel_r * dt, 6.2831853f);
        return;
    }
    while (t < dt - 1e-5f) {
        float h = dt - t < SUBSTEP ? dt - t : SUBSTEP;
        physics_substep(c, h);
        collide_static(c);
        t += h;
    }
    /* ride over kerbs: average ground under the wheels */
    {
        V3 f = yaw_fwd(c->yaw), r = yaw_right(c->yaw);
        float hf = s->wheelbase * 0.5f, hw = s->track * 0.5f, g[4];
        int i;
        for (i = 0; i < 4; i++) {
            V3 p = v3_add(c->pos, v3_add(v3_scale(f, i < 2 ? hf : -hf), v3_scale(r, (i & 1) ? hw : -hw)));
            g[i] = city_ground(p.x, p.z);
        }
        gy = (g[0] + g[1] + g[2] + g[3]) * 0.25f;
        c->y_v += ((gy - c->pos.y) * 160.0f - c->y_v * 14.0f) * dt;
        c->pos.y += c->y_v * dt;
        c->curb_pitch = ((g[0] + g[1]) - (g[2] + g[3])) * 0.5f / s->wheelbase;
        c->curb_roll = ((g[1] + g[3]) - (g[0] + g[2])) * 0.5f / s->track;
    }
    c->wheel_spin = fmodf(c->wheel_spin + car_speed(c) / s->wheel_r * dt, 6.2831853f);
}

/* -------------------------------------------------------------- drivers */

static int dir_of_yaw(float yaw)
{
    float best = 10.0f;
    int d, k = 0;
    for (d = 0; d < 4; d++) {
        static const float Y[4] = { -PI_F / 2, PI_F, PI_F / 2, 0.0f };
        float e = fabsf(wrap_pi(yaw - Y[d]));
        if (e < best) { best = e; k = d; }
    }
    return k;
}

static float dir_yaw(int d)
{
    static const float Y[4] = { -PI_F / 2, PI_F, PI_F / 2, 0.0f };
    return Y[d & 3];
}

static const int DX[4] = { 1, 0, -1, 0 };
static const int DZ[4] = { 0, 1, 0, -1 };

/* steer towards a world point with pure pursuit */
static float pursue(const Car *c, V3 target)
{
    const CarSpec *s = &CAR_SPECS[c->type];
    V3 d = v3_sub(target, c->pos);
    float fd = v3_dot(d, yaw_fwd(c->yaw)), rd = v3_dot(d, yaw_right(c->yaw));
    float l2 = fd * fd + rd * rd, kappa, delta;
    if (l2 < 0.01f) return 0.0f;
    kappa = -2.0f * rd / l2;
    if (fd < 0.0f) kappa = rd > 0 ? -1.0f : 1.0f;
    delta = atanf(kappa * s->wheelbase);
    return clampf(delta / s->max_steer, -1.0f, 1.0f);
}

static V3 walkers[16];
static int nwalkers;

void cars_set_walkers(const V3 *pos, int n)
{
    int i;
    nwalkers = n < 16 ? n : 16;
    for (i = 0; i < nwalkers; i++) walkers[i] = pos[i];
}

static float obstacle_ahead(const Car *c, const V3 *player, int *by_player)
{
    V3 f = yaw_fwd(c->yaw), r = yaw_right(c->yaw);
    float best = 1e9f;
    int i;
    *by_player = 0;
    if (player) {
        V3 d = v3_sub(*player, c->pos);
        float a = v3_dot(d, f), sd = v3_dot(d, r);
        if (a > 0.0f && fabsf(sd) < 1.6f && a < best) { best = a; *by_player = 1; }
    }
    for (i = 0; i < nwalkers; i++) {
        V3 d = v3_sub(walkers[i], c->pos);
        float a = v3_dot(d, f), sd = v3_dot(d, r);
        if (a > 0.0f && fabsf(sd) < 1.5f && a < best) { best = a; *by_player = 0; }
    }
    for (i = 0; i < MAX_CARS; i++) {
        const Car *o = &cars[i];
        V3 d;
        float a, sd;
        if (o == c || o->kind == CK_FREE) continue;
        d = v3_sub(o->pos, c->pos);
        a = v3_dot(d, f);
        sd = v3_dot(d, r);
        if (a > 0.0f && fabsf(sd) < 2.0f && a < best) {
            best = a;
            *by_player = o->kind == CK_PLAYER;
        }
    }
    return best;
}

static void plan_turn(Car *c, int nd, int band_ahead)
{
    int axis_x = (c->dir & 1) == 0, right = nd == ((c->dir + 1) & 3);
    float R = right ? 3.5f : 7.5f;
    float lane_old = axis_x ? c->pos.z : c->pos.x;
    float turn_at = city_lane(nd, band_ahead), xx, xz;
    float dox = (float)DX[c->dir], doz = (float)DZ[c->dir], dnx = (float)DX[nd], dnz = (float)DZ[nd];
    lane_old = city_lane(c->dir, city_band_index(lane_old));
    xx = axis_x ? turn_at : lane_old;
    xz = axis_x ? lane_old : turn_at;
    c->tr = R;
    c->tsx = xx - R * dox;
    c->tsz = xz - R * doz;
    c->tcx = c->tsx + R * dnx;
    c->tcz = c->tsz + R * dnz;
    c->tex = xx + R * dnx;
    c->tez = xz + R * dnz;
    /* rotation sense: cross(-d_new, d_old) in the x-z plane */
    c->tsgn = ((-dnx) * doz - (-dnz) * dox) > 0 ? 1 : -1;
    c->turn_at = turn_at;
}

static void traffic_drive(Car *c, const V3 *player, float dt, int *ev)
{
    int axis_x = (c->dir & 1) == 0, by_player;
    float along = axis_x ? c->pos.x : c->pos.z, s = (c->dir == 0 || c->dir == 1) ? 1.0f : -1.0f;
    float lane = city_lane(c->dir, city_band_index(axis_x ? c->pos.z : c->pos.x));
    float speed = car_speed(c), target_speed = TRAFFIC_SPEED, look = 6.0f + fabsf(speed) * 0.35f, gap;
    V3 target;
    int band_ahead = city_band_index(along + s * 16.0f);

    /* decide at the next crossing */
    if (!c->turning && band_ahead != c->band_done && city_band_offset(along + s * 16.0f) < ROAD) {
        int bi = axis_x ? band_ahead : city_band_index(c->pos.x), bj = axis_x ? city_band_index(c->pos.z) : band_ahead;
        int cand[3] = { c->dir, (c->dir + 1) & 3, (c->dir + 3) & 3 }, ok[3], n = 0, k;
        for (k = 0; k < 3; k++) {
            ok[k] = city_band_ok(bi + DX[cand[k]]) && city_band_ok(bj + DZ[cand[k]]);
            n += ok[k];
        }
        c->band_done = band_ahead;
        c->next_dir = c->dir;
        if (n) {
            unsigned int pick = rnd() % 4u; /* 2/4 straight, 1/4 right, 1/4 left */
            k = pick < 2 ? 0 : (int)pick - 1;
            if (!ok[k]) for (k = 0; k < 3 && !ok[k]; k++) {}
            c->next_dir = cand[k];
        }
        if (c->next_dir != c->dir) plan_turn(c, c->next_dir, band_ahead);
    }

    if (c->next_dir != c->dir && (c->turning || s * (along - (axis_x ? c->tsx : c->tsz)) >= 0.0f)) {
        /* follow the arc around the corner */
        int nd = c->next_dir;
        float vx = c->pos.x - c->tcx, vz = c->pos.z - c->tcz, phi = atan2f(vz, vx);
        float phe = atan2f(c->tez - c->tcz, c->tex - c->tcx), ahead = 3.0f / c->tr;
        float rem = wrap_pi(phe - phi) * c->tsgn;
        c->turning = 1;
        if (rem > ahead) {
            float pt = phi + c->tsgn * ahead;
            target = v3(c->tcx + cosf(pt) * c->tr, 0, c->tcz + sinf(pt) * c->tr);
        } else {
            target = v3(c->tex + DX[nd] * 3.0f, 0, c->tez + DZ[nd] * 3.0f);
        }
        target_speed = c->tr < 5.0f ? 4.5f : 6.5f;
        if ((c->pos.x - c->tex) * DX[nd] + (c->pos.z - c->tez) * DZ[nd] >= 0.0f) {
            c->dir = nd;
            c->turning = 0;
            c->band_done = city_band_index((nd & 1) == 0 ? c->pos.x : c->pos.z);
        }
    } else {
        target = axis_x ? v3(along + s * look, 0, lane) : v3(lane, 0, along + s * look);
        if (c->next_dir != c->dir) {
            /* slow down before the corner */
            float to_turn = s * ((axis_x ? c->tsx : c->tsz) - along);
            target_speed = fminf(target_speed, 5.0f + to_turn * 0.6f);
        }
    }

    gap = obstacle_ahead(c, player, &by_player);
    if (gap < 18.0f) target_speed = fminf(target_speed, (gap - 6.0f) * 0.9f);
    if (gap < 9.0f) {
        c->blocked += dt;
        if (by_player && c->blocked > 1.2f && c->honk_cd <= 0.0f) {
            *ev |= CAR_EV_HONK;
            c->honk_cd = 3.5f;
        }
    } else {
        c->blocked = 0.0f;
    }
    if (c->honk_cd > 0.0f) c->honk_cd -= dt;
    if (target_speed < 0.0f) target_speed = 0.0f;
    c->in_steer = pursue(c, target);
    c->in_throttle = clampf((target_speed - speed) * 0.35f, -1.0f, 0.7f);
    c->in_handbrake = 0;
    if (target_speed < 0.3f && speed < 1.0f) {
        /* hold still instead of creeping into reverse */
        c->in_throttle = 0.0f;
        c->in_handbrake = 1;
        c->vel = v3_scale(c->vel, 0.9f);
    }
}

static void chaser_drive(Car *c, V3 target, float dt)
{
    float speed = car_speed(c), steer = pursue(c, target);
    V3 d = v3_sub(target, c->pos);
    float dist = sqrtf(d.x * d.x + d.z * d.z), ang = fabsf(wrap_pi(yaw_of(d.x, d.z) - c->yaw));
    c->in_steer = steer;
    c->in_throttle = ang > 1.4f && speed > 10.0f ? -0.3f : (dist < 6.0f ? 0.4f : 1.0f);
    c->in_handbrake = ang > 1.2f && speed > 12.0f;
    if (c->reverse_t > 0.0f) {
        c->reverse_t -= dt;
        c->in_throttle = -1.0f;
        c->in_steer = -steer;
    } else if (fabsf(speed) < 1.0f) {
        c->stuck += dt;
        if (c->stuck > 1.2f) {
            c->stuck = 0.0f;
            c->reverse_t = 1.0f;
        }
    } else {
        c->stuck = 0.0f;
    }
}

static void try_spawn_traffic(V3 focus)
{
    int axis = (int)(rnd() & 1u), dir = axis ? ((rnd() & 1u) ? 1 : 3) : ((rnd() & 1u) ? 0 : 2), i, type;
    int band;
    float along = (frnd() * 60.0f + 45.0f) * ((rnd() & 1u) ? 1.0f : -1.0f);
    V3 pos;
    static const int TYPES[20] = { CAR_KLASYKA, CAR_KLASYKA, CAR_KLASYKA, CAR_LASTIVKA, CAR_LASTIVKA, CAR_LASTIVKA,
                                   CAR_POLE, CAR_POLE, CAR_KAISER, CAR_KAISER, CAR_PANTERA, CAR_GELENDER,
                                   CAR_STOLYTSIA, CAR_STOLYTSIA, CAR_STOLYTSIA, CAR_BOBIK, CAR_BOBIK, CAR_BUKHANKA,
                                   CAR_TAIGA, CAR_LASTIVKA };
    if (axis == 0) {
        band = city_band_index(focus.z) + (int)(rnd() % 3u) - 1;
        pos = v3(focus.x + along, 0, city_lane(dir, band));
    } else {
        band = city_band_index(focus.x) + (int)(rnd() % 3u) - 1;
        pos = v3(city_lane(dir, band), 0, focus.z + along);
    }
    if (!city_band_ok(band) || !city_in_town(pos.x, pos.z) || !city_on_road(pos.x, pos.z)) return;
    if (city_band_offset(axis ? pos.z : pos.x) < ROAD + 6.0f) return; /* not inside a crossing */
    for (i = 0; i < MAX_CARS; i++) {
        V3 d;
        if (cars[i].kind == CK_FREE) continue;
        d = v3_sub(cars[i].pos, pos);
        if (d.x * d.x + d.z * d.z < 100.0f) return;
    }
    type = TYPES[rnd() % 20u];
    i = car_spawn(CK_TRAFFIC, type, pos, dir_yaw(dir), PAINTS[rnd() % (sizeof(PAINTS) / sizeof(PAINTS[0]))]);
    if (i >= 0) {
        Car *c = &cars[i];
        /* emergency vehicles on a call now and then; the taxi's lamp is always lit */
        c->siren = CAR_SPECS[type].beacon == BEACON_TAXI || (CAR_SPECS[type].beacon != BEACON_NONE && (rnd() & 1u));
        c->dir = dir;
        c->next_dir = dir;
        c->vel = v3_scale(yaw_fwd(c->yaw), TRAFFIC_SPEED * 0.8f);
    }
}

int cars_update(float dt, V3 focus, const V3 *player, int traffic)
{
    static float spawn_t;
    int i, j, ev = 0, n_traffic = 0;

    for (i = 0; i < MAX_CARS; i++) {
        Car *c = &cars[i];
        V3 d;
        if (c->kind == CK_FREE) continue;
        d = v3_sub(c->pos, focus);
        if (c->kind == CK_TRAFFIC) {
            if (d.x * d.x + d.z * d.z > 150.0f * 150.0f) {
                c->kind = CK_FREE;
                continue;
            }
            n_traffic++;
            if (c->dir < 0 || c->dir > 3) c->dir = dir_of_yaw(c->yaw);
            traffic_drive(c, player, dt, &ev);
        } else if (c->kind == CK_CHASER) {
            chaser_drive(c, focus, dt);
        } else if (c->kind == CK_PARKED) {
            c->in_throttle = 0.0f;
            c->in_steer = 0.0f;
            c->in_handbrake = 1;
        }
        if (c->kind != CK_PLAYER) car_step(c, dt);
    }
    for (i = 0; i < MAX_CARS; i++)
        for (j = i + 1; j < MAX_CARS; j++)
            if (cars[i].kind != CK_FREE && cars[j].kind != CK_FREE) collide_cars(&cars[i], &cars[j]);
    for (i = 0; i < MAX_CARS; i++)
        if (cars[i].kind != CK_FREE && cars[i].impact > 5.0f) ev |= CAR_EV_CRASH;

    spawn_t -= dt;
    if (traffic && n_traffic < TRAFFIC_MAX && spawn_t <= 0.0f && city_in_town(focus.x, focus.z)) {
        spawn_t = 0.35f;
        try_spawn_traffic(focus);
    }
    return ev;
}
