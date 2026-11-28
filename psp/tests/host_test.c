/* Host-side tests for the platform-independent game code.
 * Build & run: make -C psp/tests check */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../src/actor.h"
#include "../src/car.h"
#include "../src/city.h"
#include "../src/crowd.h"
#include "../src/cutscene.h"
#include "../src/ride.h"
#include "../src/story.h"
#include "../src/synth.h"
#include "../src/texgen.h"

static int failures;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); failures++; } } while (0)

static int point_blocked(V3 p, float r)
{
    const Box *b[16];
    int n = city_boxes(p.x - r, p.z - r, p.x + r, p.z + r, b, 16), i;
    for (i = 0; i < n; i++)
        if (b[i]->y1 > p.y + 0.4f && b[i]->y0 < p.y + 1.7f && p.x + r > b[i]->x0 && p.x - r < b[i]->x1 &&
            p.z + r > b[i]->z0 && p.z - r < b[i]->z1)
            return 1;
    return 0;
}

static void test_city(void)
{
    const V3 *pois[] = { &POI_SPAWN, &POI_GARAGE, &POI_TARAS, &POI_CAR, &POI_MARKET, &POI_GALYA,
                         &POI_KIOSK, &POI_SELLER, &POI_KABAN, &POI_KABAN_NPC };
    int i, verts = 0, ground = 0, j;
    for (j = CELL_MIN; j < CELL_MIN + CELLS; j++)
        for (i = CELL_MIN; i < CELL_MIN + CELLS; i++) verts += city_cell(i, j)->solid.n, ground += city_cell(i, j)->ground.n;
    printf("city: %d vertices + %d in ground strips, %d collision boxes\n", verts, ground, city_box_count());
    CHECK(ground > 0 && ground % 3 == 0);
    CHECK(verts > 100000 && verts % 3 == 0);
    CHECK(city_box_count() > 500);
    for (i = 0; i < (int)(sizeof(pois) / sizeof(pois[0])); i++) {
        if (point_blocked(*pois[i], 0.3f)) printf("POI %d blocked at %.1f %.1f\n", i, pois[i]->x, pois[i]->z);
        CHECK(!point_blocked(*pois[i], 0.3f));
        CHECK(city_in_town(pois[i]->x, pois[i]->z));
        CHECK(fabsf(city_ground(pois[i]->x, pois[i]->z) - WALK_H) < 0.01f);
    }
    /* road surface and kerbs */
    CHECK(city_ground(30.0f, 8.0f) == 0.0f);         /* asphalt */
    CHECK(city_ground(30.0f, 1.0f) == WALK_H);       /* sidewalk */
    CHECK(city_on_road(30.0f, 8.0f) && !city_on_road(30.0f, 1.0f));
    CHECK(city_on_road(400.0f, 8.0f));               /* highway */
    CHECK(city_lane(0, 0) == 10.5f && city_lane(2, 0) == 5.5f);
    CHECK(city_lot_type(0, 0) == LOT_PANEL5 && city_lot_type(1, 0) == LOT_GARAGES);
    /* the home block is solid, the yard is not */
    CHECK(point_blocked(v3(40.0f, WALK_H, 34.0f), 0.3f));
    CHECK(!point_blocked(v3(40.0f, WALK_H, 50.0f), 0.3f));
    /* camera ray stops at a wall */
    CHECK(city_raycast(v3(40.0f, 2.0f, 50.0f), v3(0, 0, -1), 30.0f) < 11.0f);
    CHECK(city_raycast(v3(40.0f, 2.0f, 50.0f), v3(0, 0, 1), 5.0f) >= 5.0f);
}

static void test_car_physics(void)
{
    int t, i;
    for (t = 0; t < CAR_TYPES; t++) {
        int k;
        float t100 = -1.0f, target = fminf(100.0f, CAR_SPECS[t].top_kmh * 0.7f);
        Car *c;
        if (CAR_IS_BIKE(t)) continue;   /* tested with the motorcycle */
        cars_clear();
        k = car_spawn(CK_PLAYER, t, v3(-600.0f, 0, 10.5f), -PI_F / 2, 0xFFFFFFFFu);
        c = &cars[k];
        for (i = 0; i < 60 * 45 && t100 < 0; i++) {
            c->in_throttle = 1.0f;
            car_step(c, 1.0f / 60);
            if (car_speed(c) * 3.6f >= target) t100 = i / 60.0f;
            CHECK(!isnan(c->pos.x) && !isnan(c->yaw));
        }
        printf("%-14s 0-%.0f km/h in %.1f s\n", CAR_SPECS[t].name, target, t100);
        CHECK(t100 > 3.0f && t100 < (CAR_SPECS[t].mass > 1600.0f ? 40.0f : 25.0f));
        CHECK(fabsf(c->pos.z - 10.5f) < 1.0f);  /* goes straight */
        CHECK(c->gear >= 2);
    }
    /* steering turns the car, the handbrake makes it slide */
    {
        Car *c;
        float yaw0, maxskid = 0.0f;
        cars_clear();
        c = &cars[car_spawn(CK_PLAYER, CAR_KAISER, v3(400, 0, -300), 0, 0xFFFFFFFFu)];
        c->vel = v3(0, 0, -20.0f);
        yaw0 = c->yaw;
        for (i = 0; i < 90; i++) {
            c->in_throttle = 0.3f;
            c->in_steer = 1.0f;
            c->in_handbrake = i < 40;
            car_step(c, 1.0f / 60);
            if (c->skid > maxskid) maxskid = c->skid;
        }
        CHECK(wrap_pi(c->yaw - yaw0) > 0.5f);     /* turned left */
        CHECK(maxskid > 0.5f);
        CHECK(fabsf(c->body_roll) < 0.2f);
    }
    /* crash into the home block: stops at the wall, reports an impact */
    {
        Car *c;
        float impact = 0.0f;
        cars_clear();
        c = &cars[car_spawn(CK_PLAYER, CAR_LASTIVKA, v3(40.0f, 0, 10.5f), PI_F, 0xFFFFFFFFu)];
        for (i = 0; i < 240; i++) {
            c->in_throttle = 1.0f;
            car_step(c, 1.0f / 60);
            if (c->impact > impact) impact = c->impact;
        }
        CHECK(c->pos.z + CAR_SPECS[CAR_LASTIVKA].length * 0.5f < 28.3f);
        CHECK(impact > 4.0f);
    }
    /* rolling backwards: the gas pedal brakes first, then drives forward */
    {
        Car *c;
        cars_clear();
        c = &cars[car_spawn(CK_PLAYER, CAR_LASTIVKA, v3(400, 0, -300), 0, 0xFFFFFFFFu)];
        c->vel = v3(0, 0, 6.0f); /* moving backwards (forward is -z) */
        c->gear = -1;
        for (i = 0; i < 180; i++) {
            c->in_throttle = 1.0f;
            car_step(c, 1.0f / 60);
        }
        CHECK(car_speed(c) > 3.0f && c->gear >= 1);
        /* and the brake pedal while rolling forward brakes, then reverses */
        for (i = 0; i < 240; i++) {
            c->in_throttle = -1.0f;
            car_step(c, 1.0f / 60);
        }
        CHECK(car_speed(c) < -1.0f && c->gear == -1);
    }
    /* the kerb lifts the car */
    {
        Car *c;
        float maxy = 0.0f;
        cars_clear();
        c = &cars[car_spawn(CK_PLAYER, CAR_POLE, v3(30.0f, 0, 9.0f), 0.0f, 0xFFFFFFFFu)];
        for (i = 0; i < 200; i++) {
            c->in_throttle = 0.8f;
            car_step(c, 1.0f / 60);
            if (c->pos.y > maxy) maxy = c->pos.y;
        }
        CHECK(maxy > 0.08f);
    }
}

