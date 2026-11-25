/* Kubik — an original voxel sandbox for the PSP (PSPSDK + sceGu). */
#include <pspkernel.h>
#include <pspctrl.h>
#include <psppower.h>

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "gen.h"
#include "mesh.h"
#include "render.h"
#include "world.h"

PSP_MODULE_INFO("Kubik", 0, 1, 1);
PSP_MAIN_THREAD_ATTR(THREAD_ATTR_USER | THREAD_ATTR_VFPU);
PSP_HEAP_SIZE_KB(-1024);

#define WORLD_DIR "kubik_world"
#define LEVEL_FILE WORLD_DIR "/level.dat"
#define DAY_LENGTH 600.0f    /* seconds per full day */
#define AUTOSAVE_EVERY 180.0f
#define REACH 5.0f
#define HOTBAR_N 9
#define PI 3.14159265f

typedef struct {
    char magic[8];
    unsigned int seed;
    float pos[3];
    float yaw, pitch;
    float time;          /* 0..1, 0 = sunrise, 0.25 = noon */
    int flying;
    int hotbar[HOTBAR_N];
    int sel;
    int radius;
    int look_speed;      /* 1..5 */
    int day_cycle;
    int show_info;
} Level;

static const char LEVEL_MAGIC[8] = { 'K', 'U', 'B', 'I', 'K', 'L', '0', '2' };

enum { MODE_PLAY, MODE_PAUSE, MODE_INVENTORY };
enum { MI_RESUME, MI_RADIUS, MI_LOOK, MI_CYCLE, MI_TIME, MI_INFO, MI_QUIT, MI_COUNT };

static Level L;
static Player player;
static volatile int running = 1;

typedef struct { signed char dx, dz; short d2; } Offset;
static Offset offsets[(2 * (MAX_RADIUS + 1) + 1) * (2 * (MAX_RADIUS + 1) + 1)];
static int n_offsets;

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

/* ---------------------------------------------------------------- helpers */

static unsigned int now_us(void) { return sceKernelGetSystemTimeLow(); }

static float clampf(float v, float a, float b) { return v < a ? a : (v > b ? b : v); }

static unsigned int rgb(int r, int g, int b)
{
    return 0xFF000000u | ((unsigned int)b << 16) | ((unsigned int)g << 8) | (unsigned int)r;
}

static unsigned int mix(unsigned int a, unsigned int b, float t)
{
    unsigned int out = 0xFF000000u;
    int sh;
    for (sh = 0; sh < 24; sh += 8) {
        float ca = (float)((a >> sh) & 0xFF), cb = (float)((b >> sh) & 0xFF);
        out |= (unsigned int)(ca + (cb - ca) * t) << sh;
    }
    return out;
}

static int cmp_offset(const void *a, const void *b)
{
    return ((const Offset *)a)->d2 - ((const Offset *)b)->d2;
}

static void build_offsets(void)
{
    int dx, dz, r = MAX_RADIUS + 1;
    n_offsets = 0;
    for (dz = -r; dz <= r; dz++)
        for (dx = -r; dx <= r; dx++) {
            offsets[n_offsets].dx = (signed char)dx;
            offsets[n_offsets].dz = (signed char)dz;
            offsets[n_offsets].d2 = (short)(dx * dx + dz * dz);
            n_offsets++;
        }
    qsort(offsets, (size_t)n_offsets, sizeof(Offset), cmp_offset);
}

static int cheb(const Offset *o) { return abs(o->dx) > abs(o->dz) ? abs(o->dx) : abs(o->dz); }

/* ------------------------------------------------------------ worker thread
 * Terrain generation and mesh building run on a lower-priority thread, so they
 * only use the time the main thread spends waiting for the GPU and vblank. */

enum { JOB_IDLE = 0, JOB_BUSY, JOB_DONE };
enum { JOB_GEN = 1, JOB_MESH };

