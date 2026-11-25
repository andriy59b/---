#include "vehicle.h"
#include "town.h"
#include "world.h"

#include <math.h>
#include <string.h>

#define PI 3.14159265f
#define TRAFFIC_SPEED 8.5f
#define TRAFFIC_MAX 8

Car cars[MAX_CARS];

static unsigned int rng = 0x2545F491u;
static unsigned int rnd(void)
{
    rng ^= rng << 13;
    rng ^= rng >> 17;
    rng ^= rng << 5;
    return rng;
}
static float frnd(void) { return (rnd() & 0xFFFF) / 65535.0f; }

static const int DIR_X[4] = { 1, 0, -1, 0 };
static const int DIR_Z[4] = { 0, 1, 0, -1 };

static const unsigned int TRAFFIC_COLORS[] = {
    0xFFE8E8E8u, /* white */
    0xFF9CC8DCu, /* beige */
    0xFF2A2AB4u, /* red */
    0xFFA05A28u, /* blue */
    0xFF3C7846u, /* green */
    0xFF5A2A5Au, /* aubergine */
    0xFF40A0E0u, /* orange */
    0xFF8C8C8Cu, /* grey */
};

float car_dir_yaw(int dir)
{
    static const float Y[4] = { -PI / 2, PI, PI / 2, 0.0f };
    return Y[dir & 3];
}

static float wrap_angle(float a)
{
    while (a > PI) a -= 2 * PI;
    while (a < -PI) a += 2 * PI;
    return a;
}

void cars_clear(void)
{
    memset(cars, 0, sizeof(cars));
}