static void test_traffic(void)
{
    int t, i, off = 0, samples = 0, turns = 0, last[MAX_CARS];
    V3 focus = v3(30, 0, 30);
    cars_clear();
    for (i = 0; i < MAX_CARS; i++) last[i] = -1;
    for (t = 0; t < 60 * 120; t++) {
        cars_update(1.0f / 60, focus, NULL, 1);
        for (i = 0; i < MAX_CARS; i++) {
            const Car *c = &cars[i];
            if (c->kind != CK_TRAFFIC) continue;
            samples++;
            if (!city_on_road(c->pos.x, c->pos.z)) off++;
            if (last[i] >= 0 && last[i] != c->dir) turns++;
            last[i] = c->dir;
        }
    }
    printf("traffic: %d car-frames, %d off the asphalt, %d turns\n", samples, off, turns);
    CHECK(samples > 20000 && turns > 20);
    CHECK(off * 200 < samples);
    /* traffic stops for a pedestrian in its lane */
    {
        Car *c;
        V3 ped = v3(44.0f, 0.0f, 10.5f); /* before the next crossing, so the car cannot turn away */
        cars_clear();
        c = &cars[car_spawn(CK_TRAFFIC, CAR_KLASYKA, v3(18.0f, 0, 10.5f), -PI_F / 2, 0xFFFFFFFFu)];
        c->dir = 0;
        c->next_dir = 0;
        c->vel = v3(10.0f, 0, 0);
        for (t = 0; t < 60 * 6; t++) cars_update(1.0f / 60, focus, &ped, 0);
        CHECK(c->pos.x < 44.0f - 3.0f);
        CHECK(car_speed(c) < 0.5f);
    }
}

static void test_actor(void)
{
    Ped p;
    int i, steps = 0, ev, landed = 0;
    float dist;
    /* walking: the cycle advances with distance (no foot sliding) */
    ped_init(&p, CH_ANDRIY, v3(30.0f, WALK_H, 50.0f), 0.0f);
    for (i = 0; i < 240; i++) {
        ev = ped_update(&p, v3(1, 0, 0), 0, 0, 1.0f / 60);
        if (ev & PED_EV_STEP) steps++;
    }
    dist = p.pos.x - 30.0f;
    printf("jog: %.1f m in 4 s, %d steps, phase %.1f\n", dist, steps, p.anim.phase);
    CHECK(dist > 12.0f && dist < 19.0f);
    CHECK(steps > 10 && steps < 30);
    CHECK(p.anim.move > 0.9f && p.anim.run > 0.8f);
    CHECK(fabsf(wrap_pi(p.yaw - yaw_of(1, 0))) < 0.1f); /* faces the way it moves */
    /* stopping takes a moment (momentum) */
    ped_update(&p, v3(0, 0, 0), 0, 0, 1.0f / 60);
    CHECK(sqrtf(p.vel.x * p.vel.x + p.vel.z * p.vel.z) > 3.0f);
    for (i = 0; i < 60; i++) ped_update(&p, v3(0, 0, 0), 0, 0, 1.0f / 60);
    CHECK(sqrtf(p.vel.x * p.vel.x + p.vel.z * p.vel.z) < 0.01f);
    /* jump: crouch, take off, land */
    {
        float maxy = p.pos.y;
        ped_update(&p, v3(0, 0, 0), 0, 1, 1.0f / 60);
        CHECK(p.anim.crouch > 0.0f || !p.on_ground);
        for (i = 0; i < 120; i++) {
            ev = ped_update(&p, v3(0, 0, 0), 0, 0, 1.0f / 60);
            if (p.pos.y > maxy) maxy = p.pos.y;
            if (ev & PED_EV_LAND) landed = 1;
        }
        CHECK(maxy - WALK_H > 0.5f && maxy - WALK_H < 1.0f);
        CHECK(landed && p.on_ground);
    }
    /* walls stop the character */
    ped_init(&p, CH_ANDRIY, v3(40.0f, WALK_H, 44.0f), PI_F);
    for (i = 0; i < 300; i++) ped_update(&p, v3(0, 0, -1), 1, 0, 1.0f / 60);
    CHECK(p.pos.z > 40.0f - 0.01f && p.pos.z < 41.0f);
    /* kerbs: walking off the sidewalk onto the road and back */
    ped_init(&p, CH_ANDRIY, v3(30.0f, WALK_H, 1.5f), PI_F);
    for (i = 0; i < 120; i++) ped_update(&p, v3(0, 0, 1), 0, 0, 1.0f / 60);
    CHECK(p.pos.y < 0.01f);
    /* NPC goals */
    ped_init(&p, CH_TARAS, v3(30.0f, WALK_H, 50.0f), 0.0f);
    p.goal = v3(34.0f, WALK_H, 52.0f);
    p.has_goal = 1;
    for (i = 0; i < 600 && p.has_goal; i++) ped_update(&p, v3(0, 0, 0), 0, 0, 1.0f / 60);
    CHECK(!p.has_goal && fabsf(p.pos.x - 34.0f) < 0.3f);
    /* meshes and poses for everyone, every gesture */
    for (i = 0; i < CH_COUNT; i++) {
        GMesh m;
        int g, k, bad = 0;
        ped_init(&p, i, v3(0, 0, 0), 0.3f);
        gm_init(&m);
        for (g = 0; g < GEST_COUNT; g++) {
            p.anim.gesture = g;
            p.anim.gesture_w = 1.0f;
            p.anim.talk = 1.0f;
            p.anim.sit = g == 3 ? 1.0f : 0.0f;
            m.n = 0;
            ped_append_mesh(&m, &p);
            for (k = 0; k < m.n; k++)
                if (isnan(m.v[k].x) || fabsf(m.v[k].y) > 3.0f) bad++;
        }
        CHECK(m.n > 300 && bad == 0);
        gm_free(&m);
    }
}


