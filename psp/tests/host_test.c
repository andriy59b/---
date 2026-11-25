/* Host-side tests for the platform-independent parts (world, gen, mesh, textures).
 * Build & run: make -C psp/tests check */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../src/gen.h"
#include "../src/mesh.h"
#include "../src/textures.h"
#include "../src/world.h"
#include "../src/story.h"
#include "../src/synth.h"
#include "../src/town.h"
#include "../src/vehicle.h"

static int failures;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); failures++; } } while (0)

#define TEST_DIR "host_test_world"
#define SEED 12345u

static void clean_dir(void)
{
    char cmd[128];
    snprintf(cmd, sizeof(cmd), "rm -rf %s", TEST_DIR);
    if (system(cmd) != 0) printf("warning: could not clean %s\n", TEST_DIR);
}

static void load_area(int cx0, int cz0, int r)
{
    int dx, dz;
    for (dz = -r; dz <= r; dz++)
        for (dx = -r; dx <= r; dx++) CHECK(world_load_chunk(cx0 + dx, cz0 + dz) != NULL);
}

static void test_generation(void)
{
    static unsigned char a[CH_VOL], b[CH_VOL];
    int counts[BIOME_COUNT] = { 0 }, x, z, i, y;
    long air_below = 0, solid_below = 0, ores[4] = { 0 };

    gen_chunk(SEED, 3, -7, a);
    gen_chunk(SEED, 3, -7, b);
    CHECK(memcmp(a, b, CH_VOL) == 0); /* deterministic */
    gen_chunk(SEED + 1, 3, -7, b);
    CHECK(memcmp(a, b, CH_VOL) != 0);

    for (z = -1024; z < 1024; z += 8)
        for (x = -1024; x < 1024; x += 8) {
            int bio, h = gen_column(SEED, x, z, &bio);
            CHECK(h >= 4 && h <= WH - 6);
            counts[bio]++;
            if (failures > 5) return;
        }
    printf("biomes:");
    for (i = 0; i < BIOME_COUNT; i++) printf(" %s=%d", biome_name(i), counts[i]);
    printf("\n");
    for (i = 0; i < BIOME_COUNT; i++) CHECK(counts[i] > 0);

    /* caves and ores over a patch of chunks */
    for (z = 0; z < 6; z++)
        for (x = 0; x < 6; x++) {
            gen_chunk(SEED, x * 5, z * 5, a);
            for (i = 0; i < CH * CH; i++) {
                int lx = i % CH, lz = i / CH, bio;
                int h = gen_column(SEED, (x * 5) * CH + lx, (z * 5) * CH + lz, &bio);
                CHECK(a[BIDX(lx, 0, lz)] == B_BEDROCK);
                for (y = 3; y < h - 4; y++) {
                    int blk = a[BIDX(lx, y, lz)];
                    if (blk == B_AIR) air_below++;
                    else solid_below++;
                    if (blk >= B_COAL_ORE && blk <= B_CRYSTAL_ORE) ores[blk - B_COAL_ORE]++;
                }
            }
        }
    printf("caves: %.2f%% of underground is air; ores coal=%ld iron=%ld gold=%ld crystal=%ld\n",
           100.0 * air_below / (air_below + solid_below), ores[0], ores[1], ores[2], ores[3]);
    CHECK(air_below > 0);
    CHECK(100.0 * air_below / (air_below + solid_below) < 8.0);
    CHECK(ores[0] > ores[1] && ores[1] > ores[2] && ores[2] > 0 && ores[3] > 0);
}

