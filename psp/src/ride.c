#include "ride.h"
#include "city.h"

#include <string.h>

#define T_OPEN 0.7f
#define T_IN 1.25f      /* to the opening, sit down sideways, swing round into the seat */
#define T_SLIDE 0.55f   /* extra time to slide across from the passenger seat */
#define T_MOUNT 0.75f   /* swing a leg over the motorcycle */
#define T_CLOSE 0.45f
#define GRAB_DIST 1.0f      /* close enough to the door to grab the handle */
#define GRAB_SPEED 2.5f     /* m/s: a car going faster cannot be opened */
#define CHASE_GIVE_UP 20.0f /* the car got this far away: stop chasing */
#define WALK_GIVE_UP 12.0f  /* seconds */
#define T_EXIT_OPEN 0.45f
#define T_EXIT_OUT 1.1f
#define T_EXIT_CLOSE 0.6f

static int is_bike(const Car *c) { return CAR_SPECS[c->type].body == BODY_BIKE; }

/* a point beside the car: x across (negative left), d from the front bumper, y up */
static V3 at(const Car *c, float x, float y, float d)
{
    V3 p = car_local(c, v3(x, 0.0f, -CAR_SPECS[c->type].length * 0.5f + d));
    p.y = c->pos.y + y;
    return p;
}

/* Where getting in passes: E just outside the opening, S1 on the seat edge
 * (sitting sideways), S on the seat (sx: which seat, -1 driver) */
static void entry_points(const Car *c, int side, float seat_side, V3 *E, V3 *S1, V3 *S)
{
    const CarSpec *s = &CAR_SPECS[c->type];
    CarLayout L;
    float sx = side ? 1.0f : -1.0f, hw = s->width * 0.5f;
    car_layout(c->type, &L);
    *E = at(c, sx * (hw + 0.3f), 0.0f, L.seat_d);
    *S1 = at(c, sx * hw * 0.66f, L.seat_y, L.seat_d);
    *S = at(c, seat_side * fabsf(L.seat_x), L.seat_y, L.seat_d);
}

/* facing away from the car on that side (the back to the seat) */
static float away_yaw(const Car *c, int side)
{
    V3 r = yaw_right(c->yaw);
    return side ? yaw_of(r.x, r.z) : yaw_of(-r.x, -r.z);
}

static float turn_to(float from, float to, float k) { return wrap_pi(from + wrap_pi(to - from) * k); }

void ride_reset(Ride *r)
{
    memset(r, 0, sizeof(*r));
    r->car = -1;
}

int ride_busy(const Ride *r) { return r->state != RIDE_FOOT && r->state != RIDE_DRIVE; }

static float face_car_yaw(const Car *c, int side)
{
    V3 r = yaw_right(c->yaw);
    /* standing on the left we look right (into the car), and vice versa */
    return side ? yaw_of(-r.x, -r.z) : yaw_of(r.x, r.z);
}

static int door_blocked(const Car *c, int side)
{
    V3 p = car_door_point(c, side);
    const Box *b[8];
    int n = city_boxes(p.x - 0.35f, p.z - 0.35f, p.x + 0.35f, p.z + 0.35f, b, 8), i;
    for (i = 0; i < n; i++)
        if (b[i]->y1 > p.y + 0.5f && p.x + 0.3f > b[i]->x0 && p.x - 0.3f < b[i]->x1 &&
            p.z + 0.3f > b[i]->z0 && p.z - 0.3f < b[i]->z1)
            return 1;
    return 0;
}

void ride_enter(Ride *r, Ped *p, int car, int side)
{
    Car *c = &cars[car];
    r->state = RIDE_WALK;
    r->car = car;
    r->side = side;
    r->t = 0.0f;
    p->goal = car_door_point(c, side);
    p->has_goal = 1;
    p->walk_speed = 3.0f;
    /* a car with a driver keeps going: it has to be caught first (or stopped
     * by standing in its way), like in the classic open-world games */
}

void ride_cancel(Ride *r, Ped *p)
{
    if (r->state != RIDE_WALK) return;
    p->has_goal = 0;
    ride_reset(r);
}