static int inside_car(const Car *c, V3 p)
{
    V3 d = v3_sub(p, c->pos), f = yaw_fwd(c->yaw), r = yaw_right(c->yaw);
    return fabsf(v3_dot(d, f)) < CAR_SPECS[c->type].length * 0.5f && fabsf(v3_dot(d, r)) < CAR_SPECS[c->type].width * 0.5f;
}

/* Number of triangles lying on another parallel one closer than 2 mm: those
 * fight for the depth buffer and flicker. */
static int coplanar_overlaps(const GMesh *m)
{
    int n = m->n / 3, a, b, count = 0;
    for (a = 0; a < n; a++) {
        const GVertex *g = &m->v[a * 3];
        V3 p0 = v3(g[0].x, g[0].y, g[0].z), p1 = v3(g[1].x, g[1].y, g[1].z), p2 = v3(g[2].x, g[2].y, g[2].z);
        V3 na = v3_cross(v3_sub(p1, p0), v3_sub(p2, p0)), c = v3_scale(v3_add(v3_add(p0, p1), p2), 1.0f / 3.0f);
        if (v3_len(na) < 1e-6f) continue;
        na = v3_norm(na);
        for (b = 0; b < n; b++) {
            const GVertex *h = &m->v[b * 3];
            V3 q[3], nb, q2;
            int i, inside = 1;
            if (a == b) continue;
            for (i = 0; i < 3; i++) q[i] = v3(h[i].x, h[i].y, h[i].z);
            nb = v3_cross(v3_sub(q[1], q[0]), v3_sub(q[2], q[0]));
            if (v3_len(nb) < 1e-6f) continue;
            nb = v3_norm(nb);
            /* parallel and within 2 mm: they fight for the depth buffer whichever way they face
             * (winding differs between primitives, so facing cannot be told from it) */
            if (fabsf(v3_dot(na, nb)) < 0.999f || fabsf(v3_dot(nb, v3_sub(c, q[0]))) > 0.002f) continue;
            q2 = v3_sub(c, v3_scale(nb, v3_dot(nb, v3_sub(c, q[0]))));
            for (i = 0; i < 3; i++)
                if (v3_dot(v3_cross(v3_sub(q[(i + 1) % 3], q[i]), v3_sub(q2, q[i])), nb) < 1e-5f) inside = 0;
            if (inside) {
                if (getenv("UC_DEBUG_OVERLAP"))
                    printf("  overlap at (%.2f %.2f %.2f) n(%.2f %.2f %.2f) with (%.2f %.2f %.2f)-(%.2f %.2f %.2f)-(%.2f %.2f %.2f) col %08x/%08x\n", c.x, c.y, c.z, na.x, na.y, na.z,
                           q[0].x, q[0].y, q[0].z, q[1].x, q[1].y, q[1].z, q[2].x, q[2].y, q[2].z, g[0].color, h[0].color);
                count++;
                break;
            }
        }
    }
    return count;
}