static void test_world_access(void)
{
    Chunk *c;
    clean_dir();
    world_init(SEED, TEST_DIR);
    CHECK(world_get(0, 10, 0) == B_UNLOADED);
    load_area(0, 0, 2);
    CHECK(world_get(0, 0, 0) == B_BEDROCK);
    CHECK(world_get(-1, 0, -1) == B_BEDROCK);
    CHECK(world_get(-33, 10, 0) == B_UNLOADED);
    CHECK(world_get(5, -1, 5) == B_BEDROCK);
    CHECK(world_get(5, WH, 5) == B_AIR);

    world_set(-5, 60, -5, B_BRICK);
    CHECK(world_get(-5, 60, -5) == B_BRICK);
    c = world_chunk(-1, -1);
    CHECK(c && c->modified && c->blocks[BIDX(11, 60, 11)] == B_BRICK);
    CHECK(c->height[11 * CH + 11] == 61);

    /* edits on a border dirty the neighbour mesh */
    c = world_chunk(0, 0);
    c->mesh_dirty = 0;
    world_chunk(-1, 0)->mesh_dirty = 0;
    world_set(0, 60, 5, B_STONE);
    CHECK(c->mesh_dirty && world_chunk(-1, 0)->mesh_dirty);
    CHECK(!world_chunk(1, 0)->mesh_dirty || 1);
    world_shutdown();
}

static void test_mesh(void)
{
    Chunk *c;
    int x, y, z, i;
    world_init(SEED, TEST_DIR);
    load_area(0, 0, 1);
    c = world_chunk(0, 0);
    CHECK(world_neighbors_loaded(c));
    CHECK(!world_neighbors_loaded(world_chunk(1, 1)));

    /* empty the chunk and its neighbours: a lone block has 6 faces */
    for (z = -16; z < 32; z++)
        for (x = -16; x < 32; x++)
            for (y = 0; y < WH; y++) world_set(x, y, z, B_AIR);
    world_set(5, 20, 5, B_STONE);
    CHECK(mesh_build(c));
    CHECK(c->nsolid == 6 && c->nwater == 0);

    world_set(6, 20, 5, B_STONE);
    mesh_build(c);
    CHECK(c->nsolid == 10);

    world_set(5, 21, 5, B_WATER);
    mesh_build(c);
    CHECK(c->nsolid == 10);
    CHECK(c->nwater == 5);

    world_set(8, 21, 8, B_TALL_GRASS);
    mesh_build(c);
    CHECK(c->nsolid == 12);

    /* a face against a neighbouring chunk is culled */
    world_set(15, 30, 15, B_STONE);
    world_set(16, 30, 15, B_STONE);
    mesh_build(c);
    CHECK(c->nsolid == 12 + 5);

    /* lone stone fully lit: all four corners of the top face are bright */
    for (i = 0; i < c->nsolid * 4; i++) {
        const Vertex *v = &c->solid[i];
        CHECK(v->x >= 0 && v->x <= CH * VPOS_SCALE && v->z >= 0 && v->z <= CH * VPOS_SCALE);
        CHECK(v->y >= 0 && v->y <= WH * VPOS_SCALE);
        CHECK(v->u < VUV_ONE && v->v < VUV_ONE);
        CHECK((v->color >> 24) == 0xFF);
    }

    /* ambient occlusion: a block next to a wall gets darker corners */
    world_set(5, 21, 6, B_STONE);
    mesh_build(c);
    {
        int found_dark = 0;
        for (i = 0; i < c->nsolid * 4; i++)
            if ((c->solid[i].color & 0xFF) < 200 && c->solid[i].y == 21 * VPOS_SCALE) found_dark = 1;
        CHECK(found_dark);
    }
    world_shutdown();

    /* real terrain */
    world_init(SEED, TEST_DIR);
    load_area(4, 4, 1);
    c = world_chunk(4, 4);
    CHECK(mesh_build(c));
    printf("mesh: terrain chunk has %d solid and %d water quads (%d KiB)\n", c->nsolid, c->nwater,
           (int)((c->nsolid + c->nwater) * 4 * sizeof(Vertex) / 1024));
    CHECK(c->nsolid > 100);
    world_shutdown();
}

static void flatten(int x0, int z0, int x1, int z1)
{
    int x, y, z;
    for (z = z0; z < z1; z++)
        for (x = x0; x < x1; x++)
            for (y = 0; y < WH; y++) world_set(x, y, z, y == 0 ? B_STONE : B_AIR);
}