static volatile int job_state = JOB_IDLE;
static int job_type, job_cx, job_cz;
static unsigned char job_blocks[CH_VOL];
static SceUID job_sema = -1;

static int worker_thread(SceSize args, void *argp)
{
    (void)args; (void)argp;
    for (;;) {
        sceKernelWaitSema(job_sema, 1, NULL);
        if (job_type == JOB_GEN) gen_chunk(L.seed, job_cx, job_cz, job_blocks);
        else mesh_compute();
        job_state = JOB_DONE;
    }
    return 0;
}

static void start_worker(void)
{
    SceUID th;
    job_sema = sceKernelCreateSema("kubik_job", 0, 0, 1, NULL);
    th = sceKernelCreateThread("kubik_worker", worker_thread, 0x30, 0x10000, PSP_THREAD_ATTR_USER, NULL);
    if (th >= 0) sceKernelStartThread(th, 0, NULL);
}

static void post_job(int type, int cx, int cz)
{
    job_type = type;
    job_cx = cx;
    job_cz = cz;
    job_state = JOB_BUSY;
    sceKernelSignalSema(job_sema, 1);
}

/* Commits finished work and hands the worker the next job, nearest chunks first:
 * dirty meshes (edits) before missing terrain. Saved chunks load right here. */
static void stream_chunks(int pcx, int pcz, int radius)
{
    int i, loads = 0;

    if (job_state == JOB_DONE) {
        if (job_type == JOB_GEN) {
            if (abs(job_cx - pcx) <= radius + 2 && abs(job_cz - pcz) <= radius + 2 &&
                !world_commit_chunk(job_cx, job_cz, job_blocks) && L.radius > 2)
                L.radius--; /* out of memory: draw less */
        } else {
            Chunk *c = world_chunk(job_cx, job_cz);
            if (c) {
                mesh_commit(c);
                r_flush_cache();
            }
        }
        job_state = JOB_IDLE;
    }

    if (job_state == JOB_IDLE) {
        for (i = 0; i < n_offsets; i++) {
            Chunk *c;
            if (cheb(&offsets[i]) > radius) continue;
            c = world_chunk(pcx + offsets[i].dx, pcz + offsets[i].dz);
            if (!c || !c->mesh_dirty || !world_neighbors_loaded(c)) continue;
            mesh_gather(c);
            post_job(JOB_MESH, c->cx, c->cz);
            break;
        }
    }

    for (i = 0; i < n_offsets && loads < 2; i++) {
        int cx = pcx + offsets[i].dx, cz = pcz + offsets[i].dz;
        if (cheb(&offsets[i]) > radius + 1) continue;
        if (world_chunk(cx, cz)) continue;
        if (job_state != JOB_IDLE && job_type == JOB_GEN && job_cx == cx && job_cz == cz) continue;
        if (world_has_saved(cx, cz) && world_load_saved(cx, cz)) {
            loads++;
            continue;
        }
        if (job_state == JOB_IDLE) post_job(JOB_GEN, cx, cz);
        break;
    }

    world_free_far_meshes(pcx, pcz, radius + 1);
}

static int area_ready(int pcx, int pcz, int radius)
{
    int dx, dz;
    for (dz = -radius; dz <= radius; dz++)
        for (dx = -radius; dx <= radius; dx++) {
            Chunk *c = world_chunk(pcx + dx, pcz + dz);
            if (!c || !c->meshed) return 0;
        }
    return 1;
}

/* ------------------------------------------------------------------ level */

static void level_defaults(void)
{
    static const int HOTBAR_DEFAULT[HOTBAR_N] = {
        B_GRASS, B_DIRT, B_STONE, B_COBBLE, B_PLANKS, B_LOG, B_GLASS, B_BRICK, B_STONE_BRICKS,
    };
    memset(&L, 0, sizeof(L));
    memcpy(L.magic, LEVEL_MAGIC, sizeof(LEVEL_MAGIC));
    L.seed = now_us() ^ 0x5EEDu;
    L.time = 0.04f;
    L.pitch = -0.2f;
    memcpy(L.hotbar, HOTBAR_DEFAULT, sizeof(HOTBAR_DEFAULT));
    L.radius = 5;
    L.look_speed = 3;
    L.day_cycle = 1;
    L.show_info = 0;
}