static void test_car_doors(void)
{
    int t, side;
    for (t = 0; t < CAR_TYPES; t++) {
        const CarSpec *s = &CAR_SPECS[t];
        GMesh closed, open;
        int k, i, n_closed;
        float wide = 0.0f, top = 0.0f;
        CarLayout lay;
        if (CAR_IS_BIKE(t)) continue;
        car_layout(t, &lay);
        cars_clear();
        k = car_spawn(CK_PARKED, t, v3(400, 0, -300), 0.0f, 0xFF808080u);
        gm_init(&closed);
        car_append_mesh(&closed, &cars[k]);
        n_closed = closed.n;
        for (i = 0; i < closed.n; i++) {
            wide = fmaxf(wide, fabsf(closed.v[i].x - cars[k].pos.x));
            top = fmaxf(top, closed.v[i].y);
        }
        if (getenv("UC_DEBUG_OVERLAP")) printf("car %d closed\n", t);
        CHECK(coplanar_overlaps(&closed) == 0);
        for (side = 0; side < 2; side++) {
            float hz = cars[k].pos.z - s->length * 0.5f + lay.door_d0, moved = 0.0f, maxy = -1.0f;
            cars[k].door = 1.0f;
            cars[k].door_side = side;
            gm_init(&open);
            car_append_mesh(&open, &cars[k]);
            CHECK(open.n > n_closed); /* trim and cabin added */
            if (getenv("UC_DEBUG_OVERLAP")) printf("car %d open %d\n", t, side);
            CHECK(coplanar_overlaps(&open) == 0);
            for (i = 0; i < open.n; i++) {
                const GVertex *g = &open.v[i];
                float lx = g->x - cars[k].pos.x;
                /* nothing but the open door sticks out further than the closed car */
                if (fabsf(lx) > wide + 0.02f) {
                    CHECK((side ? lx > 0 : lx < 0) && g->z > hz - 0.2f && g->z < hz + 1.7f);
                    moved = fmaxf(moved, fabsf(lx) - s->width * 0.5f);
                }
                maxy = fmaxf(maxy, g->y);
            }
            CHECK(maxy <= top + 0.01f);   /* the cabin stays inside */
            CHECK(moved > 0.6f);          /* the door swung well out */
            gm_free(&open);
        }
        gm_free(&closed);
    }
    cars_clear();
}

static void test_ride(void)
{
    int side, k, i, ev, all = 0, state_seen[RIDE_EXIT_CLOSE + 1] = { 0 }, inside_walk = 0;
    float max_door = 0.0f;
    Ride r;
    Ped p;
    Car *c;
    /* standing on the car's right (passenger) side, behind its middle */
    cars_clear();
    k = car_spawn(CK_PARKED, CAR_LASTIVKA, v3(400, 0, -300), 0.0f, 0xFFFFFFFFu);
    c = &cars[k];
    ped_init(&p, CH_ANDRIY, car_local(c, v3(2.2f, 0, 1.5f)), 0.0f);
    CHECK(car_nearest(p.pos, 4.5f, &side) == k && side == 1);
    ride_reset(&r);
    ride_enter(&r, &p, k, side);
    for (i = 0; i < 600 && r.state != RIDE_DRIVE; i++) {
        ev = ride_update(&r, &p, 1.0f / 60);
        all |= ev;
        state_seen[r.state] = 1;
        if (c->door > max_door) max_door = c->door;
        if (r.state == RIDE_WALK && inside_car(c, p.pos)) inside_walk++;
        CHECK(c->door_side == 1 || c->door < 0.01f);
    }
    CHECK(r.state == RIDE_DRIVE && c->kind == CK_PLAYER);
    CHECK(state_seen[RIDE_OPEN] && state_seen[RIDE_IN] && state_seen[RIDE_CLOSE]);
    CHECK(inside_walk == 0);            /* never walks through the car */
    CHECK(max_door > 0.95f && c->door == 0.0f);
    CHECK(!p.active && (all & RIDE_EV_ENTERED) && (all & RIDE_EV_DOOR));
    CHECK(i > 100);                     /* the whole thing takes a moment, not one frame */
    /* getting out: left door, standing next to the car, able to walk */
    CHECK(ride_exit(&r, &p));
    for (i = 0; i < 300 && r.state != RIDE_FOOT; i++) all |= ride_update(&r, &p, 1.0f / 60);
    CHECK(r.state == RIDE_FOOT && p.active && p.anim.sit == 0.0f && c->door == 0.0f && c->kind == CK_PARKED);
    CHECK(!inside_car(c, p.pos) && (all & RIDE_EV_EXITED));
    ped_update(&p, v3(1, 0, 0), 0, 0, 0.5f);
    CHECK(sqrtf(p.vel.x * p.vel.x + p.vel.z * p.vel.z) > 1.0f);
    /* taking a car from traffic: the driver runs */
    cars_clear();
    k = car_spawn(CK_TRAFFIC, CAR_KLASYKA, v3(400, 0, -300), 0.0f, 0xFFFFFFFFu);
    ped_init(&p, CH_ANDRIY, car_local(&cars[k], v3(-2.0f, 0, 0)), 0.0f);
    ride_reset(&r);
    ride_enter(&r, &p, k, 0);
    all = 0;
    for (i = 0; i < 600 && r.state != RIDE_DRIVE; i++) all |= ride_update(&r, &p, 1.0f / 60);
    CHECK(all & RIDE_EV_FLEE);

    /* a car that keeps driving has to be caught: running after it never
     * teleports into it, and the chase ends when it gets away */
    {
        V3 prev;
        float jump = 0.0f, closest = 1e9f;
        cars_clear();
        k = car_spawn(CK_TRAFFIC, CAR_KLASYKA, v3(400, 0, -300), 0.0f, 0xFFFFFFFFu);
        cars[k].vel = v3_scale(yaw_fwd(cars[k].yaw), 6.0f);
        ped_init(&p, CH_ANDRIY, car_local(&cars[k], v3(-2.0f, 0, 1.0f)), 0.0f);
        ride_reset(&r);
        ride_enter(&r, &p, k, 0);
        all = 0;
        prev = p.pos;
        for (i = 0; i < 60 * 20 && r.state != RIDE_FOOT; i++) {
            V3 d;
            cars[k].in_throttle = 0.6f;
            cars[k].in_steer = 0.0f;
            cars[k].in_handbrake = 0;
            car_step(&cars[k], 1.0f / 60);
            all |= ride_update(&r, &p, 1.0f / 60);
            d = v3_sub(p.pos, prev);
            jump = fmaxf(jump, sqrtf(d.x * d.x + d.z * d.z));
            closest = fminf(closest, v3_len(v3_sub(p.pos, car_door_point(&cars[k], 0))));
            prev = p.pos;
            CHECK(r.state == RIDE_WALK || r.state == RIDE_FOOT);
        }
        CHECK(r.state == RIDE_FOOT && (all & RIDE_EV_GAVE_UP) && !(all & RIDE_EV_FLEE));
        CHECK(jump < 0.2f);            /* ran, never jumped */
        CHECK(cars[k].kind == CK_TRAFFIC);
        printf("chase: gave up after %.1f s, closest %.2f m\n", i / 60.0f, closest);

        /* the same car stopped (at a crossing, or for us): now it can be taken */
        cars_clear();
        k = car_spawn(CK_TRAFFIC, CAR_KLASYKA, v3(400, 0, -300), 0.0f, 0xFFFFFFFFu);
        ped_init(&p, CH_ANDRIY, car_local(&cars[k], v3(-2.0f, 0, 4.0f)), 0.0f);
        ride_reset(&r);
        ride_enter(&r, &p, k, 0);
        all = 0;
        jump = 0.0f;
        prev = p.pos;
        for (i = 0; i < 600 && r.state != RIDE_DRIVE; i++) {
            V3 d;
            all |= ride_update(&r, &p, 1.0f / 60);
            d = v3_sub(p.pos, prev);
            if (p.active) jump = fmaxf(jump, sqrtf(d.x * d.x + d.z * d.z));
            prev = p.pos;
        }
        CHECK(r.state == RIDE_DRIVE && (all & RIDE_EV_FLEE));
        CHECK(jump < 0.2f);
    }
}


