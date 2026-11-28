#include "crowd.h"
#include "car.h"
#include "city.h"

#include <string.h>

Walker crowd[CROWD_MAX];

static unsigned int rng = 1u;
static float player_hurt;    /* blows taken recently */
static float assault_t;      /* time since the player last hit someone */
static V3 assault_at;

static float frand(void)
{
    /* xorshift: consecutive values are not correlated like an LCG's */
    rng ^= rng << 13;
    rng ^= rng >> 17;
    rng ^= rng << 5;
    return (float)(rng >> 8) / 16777216.0f;
}

void crowd_reset(unsigned int seed)
{
    memset(crowd, 0, sizeof(crowd));
    rng = seed ? seed * 2654435761u : 1u;
    if (!rng) rng = 1u;
    player_hurt = 0.0f;
    assault_t = 99.0f;
}

static int blocked(V3 p)
{
    const Box *b[8];
    int n = city_boxes(p.x - 0.35f, p.z - 0.35f, p.x + 0.35f, p.z + 0.35f, b, 8), i;
    for (i = 0; i < n; i++)
        if (b[i]->y1 > p.y + 0.5f && b[i]->y0 < p.y + 1.7f && p.x + 0.3f > b[i]->x0 && p.x - 0.3f < b[i]->x1 &&
            p.z + 0.3f > b[i]->z0 && p.z - 0.3f < b[i]->z1)
            return 1;
    return 0;
}

static int walkable(V3 p)
{
    return city_in_town(p.x, p.z) && !blocked(p);
}

/* a pavement point near p: the middle of the nearest strip along a road */
static V3 pavement_near(V3 p, int axis)
{
    float c = axis ? p.x : p.z, band = floorf(c / LOT) * LOT, off = c - band;
    float lane = off < 8.0f ? 1.5f : 14.5f;
    if (off >= ROAD) lane = off < (ROAD + LOT) * 0.5f ? 14.5f : LOT + 1.5f;
    if (axis) p.x = band + lane;
    else p.z = band + lane;
    p.y = city_ground(p.x, p.z);
    return p;
}

static void new_goal(Walker *w)
{
    Ped *p = &w->ped;
    int tries;
    for (tries = 0; tries < 4; tries++) {
        float dist = 8.0f + frand() * 18.0f, sgn = frand() < 0.5f ? -1.0f : 1.0f;
        V3 g = p->pos;
        if (w->axis) g.z += sgn * dist;
        else g.x += sgn * dist;
        g = pavement_near(g, w->axis);
        {
            /* do not stop in the middle of a crossing: walk on to the far kerb */
            int k;
            for (k = 0; k < 8 && city_ground(g.x, g.z) < WALK_H * 0.5f; k++) {
                if (w->axis) g.z += sgn * 2.0f;
                else g.x += sgn * 2.0f;
            }
        }
        if (walkable(g)) {
            p->goal = g;
            p->has_goal = 1;
            return;
        }
        w->axis ^= 1; /* try the other street at a corner */
    }
    p->has_goal = 0;
}

static void spawn(Walker *w, V3 focus)
{
    int tries;
    for (tries = 0; tries < 6; tries++) {
        float a = frand() * 2.0f * PI_F, r = 35.0f + frand() * 25.0f;
        int axis = frand() < 0.5f;
        V3 p = pavement_near(v3(focus.x + cosf(a) * r, 0.0f, focus.z + sinf(a) * r), axis);
        int who = CH_FIRST_PASSER + (int)(frand() * CH_PASSERS) % CH_PASSERS;
        if (!walkable(p) || city_ground(p.x, p.z) < WALK_H * 0.5f) continue;
        memset(w, 0, sizeof(*w));
        ped_init(&w->ped, who, p, 0.0f);
        w->ped.walk_speed = 1.1f + frand() * 0.5f;
        w->ped.anim.t = frand() * 10.0f;
        w->axis = axis;
        w->state = WK_WANDER;
        w->hp = LOOKS[who].temper == TEMPER_FIGHT ? 6 : 3;
        new_goal(w);
        return;
    }
}

static void flee_from(Walker *w, V3 from)
{
    Ped *p = &w->ped;
    V3 d = v3_sub(p->pos, from);
    float l = sqrtf(d.x * d.x + d.z * d.z);
    if (l < 0.1f) d = yaw_fwd(p->yaw), l = 1.0f;
    w->state = WK_FLEE;
    w->t = 0.0f;
    w->threat = from;
    p->fight_mode = 0;
    p->goal = v3(p->pos.x + d.x / l * 25.0f, p->pos.y, p->pos.z + d.z / l * 25.0f);
    p->has_goal = 1;
    p->walk_speed = 4.6f + frand() * 1.0f;
}

static void start_fight(Walker *w)
{
    w->state = WK_FIGHT;
    w->t = 0.0f;
    w->punch_cd = 0.4f + frand() * 0.5f;
    w->ped.fight_mode = 1;
}