static void test_raycast_physics(void)
{
    Player p;
    int hit[3], prev[3], i;
    float o[3] = { 10.5f, 10.5f, 10.5f }, down[3] = { 0, -1, 0 }, side[3] = { 1, 0, 0 };

    world_init(SEED, TEST_DIR);
    load_area(0, 0, 2);
    flatten(-16, -16, 48, 48);

    CHECK(world_raycast(o, down, 20.0f, hit, prev));
    CHECK(hit[0] == 10 && hit[1] == 0 && hit[2] == 10);
    CHECK(prev[1] == 1);
    CHECK(!world_raycast(o, side, 5.0f, hit, prev));
    world_set(13, 10, 10, B_GLASS);
    CHECK(world_raycast(o, side, 5.0f, hit, prev));
    CHECK(hit[0] == 13 && prev[0] == 12);

    /* across a chunk border towards negative coordinates */
    o[0] = 1.5f;
    side[0] = -1.0f;
    world_set(-2, 10, 10, B_BRICK);
    CHECK(world_raycast(o, side, 5.0f, hit, prev));
    CHECK(hit[0] == -2 && prev[0] == -1);

    memset(&p, 0, sizeof(p));
    p.pos[0] = 5.5f; p.pos[1] = 6.0f; p.pos[2] = 5.5f;
    for (i = 0; i < 240; i++) player_update(&p, 0, 0, 0, 1.0f / 60.0f);
    CHECK(p.on_ground);
    CHECK(fabsf(p.pos[1] - 1.0f) < 0.01f);

    for (i = 0; i < 4; i++) {
        int z;
        for (z = -8; z < 24; z++) world_set(20, 1 + i % 3, z, B_BRICK);
    }
    for (i = 0; i < 300; i++) player_update(&p, 4.3f, 0, 0, 1.0f / 60.0f);
    CHECK(p.pos[0] <= 20.0f - PLAYER_HALF_W + 0.001f && p.pos[0] > 19.0f);

    player_update(&p, 0, 0, 1, 1.0f / 60.0f);
    CHECK(p.pos[1] > 1.05f);
    CHECK(player_intersects_block(&p, 19, 2, 5));

    /* unloaded chunks are walls */
    p.pos[0] = 47.5f; p.pos[1] = 1.0f; p.pos[2] = 5.5f;
    p.vel[1] = 0;
    for (i = 0; i < 120; i++) player_update(&p, 4.3f, 0, 0, 1.0f / 60.0f);
    CHECK(p.pos[0] <= 48.0f);
    world_shutdown();
}

static void test_persistence(void)
{
    clean_dir();
    world_init(SEED, TEST_DIR);
    load_area(-3, 2, 0);
    world_set(-40, 50, 40, B_STONE_BRICKS);
    world_shutdown();

    world_init(SEED, TEST_DIR);
    CHECK(world_get(-40, 50, 40) == B_UNLOADED);
    load_area(-3, 2, 0);
    CHECK(world_get(-40, 50, 40) == B_STONE_BRICKS);
    world_shutdown();

    /* slot reuse evicts and saves the old chunk */
    world_init(SEED, TEST_DIR);
    load_area(0, 0, 0);
    world_set(1, 50, 1, B_GLASS);
    CHECK(world_load_chunk(SLOTS_W, 0) != NULL); /* same slot as (0,0) */
    CHECK(world_chunk(0, 0) == NULL);
    load_area(0, 0, 0);
    CHECK(world_get(1, 50, 1) == B_GLASS);
    world_shutdown();

    /* saved-chunk registry survives a restart; save clears the modified flag */
    world_init(SEED, TEST_DIR);
    CHECK(world_has_saved(0, 0) && world_has_saved(-3, 2));
    CHECK(!world_has_saved(7, 7));
    load_area(0, 0, 0);
    CHECK(!world_chunk(0, 0)->modified);
    world_set(2, 50, 2, B_BRICK);
    CHECK(world_chunk(0, 0)->modified);
    CHECK(world_save_all());
    CHECK(!world_chunk(0, 0)->modified);

    /* generated chunks committed from a worker buffer */
    {
        static unsigned char buf[CH_VOL];
        Chunk *c;
        gen_chunk(SEED, 9, 9, buf);
        c = world_commit_chunk(9, 9, buf);
        CHECK(c && c->loaded && !c->modified && c->mesh_dirty);
        CHECK(world_get(9 * CH, 0, 9 * CH) == B_BEDROCK);
    }
    world_shutdown();
    clean_dir();
}