/* passers-by: walk the pavements, run or fight, get knocked down */
static void test_crowd(void)
{
    const float dt = 1.0f / 60;
    Ped player;
    int t, i, n, ev, all, inside = 0, moved = 0;
    V3 start[CROWD_MAX];
    cars_clear();
    crowd_reset(7u);
    ped_init(&player, CH_ANDRIY, v3(30.0f, WALK_H, 1.5f), 0.0f);
    for (t = 0; t < 60 * 2; t++) crowd_update(dt, &player, player.pos, 1);
    for (i = 0, n = 0; i < CROWD_MAX; i++) {
        if (crowd[i].state == WK_FREE) continue;
        n++;
        start[i] = crowd[i].ped.pos;
        CHECK(crowd[i].ped.who >= CH_FIRST_PASSER && crowd[i].ped.who < CH_COUNT);
    }
    CHECK(n >= 8);
    for (t = 0; t < 60 * 30; t++) {
        crowd_update(dt, &player, player.pos, 1);
        for (i = 0; i < CROWD_MAX; i++)
            if (crowd[i].state != WK_FREE && point_blocked(crowd[i].ped.pos, 0.2f)) inside++;
    }
    for (i = 0; i < CROWD_MAX; i++)
        if (crowd[i].state != WK_FREE && v3_len(v3_sub(crowd[i].ped.pos, start[i])) > 3.0f) moved++;
    printf("crowd: %d walking, %d moved, %d frames inside walls\n", n, moved, inside);
    CHECK(moved >= 4);
    CHECK(inside < 60 * 30 / 10);

    /* a timid passer-by runs after one punch; a tough one fights back and
     * goes down after five; both get up again */
    crowd_reset(3u);
    for (i = 0; i < 2; i++) {
        Walker *w = &crowd[i];
        memset(w, 0, sizeof(*w));
        ped_init(&w->ped, i ? CH_VOVAN : CH_OLIA, v3(200.0f + i * 20.0f, 0.0f, -300.0f), PI_F);
        w->state = WK_WANDER;
        w->hp = i ? 6 : 3;
    }
    ped_init(&player, CH_ANDRIY, v3(200.0f, 0.0f, -300.0f + 0.85f), 0.0f);  /* facing -z, at Olia */
    all = 0;
    ped_punch(&player);
    for (t = 0; t < 30; t++)
        if (ped_update(&player, v3(0, 0, 0), 0, 0, dt) & PED_EV_PUNCH) all |= crowd_player_punch(&player);
    CHECK(all & CROWD_EV_HIT);
    CHECK(crowd[0].state == WK_FLEE);
    for (t = 0; t < 60 * 3; t++) crowd_update(dt, &player, player.pos, 0);
    CHECK(v3_len(v3_sub(crowd[0].ped.pos, player.pos)) > 6.0f);

    player.pos = v3(220.0f, 0.0f, -300.0f + 0.85f);
    player.punch_t = -1.0f;
    all = 0;
    for (t = 0; t < 60 * 8 && crowd[1].state != WK_DOWN; t++) {
        if (player.punch_t < 0.0f) ped_punch(&player);
        if (ped_update(&player, v3(0, 0, 0), 0, 0, dt) & PED_EV_PUNCH) all |= crowd_player_punch(&player);
        player.yaw = yaw_of(crowd[1].ped.pos.x - player.pos.x, crowd[1].ped.pos.z - player.pos.z);
        all |= crowd_update(dt, &player, player.pos, 0);
        if (getenv("UC_DEBUG_CROWD") && t % 6 == 0)
            printf("  t %.2f st %d hp %d dist %.2f cd %.2f punch_t %.2f yaw %.2f\n", t * dt, crowd[1].state, crowd[1].hp,
                   v3_len(v3_sub(crowd[1].ped.pos, player.pos)), crowd[1].punch_cd, crowd[1].ped.punch_t, crowd[1].ped.yaw);
    }
    CHECK(crowd[1].state == WK_DOWN);
    printf("crowd: tough one down after %.1f s, %s\n", t * dt, (all & CROWD_EV_PLAYER_HIT) ? "hit back" : "never hit back");
    CHECK(all & CROWD_EV_PLAYER_HIT);
    for (t = 0; t < 60 * 8; t++) crowd_update(dt, &player, player.pos, 0);
    CHECK(crowd[1].state != WK_DOWN && crowd[1].ped.anim.knock < 0.1f);

    /* a fighter keeps at it until the player goes down */
    crowd[1].state = WK_FIGHT;
    crowd[1].t = 0.0f;
    crowd[1].ped.fight_mode = 1;
    crowd[1].ped.pos = v3(player.pos.x, 0.0f, player.pos.z - 2.0f);
    all = 0;
    for (t = 0; t < 60 * 20 && !(all & CROWD_EV_PLAYER_DOWN); t++) {
        all |= crowd_update(dt, &player, player.pos, 0);
        ped_update(&player, v3(0, 0, 0), 0, 0, dt);
    }
    CHECK(all & CROWD_EV_PLAYER_DOWN);
    CHECK(player.knock_t > 0.0f);

    /* run over: knocked down by a car, gets up later */
    crowd_reset(5u);
    cars_clear();
    {
        Walker *w = &crowd[0];
        int k = car_spawn(CK_PARKED, CAR_KLASYKA, v3(300.0f, 0.0f, -300.0f), -PI_F / 2, 0xFFFFFFFFu); /* facing +x */
        Car *c = &cars[k];
        ped_init(&w->ped, CH_PETRO, v3(306.0f, 0.0f, -300.0f), 0.0f);
        w->state = WK_WANDER;
        w->hp = 3;
        c->kind = CK_SCRIPT;
        c->vel = v3_scale(yaw_fwd(c->yaw), 11.0f);
        all = 0;
        for (t = 0; t < 60 * 2; t++) {
            c->in_throttle = 0.3f;
            car_step(c, dt);
            all |= crowd_update(dt, NULL, w->ped.pos, 0);
        }
        CHECK(all & CROWD_EV_RUN_OVER);
        CHECK(w->state == WK_DOWN && w->ped.anim.knock > 0.5f);
        c->vel = v3(0, 0, 0);
        c->pos = v3(330.0f, 0.0f, -330.0f);
        for (t = 0; t < 60 * 10; t++) crowd_update(dt, NULL, w->ped.pos, 0);
        CHECK(w->state != WK_DOWN);
    }
    /* traffic stops for a passer-by crossing in front */
    {
        Car *c;
        V3 wp = v3(44.0f, 0.0f, 10.5f);
        cars_clear();
        c = &cars[car_spawn(CK_TRAFFIC, CAR_KLASYKA, v3(18.0f, 0, 10.5f), -PI_F / 2, 0xFFFFFFFFu)];
        c->dir = 0;
        c->next_dir = 0;
        c->vel = v3(10.0f, 0, 0);
        cars_set_walkers(&wp, 1);
        for (t = 0; t < 60 * 6; t++) cars_update(dt, v3(30, 0, 30), NULL, 0);
        CHECK(c->pos.x < 44.0f - 3.0f && car_speed(c) < 0.5f);
        cars_set_walkers(NULL, 0);
    }
    (void)ev;
    cars_clear();
    crowd_reset(1u);
}