/* trouble at `where` (a fight, a car hitting someone): the timid run */
static void alarm(V3 where, float radius, const Walker *except)
{
    int i;
    for (i = 0; i < CROWD_MAX; i++) {
        Walker *w = &crowd[i];
        V3 d;
        if (w == except || w->state != WK_WANDER) continue;
        d = v3_sub(w->ped.pos, where);
        if (d.x * d.x + d.z * d.z > radius * radius) continue;
        if (w->ped.who == CH_STEPAN) start_fight(w);   /* the policeman steps in */
        else if (LOOKS[w->ped.who].temper == TEMPER_FLEE || frand() < 0.5f) flee_from(w, where);
    }
}

static void knock_down(Walker *w, float time)
{
    w->state = WK_DOWN;
    w->t = 0.0f;
    w->ped.knock_t = time;
    w->ped.has_goal = 0;
    w->ped.fight_mode = 0;
    w->ped.punch_t = -1.0f;
}

int crowd_player_punch(const Ped *player)
{
    int i, best = -1;
    float bd = 1e9f;
    V3 f = yaw_fwd(player->yaw);
    for (i = 0; i < CROWD_MAX; i++) {
        Walker *w = &crowd[i];
        V3 d;
        float dist;
        if (w->state == WK_FREE || w->state == WK_DOWN) continue;
        d = v3_sub(w->ped.pos, player->pos);
        dist = sqrtf(d.x * d.x + d.z * d.z);
        if (dist > PUNCH_REACH + 0.25f || fabsf(d.y) > 1.0f) continue;
        if (dist > 0.2f && (d.x * f.x + d.z * f.z) / dist < 0.35f) continue;
        if (dist < bd) bd = dist, best = i;
    }
    assault_t = 0.0f;
    assault_at = player->pos;
    if (best < 0) return 0;
    {
        Walker *w = &crowd[best];
        ped_take_hit(&w->ped, player->pos, 1.0f);
        w->hp--;
        if (w->hp <= 0) {
            knock_down(w, 4.5f);
            w->hp = LOOKS[w->ped.who].temper == TEMPER_FIGHT ? 4 : 2;
        } else if (LOOKS[w->ped.who].temper == TEMPER_FIGHT) {
            if (w->state != WK_FIGHT) start_fight(w);
        } else {
            flee_from(w, player->pos);
        }
        alarm(w->ped.pos, 14.0f, w);
    }
    return CROWD_EV_HIT;
}

int crowd_fighting(void)
{
    int i;
    for (i = 0; i < CROWD_MAX; i++)
        if (crowd[i].state == WK_FIGHT) return 1;
    return 0;
}

int crowd_positions(V3 *out, int cap)
{
    int i, n = 0;
    for (i = 0; i < CROWD_MAX && n < cap; i++)
        if (crowd[i].state != WK_FREE) out[n++] = crowd[i].ped.pos;
    return n;
}

