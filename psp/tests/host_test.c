/* Host-side tests for the platform-independent game code.
 * Build & run: make -C psp/tests check */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../src/actor.h"
#include "../src/car.h"
#include "../src/city.h"
#include "../src/cutscene.h"
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
    int i, verts = 0, j;
    for (j = CELL_MIN; j < CELL_MIN + CELLS; j++)
        for (i = CELL_MIN; i < CELL_MIN + CELLS; i++) verts += city_cell(i, j)->solid.n;
    printf("city: %d vertices, %d collision boxes\n", verts, city_box_count());
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
        float t100 = -1.0f;
        Car *c;
        cars_clear();
        k = car_spawn(CK_PLAYER, t, v3(-600.0f, 0, 10.5f), -PI_F / 2, 0xFFFFFFFFu);
        c = &cars[k];
        for (i = 0; i < 60 * 30 && t100 < 0; i++) {
            c->in_throttle = 1.0f;
            car_step(c, 1.0f / 60);
            if (car_speed(c) * 3.6f >= 100.0f) t100 = i / 60.0f;
            CHECK(!isnan(c->pos.x) && !isnan(c->yaw));
        }
        printf("%-14s 0-100 km/h in %.1f s\n", CAR_SPECS[t].name, t100);
        CHECK(t100 > 4.0f && t100 < 25.0f);
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