int ride_exit(Ride *r, Ped *p)
{
    Car *c = &cars[r->car];
    (void)p;
    if (car_speed(c) > 3.0f || car_speed(c) < -3.0f) return 0;
    r->side = door_blocked(c, 0) && !door_blocked(c, 1) ? 1 : 0;
    r->state = is_bike(c) ? RIDE_EXIT_OUT : RIDE_EXIT_OPEN;
    r->t = 0.0f;
    c->door_side = r->side;
    c->kind = CK_PARKED;   /* handbrake on, rolls to a stop */
    return 1;
}

void ride_force_out(Ride *r, Ped *p)
{
    if (r->car >= 0) {
        Car *c = &cars[r->car];
        p->pos = car_door_point(c, 0);
        c->kind = CK_PARKED;
        c->door = 0.0f;
    }
    p->active = 1;
    p->has_goal = 0;
    p->anim.sit = 0.0f;
    p->anim.ride = 0.0f;
    p->anim.lean = 0.0f;
    p->anim.crouch = 0.0f;
    p->anim.gesture = GEST_NONE;
    ride_reset(r);
}

int ride_update(Ride *r, Ped *p, float dt)
{
    Car *c;
    int ev = 0;
    if (r->state == RIDE_FOOT) return 0;
    c = &cars[r->car];
    if (c->kind == CK_FREE) {
        if (r->state == RIDE_WALK) {
            /* the car we were after is gone: just stop, no jump to it */
            ride_cancel(r, p);
            return RIDE_EV_GAVE_UP;
        }
        ride_force_out(r, p);
        return RIDE_EV_EXITED;
    }
    r->t += dt;
    switch (r->state) {
    case RIDE_WALK: {
        /* run after the door (the car may still be driving), never through
         * the body; the door can only be opened standing at it with the car
         * (almost) stopped */
        V3 door = car_door_point(c, r->side), d = v3_sub(door, p->pos), dc = v3_sub(c->pos, p->pos);
        float dist = sqrtf(d.x * d.x + d.z * d.z), dcar = sqrtf(dc.x * dc.x + dc.z * dc.z);
        float speed = fabsf(car_speed(c));
        p->goal = door;
        p->has_goal = 1;
        p->walk_speed = (speed > 0.5f || dist > 3.0f) ? 6.4f : 3.0f;
        ped_update(p, v3(0, 0, 0), 0, 0, dt);
        car_push_point(&p->pos, 0.32f);
        if (dcar > CHASE_GIVE_UP || r->t > WALK_GIVE_UP) {
            ride_cancel(r, p);
            return RIDE_EV_GAVE_UP;
        }
        if (dist < GRAB_DIST && speed < GRAB_SPEED) {
            if (c->kind == CK_TRAFFIC) {
                /* caught it: the driver stamps on the brake */
                c->kind = CK_SCRIPT;
                c->in_throttle = 0.0f;
                c->in_steer = 0.0f;
                c->in_handbrake = 1;
            }
            p->has_goal = 0;
            p->vel = v3(0, 0, 0);
            p->goal_yaw = face_car_yaw(c, r->side);
            p->face_goal_yaw = 1;
            r->state = is_bike(c) ? RIDE_IN : RIDE_OPEN;  /* no door on a motorcycle */
            r->t = 0.0f;
            r->start = p->pos;
            r->start_yaw = p->yaw;
            c->door_side = r->side;
            if (!is_bike(c)) ev |= RIDE_EV_DOOR;
        }
        break;
    }
    case RIDE_OPEN:
        /* reach for the handle, then swing the door open; hold on to it
         * while a caught car brakes to a stop */
        ped_update(p, v3(0, 0, 0), 0, 0, dt);
        {
            V3 door = car_door_point(c, r->side);
            p->pos = v3_lerp(p->pos, door, clampf(dt * 8.0f, 0.0f, 1.0f));
        }
        p->anim.gesture = r->t < 0.55f ? GEST_POINT : GEST_NONE;
        c->door = smooth01((r->t - 0.25f) / 0.45f);
        if (r->t >= T_OPEN) {
            if (c->kind == CK_SCRIPT && !c->own && c->type != CAR_GELENDER) {
                /* the old driver jumps out of his own door and runs off */
                V3 f = yaw_right(c->yaw);
                r->flee_pos = car_door_point(c, r->side ? 0 : 1);
                r->flee_yaw = r->side ? yaw_of(-f.x, -f.z) : yaw_of(f.x, f.z);
                ev |= RIDE_EV_FLEE;
            }
            p->anim.gesture = GEST_NONE;
            r->state = RIDE_IN;
            r->t = 0.0f;
            r->start = p->pos;
            r->start_yaw = p->yaw;
        }
        break;
    case RIDE_IN:
        if (is_bike(c)) {
            /* swing a leg over and settle on the seat */
            float k = smooth01(clampf(r->t / T_MOUNT, 0.0f, 1.0f));
            p->pos = v3_lerp(r->start, car_rider_pos(c), k);
            p->yaw = turn_to(r->start_yaw, c->yaw, k);
            p->anim.ride = k;
            p->anim.crouch = 0.3f * sinf(k * PI_F);
            p->anim.lean = c->lean * k;
            if (r->t >= T_MOUNT) {
                c->kind = CK_PLAYER;
                c->lean = 0.0f;
                p->anim.crouch = 0.0f;
                r->state = RIDE_DRIVE;
                ev |= RIDE_EV_ENTERED;
            }
        } else {
            /* step into the opening and turn the back to the seat, sit down
             * sideways on its edge, then swing round with the legs coming in;
             * from the passenger side slide across to the wheel */
            float t = r->t, seat_side = r->side ? 1.0f : -1.0f, ya = away_yaw(c, r->side);
            V3 E, S1, S, D;
            entry_points(c, r->side, seat_side, &E, &S1, &S);
            if (t < 0.35f) {
                float k = smooth01(t / 0.35f);
                p->pos = v3_lerp(r->start, E, k);
                p->yaw = turn_to(r->start_yaw, ya, k);
                p->anim.sit = 0.0f;
                p->anim.crouch = 0.15f * k;
            } else if (t < 0.75f) {
                float k = smooth01((t - 0.35f) / 0.4f);
                p->pos = v3_lerp(E, S1, k);
                p->yaw = ya;
                p->anim.sit = k;
                p->anim.crouch = 0.15f + 0.45f * sinf(k * PI_F);
            } else if (t < T_IN) {
                float k = smooth01((t - 0.75f) / (T_IN - 0.75f));
                p->pos = v3_lerp(S1, S, k);
                p->yaw = turn_to(ya, c->yaw, k);
                p->anim.sit = 1.0f;
                p->anim.crouch = 0.25f * (1.0f - k);
            } else {
                float k = r->side ? smooth01((t - T_IN) / T_SLIDE) : 1.0f;
                entry_points(c, r->side, -1.0f, &E, &S1, &D);
                p->pos = v3_lerp(S, D, k);
                p->yaw = c->yaw;
                p->anim.sit = 1.0f;
                p->anim.crouch = 0.0f;
                if (k >= 1.0f) {
                    r->state = RIDE_CLOSE;
                    r->t = 0.0f;
                }
            }
        }
        break;
    case RIDE_CLOSE:
        /* pull the door shut from the inside, then disappear into the car */
        c->door = 1.0f - smooth01(r->t / T_CLOSE);
        p->anim.gesture = r->t < T_CLOSE * 0.8f ? GEST_POINT : GEST_NONE;
        if (r->t >= T_CLOSE) {
            c->door = 0.0f;
            c->kind = CK_PLAYER;
            p->anim.sit = 1.0f;
            p->anim.gesture = GEST_NONE;
            p->active = 0;
            r->state = RIDE_DRIVE;
            ev |= RIDE_EV_DOOR | RIDE_EV_ENTERED;
        }
        break;
    case RIDE_DRIVE:
        if (is_bike(c)) {
            p->pos = car_rider_pos(c);
            p->anim.ride = 1.0f;
            p->anim.lean = c->lean;
        } else {
            p->pos = car_seat_point(c);
        }
        p->yaw = c->yaw;
        break;
    case RIDE_EXIT_OPEN:
        c->door = smooth01(r->t / T_EXIT_OPEN);
        if (r->t >= T_EXIT_OPEN) {
            V3 E, S1, S;
            entry_points(c, r->side, -1.0f, &E, &S1, &S);
            p->active = 1;
            p->pos = S;
            p->yaw = c->yaw;
            p->vel = v3(0, 0, 0);
            p->anim.sit = 1.0f;
            r->state = RIDE_EXIT_OUT;
            r->t = 0.0f;
            ev |= RIDE_EV_DOOR;
        }
        break;
    case RIDE_EXIT_OUT:
        if (is_bike(c)) {
            float k = smooth01(clampf(r->t / T_MOUNT, 0.0f, 1.0f));
            p->pos = v3_lerp(car_rider_pos(c), car_door_point(c, r->side), k);
            p->yaw = c->yaw;
            p->anim.ride = 1.0f - k;
            p->anim.lean = c->lean * (1.0f - k);
            p->anim.crouch = 0.3f * sinf(k * PI_F);
            if (r->t >= T_MOUNT) {
                p->anim.ride = 0.0f;
                p->anim.lean = 0.0f;
                p->anim.crouch = 0.0f;
                p->pos.y = city_ground(p->pos.x, p->pos.z);
                ev |= RIDE_EV_EXITED;
                r->state = RIDE_FOOT;
                r->car = -1;
            }
        } else {
            /* the reverse: (slide across,) swing the legs out, stand up, step away */
            float slide = r->side ? T_SLIDE : 0.0f, t = r->t, ya = away_yaw(c, r->side);
            V3 E, S1, S, D, door = car_door_point(c, r->side);
            entry_points(c, r->side, r->side ? 1.0f : -1.0f, &E, &S1, &S);
            if (t < slide) {
                entry_points(c, r->side, -1.0f, &E, &S1, &D);
                p->pos = v3_lerp(D, S, smooth01(t / slide));
                p->yaw = c->yaw;
            } else if (t < slide + 0.4f) {
                float k = smooth01((t - slide) / 0.4f);
                p->pos = v3_lerp(S, S1, k);
                p->yaw = turn_to(c->yaw, ya, k);
                p->anim.sit = 1.0f;
            } else if (t < slide + 0.8f) {
                float k = smooth01((t - slide - 0.4f) / 0.4f);
                p->pos = v3_lerp(S1, E, k);
                p->yaw = ya;
                p->anim.sit = 1.0f - k;
                p->anim.crouch = 0.45f * sinf(k * PI_F);
            } else {
                float k = smooth01((t - slide - 0.8f) / (T_EXIT_OUT - 0.8f));
                p->pos = v3_lerp(E, door, k);
                p->yaw = ya;
                p->anim.sit = 0.0f;
                p->anim.crouch = 0.0f;
                if (t >= slide + T_EXIT_OUT) {
                    p->pos.y = city_ground(p->pos.x, p->pos.z);
                    r->state = RIDE_EXIT_CLOSE;
                    r->t = 0.0f;
                }
            }
        }
        break;
    case RIDE_EXIT_CLOSE:
        /* turn round and push the door shut */
        ped_update(p, v3(0, 0, 0), 0, 0, dt);
        p->goal_yaw = face_car_yaw(c, r->side);
        p->face_goal_yaw = 1;
        p->anim.gesture = r->t > 0.15f && r->t < 0.4f ? GEST_POINT : GEST_NONE;
        c->door = r->t < 0.2f ? 1.0f : 1.0f - smooth01((r->t - 0.2f) / 0.35f);
        if (r->t >= T_EXIT_CLOSE) {
            c->door = 0.0f;
            p->anim.gesture = GEST_NONE;
            ev |= RIDE_EV_DOOR | RIDE_EV_EXITED;
            r->state = RIDE_FOOT;
            r->car = -1;
        }
        break;
    default:
        break;
    }
    return ev;
}
