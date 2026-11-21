/* Host-side tests for the platform-independent parts (world, mesh, textures).
 * Build: make -C psp/tests && ./psp/tests/host_test */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

#include "../src/mesh.h"
#include "../src/textures.h"
#include "../src/world.h"

static int failures;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); failures++; } } while (0)

static void test_generation(void)
{
    int x, z, trees = 0, water = 0;
    world_generate(1234);
    for (z = 0; z < WZ; z++)
        for (x = 0; x < WX; x++) {
            int s = world_surface(x, z);
            CHECK(s > 0 && s < WY - 1);
            CHECK(world_get(x, 0, z) != B_AIR);
            if (world_get(x, WATER_LEVEL, z) == B_WATER) water++;
            if (world_get(x, s, z) == B_LEAVES) trees++;
        }
    CHECK(trees > 0);
    printf("generation: %d leaf-topped columns, %d water columns\n", trees, water);
    CHECK(world_get(-1, 5, 0) == B_AIR);
    CHECK(world_get(0, -1, 0) == B_STONE);
}

static void test_raycast(void)
{
    int hit[3], prev[3];
    float o[3] = { 10.5f, 0, 10.5f }, down[3] = { 0, -1, 0 };
    int s = world_surface(10, 10);
    o[1] = (float)s + 3.5f;
    /* clear the column above the surface so leaves do not interfere */
    world_set(10, s + 1, 10, B_AIR); world_set(10, s + 2, 10, B_AIR); world_set(10, s + 3, 10, B_AIR);
    CHECK(world_raycast(o, down, 5.0f, hit, prev));
    CHECK(hit[0] == 10 && hit[1] == s && hit[2] == 10);
    CHECK(prev[0] == 10 && prev[1] == s + 1 && prev[2] == 10);
    o[1] = (float)s + 20.0f;
    CHECK(!world_raycast(o, down, 5.0f, hit, prev));
}

static void test_physics(void)
{
    Player p = { { 20.5f, 0, 20.5f }, { 0, 0, 0 }, 0, 0 };
    int i, s = world_surface(20, 20);
    p.pos[1] = (float)s + 5.0f;
    for (i = 0; i < 240; i++) player_update(&p, 0, 0, 0, 1.0f / 60.0f);
    CHECK(p.on_ground || world_get(20, s + 1, 20) == B_WATER);
    CHECK(p.pos[1] >= s + 1 - 0.01f && p.pos[1] < s + 1.1f);

    /* a wall stops horizontal movement */
    world_generate(99);
    {
        int x, y, z;
        for (y = 0; y < WY; y++)
            for (z = 0; z < WZ; z++)
                for (x = 0; x < WX; x++)
                    world_set(x, y, z, y == 0 ? B_STONE : B_AIR);
        for (y = 1; y < 4; y++)
            for (z = 0; z < WZ; z++) world_set(30, y, z, B_BRICK);
    }
    p.pos[0] = 25.5f; p.pos[1] = 1.0f; p.pos[2] = 20.5f;
    p.vel[0] = p.vel[1] = p.vel[2] = 0;
    for (i = 0; i < 300; i++) player_update(&p, 4.3f, 0, 0, 1.0f / 60.0f);
    CHECK(p.pos[0] <= 30.0f - PLAYER_HALF_W + 0.001f);
    CHECK(p.pos[0] > 29.0f);
    CHECK(p.on_ground);

    /* jump */
    player_update(&p, 0, 0, 1, 1.0f / 60.0f);
    CHECK(p.pos[1] > 1.05f);
    CHECK(player_intersects_block(&p, 29, 2, 20));
    CHECK(!player_intersects_block(&p, 31, 2, 20));
}

static void test_mesh(void)
{
    ChunkMesh m = { { 0, 0, 0 }, { 0, 0, 0 }, 1 };
    int x, y, z;
    for (y = 0; y < WY; y++)
        for (z = 0; z < WZ; z++)
            for (x = 0; x < WX; x++) world_set(x, y, z, B_AIR);

    world_set(5, 5, 5, B_STONE);
    mesh_build(&m, 0, 0);
    CHECK(m.solid.count == 36); /* 6 faces * 2 triangles */
    CHECK(m.water.count == 0);

    world_set(6, 5, 5, B_STONE); /* shared face is hidden */
    mesh_build(&m, 0, 0);
    CHECK(m.solid.count == 60);

    world_set(5, 6, 5, B_WATER);
    mesh_build(&m, 0, 0);
    CHECK(m.solid.count == 60);  /* stone top now borders water: still drawn */
    CHECK(m.water.count == 30);  /* water bottom against stone is hidden */

    world_generate(4321);
    mesh_build(&m, 1, 2);
    CHECK(m.solid.count > 0 && m.solid.count % 6 == 0);
    for (x = 0; x < m.solid.count; x++) {
        const Vertex *v = &m.solid.v[x];
        CHECK(v->u >= 0.0f && v->u <= 1.0f && v->v >= 0.0f && v->v <= 1.0f);
        CHECK(v->x >= 16.0f && v->x <= 32.0f && v->z >= 32.0f && v->z <= 48.0f);
        if (failures) break;
    }
    printf("mesh: chunk (1,2) has %d solid and %d water vertices\n", m.solid.count, m.water.count);
    mesh_free(&m);
}

static void test_save_load(void)
{
    const char *path = "host_test_world.dat";
    world_generate(77);
    world_set(1, 20, 1, B_BRICK);
    CHECK(world_save(path));
    world_generate(78);
    CHECK(world_get(1, 20, 1) != B_BRICK);
    CHECK(world_load(path));
    CHECK(world_get(1, 20, 1) == B_BRICK);
    remove(path);
    CHECK(!world_load("does_not_exist.dat"));
}

static void test_textures(void)
{
    static unsigned int atlas[ATLAS_SIZE * ATLAS_SIZE];
    int i, transparent = 0;
    textures_generate(atlas);
    for (i = 0; i < ATLAS_SIZE * ATLAS_SIZE; i++)
        if ((atlas[i] >> 24) == 0) transparent++;
    CHECK(transparent > 0); /* leaves + glass have holes */
    CHECK((atlas[0] >> 24) == 0xFF);
}

int main(void)
{
    test_generation();
    test_raycast();
    test_physics();
    test_mesh();
    test_save_load();
    test_textures();
    if (failures) {
        printf("%d check(s) failed\n", failures);
        return 1;
    }
    printf("all host tests passed\n");
    return 0;
}