/* What a frame costs, counted the way the PSP renderer draws it: vertices
 * sent to the GE (city by level of detail, ground strips, the draw list of
 * cars and people) and vertices cut on the CPU near the camera. Budgets keep
 * new features from quietly eating the frame rate. */
typedef struct { int ge, cut, items, dyn; } FrameCost;

static FrameCost frame_cost(V3 eye, float yaw, float pitch, float radius, const Ped *player)
{
    static CellDraw vis[CELLS * CELLS];
    static GVertex buf[20000];
    FrameCost fc = { 0, 0, 0, 0 };
    ViewCone cone;
    DrawList dl;
    int n = city_visible_cells(eye, yaw, pitch, radius, vis), i, k;
    view_cone(&cone, eye, yaw, pitch, 6.0f);
    for (i = 0; i < n; i++) {
        const CityCell *c = vis[i].cell;
        int inside = view_cone_contains(&cone, v3(c->cx - LOT * 0.5f, -1.0f, c->cz - LOT * 0.5f), v3(c->cx + LOT * 0.5f, 60.0f, c->cz + LOT * 0.5f));
        const GMesh *ms[2] = { &c->ground, &c->solid };
        const GChunk *chs[2] = { c->gchunks, c->chunks };
        int ncs[2] = { c->ngchunks, c->nchunks }, m;
        for (m = 0; m < 2; m++) {
            if (inside) {
                fc.ge += (m == 1 && vis[i].far) ? c->core_n : ms[m]->n;
                continue;
            }
            for (k = 0; k < ncs[m]; k++) {
                int s;
                if (m == 1 && vis[i].far && chs[m][k].first >= c->core_n) break;
                s = view_cone_sphere(&cone, chs[m][k].c, chs[m][k].r);
                if (s > 0) fc.ge += chs[m][k].count;
                else if (s == 0) {
                    int out = gm_cone_clip(ms[m]->v + chs[m][k].first, chs[m][k].count, &cone, buf, 20000);
                    fc.cut += chs[m][k].count;
                    fc.ge += out > 0 ? out : 0;
                }
            }
        }
    }
    dl_init(&dl);
    for (i = 0; i < MAX_CARS; i++) {
        V3 d = v3_sub(cars[i].pos, eye);
        if (cars[i].kind != CK_FREE && d.x * d.x + d.z * d.z < radius * radius && v3_dot(d, yaw_fwd(yaw)) > -8.0f) car_draw(&dl, &cars[i], car_lod(sqrtf(d.x * d.x + d.z * d.z)));
    }
    crowd_draw(&dl, eye, yaw_fwd(yaw), 70.0f, 16.0f);
    if (player) ped_draw(&dl, player, ped_lod(v3_len(v3_sub(player->pos, eye))));
    fc.ge += dl.verts;
    fc.dyn = dl.verts;
    fc.items = dl.n;
    dl_free(&dl);
    return fc;
}

