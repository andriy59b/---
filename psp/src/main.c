/* Kubik — an original voxel sandbox for the PSP (PSPSDK + sceGu). */
#include <pspkernel.h>
#include <pspdisplay.h>
#include <pspctrl.h>
#include <pspgu.h>
#include <pspgum.h>

#include <malloc.h>
#include <math.h>
#include <string.h>

#include "world.h"
#include "mesh.h"
#include "textures.h"

PSP_MODULE_INFO("Kubik", 0, 1, 0);
PSP_MAIN_THREAD_ATTR(THREAD_ATTR_USER | THREAD_ATTR_VFPU);
PSP_HEAP_SIZE_KB(-1024);

#define BUF_WIDTH 512
#define SCR_WIDTH 480
#define SCR_HEIGHT 272
#define SAVE_FILE "world.dat"
#define REACH 5.0f
#define MAX_BATCH 60000 /* vertices per draw call, multiple of 3 */

#define VTYPE_WORLD (GU_TEXTURE_32BITF | GU_COLOR_8888 | GU_VERTEX_32BITF | GU_TRANSFORM_3D)
#define VTYPE_LINE  (GU_COLOR_8888 | GU_VERTEX_32BITF | GU_TRANSFORM_3D)
#define VTYPE_RECT  (GU_COLOR_8888 | GU_VERTEX_32BITF | GU_TRANSFORM_2D)
#define VTYPE_ICON  (GU_TEXTURE_32BITF | GU_COLOR_8888 | GU_VERTEX_32BITF | GU_TRANSFORM_2D)

#define SKY_COLOR 0xFFF0C890u /* ABGR */

typedef struct {
    unsigned int color;
    float x, y, z;
} ColorVertex;

static unsigned int __attribute__((aligned(16))) gu_list[262144];
static unsigned int *atlas;
static ChunkMesh chunks[NCX * NCZ];
static volatile int running = 1;

static const int HOTBAR[] = {
    B_GRASS, B_DIRT, B_STONE, B_SAND, B_WOOD, B_LEAVES, B_PLANKS, B_GLASS, B_BRICK,
};
#define HOTBAR_N ((int)(sizeof(HOTBAR) / sizeof(HOTBAR[0])))

/* ------------------------------------------------------------- callbacks */

static int exit_callback(int arg1, int arg2, void *common)
{
    (void)arg1; (void)arg2; (void)common;
    running = 0;
    return 0;
}

static int callback_thread(SceSize args, void *argp)
{
    int cbid;
    (void)args; (void)argp;
    cbid = sceKernelCreateCallback("Exit Callback", exit_callback, NULL);
    sceKernelRegisterExitCallback(cbid);
    sceKernelSleepThreadCB();
    return 0;
}

static void setup_callbacks(void)
{
    int thid = sceKernelCreateThread("kubik_cb", callback_thread, 0x11, 0xFA0, 0, 0);
    if (thid >= 0) sceKernelStartThread(thid, 0, 0);
}

/* ----------------------------------------------------------------- chunks */

static void mark_dirty_at(int x, int z)
{
    int cx = x / CHUNK, cz = z / CHUNK;
    if (x < 0 || z < 0 || cx >= NCX || cz >= NCZ) return;
    chunks[cz * NCX + cx].dirty = 1;
}

static void block_changed(int x, int y, int z)
{
    (void)y;
    mark_dirty_at(x, z);
    mark_dirty_at(x - 1, z);
    mark_dirty_at(x + 1, z);
    mark_dirty_at(x, z - 1);
    mark_dirty_at(x, z + 1);
}

static void rebuild_dirty(void)
{
    int i, any = 0;
    for (i = 0; i < NCX * NCZ; i++)
        if (chunks[i].dirty) {
            mesh_build(&chunks[i], i % NCX, i / NCX);
            any = 1;
        }
    /* the GE reads vertices straight from RAM */
    if (any) sceKernelDcacheWritebackInvalidateAll();
}

/* --------------------------------------------------------------- graphics */