static int level_load(void)
{
    FILE *f = fopen(LEVEL_FILE, "rb");
    int ok;
    if (!f) return 0;
    ok = fread(&L, 1, sizeof(L), f) == sizeof(L) && memcmp(L.magic, LEVEL_MAGIC, sizeof(LEVEL_MAGIC)) == 0;
    fclose(f);
    if (ok) {
        if (L.radius < 2 || L.radius > MAX_RADIUS) L.radius = 5;
        if (L.look_speed < 1 || L.look_speed > 5) L.look_speed = 3;
        if (L.sel < 0 || L.sel >= HOTBAR_N) L.sel = 0;
    }
    return ok;
}

static void level_save(void)
{
    FILE *f;
    memcpy(L.pos, player.pos, sizeof(L.pos));
    L.flying = player.flying;
    world_save_all();
    f = fopen(LEVEL_FILE, "wb");
    if (!f) return;
    fwrite(&L, 1, sizeof(L), f);
    fclose(f);
}

static void find_spawn(float pos[3])
{
    int i, bio, h = 0;
    for (i = 0; i < 400; i++) {
        int x = (i % 20) * 16 * ((i / 20) % 2 ? -1 : 1), z = (i / 20) * 16;
        h = gen_column(L.seed, x, z, &bio);
        if (h > SEA_LEVEL + 1 && bio != BIOME_OCEAN && bio != BIOME_MOUNTAINS) {
            pos[0] = x + 0.5f;
            pos[2] = z + 0.5f;
            break;
        }
    }
    pos[1] = (float)(h + 1);
}

/* -------------------------------------------------------------------- sky */

static void compute_sky(SkyState *s, float time_s, int underwater)
{
    float a = L.time * 2.0f * PI, sh = sinf(a);
    float light = clampf(0.5f + sh * 2.5f, 0.0f, 1.0f);
    float glow = clampf(1.0f - fabsf(sh) * 4.0f, 0.0f, 1.0f);
    float ov = (1.0f - light) * 0.72f;
    unsigned int night_overlay = rgb(8, 10, 30);
    int sh8;

    s->sun_angle = a;
    s->time_s = time_s;
    s->underwater = underwater;
    s->night = 1.0f - light;
    s->zenith = mix(rgb(4, 6, 20), rgb(78, 140, 230), light);
    s->horizon = mix(mix(rgb(16, 20, 44), rgb(176, 210, 246), light), rgb(244, 150, 90), glow * 0.55f);
    s->cloud = mix(mix(rgb(46, 50, 68), rgb(255, 255, 255), light), rgb(255, 196, 160), glow * 0.45f);

    /* terrain gets darkened by the night overlay: pre-compensate the fog so the
     * horizon of the terrain matches the sky */
    s->fog = 0xFF000000u;
    for (sh8 = 0; sh8 < 24; sh8 += 8) {
        float h = (float)((s->horizon >> sh8) & 0xFF), o = (float)((night_overlay >> sh8) & 0xFF);
        float f = (h - o * ov) / (1.0f - ov);
        s->fog |= (unsigned int)clampf(f, 0.0f, 255.0f) << sh8;
    }
    s->fog_end = L.radius * CH - 4.0f;
    s->fog_start = s->fog_end * 0.55f;

    if (underwater) {
        s->fog = mix(rgb(6, 14, 36), rgb(24, 64, 140), light);
        s->fog_start = 1.0f;
        s->fog_end = 22.0f;
    }
}

/* -------------------------------------------------------------------- HUD */

static void text_center(float y, int scale, unsigned int color, const char *s)
{
    r_text((SCR_W - r_text_width(scale, s)) * 0.5f, y, scale, color, s);
}