static void test_frame_budget(void)
{
    static const struct { const char *name; float x, y, z, yaw, pitch; } SHOTS[] = {
        { "street", 10.5f, 2.3f, 70.0f, 0.0f, -0.15f },
        { "yard", 30.0f, 2.3f, 58.0f, 0.5f, -0.1f },
        { "crossing", 8.0f, 2.3f, 8.0f, 0.8f, -0.1f },
        { "highway", 300.0f, 2.6f, 10.5f, -PI_F / 2, -0.12f },
        { "town edge", -150.0f, 2.3f, 140.0f, 0.8f, -0.1f },
    };
    const float RADII[2] = { 192.0f, 256.0f };
    Ped player;
    int s, r, t, worst = 0, worst_cut = 0;
    cars_clear();
    crowd_reset(9u);
    ped_init(&player, CH_ANDRIY, v3(10.5f, WALK_H, 66.0f), 0.0f);
    for (t = 0; t < 60 * 20; t++) {
        cars_update(1.0f / 60, player.pos, &player.pos, 1);
        crowd_update(1.0f / 60, &player, player.pos, 1);
    }
    for (s = 0; s < (int)(sizeof(SHOTS) / sizeof(SHOTS[0])); s++)
        for (r = 0; r < 2; r++) {
            int a;
            for (a = 0; a < 8; a++) {   /* look all around */
                FrameCost fc = frame_cost(v3(SHOTS[s].x, SHOTS[s].y, SHOTS[s].z), SHOTS[s].yaw + a * PI_F / 4, SHOTS[s].pitch,
                                          RADII[r], &player);
                if (fc.ge > worst) worst = fc.ge;
                if (fc.cut > worst_cut) worst_cut = fc.cut;
                if (a == 0) printf("frame %-9s r%3.0f: %6d GE vertices, %5d cut on the CPU, %3d draw items (%d vertices)\n", SHOTS[s].name,
                                   RADII[r], fc.ge, fc.cut, fc.items, fc.dyn);
            }
        }
    /* budgets: keep them when adding things (raise only on purpose) */
    CHECK(worst <= 60000);
    CHECK(worst_cut <= 4000);
    printf("frame budget: worst %d GE vertices, %d cut; person %d / %d vertices (full / medium)\n", worst, worst_cut,
           ped_model_verts(CH_ANDRIY, PED_LOD_FULL), ped_model_verts(CH_ANDRIY, PED_LOD_MEDIUM));
    cars_clear();
    crowd_reset(1u);
}


/* the motorcycle: accelerates, leans into turns, brakes, stands on its stand;
 * getting on and off leaves the rider visible and on the seat */
static void test_bike(void)
{
    const float dt = 1.0f / 60;
    Car *c;
    Ped p;
    Ride r;
    int i, k, all;
    float vmax = 0.0f, lean_max = 0.0f, jump = 0.0f;
    V3 prev;
    cars_clear();
    k = car_spawn(CK_PLAYER, CAR_JAVIR, v3(-600.0f, 0, 10.5f), -PI_F / 2, 0);
    c = &cars[k];
    for (i = 0; i < 60 * 10; i++) {
        c->in_throttle = 1.0f;
        c->in_steer = 0.0f;
        car_step(c, dt);
        vmax = fmaxf(vmax, car_speed(c) * 3.6f);
        CHECK(!isnan(c->pos.x));
    }
    printf("bike: %.0f km/h after 10 s, gear %d\n", vmax, c->gear);
    CHECK(vmax > 80.0f && vmax < 130.0f);
    CHECK(fabsf(c->lean) < 0.05f);
    for (i = 0; i < 60 * 2; i++) {          /* steer left: lean left (positive roll) */
        c->in_throttle = 0.3f;
        c->in_steer = 1.0f;
        car_step(c, dt);
        lean_max = fmaxf(lean_max, c->lean);
    }
    CHECK(lean_max > 0.2f && lean_max <= 0.76f);
    for (i = 0; i < 60 * 8; i++) {
        c->in_throttle = -1.0f;
        c->in_steer = 0.0f;
        car_step(c, dt);
    }
    CHECK(fabsf(car_speed(c)) < 1.6f);
    c->kind = CK_PARKED;
    c->vel = v3(0, 0, 0);
    for (i = 0; i < 60 * 2; i++) {
        c->in_throttle = 0.0f;
        car_step(c, dt);
    }
    CHECK(fabsf(c->lean - 0.12f) < 0.02f);     /* on its stand */

    /* getting on and off */
    ped_init(&p, CH_ANDRIY, car_local(c, v3(-2.0f, 0, 0)), 0.0f);
    ride_reset(&r);
    ride_enter(&r, &p, k, 0);
    all = 0;
    prev = p.pos;
    for (i = 0; i < 60 * 8 && r.state != RIDE_DRIVE; i++) {
        V3 d;
        all |= ride_update(&r, &p, dt);
        d = v3_sub(p.pos, prev);
        jump = fmaxf(jump, v3_len(d));
        prev = p.pos;
    }
    CHECK(r.state == RIDE_DRIVE && (all & RIDE_EV_ENTERED) && c->kind == CK_PLAYER);
    CHECK(p.active && p.anim.ride > 0.99f);
    CHECK(jump < 0.2f);
    CHECK(v3_len(v3_sub(p.pos, car_rider_pos(c))) < 0.01f);
    CHECK(ride_exit(&r, &p));
    for (i = 0; i < 60 * 4 && r.state != RIDE_FOOT; i++) all |= ride_update(&r, &p, dt);
    CHECK(r.state == RIDE_FOOT && (all & RIDE_EV_EXITED) && p.anim.ride == 0.0f && c->kind == CK_PARKED);
    cars_clear();
}

