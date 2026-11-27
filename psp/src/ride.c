#include "ride.h"
#include "city.h"

#include <string.h>

#define T_OPEN 0.7f
#define T_IN 0.75f
#define T_SLIDE 0.55f   /* extra time to slide across from the passenger seat */
#define T_CLOSE 0.4f
#define T_EXIT_OPEN 0.45f
#define T_EXIT_OUT 0.6f
#define T_EXIT_CLOSE 0.6f

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
    if (c->kind == CK_TRAFFIC) {
        /* the driver stops */
        c->kind = CK_SCRIPT;
        c->in_throttle = -1.0f;
        c->in_steer = 0.0f;
    }
}

int ride_exit(Ride *r, Ped *p)
{
    Car *c = &cars[r->car];
    (void)p;
    if (car_speed(c) > 3.0f || car_speed(c) < -3.0f) return 0;
    r->side = door_blocked(c, 0) && !door_blocked(c, 1) ? 1 : 0;
    r->state = RIDE_EXIT_OPEN;
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
        ride_force_out(r, p);
        return RIDE_EV_EXITED;
    }
    r->t += dt;
    switch (r->state) {
    case RIDE_WALK:
        /* keep the goal on the door even if the car is still rolling */
        p->goal = car_door_point(c, r->side);
        ped_update(p, v3(0, 0, 0), 0, 0, dt);
        car_push_point(&p->pos, 0.32f);  /* walk around the body, never through it */
        if (!p->has_goal || r->t > 4.0f) {
            p->has_goal = 0;
            p->vel = v3(0, 0, 0);
            p->goal_yaw = face_car_yaw(c, r->side);
            p->face_goal_yaw = 1;
            r->state = RIDE_OPEN;
            r->t = 0.0f;
            c->door_side = r->side;
            ev |= RIDE_EV_DOOR;
        }
        break;
    case RIDE_OPEN:
        /* reach for the handle, then swing the door open */
        ped_update(p, v3(0, 0, 0), 0, 0, dt);
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
        }
        break;
    case RIDE_IN: {
        /* duck, step in and sit down; from the right side slide across to the wheel */
        float total = T_IN + (r->side ? T_SLIDE : 0.0f), k = clampf(r->t / total, 0.0f, 1.0f);
        V3 door = car_door_point(c, r->side), seat = car_seat_point(c);
        p->anim.crouch = 0.75f * sinf(clampf(r->t / T_IN, 0.0f, 1.0f) * PI_F);
        p->anim.sit = smooth01((r->t - T_IN * 0.4f) / (T_IN * 0.6f));
        p->pos = v3_lerp(door, seat, smooth01(k));
        p->yaw = wrap_pi(p->yaw + wrap_pi(c->yaw - p->yaw) * clampf(dt * 5.0f, 0.0f, 1.0f));
        if (k >= 1.0f) {
            p->active = 0;
            r->state = RIDE_CLOSE;
            r->t = 0.0f;
        }
        break;
    }
    case RIDE_CLOSE:
        c->door = 1.0f - smooth01(r->t / T_CLOSE);
        if (r->t >= T_CLOSE) {
            c->door = 0.0f;
            c->kind = CK_PLAYER;
            p->anim.sit = 1.0f;
            r->state = RIDE_DRIVE;
            ev |= RIDE_EV_DOOR | RIDE_EV_ENTERED;
        }
        break;
    case RIDE_DRIVE:
        p->pos = car_seat_point(c);
        p->yaw = c->yaw;
        break;
    case RIDE_EXIT_OPEN:
        c->door = smooth01(r->t / T_EXIT_OPEN);
        if (r->t >= T_EXIT_OPEN) {
            p->active = 1;
            p->pos = car_seat_point(c);
            p->yaw = c->yaw;
            p->vel = v3(0, 0, 0);
            r->state = RIDE_EXIT_OUT;
            r->t = 0.0f;
            ev |= RIDE_EV_DOOR;
        }
        break;
    case RIDE_EXIT_OUT: {
        /* turn to the door, duck out and stand up */
        float k = clampf(r->t / T_EXIT_OUT, 0.0f, 1.0f);
        float out_yaw = face_car_yaw(c, r->side) + PI_F;
        p->pos = v3_lerp(car_seat_point(c), car_door_point(c, r->side), smooth01(k));
        p->yaw = wrap_pi(c->yaw + wrap_pi(out_yaw - c->yaw) * smooth01(k));
        p->anim.sit = 1.0f - smooth01(k * 1.4f);
        p->anim.crouch = 0.7f * sinf(k * PI_F);
        if (k >= 1.0f) {
            p->anim.sit = 0.0f;
            p->anim.crouch = 0.0f;
            r->state = RIDE_EXIT_CLOSE;
            r->t = 0.0f;
        }
        break;
    }
    case RIDE_EXIT_CLOSE:
        /* take half a step away, then push the door shut */
        ped_update(p, v3(0, 0, 0), 0, 0, dt);
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