static void draw_hotbar(void)
{
    const float slot = 26.0f, gap = 2.0f;
    float total = HOTBAR_N * slot + (HOTBAR_N - 1) * gap;
    float x0 = (SCR_W - total) * 0.5f, y0 = SCR_H - slot - 4.0f;
    int i;
    for (i = 0; i < HOTBAR_N; i++) {
        float x = x0 + i * (slot + gap);
        if (i == L.sel) r_rect(x - 2, y0 - 2, slot + 4, slot + 4, 0xFFFFFFFFu);
        r_rect(x, y0, slot, slot, 0xB0201C18u);
        r_block_icon(x + 3, y0 + 3, slot - 6, L.hotbar[i]);
    }
}

static void draw_hud(float fps, int has_hit, const int hit[3], float name_timer)
{
    char buf[96];
    r_begin_2d();

    if (player.in_water && world_get((int)floorf(player.pos[0]), (int)floorf(player.pos[1] + PLAYER_EYE),
                                     (int)floorf(player.pos[2])) == B_WATER)
        r_rect(0, 0, SCR_W, SCR_H, 0x60702810u);

    r_rect(SCR_W / 2 - 7, SCR_H / 2 - 1, 14, 2, 0xC0FFFFFFu);
    r_rect(SCR_W / 2 - 1, SCR_H / 2 - 7, 2, 14, 0xC0FFFFFFu);

    draw_hotbar();

    if (name_timer > 0.0f) {
        unsigned int a = (unsigned int)(clampf(name_timer, 0.0f, 1.0f) * 255.0f);
        text_center(SCR_H - 50, 1, (a << 24) | 0xFFFFFF, BLOCKS[L.hotbar[L.sel]].name);
    }
    if (player.flying) r_text(SCR_W - 34, 4, 1, 0xFFA0E0FFu, "FLY");

    if (L.show_info) {
        int bio, px = (int)floorf(player.pos[0]), pz = (int)floorf(player.pos[2]);
        int hours = ((int)(L.time * 24.0f) + 6) % 24, mins = (int)(L.time * 24.0f * 60.0f) % 60;
        gen_column(L.seed, px, pz, &bio);
        snprintf(buf, sizeof(buf), "FPS %d  XYZ %d %d %d", (int)(fps + 0.5f), px,
                 (int)floorf(player.pos[1]), pz);
        r_text(4, 4, 1, 0xFFFFFFFFu, buf);
        snprintf(buf, sizeof(buf), "%s  %02d:%02d  VIEW %d", biome_name(bio), hours, mins, L.radius);
        r_text(4, 14, 1, 0xFFFFFFFFu, buf);
        if (has_hit) {
            snprintf(buf, sizeof(buf), "LOOKING AT %s", BLOCKS[world_get(hit[0], hit[1], hit[2])].name);
            r_text(4, 24, 1, 0xFFD0D0D0u, buf);
        }
    }
}

static void draw_pause(int sel)
{
    static const char *TITLES[MI_COUNT] = {
        "RESUME", "RENDER DISTANCE", "LOOK SPEED", "DAY CYCLE", "SET TIME", "SHOW INFO", "SAVE AND QUIT",
    };
    char buf[64];
    int i;
    r_begin_2d();
    r_rect(0, 0, SCR_W, SCR_H, 0x90000000u);
    text_center(28, 4, 0xFFFFFFFFu, "KUBIK");
    for (i = 0; i < MI_COUNT; i++) {
        unsigned int col = i == sel ? 0xFF40E0FFu : 0xFFE0E0E0u;
        switch (i) {
        case MI_RADIUS: snprintf(buf, sizeof(buf), "%s: < %d >", TITLES[i], L.radius); break;
        case MI_LOOK:   snprintf(buf, sizeof(buf), "%s: < %d >", TITLES[i], L.look_speed); break;
        case MI_CYCLE:  snprintf(buf, sizeof(buf), "%s: %s", TITLES[i], L.day_cycle ? "ON" : "OFF"); break;
        case MI_TIME:   snprintf(buf, sizeof(buf), "%s: %s", TITLES[i],
                                 (L.time > 0.02f && L.time < 0.48f) ? "DAY" : "NIGHT"); break;
        case MI_INFO:   snprintf(buf, sizeof(buf), "%s: %s", TITLES[i], L.show_info ? "ON" : "OFF"); break;
        default:        snprintf(buf, sizeof(buf), "%s", TITLES[i]);
        }
        text_center(78 + i * 22, 2, col, buf);
    }
    text_center(SCR_H - 16, 1, 0xFFA0A0A0u, "UP/DOWN: SELECT   X: CHANGE   LEFT/RIGHT: ADJUST   START: BACK");
}