static void test_cutscenes_and_story(void)
{
    static Ped actors[N_ACTORS];
    static const int WHO[N_ACTORS] = { CH_ANDRIY, CH_TARAS, CH_GALYA, CH_KABAN, CH_SELLER, CH_THUG, CH_THUG };
    Cutscene cs;
    CsIO io;
    Story s;
    StoryView v;
    StoryOut o;
    int id, i, f;
    memset(&io, 0, sizeof(io));
    for (i = 0; i < N_ACTORS; i++) {
        ped_init(&actors[i], WHO[i], v3(0, 0, 0), 0);
        io.actors[i] = &actors[i];
    }
    for (id = CS_INTRO; id < CS_COUNT; id++) {
        int lines = 0, reqs = 0;
        cs_start(&cs, id);
        for (f = 0; f < 60 * 120 && cs.running; f++) {
            cs_update(&cs, &io, 1.0f / 60, 0, 0);
            reqs |= cs.requests;
            if (cs.sub_text && cs.sub_t < 1.0f / 59) lines++;
            for (i = 0; i < N_ACTORS; i++) ped_update(&actors[i], v3(0, 0, 0), 0, 0, 1.0f / 60);
        }
        printf("cutscene %d: %.1f s, %d lines\n", id, f / 60.0f, lines);
        CHECK(f / 60.0f > lines * 1.2f);   /* every subtitle stays long enough to read */
        CHECK(!cs.running);
        CHECK(lines >= 1);
        if (id == CS_KIOSK) CHECK(reqs & CS_REQ_CHASER);
        if (id == CS_KABAN) CHECK(reqs & CS_REQ_KABAN_SETUP);
        /* skipping works from the start */
        cs_start(&cs, id);
        cs_update(&cs, &io, 1.0f / 60, 0, 1);
        CHECK(!cs.running);
    }

    /* the whole chapter */
    story_start(&s);
    memset(&v, 0, sizeof(v));
    v.pos = POI_SPAWN;
    story_update(&s, &v, 0.1f, &o);
    CHECK(o.scene == CS_INTRO);
    story_scene_done(&s, CS_INTRO, &o);
    CHECK(s.mission == M_HOME);
    story_update(&s, &v, 0.1f, &o);
    CHECK(o.scene == 0);
    v.pos = POI_GARAGE;
    story_update(&s, &v, 0.1f, &o);
    CHECK(o.scene == CS_GARAGE);
    story_scene_done(&s, CS_GARAGE, &o);
    CHECK(s.mission == M_MARKET && o.mission_passed);
    v.pos = POI_MARKET;
    story_update(&s, &v, 0.1f, &o);
    CHECK(o.scene == 0);                              /* must take the car first */
    v.in_car = v.in_own_car = 1;
    story_update(&s, &v, 0.1f, &o);
    story_update(&s, &v, 0.1f, &o);
    CHECK(o.scene == CS_MARKET);
    story_scene_done(&s, CS_MARKET, &o);
    CHECK(s.mission == M_PARCEL && s.timer > 100.0f);
    v.pos = POI_SPAWN;
    for (i = 0; i < 400 && !o.mission_failed; i++) story_update(&s, &v, 1.0f, &o);
    CHECK(o.mission_failed && o.scene == CS_FAIL && s.timer > 100.0f);
    v.pos = POI_KIOSK;
    story_update(&s, &v, 0.1f, &o);
    CHECK(o.scene == CS_KIOSK);
    story_scene_done(&s, CS_KIOSK, &o);
    CHECK(s.mission == M_TAIL && s.money == 800000);
    v.chaser_alive = 1;
    v.chaser_dist = 30.0f;
    for (i = 0; i < 100; i++) story_update(&s, &v, 0.1f, &o);
    CHECK(s.mission == M_TAIL);
    v.chaser_dist = 150.0f;
    for (i = 0; i < 60 && s.mission == M_TAIL; i++) story_update(&s, &v, 0.1f, &o);
    CHECK(s.mission == M_KABAN && o.pager && o.kaban_setup && o.remove_chaser);
    v.pos = POI_KABAN;
    story_update(&s, &v, 0.1f, &o);
    CHECK(o.scene == CS_KABAN);
    story_scene_done(&s, CS_KABAN, &o);
    CHECK(s.mission == M_FREE && s.money == 1000000);
}

static void test_audio(void)
{
    static short buf[SYNTH_RATE * 2 * 4];
    int t, c, i, n;
    double sum;
    synth_init();
    for (t = 0; t < MUS_COUNT; t++) {
        int lead = synth_track_steps(t, 0);
        for (c = 1; c < 4; c++) CHECK(synth_track_steps(t, c) > 0 && lead % synth_track_steps(t, c) == 0);
    }
    synth_music(MUS_DISCO);
    synth_sfx(SFX_STEP);
    synth_render(buf, 4096);
    sum = 0.0;
    for (i = 0; i < 8192; i++) sum += (double)buf[i] * buf[i];
    CHECK(sum > 0.0);
    (void)n;
}

static void test_text_and_textures(void)
{
    static unsigned int atlas[TEX_SIZE * TEX_SIZE];
    const char *s = "Ґанок Їжака", *p = s;
    unsigned int cps[16];
    int n = 0, i, transparent = 0;
    while (*p && n < 16) cps[n++] = utf8_next(&p);
    CHECK(n == 11 && cps[0] == 0x490 && cps[6] == 0x407);
    CHECK(font_glyph('A') == font_glyph(0x410));
    CHECK(font_glyph(0x44F) == font_glyph(0x42F));
    CHECK(font_glyph(0x2715) >= 64);
    texgen_atlas(atlas);
    for (i = 0; i < TEX_SIZE * TEX_SIZE; i++) transparent += (atlas[i] >> 24) == 0;
    CHECK(transparent > 1000);
    CHECK((atlas[0] >> 24) == 0xFF);
}

int main(void)
{
    city_build(1996);
    carmodels_build();
    test_city();
    test_car_physics();
    test_traffic();
    test_actor();
    test_ride();
    test_car_doors();
    test_crowd();
    test_bike();
    test_frame_budget();
    test_cutscenes_and_story();
    test_audio();
    CHECK(cs_line_time("Тихо.") > 1.0f && cs_line_time("Мені такі люди потрібні. Робота чиста, гроші — в доларах.") > 3.5f);
    test_text_and_textures();
    city_free();
    if (failures) {
        printf("%d check(s) failed\n", failures);
        return 1;
    }
    printf("all host tests passed\n");
    return 0;
}