static int update_one(Walker *w, float dt, Ped *player, V3 focus)
{
    Ped *p = &w->ped;
    int ev = 0, pev;
    float hit;
    V3 d = v3_sub(focus, p->pos);

    w->t += dt;
    if (d.x * d.x + d.z * d.z > 85.0f * 85.0f) {
        w->state = WK_FREE;
        return 0;
    }
    switch (w->state) {
    case WK_WANDER:
        if (!p->has_goal) {
            if (w->t > 1.0f + frand() * 3.0f) {   /* a pause, then on */
                new_goal(w);
                w->t = 0.0f;
            }
        }
        if (!player) break;
        /* the policeman sees an assault */
        if (p->who == CH_STEPAN && assault_t < 1.0f) {
            V3 a = v3_sub(assault_at, p->pos);
            if (a.x * a.x + a.z * a.z < 25.0f * 25.0f) start_fight(w);
        }
        break;
    case WK_FLEE:
        if (!p->has_goal || w->t > 7.0f) {
            w->state = WK_WANDER;
            w->t = 0.0f;
            p->walk_speed = 1.1f + frand() * 0.5f;
            w->axis = frand() < 0.5f;
            new_goal(w);
        }
        break;
    case WK_FIGHT: {
        V3 to;
        float dist;
        if (!player || w->t > 20.0f) {
            w->state = WK_WANDER;
            p->fight_mode = 0;
            p->walk_speed = 1.2f;
            new_goal(w);
            break;
        }
        to = v3_sub(player->pos, p->pos);
        dist = sqrtf(to.x * to.x + to.z * to.z);
        if (dist > 18.0f || player->knock_t > 0.0f) {
            /* got away, or he is down: enough */
            w->state = WK_WANDER;
            w->t = 0.0f;
            p->fight_mode = 0;
            p->walk_speed = 1.2f;
            new_goal(w);
            break;
        }
        /* close in, keep facing him, swing when he is within reach */
        p->goal_yaw = yaw_of(to.x, to.z);
        p->face_goal_yaw = 1;
        if (dist > PUNCH_REACH * 0.8f) {
            p->goal = player->pos;
            p->has_goal = 1;
            p->walk_speed = dist > 4.0f ? 4.8f : 2.0f;
        } else {
            p->has_goal = 0;
        }
        w->punch_cd -= dt;
        if (dist < PUNCH_REACH + 0.1f && w->punch_cd <= 0.0f && ped_punch(p)) {
            w->punch_cd = 0.7f + frand() * 0.6f;
            ev |= CROWD_EV_SWING;
        }
        break;
    }
    case WK_DOWN:
        p->has_goal = 0;
        if (p->knock_t <= 0.0f && p->anim.knock < 0.05f) {
            /* back on his feet: the timid run, fighters come back for more */
            if (LOOKS[p->who].temper == TEMPER_FIGHT && player && frand() < 0.6f) start_fight(w);
            else flee_from(w, player ? player->pos : p->pos);
        }
        break;
    default:
        break;
    }

    pev = ped_update(p, v3(0, 0, 0), 0, 0, dt);
    if ((pev & PED_EV_PUNCH) && player && player->knock_t <= 0.0f) {
        V3 to = v3_sub(player->pos, p->pos), f = yaw_fwd(p->yaw);
        float dist = sqrtf(to.x * to.x + to.z * to.z);
        if (dist < PUNCH_REACH + 0.2f && fabsf(to.y) < 1.0f && (dist < 0.2f || (to.x * f.x + to.z * f.z) / dist > 0.4f)) {
            ped_take_hit(player, p->pos, 0.8f);
            player_hurt += 1.0f;
            ev |= CROWD_EV_PLAYER_HIT;
            if (player_hurt >= 5.0f) {
                player->knock_t = 2.6f;
                player_hurt = 0.0f;
                ev |= CROWD_EV_PLAYER_DOWN;
            }
        }
    }

    /* cars: pushed out of their way, knocked down by a hit */
    hit = car_push_point(&p->pos, 0.3f);
    if (hit > 3.0f && w->state != WK_DOWN) {
        knock_down(w, clampf(2.5f + hit * 0.2f, 3.0f, 7.0f));
        p->vel.y = clampf(hit * 0.25f, 1.0f, 5.0f);
        p->on_ground = 0;
        alarm(p->pos, 18.0f, w);
        ev |= CROWD_EV_RUN_OVER;
    }
    return ev;
}

int crowd_update(float dt, Ped *player, V3 focus, int do_spawn)
{
    int i, ev = 0, alive = 0;
    assault_t += dt;
    player_hurt = player_hurt > dt * 0.25f ? player_hurt - dt * 0.25f : 0.0f;
    for (i = 0; i < CROWD_MAX; i++) {
        if (crowd[i].state == WK_FREE) continue;
        ev |= update_one(&crowd[i], dt, player, focus);
        if (crowd[i].state != WK_FREE) alive++;
    }
    /* keep the pavements populated in town, one newcomer per frame at most */
    if (do_spawn && city_in_town(focus.x, focus.z) && alive < CROWD_MAX - 2)
        for (i = 0; i < CROWD_MAX; i++)
            if (crowd[i].state == WK_FREE) {
                spawn(&crowd[i], focus);
                break;
            }
    return ev;
}

#define CROWD_DETAILED 3   /* at most this many get the full model */

void crowd_draw(DrawList *dl, V3 eye, V3 fwd, float radius, float near)
{
    float d2[CROWD_MAX];
    int i, j, detailed[CROWD_MAX];
    for (i = 0; i < CROWD_MAX; i++) {
        V3 d = v3_sub(crowd[i].ped.pos, eye);
        /* behind the camera: not drawn */
        d2[i] = (crowd[i].state == WK_FREE || d.x * fwd.x + d.z * fwd.z < -2.0f) ? 1e30f : d.x * d.x + d.z * d.z;
    }
    for (i = 0; i < CROWD_MAX; i++) {
        int closer = 0;
        for (j = 0; j < CROWD_MAX; j++)
            if (d2[j] < d2[i] || (d2[j] == d2[i] && j < i)) closer++;
        detailed[i] = ped_lod(sqrtf(d2[i]));
        if (detailed[i] == PED_LOD_FULL && closer >= CROWD_DETAILED) detailed[i] = PED_LOD_MEDIUM;
    }
    (void)near;
    for (i = 0; i < CROWD_MAX; i++)
        if (d2[i] <= radius * radius) ped_draw(dl, &crowd[i].ped, detailed[i]);
}

void crowd_append_meshes(GMesh *m, V3 eye, float radius, float near)
{
    DrawList dl;
    dl_init(&dl);
    crowd_draw(&dl, eye, v3(0, 0, 0), radius, near);
    dl_flatten(&dl, m);
    dl_free(&dl);
}