int car_spawn(int kind, float x, float y, float z, float yaw, unsigned int color)
{
    int i;
    for (i = 0; i < MAX_CARS; i++)
        if (cars[i].kind == CAR_FREE) {
            memset(&cars[i], 0, sizeof(Car));
            cars[i].kind = kind;
            cars[i].pos[0] = x;
            cars[i].pos[1] = y;
            cars[i].pos[2] = z;
            cars[i].yaw = yaw;
            cars[i].color = color;
            cars[i].band_done = -9999;
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

int car_nearest(const float pos[3], float maxd)
{
    int i, best = -1;
    float bd = maxd * maxd;
    for (i = 0; i < MAX_CARS; i++) {
        float dx, dz, d;
        if (cars[i].kind == CAR_FREE || cars[i].kind == CAR_CHASER) continue;
        dx = cars[i].pos[0] - pos[0];
        dz = cars[i].pos[2] - pos[2];
        d = dx * dx + dz * dz;
        if (d < bd && fabsf(cars[i].pos[1] - pos[1]) < 3.0f) {
            bd = d;
            best = i;
        }
    }
    return best;
}

/* ---------------------------------------------------------------- physics */

static void local_to_world(const Car *c, float lx, float lz, float *wx, float *wz)
{
    float s = sinf(c->yaw), co = cosf(c->yaw);
    /* right = (cos, -sin), forward = (-sin, -cos) */
    *wx = c->pos[0] + lx * co - lz * s;
    *wz = c->pos[2] - lx * s - lz * co;
}

static int solid_at(float x, float y, float z)
{
    return block_solid(world_get((int)floorf(x), (int)floorf(y), (int)floorf(z)));
}

static int hits_world(const Car *c)
{
    static const float P[8][2] = {
        { -0.85f, -1.95f }, { 0.85f, -1.95f }, { -0.85f, 1.95f }, { 0.85f, 1.95f },
        { -0.85f, 0.0f }, { 0.85f, 0.0f }, { 0.0f, -1.95f }, { 0.0f, 1.95f },
    };
    int i;
    for (i = 0; i < 8; i++) {
        float wx, wz;
        local_to_world(c, P[i][0], P[i][1], &wx, &wz);
        /* the lowest metre is handled by stepping up kerbs */
        if (solid_at(wx, c->pos[1] + 1.1f, wz) || solid_at(wx, c->pos[1] + 1.6f, wz)) return 1;
    }
    return 0;
}

static int hits_car(const Car *c)
{
    int i, a, b;
    float fx = -sinf(c->yaw), fz = -cosf(c->yaw);
    for (i = 0; i < MAX_CARS; i++) {
        const Car *o = &cars[i];
        float ofx, ofz;
        if (o == c || o->kind == CAR_FREE) continue;
        if (fabsf(o->pos[0] - c->pos[0]) > 6.0f || fabsf(o->pos[2] - c->pos[2]) > 6.0f) continue;
        ofx = -sinf(o->yaw);
        ofz = -cosf(o->yaw);
        for (a = -1; a <= 1; a += 2)
            for (b = -1; b <= 1; b += 2) {
                float dx = (c->pos[0] + fx * a * 1.0f) - (o->pos[0] + ofx * b * 1.0f);
                float dz = (c->pos[2] + fz * a * 1.0f) - (o->pos[2] + ofz * b * 1.0f);
                if (dx * dx + dz * dz < 1.9f * 1.9f) return 1;
            }
    }
    return 0;
}

static int ground_below(float x, float y, float z)
{
    int yy, top = (int)floorf(y + 1.0f);
    for (yy = top; yy >= top - 6 && yy >= 0; yy--)
        if (solid_at(x + 0.5f - 0.5f, (float)yy, z)) return yy;
    return -100;
}

int car_physics(Car *c, float throttle, float steer, float dt)
{
    float fx, fz, ox = c->pos[0], oz = c->pos[2], oyaw = c->yaw, grip;
    int ev = 0, g;

    if (throttle > 0.0f) c->speed += throttle * (c->speed >= 0.0f ? 8.0f : 16.0f) * dt;
    else if (throttle < 0.0f) c->speed += throttle * (c->speed > 0.0f ? 16.0f : 6.0f) * dt;
    else {
        float k = 3.0f * dt;
        c->speed = fabsf(c->speed) < k ? 0.0f : c->speed - (c->speed > 0 ? k : -k);
    }
    c->speed -= c->speed * 0.12f * dt;
    if (c->speed > CAR_MAX_SPEED) c->speed = CAR_MAX_SPEED;
    if (c->speed < -7.0f) c->speed = -7.0f;

    grip = c->speed / 7.0f;
    if (grip > 1.0f) grip = 1.0f - (c->speed - 7.0f) / 60.0f;
    if (grip < -1.0f) grip = -1.0f;
    c->yaw = wrap_angle(c->yaw + steer * grip * 1.9f * dt);

    fx = -sinf(c->yaw);
    fz = -cosf(c->yaw);
    c->pos[0] += fx * c->speed * dt;
    c->pos[2] += fz * c->speed * dt;
    if (hits_world(c) || hits_car(c)) {
        /* try sliding along one axis */
        float nx = c->pos[0], nz = c->pos[2];
        c->pos[2] = oz;
        if (hits_world(c) || hits_car(c)) {
            c->pos[0] = ox;
            c->pos[2] = nz;
            if (hits_world(c) || hits_car(c)) {
                c->pos[2] = oz;
                c->yaw = oyaw;
                if (fabsf(c->speed) > 6.0f) ev |= CAR_EV_CRASH;
                c->speed *= -0.25f;
            }
        }
        (void)nx;
    }

    g = ground_below(c->pos[0], c->pos[1], c->pos[2]);
    if (g > -100 && g + 1.0f >= c->pos[1] - 0.01f && g + 1.0f <= c->pos[1] + 1.01f) {
        c->pos[1] = g + 1.0f;
        c->vy = 0.0f;
    } else {
        c->vy -= 25.0f * dt;
        c->pos[1] += c->vy * dt;
        if (g > -100 && c->pos[1] < g + 1.0f) {
            c->pos[1] = g + 1.0f;
            c->vy = 0.0f;
        }
        if (c->pos[1] < -20.0f) c->pos[1] = TOWN_Y + 1.0f;
    }
    return ev;
}

int car_push_out(const Car *c, float *x, float *z, float margin)
{
    float s = sinf(c->yaw), co = cosf(c->yaw);
    float dx = *x - c->pos[0], dz = *z - c->pos[2];
    float lx = dx * co - dz * s;   /* along right */
    float lz = -dx * s - dz * co;  /* along forward */
    float hw = CAR_HALF_W + margin, hl = CAR_HALF_L + margin;
    if (fabsf(lx) >= hw || fabsf(lz) >= hl) return 0;
    if (hw - fabsf(lx) < hl - fabsf(lz)) lx = lx < 0 ? -hw : hw;
    else lz = lz < 0 ? -hl : hl;
    *x = c->pos[0] + lx * co - lz * s;
    *z = c->pos[2] - lx * s - lz * co;
    return 1;
}

void car_exit_point(const Car *c, float out[3])
{
    int side;
    for (side = -1; side <= 1; side += 2) {
        float wx, wz;
        local_to_world(c, side * 2.0f, 0.0f, &wx, &wz);
        if (!solid_at(wx, c->pos[1] + 0.2f, wz) && !solid_at(wx, c->pos[1] + 1.2f, wz)) {
            out[0] = wx;
            out[1] = c->pos[1];
            out[2] = wz;
            return;
        }
    }
    out[0] = c->pos[0];
    out[1] = c->pos[1] + 2.0f;
    out[2] = c->pos[2];
}

/* ---------------------------------------------------------------- traffic */

static float lane_coord(int dir, int band)
{
    /* drive on the right: lanes at 2.0 / 5.0 inside the 7-wide road band */
    float base = (float)(band * LOT);
    switch (dir & 3) {
    case 0: return base + 5.0f; /* +x: z lane */
    case 2: return base + 2.0f; /* -x */
    case 1: return base + 2.0f; /* +z: x lane */
    default: return base + 5.0f;/* -z */
    }
}

static int band_ok(int i)
{
    return i >= TOWN_LOT_MIN && i <= TOWN_LOT_MAX + 1;
}

/* Something in front within dist (player on foot or another car)? */
static int obstacle_ahead(const Car *c, const float *foot, float dist, int *by_player)
{
    float fx = -sinf(c->yaw), fz = -cosf(c->yaw);
    int i;
    *by_player = 0;
    if (foot) {
        float dx = foot[0] - c->pos[0], dz = foot[2] - c->pos[2];
        float along = dx * fx + dz * fz, side = dx * fz - dz * fx;
        if (along > 0.0f && along < dist && fabsf(side) < 1.6f && fabsf(foot[1] - c->pos[1]) < 2.5f) {
            *by_player = 1;
            return 1;
        }
    }
    for (i = 0; i < MAX_CARS; i++) {
        const Car *o = &cars[i];
        float dx, dz, along, side;
        if (o == c || o->kind == CAR_FREE) continue;
        dx = o->pos[0] - c->pos[0];
        dz = o->pos[2] - c->pos[2];
        along = dx * fx + dz * fz;
        side = dx * fz - dz * fx;
        if (along > 0.5f && along < dist + 2.0f && fabsf(side) < 1.8f) {
            if (o->kind == CAR_PLAYER) *by_player = 1;
            return 1;
        }
    }
    return 0;
}

static void traffic_step(Car *c, const float *foot, float dt, int *ev)
{
    int axis = (c->dir & 1) ? 2 : 0, other = 2 - axis, by_player;
    int s = (c->dir == 0 || c->dir == 1) ? 1 : -1;
    float target = TRAFFIC_SPEED, a, look = 3.0f + c->speed * 0.7f;
    int band;

    if (obstacle_ahead(c, foot, look, &by_player)) {
        target = 0.0f;
        c->blocked += dt;
        if (by_player && c->blocked > 1.0f && c->honk_cd <= 0.0f) {
            *ev |= CAR_EV_HONK;
            c->honk_cd = 3.0f;
        }
    } else {
        c->blocked = 0.0f;
    }
    if (c->honk_cd > 0.0f) c->honk_cd -= dt;
    if (c->speed < target) c->speed += 5.0f * dt;
    else c->speed -= 12.0f * dt;
    if (c->speed < 0.0f) c->speed = 0.0f;

    c->pos[axis] += DIR_X[c->dir] * c->speed * dt + DIR_Z[c->dir] * c->speed * dt;
    a = c->pos[axis];

    /* entering a crossing road: choose where to go */
    band = town_div((int)floorf(a));
    if (town_mod((int)floorf(a)) < ROAD_W && band != c->band_done) {
        int bi = town_div((int)floorf(c->pos[0])), bj = town_div((int)floorf(c->pos[2]));
        int opts[3], n = 0, k;
        int cand[3] = { c->dir, (c->dir + 1) & 3, (c->dir + 3) & 3 };
        unsigned int weights[3] = { 2, 1, 1 }, total = 0, pick;
        for (k = 0; k < 3; k++) {
            int nd = cand[k];
            if (band_ok(bi + DIR_X[nd]) && band_ok(bj + DIR_Z[nd])) {
                opts[n++] = k;
                total += weights[k];
            }
        }
        c->band_done = band;
        c->next_dir = c->dir;
        if (n == 0) {
            c->next_dir = (c->dir + 2) & 3; /* dead end: U-turn */
            c->turn_at = (float)(band * LOT) + 3.5f;
        } else {
            pick = rnd() % total;
            for (k = 0; k < n; k++) {
                if (pick < weights[opts[k]]) break;
                pick -= weights[opts[k]];
            }
            if (k >= n) k = n - 1;
            c->next_dir = cand[opts[k]];
            if (c->next_dir != c->dir)
                c->turn_at = lane_coord(c->next_dir, (c->next_dir & 1) ? bi : bj);
        }
    }
    if (c->next_dir != c->dir && s * (a - c->turn_at) >= 0.0f) {
        int nb = (c->next_dir & 1) ? town_div((int)floorf(c->pos[2])) : town_div((int)floorf(c->pos[0]));
        /* turn_at is the lane coordinate of the new direction */
        c->pos[axis] = c->turn_at;
        c->dir = c->next_dir;
        c->band_done = nb;
    }

    /* smooth visual heading */
    {
        float d = wrap_angle(car_dir_yaw(c->dir) - c->yaw);
        float step = 5.0f * dt;
        c->yaw = wrap_angle(c->yaw + (d > step ? step : (d < -step ? -step : d)));
    }
    (void)other;

    {
        int g = ground_below(c->pos[0], c->pos[1], c->pos[2]);
        if (g > -100) c->pos[1] = g + 1.0f;
    }
}

static void chaser_step(Car *c, const float target[3], float dt, int *ev)
{
    float dx = target[0] - c->pos[0], dz = target[2] - c->pos[2];
    float want = atan2f(-dx, -dz), diff = wrap_angle(want - c->yaw);
    float steer = diff * 2.0f, throttle = 1.0f;
    if (steer > 1.0f) steer = 1.0f;
    if (steer < -1.0f) steer = -1.0f;
    if (fabsf(diff) > 1.4f && c->speed > 9.0f) throttle = 0.2f;
    if (dx * dx + dz * dz < 25.0f) throttle = 0.3f;

    if (c->reverse_t > 0.0f) {
        c->reverse_t -= dt;
        throttle = -1.0f;
        steer = -steer;
    } else if (fabsf(c->speed) < 1.0f) {
        c->stuck += dt;
        if (c->stuck > 1.2f) {
            c->stuck = 0.0f;
            c->reverse_t = 1.0f;
        }
    } else {
        c->stuck = 0.0f;
    }
    *ev |= car_physics(c, throttle, steer, dt);
}

static void try_spawn_traffic(const float focus[3])
{
    int axis = (int)(rnd() & 1u), dir = axis ? ((rnd() & 1u) ? 1 : 3) : ((rnd() & 1u) ? 0 : 2);
    int band, i;
    float along = (frnd() * 40.0f + 30.0f) * ((rnd() & 1u) ? 1.0f : -1.0f), pos[3];
    if (axis == 0) {
        band = town_div((int)floorf(focus[2])) + (int)(rnd() % 3u) - 1;
        pos[0] = focus[0] + along;
        pos[2] = lane_coord(dir, band);
    } else {
        band = town_div((int)floorf(focus[0])) + (int)(rnd() % 3u) - 1;
        pos[2] = focus[2] + along;
        pos[0] = lane_coord(dir, band);
    }
    if (!band_ok(band)) return;
    if (!town_contains((int)floorf(pos[0]), (int)floorf(pos[2]))) return;
    if (world_get((int)floorf(pos[0]), TOWN_Y, (int)floorf(pos[2])) == B_UNLOADED) return;
    if (!town_is_road((int)floorf(pos[0]), (int)floorf(pos[2]))) return;
    for (i = 0; i < MAX_CARS; i++) {
        float dx, dz;
        if (cars[i].kind == CAR_FREE) continue;
        dx = cars[i].pos[0] - pos[0];
        dz = cars[i].pos[2] - pos[2];
        if (dx * dx + dz * dz < 64.0f) return;
    }
    i = car_spawn(CAR_TRAFFIC, pos[0], TOWN_Y + 1.0f, pos[2], car_dir_yaw(dir),
                  TRAFFIC_COLORS[rnd() % (sizeof(TRAFFIC_COLORS) / sizeof(TRAFFIC_COLORS[0]))]);
    if (i >= 0) {
        float a = axis ? pos[2] : pos[0];
        cars[i].dir = dir;
        cars[i].next_dir = dir;
        cars[i].speed = TRAFFIC_SPEED;
        cars[i].band_done = town_mod((int)floorf(a)) < ROAD_W ? town_div((int)floorf(a)) : -9999;
    }
}

int cars_update(float dt, const float focus[3], const float *player_foot, int allow_traffic)
{
    static float spawn_timer;
    int i, ev = 0, traffic = 0;

    for (i = 0; i < MAX_CARS; i++) {
        Car *c = &cars[i];
        float dx, dz;
        if (c->kind == CAR_FREE) continue;
        dx = c->pos[0] - focus[0];
        dz = c->pos[2] - focus[2];
        if (world_get((int)floorf(c->pos[0]), (int)floorf(c->pos[1]), (int)floorf(c->pos[2])) == B_UNLOADED) {
            if (c->kind == CAR_TRAFFIC) c->kind = CAR_FREE;
            continue; /* frozen until its chunk loads */
        }
        if (c->kind == CAR_TRAFFIC) {
            if (dx * dx + dz * dz > 110.0f * 110.0f) {
                c->kind = CAR_FREE;
                continue;
            }
            traffic++;
            traffic_step(c, player_foot, dt, &ev);
        } else if (c->kind == CAR_CHASER) {
            chaser_step(c, focus, dt, &ev);
        } else if (c->kind == CAR_PARKED) {
            /* rolling to a stop after the player got out */
            if (c->speed != 0.0f) car_physics(c, 0.0f, 0.0f, dt);
            else car_physics(c, 0.0f, 0.0f, 0.0f);
        }
    }

    spawn_timer -= dt;
    if (allow_traffic && traffic < TRAFFIC_MAX && spawn_timer <= 0.0f &&
        town_distance((int)floorf(focus[0]), (int)floorf(focus[2])) < 40) {
        spawn_timer = 0.4f;
        try_spawn_traffic(focus);
    }
    return ev;
}