static void gu_init(void)
{
    void *fbp0 = (void *)0;
    void *fbp1 = (void *)(BUF_WIDTH * SCR_HEIGHT * 4);
    void *zbp = (void *)(2 * BUF_WIDTH * SCR_HEIGHT * 4);

    sceGuInit();
    sceGuStart(GU_DIRECT, gu_list);
    sceGuDrawBuffer(GU_PSM_8888, fbp0, BUF_WIDTH);
    sceGuDispBuffer(SCR_WIDTH, SCR_HEIGHT, fbp1, BUF_WIDTH);
    sceGuDepthBuffer(zbp, BUF_WIDTH);
    sceGuOffset(2048 - (SCR_WIDTH / 2), 2048 - (SCR_HEIGHT / 2));
    sceGuViewport(2048, 2048, SCR_WIDTH, SCR_HEIGHT);
    sceGuDepthRange(65535, 0);
    sceGuScissor(0, 0, SCR_WIDTH, SCR_HEIGHT);
    sceGuEnable(GU_SCISSOR_TEST);
    sceGuDepthFunc(GU_GEQUAL);
    sceGuEnable(GU_DEPTH_TEST);
    sceGuDisable(GU_CULL_FACE);
    sceGuShadeModel(GU_SMOOTH);
    sceGuEnable(GU_CLIP_PLANES);
    sceGuEnable(GU_TEXTURE_2D);
    sceGuAlphaFunc(GU_GREATER, 0x80, 0xFF);
    sceGuEnable(GU_ALPHA_TEST);
    sceGuBlendFunc(GU_ADD, GU_SRC_ALPHA, GU_ONE_MINUS_SRC_ALPHA, 0, 0);
    sceGuFog(24.0f, 58.0f, SKY_COLOR);
    sceGuFinish();
    sceGuSync(0, 0);

    sceDisplayWaitVblankStart();
    sceGuDisplay(GU_TRUE);
}

static void bind_atlas(void)
{
    sceGuTexMode(GU_PSM_8888, 0, 0, 0);
    sceGuTexImage(0, ATLAS_SIZE, ATLAS_SIZE, ATLAS_SIZE, atlas);
    sceGuTexFunc(GU_TFX_MODULATE, GU_TCC_RGBA);
    sceGuTexFilter(GU_NEAREST, GU_NEAREST);
    sceGuTexWrap(GU_CLAMP, GU_CLAMP);
    sceGuTexScale(1.0f, 1.0f);
    sceGuTexOffset(0.0f, 0.0f);
}

static void draw_buf(const VertexBuf *b)
{
    int off = 0;
    while (off < b->count) {
        int n = b->count - off;
        if (n > MAX_BATCH) n = MAX_BATCH;
        sceGumDrawArray(GU_TRIANGLES, VTYPE_WORLD, n, 0, b->v + off);
        off += n;
    }
}

static int chunk_visible(int i, const float eye[3], float fx, float fz)
{
    float cx = (i % NCX) * CHUNK + CHUNK * 0.5f - eye[0];
    float cz = (i / NCX) * CHUNK + CHUNK * 0.5f - eye[2];
    /* behind the camera by more than the chunk radius */
    return cx * fx + cz * fz > -(CHUNK * 0.75f);
}

static void draw_outline(const int b[3])
{
    static const signed char E[12][2][3] = {
        {{0,0,0},{1,0,0}}, {{1,0,0},{1,0,1}}, {{1,0,1},{0,0,1}}, {{0,0,1},{0,0,0}},
        {{0,1,0},{1,1,0}}, {{1,1,0},{1,1,1}}, {{1,1,1},{0,1,1}}, {{0,1,1},{0,1,0}},
        {{0,0,0},{0,1,0}}, {{1,0,0},{1,1,0}}, {{1,0,1},{1,1,1}}, {{0,0,1},{0,1,1}},
    };
    const float g = 0.004f;
    ColorVertex *v = (ColorVertex *)sceGuGetMemory(24 * sizeof(ColorVertex));
    int e, k;
    for (e = 0; e < 12; e++)
        for (k = 0; k < 2; k++) {
            ColorVertex *cv = &v[e * 2 + k];
            cv->color = 0xFF000000u;
            cv->x = b[0] + (E[e][k][0] ? 1.0f + g : -g);
            cv->y = b[1] + (E[e][k][1] ? 1.0f + g : -g);
            cv->z = b[2] + (E[e][k][2] ? 1.0f + g : -g);
        }
    sceGuDisable(GU_TEXTURE_2D);
    sceGumDrawArray(GU_LINES, VTYPE_LINE, 24, 0, v);
    sceGuEnable(GU_TEXTURE_2D);
}

static void rect2d(float x, float y, float w, float h, unsigned int color)
{
    ColorVertex *v = (ColorVertex *)sceGuGetMemory(2 * sizeof(ColorVertex));
    v[0].color = color; v[0].x = x;     v[0].y = y;     v[0].z = 0.0f;
    v[1].color = color; v[1].x = x + w; v[1].y = y + h; v[1].z = 0.0f;
    sceGuDrawArray(GU_SPRITES, VTYPE_RECT, 2, 0, v);
}

