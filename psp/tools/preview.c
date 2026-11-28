/* Renders preview images of the game world on a PC: make -C psp/tools && ./preview out_dir */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "raster.h"
#include "../src/city.h"
#include "../src/texgen.h"
#include "../src/car.h"
#include "../src/actor.h"
#include "../src/cutscene.h"
#include "../src/crowd.h"
#include "../src/ride.h"
#include "../src/city.h"

static unsigned int atlas[TEX_SIZE * TEX_SIZE];
static unsigned int *atlas_mip[4];
static unsigned int *ground_mip[4];

static void use_texture(Raster *r, unsigned int **mips, int w, int h, int wrap)
{
    int l;
    for (l = 0; l < 4; l++) r->mip[l] = mips[l];
    r->tex = mips[0];
    r->tw = w;
    r->th = h;
    r->wrap_u = wrap;
}

static void draw_city(Raster *r, V3 eye, float radius)
{
    int i, j;
    /* ground strips first, with the repeating ground texture */
    use_texture(r, ground_mip, GROUND_W, GROUND_H, 1);
    for (j = CELL_MIN; j < CELL_MIN + CELLS; j++)
        for (i = CELL_MIN; i < CELL_MIN + CELLS; i++) {
            CityCell *c = city_cell(i, j);
            float dx = c->cx - eye.x, dz = c->cz - eye.z;
            if (dx * dx + dz * dz > radius * radius) continue;
            rs_draw(r, c->ground.v, c->ground.n);
        }
    use_texture(r, atlas_mip, TEX_SIZE, TEX_SIZE, 0);
    for (j = CELL_MIN; j < CELL_MIN + CELLS; j++)
        for (i = CELL_MIN; i < CELL_MIN + CELLS; i++) {
            CityCell *c = city_cell(i, j);
            float dx = c->cx - eye.x, dz = c->cz - eye.z;
            if (dx * dx + dz * dz > radius * radius) continue;
            rs_draw(r, c->solid.v, c->solid.n);
        }
}

static void draw_cars(Raster *r)
{
    GMesh m;
    int i;
    gm_init(&m);
    for (i = 0; i < MAX_CARS; i++)
        if (cars[i].kind != CK_FREE) car_append_mesh(&m, &cars[i]);
    rs_draw(r, m.v, m.n);
    gm_free(&m);
}

static Ped peds[16];
static Cutscene *cur_cs;

static int res_scale = 1;
static int with_crowd;
static int npeds;

static void draw_peds(Raster *r)
{
    GMesh m;
    int i;
    gm_init(&m);
    for (i = 0; i < npeds; i++) ped_append_mesh(&m, &peds[i]);
    rs_draw(r, m.v, m.n);
    gm_free(&m);
}

static void shot(const char *dir, const char *name, V3 eye, V3 target)
{
    Raster r;
    char path[256];
    rs_init(&r, 480 * res_scale, 272 * res_scale, atlas);
    {
        /* same texture filtering as the PSP renderer (render.c) */
        int l;
        for (l = 0; l < 4; l++) r.mip[l] = atlas_mip[l];
        r.levels = 4;
        r.filter = RS_NEAREST_MIPMAP_LINEAR;
        r.lod_bias = -0.5f;
    }
    r.fog_start = 90.0f;
    r.fog_end = 180.0f;
    r.fog_color = 0xFFF0D2B0u;
    rs_clear_sky(&r, 0xFFE89A50u, 0xFFF0D2B0u);
    rs_look_at(&r, eye, target, 70.0f);
    draw_city(&r, eye, 220.0f);
    draw_cars(&r);
    draw_peds(&r);
    if (with_crowd) {
        GMesh m;
        gm_init(&m);
        crowd_append_meshes(&m, eye, 70.0f, 16.0f);
        rs_draw(&r, m.v, m.n);
        gm_free(&m);
    }
    snprintf(path, sizeof(path), "%s/%s.png", dir, name);
    rs_write_png(&r, path);
    rs_free(&r);
    printf("wrote %s\n", path);
}

