/* Renders preview images of the game world on a PC: make -C psp/tools && ./preview out_dir */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "raster.h"
#include "../src/city.h"
#include "../src/voice.h"
#include "../src/texgen.h"
#include "../src/car.h"
#include "../src/actor.h"
#include "../src/cutscene.h"
#include "../src/city.h"

static unsigned int atlas[TEX_SIZE * TEX_SIZE];

static void draw_city(Raster *r, V3 eye, float radius)
{
    int i, j;
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
static int fake_speaking(void) { return cur_cs && cur_cs->sub_text && cur_cs->sub_t < voice_duration(cur_cs->sub_text); }
static int res_scale = 1;
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
    r.fog_start = 90.0f;
    r.fog_end = 180.0f;
    r.fog_color = 0xFFF0D2B0u;
    rs_clear_sky(&r, 0xFFE89A50u, 0xFFF0D2B0u);
    rs_look_at(&r, eye, target, 70.0f);
    draw_city(&r, eye, 220.0f);
    draw_cars(&r);
    draw_peds(&r);
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
    shot(dir, "home", v3(30.0f, 1.7f, 58.0f), v3(40.0f, 6.0f, 34.0f));
    shot(dir, "garages", v3(64 + 34.0f, 1.7f, 10.0f), v3(64 + 22.0f, 1.5f, 26.0f));
    shot(dir, "market", v3(-128 + 30.0f, 1.8f, 64 + 8.0f), v3(-128 + 40.0f, 1.5f, 64 + 36.0f));
    shot(dir, "aerial", v3(-40.0f, 60.0f, 120.0f), v3(40.0f, 0.0f, 0.0f));
    shot(dir, "highway", v3(300.0f, 1.6f, 10.5f), v3(400.0f, 0.5f, 10.5f));
    shot(dir, "kiosk", v3(128 + 37.2f, 1.7f, -128 + 30.0f), v3(128 + 37.2f, 2.6f, -128 + 36.0f));
    shot(dir, "houses", v3(-256 + 40.0f, 1.7f, -256 + 8.0f), v3(-256 + 40.0f, 2.0f, -256 + 40.0f));
    /* characters: cast line-up */
    cars_clear();
    npeds = 0;
    for (i = 0; i < CH_COUNT; i++) {
        ped_init(&peds[npeds++], i, v3(400.0f + i * 1.1f, 0.0f, 9.0f), 0.0f);
        peds[npeds - 1].anim.t = 2.0f + i;
    }
    res_scale = 2;
    shot(dir, "cast", v3(402.75f, 1.2f, 5.3f), v3(402.75f, 1.0f, 9.0f));
    res_scale = 1;
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
            io.speaking = fake_speaking;
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
                for (a = 0; a < N_ACTORS; a++) {
                    peds[a].anim.mouth = (a == cs.speaker) ? 0.5f + 0.5f * sinf(f * 0.4f) : 0.0f;
                    ped_update(&peds[a], v3(0, 0, 0), 0, 0, 1.0f / 60);
                }
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