static void icon2d(float x, float y, float size, int tile)
{
    const int ts = ATLAS_SIZE / ATLAS_TILES;
    float u = (float)((tile % ATLAS_TILES) * ts), t = (float)((tile / ATLAS_TILES) * ts);
    Vertex *v = (Vertex *)sceGuGetMemory(2 * sizeof(Vertex));
    v[0].u = u;      v[0].v = t;      v[0].color = 0xFFFFFFFFu; v[0].x = x;        v[0].y = y;        v[0].z = 0.0f;
    v[1].u = u + ts; v[1].v = t + ts; v[1].color = 0xFFFFFFFFu; v[1].x = x + size; v[1].y = y + size; v[1].z = 0.0f;
    sceGuDrawArray(GU_SPRITES, VTYPE_ICON, 2, 0, v);
}

static void draw_hud(int selected, int underwater)
{
    const float slot = 24.0f, gap = 2.0f;
    float total = HOTBAR_N * slot + (HOTBAR_N - 1) * gap;
    float x0 = (SCR_WIDTH - total) * 0.5f, y0 = SCR_HEIGHT - slot - 6.0f;
    int i;

    sceGuDisable(GU_DEPTH_TEST);
    sceGuDisable(GU_FOG);
    sceGuEnable(GU_BLEND);
    sceGuDisable(GU_TEXTURE_2D);

    if (underwater) rect2d(0, 0, SCR_WIDTH, SCR_HEIGHT, 0x70A04010u);

    /* crosshair */
    rect2d(SCR_WIDTH / 2 - 6, SCR_HEIGHT / 2 - 1, 12, 2, 0xD0FFFFFFu);
    rect2d(SCR_WIDTH / 2 - 1, SCR_HEIGHT / 2 - 6, 2, 12, 0xD0FFFFFFu);

    /* hotbar frames */
    for (i = 0; i < HOTBAR_N; i++) {
        float x = x0 + i * (slot + gap);
        if (i == selected) rect2d(x - 2, y0 - 2, slot + 4, slot + 4, 0xFFFFFFFFu);
        rect2d(x, y0, slot, slot, 0xA0202020u);
    }

    sceGuEnable(GU_TEXTURE_2D);
    for (i = 0; i < HOTBAR_N; i++)
        icon2d(x0 + i * (slot + gap) + 3, y0 + 3, slot - 6, block_tile(HOTBAR[i], F_PZ));

    sceGuDisable(GU_BLEND);
    sceGuEnable(GU_DEPTH_TEST);
}

/* ------------------------------------------------------------------- main */

static float analog(unsigned char v)
{
    float f = ((int)v - 128) / 127.0f;
    if (f > -0.25f && f < 0.25f) return 0.0f;
    if (f > 1.0f) f = 1.0f;
    if (f < -1.0f) f = -1.0f;
    return f;
}