#define INV_COLS 9
static void draw_inventory(int cursor)
{
    const float cell = 30.0f;
    int n = B_COUNT - 1, rows = (n + INV_COLS - 1) / INV_COLS, i;
    float w = INV_COLS * cell + 16, h = rows * cell + 46;
    float x0 = (SCR_W - w) * 0.5f, y0 = 34.0f;

    r_begin_2d();
    r_rect(0, 0, SCR_W, SCR_H, 0x80000000u);
    r_rect(x0, y0, w, h, 0xE0302A26u);
    r_text(x0 + 8, y0 + 7, 2, 0xFFFFFFFFu, "BLOCKS");
    for (i = 0; i < n; i++) {
        float x = x0 + 8 + (i % INV_COLS) * cell, y = y0 + 28 + (i / INV_COLS) * cell;
        if (i == cursor) r_rect(x, y, cell - 2, cell - 2, 0xFFFFFFFFu);
        r_rect(x + 1, y + 1, cell - 4, cell - 4, 0xFF4A443Eu);
        r_block_icon(x + 4, y + 4, cell - 10, i + 1);
    }
    text_center(y0 + h + 6, 2, 0xFFFFFFFFu, BLOCKS[cursor + 1].name);
    draw_hotbar();
    text_center(y0 + h + 28, 1, 0xFFA0A0A0u, "X: PUT IN SLOT   L/R: CHANGE SLOT   O/SELECT: CLOSE");
}

static void draw_loading(int done, int total)
{
    char buf[64];
    r_begin_frame(rgb(30, 34, 46));
    r_begin_2d();
    text_center(110, 3, 0xFFFFFFFFu, "KUBIK");
    snprintf(buf, sizeof(buf), "GENERATING WORLD... %d%%", total ? done * 100 / total : 0);
    text_center(150, 1, 0xFFC0C0C0u, buf);
    r_rect(140, 166, 200, 6, 0xFF404040u);
    r_rect(140, 166, total ? 200.0f * done / total : 0, 6, 0xFF60C060u);
    r_end_frame();
}

/* ------------------------------------------------------------------- main */

static float analog(unsigned char v)
{
    float f = ((int)v - 128) / 127.0f;
    if (f > -0.2f && f < 0.2f) return 0.0f;
    f = (f > 0 ? f - 0.2f : f + 0.2f) / 0.8f;
    return clampf(f, -1.0f, 1.0f);
}

static void break_block(const int hit[3])
{
    int above = world_get(hit[0], hit[1] + 1, hit[2]);
    if (world_get(hit[0], hit[1], hit[2]) == B_BEDROCK) return;
    world_set(hit[0], hit[1], hit[2], B_AIR);
    if (block_shape(above) == SHAPE_CROSS || above == B_CACTUS) world_set(hit[0], hit[1] + 1, hit[2], B_AIR);
}