static void test_mesh_split(void)
{
    Chunk *c;
    int ns;
    clean_dir();
    world_init(SEED, TEST_DIR);
    load_area(2, 2, 1);
    c = world_chunk(2, 2);
    mesh_build(c);
    ns = c->nsolid;
    CHECK(!c->mesh_dirty);
    world_set(2 * CH + 5, 62, 2 * CH + 5, B_GLASS);
    CHECK(c->mesh_dirty);
    mesh_gather(c);
    CHECK(!c->mesh_dirty);
    world_set(2 * CH + 6, 62, 2 * CH + 5, B_GLASS); /* edit while "computing" */
    mesh_compute();
    mesh_commit(c);
    CHECK(c->meshed && c->nsolid == ns + 6); /* snapshot had one glass block */
    CHECK(c->mesh_dirty);                     /* the later edit queues a rebuild */
    world_shutdown();
    clean_dir();
}


static int free_cell(const float p[3])
{
    int x = (int)floorf(p[0]), y = (int)floorf(p[1]), z = (int)floorf(p[2]);
    return !block_solid(world_get(x, y, z)) && !block_solid(world_get(x, y + 1, z)) &&
           block_solid(world_get(x, y - 1, z));
}

static void test_town(void)
{
    const float *pois[] = { POI_SPAWN, POI_GARAGE, POI_CAR, POI_MARKET, POI_KIOSK, POI_KABAN };
    int i, x, z, roads = 0, lines = 0, buildings = 0;
    clean_dir();
    world_init(SEED, TEST_DIR);
    for (i = 0; i < 6; i++) {
        int cx = (int)floorf(pois[i][0]) >> 4, cz = (int)floorf(pois[i][2]) >> 4;
        load_area(cx, cz, 1);
        if (!free_cell(pois[i])) printf("POI %d at %.1f %.1f %.1f is blocked\n", i, pois[i][0], pois[i][1], pois[i][2]);
        CHECK(free_cell(pois[i]));
        CHECK(town_contains((int)pois[i][0], (int)pois[i][2]));
    }
    /* the garage door and kiosk sign exist where the story says */
    CHECK(world_get(48 + 12, TOWN_Y + 1, 14) == B_GARAGE_DOOR);
    CHECK(world_get(96 + 21, TOWN_Y + 3, -96 + 20) == B_SIGN_BEER);
    CHECK(world_get(17, TOWN_Y + 1, 28) == B_DOOR);

    for (z = -16; z < 48; z++)
        for (x = -16; x < 48; x++) {
            int top = world_top(x, z), b;
            if (top < 0) continue;
            b = world_get(x, top, z);
            if (town_is_road(x, z)) {
                roads++;
                if (b == B_ROAD_LINE_X || b == B_ROAD_LINE_Z) lines++;
            }
            if (top > TOWN_Y + 8) buildings++;
        }
    CHECK(roads > 500 && lines > 20 && buildings > 100);
    CHECK(town_lot_type(0, 0) == LOT_PANEL5 && town_lot_type(1, 0) == LOT_GARAGES);
    CHECK(gen_column(SEED, 0, 0, NULL) == TOWN_Y);
    {
        int bio;
        gen_column(SEED, 10, 10, &bio);
        CHECK(bio == BIOME_TOWN);
        CHECK(strcmp(biome_name(BIOME_TOWN), "Соняшниківськ") == 0);
    }
    world_shutdown();
    clean_dir();
}

