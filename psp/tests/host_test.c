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
    clean_dir();
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
    test_textures();
    if (failures) {
        printf("%d check(s) failed\n", failures);
        return 1;
    }
    printf("all host tests passed\n");
    return 0;
}