static void place_block(const int hit[3], const int prev[3])
{
    int b = L.hotbar[L.sel];
    const int *t = prev;
    int target, below;
    if (block_shape(world_get(hit[0], hit[1], hit[2])) == SHAPE_CROSS) t = hit; /* replace plants */
    if (t[1] < 0 || t[1] >= WH) return;
    target = world_get(t[0], t[1], t[2]);
    if (target != B_AIR && target != B_WATER && block_shape(target) != SHAPE_CROSS) return;
    below = world_get(t[0], t[1] - 1, t[2]);
    if (block_shape(b) == SHAPE_CROSS && below != B_GRASS && below != B_DIRT && below != B_SNOW_GRASS) return;
    if (block_solid(b) && player_intersects_block(&player, t[0], t[1], t[2])) return;
    world_set(t[0], t[1], t[2], b);
}

int main(void)
{
    SceCtrlData pad;
    SkyState sky;
    unsigned int old_buttons = 0, last_time;
    float action_timer = 0.0f, swing = 0.0f, bob = 0.0f, name_timer = 0.0f;
    float last_up = -1.0f, clock_s = 0.0f, autosave = 0.0f;
    float fps = 60.0f, fps_acc = 0.0f;
    int fps_frames = 0, mode = MODE_PLAY, menu_sel = 0, inv_cursor = 0, is_new;

    setup_callbacks();
    sceCtrlSetSamplingCycle(0);
    sceCtrlSetSamplingMode(PSP_CTRL_MODE_ANALOG);

    scePowerSetClockFrequency(333, 333, 166);
    r_init();
    build_offsets();
    start_worker();

    is_new = !level_load();
    if (is_new) level_defaults();
    world_init(L.seed, WORLD_DIR);
    memset(&player, 0, sizeof(player));
    if (is_new) {
        find_spawn(player.pos);
    } else {
        memcpy(player.pos, L.pos, sizeof(player.pos));
        player.flying = L.flying;
    }

    /* load the area around the player before starting */
    {
        int pcx = (int)floorf(player.pos[0]) >> 4, pcz = (int)floorf(player.pos[2]) >> 4;
        int r = L.radius < 3 ? L.radius : 3, side = 2 * r + 1, total = side * side;
        while (running && !area_ready(pcx, pcz, r)) {
            int dx, dz, done = 0;
            stream_chunks(pcx, pcz, r);
            for (dz = -r; dz <= r; dz++)
                for (dx = -r; dx <= r; dx++) {
                    Chunk *c = world_chunk(pcx + dx, pcz + dz);
                    if (c && c->meshed) done++;
                }
            draw_loading(done, total);
        }
        if (is_new) {
            int top = world_top((int)floorf(player.pos[0]), (int)floorf(player.pos[2]));
            player.pos[1] = (float)(top + 1);
        }
    }

    last_time = now_us();

    while (running) {
        unsigned int now = now_us(), pressed;
        float dt = (now - last_time) / 1000000.0f, fx, fz, dir[3], eye[3];
        int hit[3] = { 0, 0, 0 }, prev[3], has_hit = 0, pcx, pcz, underwater;

        last_time = now;
        if (dt > 0.05f) dt = 0.05f;
        clock_s += dt;

        fps_acc += dt;
        fps_frames++;
        if (fps_acc >= 0.5f) {
            fps = fps_frames / fps_acc;
            fps_acc = 0.0f;
            fps_frames = 0;
        }

        sceCtrlReadBufferPositive(&pad, 1);
        pressed = pad.Buttons & ~old_buttons;
        old_buttons = pad.Buttons;

        /* ---------------------------------------------------------- input */
        if (mode == MODE_PAUSE) {
            if (pressed & (PSP_CTRL_START | PSP_CTRL_CIRCLE)) mode = MODE_PLAY;
            if (pressed & PSP_CTRL_UP) menu_sel = (menu_sel + MI_COUNT - 1) % MI_COUNT;
            if (pressed & PSP_CTRL_DOWN) menu_sel = (menu_sel + 1) % MI_COUNT;
            if (pressed & (PSP_CTRL_LEFT | PSP_CTRL_RIGHT | PSP_CTRL_CROSS)) {
                int d = (pressed & PSP_CTRL_LEFT) ? -1 : 1;
                switch (menu_sel) {
                case MI_RESUME: if (pressed & PSP_CTRL_CROSS) mode = MODE_PLAY; break;
                case MI_RADIUS: L.radius += d; if (L.radius < 2) L.radius = MAX_RADIUS; if (L.radius > MAX_RADIUS) L.radius = 2; break;
                case MI_LOOK:   L.look_speed += d; if (L.look_speed < 1) L.look_speed = 5; if (L.look_speed > 5) L.look_speed = 1; break;
                case MI_CYCLE:  L.day_cycle = !L.day_cycle; break;
                case MI_TIME:   L.time = (L.time > 0.02f && L.time < 0.48f) ? 0.55f : 0.05f; break;
                case MI_INFO:   L.show_info = !L.show_info; break;
                case MI_QUIT:   if (pressed & PSP_CTRL_CROSS) running = 0; break;
                }
            }
        } else if (mode == MODE_INVENTORY) {
            int n = B_COUNT - 1;
            if (pressed & (PSP_CTRL_SELECT | PSP_CTRL_CIRCLE)) mode = MODE_PLAY;
            if (pressed & PSP_CTRL_LEFT) inv_cursor = (inv_cursor + n - 1) % n;
            if (pressed & PSP_CTRL_RIGHT) inv_cursor = (inv_cursor + 1) % n;
            if ((pressed & PSP_CTRL_UP) && inv_cursor >= INV_COLS) inv_cursor -= INV_COLS;
            if ((pressed & PSP_CTRL_DOWN) && inv_cursor + INV_COLS < n) inv_cursor += INV_COLS;
            if (pressed & PSP_CTRL_LTRIGGER) L.sel = (L.sel + HOTBAR_N - 1) % HOTBAR_N;
            if (pressed & PSP_CTRL_RTRIGGER) L.sel = (L.sel + 1) % HOTBAR_N;
            if (pressed & PSP_CTRL_CROSS) {
                L.hotbar[L.sel] = inv_cursor + 1;
                name_timer = 2.0f;
            }
        } else {
            float ls = 1.2f + 0.6f * L.look_speed, ax, ay, speed, mx, mz;
            int up, moving;

            if (pressed & PSP_CTRL_START) { mode = MODE_PAUSE; menu_sel = 0; }
            if (pressed & PSP_CTRL_SELECT) { mode = MODE_INVENTORY; }

            if (pad.Buttons & PSP_CTRL_SQUARE)   L.yaw += ls * dt;
            if (pad.Buttons & PSP_CTRL_CIRCLE)   L.yaw -= ls * dt;
            if (pad.Buttons & PSP_CTRL_TRIANGLE) L.pitch += ls * 0.75f * dt;
            if (pad.Buttons & PSP_CTRL_CROSS)    L.pitch -= ls * 0.75f * dt;
            L.pitch = clampf(L.pitch, -1.55f, 1.55f);
            if (L.yaw > PI) L.yaw -= 2 * PI;
            if (L.yaw < -PI) L.yaw += 2 * PI;

            if (pressed & PSP_CTRL_RIGHT) { L.sel = (L.sel + 1) % HOTBAR_N; name_timer = 2.0f; }
            if (pressed & PSP_CTRL_LEFT)  { L.sel = (L.sel + HOTBAR_N - 1) % HOTBAR_N; name_timer = 2.0f; }

            if (pressed & PSP_CTRL_UP) {
                if (clock_s - last_up < 0.3f) {
                    player.flying = !player.flying;
                    player.vel[1] = 0.0f;
                    last_up = -1.0f;
                } else {
                    last_up = clock_s;
                }
            }

            fx = -sinf(L.yaw);
            fz = -cosf(L.yaw);
            ax = analog(pad.Lx);
            ay = -analog(pad.Ly);
            speed = player.flying ? 10.0f : 4.3f;
            mx = (fx * ay - fz * ax) * speed;
            mz = (fz * ay + fx * ax) * speed;
            up = (pad.Buttons & PSP_CTRL_UP) ? 1 : ((pad.Buttons & PSP_CTRL_DOWN) && player.flying ? -1 : 0);
            if (world_chunk((int)floorf(player.pos[0]) >> 4, (int)floorf(player.pos[2]) >> 4))
                player_update(&player, mx, mz, up, dt);
            moving = (ax != 0.0f || ay != 0.0f) && player.on_ground;
            if (moving) bob += dt * 9.0f;

            eye[0] = player.pos[0];
            eye[1] = player.pos[1] + PLAYER_EYE;
            eye[2] = player.pos[2];
            dir[0] = fx * cosf(L.pitch);
            dir[1] = sinf(L.pitch);
            dir[2] = fz * cosf(L.pitch);
            has_hit = world_raycast(eye, dir, REACH, hit, prev);

            action_timer -= dt;
            if (pressed & (PSP_CTRL_LTRIGGER | PSP_CTRL_RTRIGGER)) action_timer = 0.0f;
            if ((pad.Buttons & (PSP_CTRL_LTRIGGER | PSP_CTRL_RTRIGGER)) && action_timer <= 0.0f) {
                if (has_hit) {
                    if (pad.Buttons & PSP_CTRL_LTRIGGER) break_block(hit);
                    else place_block(hit, prev);
                }
                swing = 1.0f;
                action_timer = 0.22f;
            }
        }

        swing = swing > 0.0f ? swing - dt * 5.0f : 0.0f;
        if (name_timer > 0.0f) name_timer -= dt;
        if (L.day_cycle) {
            L.time += dt / DAY_LENGTH;
            if (L.time >= 1.0f) L.time -= 1.0f;
        }
        autosave += dt;
        if (autosave > AUTOSAVE_EVERY) {
            autosave = 0.0f;
            level_save();
        }

        /* --------------------------------------------------------- world */
        pcx = (int)floorf(player.pos[0]) >> 4;
        pcz = (int)floorf(player.pos[2]) >> 4;
        stream_chunks(pcx, pcz, L.radius);

        eye[0] = player.pos[0];
        eye[1] = player.pos[1] + PLAYER_EYE;
        eye[2] = player.pos[2];
        if (mode != MODE_PLAY) {
            fx = -sinf(L.yaw);
            fz = -cosf(L.yaw);
            dir[0] = fx * cosf(L.pitch);
            dir[1] = sinf(L.pitch);
            dir[2] = fz * cosf(L.pitch);
            has_hit = world_raycast(eye, dir, REACH, hit, prev);
        }
        underwater = world_get((int)floorf(eye[0]), (int)floorf(eye[1]), (int)floorf(eye[2])) == B_WATER;
        compute_sky(&sky, clock_s, underwater);

        /* -------------------------------------------------------- render */
        r_begin_frame(underwater ? sky.fog : sky.zenith);
        r_sky(&sky, L.yaw, L.pitch);
        r_camera(eye, L.yaw, L.pitch);
        r_world(eye, L.yaw, L.pitch, pcx, pcz, L.radius, &sky);
        if (has_hit && mode == MODE_PLAY) r_outline(hit);
        r_night_overlay(&sky);
        r_camera(eye, L.yaw, L.pitch);
        r_clouds(eye, &sky);
        if (mode == MODE_PLAY) r_held_block(L.hotbar[L.sel], swing, bob, 1.0f - sky.night * 0.65f);

        if (mode == MODE_PLAY) draw_hud(fps, has_hit, hit, name_timer);
        else if (mode == MODE_PAUSE) draw_pause(menu_sel);
        else draw_inventory(inv_cursor);
        r_end_frame();
    }

    while (job_state == JOB_BUSY) sceKernelDelayThread(1000);
    level_save();
    world_shutdown();
    sceKernelExitGame();
    return 0;
}