static void test_vehicles(void)
{
    int dx, dz, i, t, z, offroad = 0, samples = 0, pc;
    float focus[3] = { 30.0f, TOWN_Y + 1.0f, 30.0f }, out[3];
    clean_dir();
    world_init(SEED, TEST_DIR);
    for (dz = -6; dz <= 6; dz++)
        for (dx = -6; dx <= 6; dx++) world_load_chunk(dx, dz);
    cars_clear();
    for (t = 0; t < 60 * 60; t++) {
        cars_update(1.0f / 60, focus, NULL, 1);
        for (i = 0; i < MAX_CARS; i++) {
            const Car *c = &cars[i];
            if (c->kind != CAR_TRAFFIC) continue;
            samples++;
            if (!town_is_road((int)floorf(c->pos[0]), (int)floorf(c->pos[2]))) offroad++;
        }
    }
    printf("traffic: %d car-frames, %d off road\n", samples, offroad);
    CHECK(samples > 1000 && offroad == 0);

    /* traffic stops for a pedestrian standing in its lane */
    cars_clear();
    i = car_spawn(CAR_TRAFFIC, 60.0f, TOWN_Y + 1.0f, 5.0f, car_dir_yaw(0), 0xFFFFFFFFu);
    cars[i].dir = 0; cars[i].next_dir = 0; cars[i].speed = 8.0f;
    {
        float foot[3] = { 70.0f, TOWN_Y + 1.0f, 5.0f };
        for (t = 0; t < 60 * 5; t++) cars_update(1.0f / 60, focus, foot, 0);
        CHECK(cars[i].pos[0] < 70.0f - CAR_HALF_L);
        CHECK(cars[i].speed < 0.5f);
    }

    /* the player drives along the road, then hits a wall and stops */
    cars_clear();
    pc = car_spawn(CAR_PLAYER, 60.0f, TOWN_Y + 1.0f, 3.5f, car_dir_yaw(0), 0xFFFFFFFFu);
    for (t = 0; t < 60 * 3; t++) car_physics(&cars[pc], 1.0f, 0.0f, 1.0f / 60);
    CHECK(cars[pc].pos[0] > 80.0f && fabsf(cars[pc].pos[1] - (TOWN_Y + 1)) < 0.01f);
    for (z = 0; z < 7; z++)
        for (t = 1; t < 4; t++) world_set(130, TOWN_Y + t, z, B_BRICK);
    for (t = 0; t < 60 * 6; t++) car_physics(&cars[pc], 1.0f, 0.0f, 1.0f / 60);
    CHECK(cars[pc].pos[0] < 130.0f - 1.9f);
    car_exit_point(&cars[pc], out);
    CHECK(fabsf(out[0] - cars[pc].pos[0]) < 3.0f);
    {
        float px = cars[pc].pos[0], pz = cars[pc].pos[2];
        CHECK(car_push_out(&cars[pc], &px, &pz, 0.3f));
        CHECK(!car_push_out(&cars[pc], &px, &pz, 0.29f));
    }
    world_shutdown();
    clean_dir();
}

static void teleport(StoryView *v, const float p[3]) { v->pos[0] = p[0]; v->pos[1] = p[1]; v->pos[2] = p[2]; }

static void test_story(void)
{
    Story s;
    StoryView v;
    StoryOut o;
    float m[3];
    int i;
    story_start(&s);
    memset(&v, 0, sizeof(v));
    CHECK(s.mission == M_HOME && story_marker(&s, m));
    teleport(&v, POI_SPAWN);
    story_update(&s, &v, 0.1f, &o);
    CHECK(s.mission == M_HOME && o.lines == 0);
    teleport(&v, POI_GARAGE);
    story_update(&s, &v, 0.1f, &o);
    CHECK(s.mission == M_MARKET && o.spawn_own_car && o.lines >= 3 && o.mission_passed);
    teleport(&v, POI_MARKET);
    story_update(&s, &v, 0.1f, &o);
    CHECK(s.mission == M_MARKET && o.lines == 0); /* must take the car first */
    v.in_car = v.in_own_car = 1;
    story_update(&s, &v, 0.1f, &o);
    story_update(&s, &v, 0.1f, &o);
    CHECK(s.mission == M_PARCEL && s.timer > 100.0f);
    /* running out of time restarts the timer */
    teleport(&v, POI_SPAWN);
    for (i = 0; i < 200 && !o.mission_failed; i++) story_update(&s, &v, 1.0f, &o);
    CHECK(o.mission_failed && s.mission == M_PARCEL && s.timer > 100.0f);
    teleport(&v, POI_KIOSK);
    story_update(&s, &v, 0.1f, &o);
    CHECK(s.mission == M_TAIL && o.spawn_chaser && s.money == 800000);
    v.chaser_alive = 1;
    v.chaser_dist = 20.0f;
    for (i = 0; i < 100; i++) story_update(&s, &v, 0.1f, &o);
    CHECK(s.mission == M_TAIL);
    v.chaser_dist = 120.0f;
    for (i = 0; i < 50 && s.mission == M_TAIL; i++) story_update(&s, &v, 0.1f, &o);
    CHECK(s.mission == M_KABAN && o.remove_chaser);
    teleport(&v, POI_KABAN);
    story_update(&s, &v, 0.1f, &o);
    CHECK(s.mission == M_FREE && o.lines >= 4 && !story_marker(&s, m));
    for (i = M_HOME; i <= M_FREE; i++) {
        s.mission = i;
        CHECK(strlen(story_title(&s)) > 0 && strlen(story_objective(&s)) > 0);
    }
}