int main(void)
{
    Player player;
    float yaw = 0.0f, pitch = -0.3f;
    int selected = 0, i;
    unsigned int old_buttons = 0;
    float action_timer = 0.0f;
    unsigned int last_time;

    setup_callbacks();

    sceCtrlSetSamplingCycle(0);
    sceCtrlSetSamplingMode(PSP_CTRL_MODE_ANALOG);

    atlas = (unsigned int *)memalign(16, ATLAS_SIZE * ATLAS_SIZE * 4);
    textures_generate(atlas);

    if (!world_load(SAVE_FILE)) world_generate(sceKernelGetSystemTimeLow());

    memset(chunks, 0, sizeof(chunks));
    for (i = 0; i < NCX * NCZ; i++) chunks[i].dirty = 1;
    rebuild_dirty();
    sceKernelDcacheWritebackInvalidateAll();

    memset(&player, 0, sizeof(player));
    player.pos[0] = WX / 2 + 0.5f;
    player.pos[2] = WZ / 2 + 0.5f;
    player.pos[1] = (float)(world_surface(WX / 2, WZ / 2) + 1);

    gu_init();
    sceGuStart(GU_DIRECT, gu_list);
    sceGuTexFlush();
    sceGuFinish();
    sceGuSync(0, 0);

    last_time = sceKernelGetSystemTimeLow();

    while (running) {
        SceCtrlData pad;
        unsigned int now, pressed;
        float dt, fx, fz, dir[3], eye[3], mx, mz;
        int hit[3], prev[3], has_hit, up, underwater;

        now = sceKernelGetSystemTimeLow();
        dt = (now - last_time) / 1000000.0f;
        last_time = now;
        if (dt > 0.05f) dt = 0.05f;

        sceCtrlReadBufferPositive(&pad, 1);
        pressed = pad.Buttons & ~old_buttons;
        old_buttons = pad.Buttons;

        if (pressed & PSP_CTRL_START) break;

        /* look */
        if (pad.Buttons & PSP_CTRL_SQUARE)   yaw += 2.4f * dt;
        if (pad.Buttons & PSP_CTRL_CIRCLE)   yaw -= 2.4f * dt;
        if (pad.Buttons & PSP_CTRL_TRIANGLE) pitch += 1.8f * dt;
        if (pad.Buttons & PSP_CTRL_CROSS)    pitch -= 1.8f * dt;
        if (pitch > 1.55f) pitch = 1.55f;
        if (pitch < -1.55f) pitch = -1.55f;

        /* hotbar */
        if (pressed & PSP_CTRL_RIGHT) selected = (selected + 1) % HOTBAR_N;
        if (pressed & PSP_CTRL_LEFT)  selected = (selected + HOTBAR_N - 1) % HOTBAR_N;
        if (pressed & PSP_CTRL_SELECT) {
            player.flying = !player.flying;
            player.vel[1] = 0.0f;
        }

        /* move: forward is -Z at yaw 0 */
        fx = -sinf(yaw);
        fz = -cosf(yaw);
        {
            float ax = analog(pad.Lx), ay = -analog(pad.Ly);
            float speed = player.flying ? 8.0f : 4.3f;
            mx = (fx * ay + -fz * ax) * speed;
            mz = (fz * ay + fx * ax) * speed;
        }
        up = (pad.Buttons & PSP_CTRL_UP) ? 1 : ((pad.Buttons & PSP_CTRL_DOWN) && player.flying ? -1 : 0);
        player_update(&player, mx, mz, up, dt);

        /* aim */
        eye[0] = player.pos[0];
        eye[1] = player.pos[1] + PLAYER_EYE;
        eye[2] = player.pos[2];
        dir[0] = fx * cosf(pitch);
        dir[1] = sinf(pitch);
        dir[2] = fz * cosf(pitch);
        has_hit = world_raycast(eye, dir, REACH, hit, prev);

        /* break (L) / place (R), with auto-repeat while held */
        action_timer -= dt;
        if (pressed & (PSP_CTRL_LTRIGGER | PSP_CTRL_RTRIGGER)) action_timer = 0.0f;
        if (has_hit && (pad.Buttons & (PSP_CTRL_LTRIGGER | PSP_CTRL_RTRIGGER)) && action_timer <= 0.0f) {
            if (pad.Buttons & PSP_CTRL_LTRIGGER) {
                world_set(hit[0], hit[1], hit[2], B_AIR);
                block_changed(hit[0], hit[1], hit[2]);
            } else {
                int target = world_get(prev[0], prev[1], prev[2]);
                if ((target == B_AIR || target == B_WATER) && prev[1] >= 0 && prev[1] < WY &&
                    !player_intersects_block(&player, prev[0], prev[1], prev[2])) {
                    world_set(prev[0], prev[1], prev[2], HOTBAR[selected]);
                    block_changed(prev[0], prev[1], prev[2]);
                }
            }
            action_timer = 0.25f;
        }
        rebuild_dirty();

        underwater = world_get((int)floorf(eye[0]), (int)floorf(eye[1]), (int)floorf(eye[2])) == B_WATER;

        /* ---- render ---- */
        sceGuStart(GU_DIRECT, gu_list);
        sceGuClearColor(underwater ? 0xFF804020u : SKY_COLOR);
        sceGuClearDepth(0);
        sceGuClear(GU_COLOR_BUFFER_BIT | GU_DEPTH_BUFFER_BIT);

        sceGumMatrixMode(GU_PROJECTION);
        sceGumLoadIdentity();
        sceGumPerspective(70.0f, (float)SCR_WIDTH / SCR_HEIGHT, 0.1f, 100.0f);

        sceGumMatrixMode(GU_VIEW);
        sceGumLoadIdentity();
        sceGumRotateX(-pitch);
        sceGumRotateY(-yaw);
        {
            ScePspFVector3 t;
            t.x = -eye[0]; t.y = -eye[1]; t.z = -eye[2];
            sceGumTranslate(&t);
        }

        sceGumMatrixMode(GU_MODEL);
        sceGumLoadIdentity();

        sceGuEnable(GU_FOG);
        bind_atlas();

        for (i = 0; i < NCX * NCZ; i++)
            if (chunk_visible(i, eye, fx, fz)) draw_buf(&chunks[i].solid);

        /* translucent water: blended, no depth writes */
        sceGuEnable(GU_BLEND);
        sceGuDepthMask(GU_TRUE);
        for (i = 0; i < NCX * NCZ; i++)
            if (chunk_visible(i, eye, fx, fz)) draw_buf(&chunks[i].water);
        sceGuDepthMask(GU_FALSE);
        sceGuDisable(GU_BLEND);

        if (has_hit) draw_outline(hit);

        draw_hud(selected, underwater);

        sceGuFinish();
        sceGuSync(0, 0);
        sceDisplayWaitVblankStart();
        sceGuSwapBuffers();
    }

    world_save(SAVE_FILE);
    for (i = 0; i < NCX * NCZ; i++) mesh_free(&chunks[i]);
    sceGuTerm();
    sceKernelExitGame();
    return 0;
}