int main(int argc, char **argv)
{
    const char *dir = argc > 1 ? argv[1] : ".";
    int i, j, verts = 0;
    texgen_atlas(atlas);
    atlas_mip[0] = atlas;
    for (i = 1; i < 4; i++) {
        atlas_mip[i] = (unsigned int *)malloc((size_t)(TEX_SIZE >> i) * (TEX_SIZE >> i) * 4);
        texgen_mip(atlas_mip[i - 1], TEX_SIZE >> (i - 1), atlas_mip[i]);
    }
    ground_mip[0] = (unsigned int *)malloc(GROUND_W * GROUND_H * 4);
    texgen_ground(atlas, ground_mip[0]);
    for (i = 1; i < 4; i++) {
        ground_mip[i] = (unsigned int *)malloc((size_t)(GROUND_W >> i) * (GROUND_H >> i) * 4);
        texgen_mip_rect(ground_mip[i - 1], GROUND_W >> (i - 1), GROUND_H >> (i - 1), ground_mip[i]);
    }
    city_build(1996);
    for (j = CELL_MIN; j < CELL_MIN + CELLS; j++)
        for (i = CELL_MIN; i < CELL_MIN + CELLS; i++) verts += city_cell(i, j)->solid.n;
    printf("city: %d vertices (%.1f MB), %d collision boxes\n", verts, verts * 24 / 1048576.0, city_box_count());
    carmodels_build();
    {
        static const unsigned int PAINT[CAR_TYPES] = { 0xFF2A1E14u, 0xFF3A2A9Au, 0xFF9CC4D8u, 0xFF3C5A30u, 0xFF141414u, 0xFFE8E8E8u };
        for (i = 0; i < CAR_TYPES; i++) {
            int k = car_spawn(CK_PARKED, i, v3(-28.0f + i * 6.0f, 0, 10.5f), 0.6f, PAINT[i]);
            cars[k].door = i == 4 ? 1.0f : 0.0f;
            cars[k].steer = i == 0 ? 0.4f : 0.0f;
        }
    }
    cars_clear();
    {
        static const unsigned int PAINT[CAR_TYPES] = { 0xFF3A2414u, 0xFF3A2A9Au, 0xFF9CC4D8u, 0xFF3C5A30u, 0xFF181818u, 0xFFE8E8E8u };
        char name[32];
        for (i = 0; i < CAR_TYPES; i++) {
            int k;
            cars_clear();
            k = car_spawn(CK_PARKED, i, v3(420.0f, 0, 8.0f), -0.9f, PAINT[i]);
            cars[k].steer = 0.35f;
            cars[k].door = i == CAR_GELENDER ? 1.0f : 0.0f;
            snprintf(name, sizeof(name), "car%d", i);
            shot(dir, name, v3(424.9f, 1.4f, 7.2f), v3(420.0f, 0.7f, 8.0f));
        }
        cars_clear();
    }
    shot(dir, "street", v3(10.5f, 1.7f, 70.0f), v3(10.5f, 3.0f, 20.0f));
    {
        /* passers-by on the pavements */
        Ped me;
        int t;
        ped_init(&me, CH_ANDRIY, v3(14.5f, WALK_H, 52.0f), 0.0f);
        crowd_reset(11u);
        for (t = 0; t < 60 * 2; t++) crowd_update(1.0f / 60, &me, me.pos, 1);
        /* bring the first eight onto this street, one of each */
        for (t = 0; t < CH_PASSERS && t < CROWD_MAX; t++) {
            Ped *p = &crowd[t].ped;
            ped_init(p, CH_FIRST_PASSER + t, v3(t & 1 ? 1.5f : 14.5f, WALK_H, 60.0f - t * 2.6f), t & 1 ? PI_F : 0.0f);
            crowd[t].state = WK_WANDER;
            crowd[t].axis = 1;
            p->goal = v3(p->pos.x, WALK_H, p->pos.z + (t & 1 ? 20.0f : -20.0f));
            p->has_goal = 1;
        }
        for (t = 0; t < 60 * 1; t++) crowd_update(1.0f / 60, &me, me.pos, 0);
        with_crowd = 1;
        res_scale = 2;
        shot(dir, "crowd", v3(10.5f, 2.2f, 66.0f), v3(13.0f, 1.0f, 46.0f));
        res_scale = 1;
        with_crowd = 0;
    }
    shot(dir, "home", v3(30.0f, 1.7f, 58.0f), v3(40.0f, 6.0f, 34.0f));
    shot(dir, "garages", v3(64 + 34.0f, 1.7f, 10.0f), v3(64 + 22.0f, 1.5f, 26.0f));
    shot(dir, "market", v3(-128 + 30.0f, 1.8f, 64 + 8.0f), v3(-128 + 40.0f, 1.5f, 64 + 36.0f));
    shot(dir, "aerial", v3(-40.0f, 60.0f, 120.0f), v3(40.0f, 0.0f, 0.0f));
    shot(dir, "highway", v3(300.0f, 1.6f, 10.5f), v3(400.0f, 0.5f, 10.5f));
    shot(dir, "kiosk", v3(128 + 37.2f, 1.7f, -128 + 30.0f), v3(128 + 37.2f, 2.6f, -128 + 36.0f));
    shot(dir, "houses", v3(-256 + 40.0f, 1.7f, -256 + 8.0f), v3(-256 + 40.0f, 2.0f, -256 + 40.0f));
    /* getting into a car: door half open with Andriy reaching, door fully open with Andriy ducking */
    {
        int k, d;
        for (d = 0; d < 2; d++) {
            char name[32];
            cars_clear();
            k = car_spawn(CK_PARKED, CAR_LASTIVKA, v3(410.0f, 0, 8.0f), -PI_F / 2, 0xFF3A2A9Au);
            cars[k].door = d ? 1.0f : 0.5f;
            npeds = 1;
            ped_init(&peds[0], CH_ANDRIY, car_door_point(&cars[k], 0), yaw_of(yaw_right(cars[k].yaw).x, yaw_right(cars[k].yaw).z));
            if (d) { peds[0].anim.crouch = 0.6f; peds[0].anim.sit = 0.3f; peds[0].pos = v3_lerp(car_door_point(&cars[k], 0), car_seat_point(&cars[k]), 0.4f); }
            else { peds[0].anim.gesture = GEST_POINT; peds[0].anim.gesture_w = 1.0f; }
            snprintf(name, sizeof(name), "door%d", d);
            res_scale = 2;
            shot(dir, name, v3(407.0f, 1.8f, 3.2f), v3(410.0f, 0.9f, 7.0f));
            snprintf(name, sizeof(name), "doorB%d", d);
            shot(dir, name, v3(406.0f, 1.9f, 6.5f), v3(410.5f, 0.9f, 8.0f));
            res_scale = 1;
        }
        npeds = 0;
        cars_clear();
    }
    /* every vehicle from the front and the back three-quarters */
    {
        int t;
        for (t = 0; t < CAR_TYPES; t++) {
            char name[32];
            const CarSpec *cs = &CAR_SPECS[t];
            float dist = cs->length * 0.62f + 1.1f;
            Car *c;
            cars_clear();
            c = &cars[car_spawn(CK_PARKED, t, v3(410.0f, 0, 8.0f), -PI_F / 2, 0xFF3A2A9Au)];
            res_scale = 2;
            snprintf(name, sizeof(name), "veh%d_front", t);
            shot(dir, name, car_local(c, v3(-dist * 0.55f, cs->height * 0.75f + 0.4f, -dist * 0.8f)), car_local(c, v3(0, cs->height * 0.42f, 0)));
            snprintf(name, sizeof(name), "veh%d_rear", t);
            shot(dir, name, car_local(c, v3(dist * 0.55f, cs->height * 0.75f + 0.4f, dist * 0.8f)), car_local(c, v3(0, cs->height * 0.42f, 0)));
            res_scale = 1;
        }
        cars_clear();
    }
    /* doors of every car: closed, driver door open, passenger door open, and
     * the chase camera behind the car while the driver door is open */
    {
        int t, k;
        static const unsigned int PAINT[CAR_TYPES] = { 0xFF3A2414u, 0xFF3A2A9Au, 0xFF9CC4D8u, 0xFF3C5A30u, 0xFF181818u, 0xFFE8E8E8u };
        npeds = 0;
        for (t = 0; t < CAR_TYPES; t++) {
            char name[32];
            Car *c;
            if (CAR_IS_BIKE(t)) continue;
            cars_clear();
            k = car_spawn(CK_PARKED, t, v3(410.0f, 0, 8.0f), -PI_F / 2, PAINT[t]);
            c = &cars[k];
            res_scale = 2;
            c->door = 0.0f;
            snprintf(name, sizeof(name), "cardoor%d_closed", t);
            shot(dir, name, car_local(c, v3(-4.6f, 1.3f, -0.6f)), car_local(c, v3(0, 0.75f, -0.3f)));
            c->door = 1.0f;
            c->door_side = 0;
            snprintf(name, sizeof(name), "cardoor%d_left", t);
            shot(dir, name, car_local(c, v3(-3.6f, 1.5f, -3.4f)), car_local(c, v3(0, 0.7f, 0.0f)));
            /* looking into the opening with Andriy in the driver's seat */
            npeds = 1;
            ped_init(&peds[0], CH_ANDRIY, car_seat_point(c), c->yaw);
            peds[0].anim.sit = 1.0f;
            { CarLayout lay; car_layout(t, &lay); peds[0].pos.y = lay.seat_y; }
            snprintf(name, sizeof(name), "cardoor%d_inside", t);
            shot(dir, name, car_local(c, v3(-3.0f, 1.7f, 1.4f)), car_local(c, v3(-0.3f, 0.8f, -0.5f)));
            npeds = 0;
            snprintf(name, sizeof(name), "cardoor%d_chase", t);
            shot(dir, name, car_local(c, v3(-1.6f, 2.3f, 5.2f)), car_local(c, v3(-0.4f, 0.9f, -0.6f)));
            c->door = 0.6f;
            c->door_side = 1;
            snprintf(name, sizeof(name), "cardoor%d_right", t);
            shot(dir, name, car_local(c, v3(3.6f, 1.5f, -3.4f)), car_local(c, v3(0, 0.7f, 0.0f)));
            res_scale = 1;
        }
        cars_clear();
    }
    /* the motorcycle with Andriy riding it into a left turn, and getting into a car */
    {
        Car *c;
        int k2;
        Ride rd;
        float tt;
        cars_clear();
        k2 = car_spawn(CK_PLAYER, CAR_JAVIR, v3(410.0f, 0, 8.0f), -PI_F / 2, 0);
        c = &cars[k2];
        c->lean = 0.35f;
        c->body_roll = 0.35f;
        c->steer = 0.12f;
        npeds = 1;
        ped_init(&peds[0], CH_ANDRIY, car_rider_pos(c), c->yaw);
        peds[0].anim.ride = 1.0f;
        peds[0].anim.lean = c->lean;
        res_scale = 2;
        shot(dir, "bike_ride", car_local(c, v3(-2.6f, 1.3f, -2.2f)), car_local(c, v3(0, 0.8f, 0)));
        shot(dir, "bike_ride_back", car_local(c, v3(1.2f, 1.6f, 3.2f)), car_local(c, v3(0, 0.8f, 0)));
        /* getting into the Lastivka: frames through the new animation */
        cars_clear();
        k2 = car_spawn(CK_PARKED, CAR_LASTIVKA, v3(410.0f, 0, 8.0f), -PI_F / 2, 0xFF3A2A9Au);
        c = &cars[k2];
        ped_init(&peds[0], CH_ANDRIY, car_door_point(c, 0), 0.0f);
        ride_reset(&rd);
        ride_enter(&rd, &peds[0], k2, 0);
        for (tt = 0.0f; tt < 8.0f && rd.state != RIDE_DRIVE; tt += 1.0f / 60) {
            int before = rd.state;
            float rt = rd.t;
            ride_update(&rd, &peds[0], 1.0f / 60);
            if (rd.state == RIDE_IN && ((rt < 0.3f && rd.t >= 0.3f) || (rt < 0.6f && rd.t >= 0.6f) || (rt < 0.95f && rd.t >= 0.95f))) {
                char name[32];
                snprintf(name, sizeof(name), "enter_%d", (int)(rd.t * 10));
                shot(dir, name, car_local(c, v3(-3.2f, 1.6f, 1.6f)), car_local(c, v3(-0.4f, 0.8f, -0.2f)));
            }
            (void)before;
        }
        res_scale = 1;
        npeds = 0;
        cars_clear();
    }
    /* characters: cast line-up, the passers-by, close-ups of faces */
    cars_clear();
    {
        int row;
        for (row = 0; row < 2; row++) {
            int first = row ? CH_FIRST_PASSER : 0, count = row ? CH_PASSERS : CH_FIRST_PASSER;
            float span = (count - 1) * 1.1f;
            npeds = 0;
            for (i = 0; i < count; i++) {
                ped_init(&peds[npeds++], first + i, v3(400.0f + i * 1.1f, 0.0f, 9.0f), 0.0f);
                peds[npeds - 1].anim.t = 2.0f + i;
            }
            res_scale = 2;
            shot(dir, row ? "passers" : "cast", v3(400.0f + span * 0.5f, 1.2f, 9.0f - 1.1f - span * 0.62f),
                 v3(400.0f + span * 0.5f, 1.0f, 9.0f));
            for (i = 0; i < count; i++) {
                char name[32];
                snprintf(name, sizeof(name), "face%d", first + i);
                shot(dir, name, v3(400.0f + i * 1.1f + 0.35f, 1.6f * LOOKS[first + i].height, 9.0f - 0.85f),
                     v3(400.0f + i * 1.1f, 1.5f * LOOKS[first + i].height, 9.0f));
            }
            res_scale = 1;
        }
        /* fists up and a punch */
        npeds = 2;
        ped_init(&peds[0], CH_ANDRIY, v3(400.0f, 0.0f, 9.0f), -PI_F / 2);
        ped_init(&peds[1], CH_VOVAN, v3(401.0f, 0.0f, 9.0f), PI_F / 2);
        peds[0].anim.fight = 1.0f;
        peds[0].anim.punch = 1.0f;
        peds[0].anim.punch_arm = 1;
        peds[1].anim.fight = 1.0f;
        peds[1].anim.hurt = 0.7f;
        res_scale = 2;
        shot(dir, "fight", v3(400.5f, 1.3f, 11.6f), v3(400.5f, 1.0f, 9.0f));
        res_scale = 1;
    }
    /* walk and run cycles seen from the side */
    {
        int row;
        for (row = 0; row < 2; row++) {
            npeds = 0;
            for (i = 0; i < 8; i++) {
                Ped *p = &peds[npeds++];
                ped_init(p, CH_ANDRIY, v3(400.0f + i * 1.05f, 0.0f, 9.0f), -PI_F / 2);
                p->anim.move = 1.0f;
                p->anim.run = (float)row;
                p->anim.phase = i * PI_F / 4.0f;
                p->anim.t = 1.0f;
            }
            res_scale = 2;
            shot(dir, row ? "run_cycle" : "walk_cycle", v3(403.7f, 1.0f, 12.9f), v3(403.7f, 0.9f, 9.0f));
            res_scale = 1;
        }
    }
    /* poses: jump up, fall, land, sit, explain, point, hips, crossed, wave, knocked */
    {
        static const char *NAMES[2] = { "poses1", "poses2" };
        int row;
        for (row = 0; row < 2; row++) {
            npeds = 0;
            for (i = 0; i < 5; i++) {
                Ped *p = &peds[npeds++];
                int k = row * 5 + i;
                ped_init(p, k == 6 || k == 7 ? CH_KABAN : (k == 8 ? CH_THUG : (k == 4 ? CH_TARAS : CH_ANDRIY)),
                         v3(400.0f + i * 1.4f, 0.0f, 9.0f), -0.35f);
                p->anim.t = 3.0f + k;
                switch (k) {
                case 0: p->anim.air = 1; p->anim.vy = 4.0f; p->pos.y = 0.5f; break;
                case 1: p->anim.air = 1; p->anim.vy = -5.0f; p->pos.y = 0.3f; break;
                case 2: p->anim.land = 1.0f; break;
                case 3: p->anim.sit = 1.0f; p->pos.y = 0.0f; break;
                case 4: p->anim.gesture = GEST_EXPLAIN; p->anim.gesture_w = 1; p->anim.talk = 1; p->anim.mouth = 1; break;
                case 5: p->anim.gesture = GEST_POINT; p->anim.gesture_w = 1; break;
                case 6: p->anim.gesture = GEST_HIPS; p->anim.gesture_w = 1; break;
                case 7: p->anim.gesture = GEST_CROSSED; p->anim.gesture_w = 1; break;
                case 8: p->anim.gesture = GEST_WAVE; p->anim.gesture_w = 1; break;
                default: p->anim.knock = 1.0f; break;
                }
            }
            res_scale = 2;
            shot(dir, NAMES[row], v3(402.8f, 1.2f, 12.9f), v3(402.8f, 0.9f, 9.0f));
            res_scale = 1;
        }
    }
    /* cutscene frames */
    {
        static const int SCENES[5] = { CS_INTRO, CS_GARAGE, CS_MARKET, CS_KIOSK, CS_KABAN };
        static const int WHO[N_ACTORS] = { CH_ANDRIY, CH_TARAS, CH_GALYA, CH_KABAN, CH_SELLER, CH_THUG, CH_THUG };
        int sc;
        for (sc = 0; sc < 5; sc++) {
            Cutscene cs;
            CsIO io;
            int f, shotn = 0, a;
            memset(&io, 0, sizeof(io));
            cur_cs = &cs;
            cars_clear();
            car_spawn(CK_PARKED, CAR_LASTIVKA, POI_CAR, POI_CAR_YAW, 0xFF3A2A9Au);
            if (SCENES[sc] == CS_KABAN) car_spawn(CK_PARKED, CAR_GELENDER, v3(-19.5f, 0, -85.0f), PI_F / 2, 0xFF181818u);
            for (a = 0; a < N_ACTORS; a++) {
                ped_init(&peds[a], WHO[a], v3(0, -50, 0), 0);
                io.actors[a] = &peds[a];
            }
            ped_init(&peds[A_TARAS], CH_TARAS, POI_TARAS, PI_F);
            ped_init(&peds[A_GALYA], CH_GALYA, POI_GALYA, 0);
            ped_init(&peds[A_SELLER], CH_SELLER, POI_SELLER, 0);
            ped_init(&peds[A_ANDRIY], CH_ANDRIY, POI_SPAWN, 0);
            npeds = N_ACTORS;
            cs_start(&cs, SCENES[sc]);
            for (f = 0; f < 60 * 120 && cs.running; f++) {
                int adv = 0;
                cs_update(&cs, &io, 1.0f / 60, adv, 0);
                for (a = 0; a < N_ACTORS; a++) ped_update(&peds[a], v3(0, 0, 0), 0, 0, 1.0f / 60);
                if ((f % 170) == 90 && shotn < 6 && cs.running) {
                    V3 eye, tgt;
                    char name[32];
                    cs_camera(&cs, &io, &eye, &tgt);
                    snprintf(name, sizeof(name), "scene%d_%d", sc, shotn++);
                    res_scale = 1;
                    shot(dir, name, eye, tgt);
                }
            }
            if (SCENES[sc] == CS_MARKET) shot(dir, "market_top", v3(-93.0f, 9.0f, 90.0f), v3(-93.2f, 0.0f, 97.0f));
            printf("scene %d finished after %.1fs (running=%d)\n", SCENES[sc], f / 60.0f, cs.running);
        }
    }
    npeds = 0;
    city_free();
    return 0;
}