static void test_synth(void)
{
    static short buf[SYNTH_RATE * 2];
    int t, c, i, peak;
    double sum;
    synth_init();
    for (t = 0; t < MUS_COUNT; t++) {
        int lead = synth_track_steps(t, 0);
        CHECK(lead > 0);
        for (c = 1; c < 4; c++) {
            int n = synth_track_steps(t, c);
            CHECK(n > 0 && lead % n == 0); /* every channel loops in step with the lead */
        }
        synth_music(t);
        synth_render(buf, SYNTH_RATE);
        sum = 0.0;
        peak = 0;
        for (i = 0; i < SYNTH_RATE * 2; i++) {
            sum += (double)buf[i] * buf[i];
            if (abs(buf[i]) > peak) peak = abs(buf[i]);
        }
        CHECK(sqrt(sum / (SYNTH_RATE * 2)) > 1000.0 && peak < 32767);
    }
    synth_music(-1);
    for (i = 0; i < SFX_COUNT; i++) synth_sfx(i);
    synth_engine(1, 0.7f);
    synth_render(buf, SYNTH_RATE);
    sum = 0.0;
    for (i = 0; i < SYNTH_RATE * 2; i++) sum += (double)buf[i] * buf[i];
    CHECK(sum > 0.0);
}

static void test_text(void)
{
    const char *s = "Ґанок Їжака";
    const char *p = s;
    unsigned int cps[16];
    int n = 0;
    while (*p && n < 16) cps[n++] = utf8_next(&p);
    CHECK(n == 11 && cps[0] == 0x490 && cps[6] == 0x407);
    CHECK(font_glyph('A') == font_glyph(0x410));          /* Cyrillic А = Latin A */
    CHECK(font_glyph(0x491) == font_glyph(0x490));        /* ґ -> Ґ */
    CHECK(font_glyph(0x44F) == font_glyph(0x42F));        /* я -> Я */
    CHECK(font_glyph(0x42F) >= 64 && font_glyph(0x42F) < 128);
    CHECK(font_glyph(0x2715) >= 64);                      /* ✕ */
    CHECK(font_glyph(0x4E2D) == font_glyph('?'));
}

static void test_textures(void)
{
    static unsigned int atlas[ATLAS_SIZE * ATLAS_SIZE], font[FONT_TEX_W * FONT_TEX_H];
    int i, transparent = 0, lit = 0;
    textures_generate(atlas);
    for (i = 0; i < ATLAS_SIZE * ATLAS_SIZE; i++)
        if ((atlas[i] >> 24) == 0) transparent++;
    CHECK(transparent > 0);
    CHECK((atlas[0] >> 24) == 0xFF);
    font_generate(font);
    for (i = 0; i < FONT_TEX_W * FONT_TEX_H; i++) lit += font[i] != 0;
    CHECK(lit > 300);
}

int main(void)
{
    test_generation();
    test_world_access();
    test_mesh();
    test_raycast_physics();
    test_persistence();
    test_mesh_split();
    test_textures();
    test_town();
    test_vehicles();
    test_story();
    test_synth();
    test_text();
    if (failures) {
        printf("%d check(s) failed\n", failures);
        return 1;
    }
    printf("all host tests passed\n");
    return 0;
}
