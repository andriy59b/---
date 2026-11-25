/* UkraineCraft — an original open-world voxel game for the PSP set in 1996.
 * Main loop: modes (splash, menus, intro, play, dialogue), input, cameras,
 * chunk streaming on a worker thread, saving. */
#include <pspkernel.h>
#include <pspctrl.h>
#include <psppower.h>

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "audio.h"
#include "game.h"
#include "gen.h"
#include "mesh.h"
#include "render.h"
#include "story.h"
#include "synth.h"
#include "town.h"
#include "vehicle.h"
#include "world.h"

PSP_MODULE_INFO("UkraineCraft", 0, 1, 1);
PSP_MAIN_THREAD_ATTR(THREAD_ATTR_USER | THREAD_ATTR_VFPU);
PSP_HEAP_SIZE_KB(-1024);

#define DAY_LENGTH 720.0f
#define AUTOSAVE_EVERY 180.0f
#define REACH 5.0f
#define PI 3.14159265f
#define FAMILY_CAR_COLOR 0xFFA8C8DCu
#define CHASER_COLOR 0xFF141414u

enum {
    MODE_SPLASH, MODE_MENU, MODE_CONFIRM, MODE_SETTINGS, MODE_LOADING,
    MODE_INTRO, MODE_PLAY, MODE_PAUSE, MODE_INVENTORY, MODE_DIALOG
};

static const char LEVEL_MAGIC[8] = { 'U', 'K', 'R', 'C', 'R', '0', '0', '1' };

static Level L;
static Player player;
static volatile int running = 1;

static int mode = MODE_SPLASH, settings_return = MODE_MENU, loading_next = MODE_PLAY;
static int has_save, in_game;
static float mode_time;

static int car_idx = -1;
static float cam_yaw, body_yaw;

#define DLG_MAX 32
static const char *dlg_speaker[DLG_MAX], *dlg_text[DLG_MAX];
static int dlg_n, dlg_i;

static const char *banner, *banner_sub;
static float banner_t;
static char banner_buf[96];

static const char *const STATION_NAMES[RADIO_STATIONS] = {
    "Радіо Соняшник FM 103.5", "Шансон-хвиля 88.2", "Радіо вимкнено",
};
static const int STATION_TRACKS[RADIO_STATIONS][2] = {
    { MUS_DISCO, MUS_ROCK }, { MUS_WALTZ, MUS_WALTZ }, { -1, -1 },
};
static int radio_track_i;
static float radio_timer;
static char radio_buf[96];

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
    int thid = sceKernelCreateThread("uc_cb", callback_thread, 0x11, 0xFA0, 0, 0);
    if (thid >= 0) sceKernelStartThread(thid, 0, 0);
}

/* ---------------------------------------------------------------- helpers */

static unsigned int now_us(void) { return sceKernelGetSystemTimeLow(); }
static float clampf(float v, float a, float b) { return v < a ? a : (v > b ? b : v); }

static float wrapf(float a)
{
    while (a > PI) a -= 2 * PI;
    while (a < -PI) a += 2 * PI;
    return a;
}

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

static float analog(unsigned char v)
{
    float f = ((int)v - 128) / 127.0f;
    if (f > -0.2f && f < 0.2f) return 0.0f;
    f = (f > 0 ? f - 0.2f : f + 0.2f) / 0.8f;
    return clampf(f, -1.0f, 1.0f);
}

static void show_banner(const char *text, const char *sub)
{
    snprintf(banner_buf, sizeof(banner_buf), "%s", text);
    banner = banner_buf;
    banner_sub = sub;
    banner_t = 3.0f;
}

static void apply_volumes(void)
{
    synth_volumes(L.music_vol / 10.0f, L.sfx_vol / 10.0f);
}

/* ------------------------------------------------------------ worker thread */

enum { JOB_IDLE = 0, JOB_BUSY, JOB_DONE };
enum { JOB_GEN = 1, JOB_MESH };

static volatile int job_state = JOB_IDLE;
static int job_type, job_cx, job_cz;
static unsigned int job_seed;
static unsigned char job_blocks[CH_VOL];
static SceUID job_sema = -1;

static int worker_thread(SceSize args, void *argp)
{
    (void)args; (void)argp;
    for (;;) {
        sceKernelWaitSema(job_sema, 1, NULL);
        if (job_type == JOB_GEN) gen_chunk(job_seed, job_cx, job_cz, job_blocks);
        else mesh_compute();
        job_state = JOB_DONE;
    }
    return 0;
}

static void start_worker(void)
{
    SceUID th;
    job_sema = sceKernelCreateSema("uc_job", 0, 0, 1, NULL);
    th = sceKernelCreateThread("uc_worker", worker_thread, 0x30, 0x10000, PSP_THREAD_ATTR_USER, NULL);
    if (th >= 0) sceKernelStartThread(th, 0, NULL);
}

static void post_job(int type, int cx, int cz)
{
    job_type = type;
    job_cx = cx;
    job_cz = cz;
    job_seed = L.seed;
    job_state = JOB_BUSY;
    sceKernelSignalSema(job_sema, 1);
}

static void worker_drain(void)
{
    while (job_state == JOB_BUSY) sceKernelDelayThread(1000);
    job_state = JOB_IDLE;
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

static void stream_chunks(int pcx, int pcz, int radius)
{
    int i, loads = 0;

    if (job_state == JOB_DONE) {
        if (job_type == JOB_GEN) {
            if (abs(job_cx - pcx) <= radius + 2 && abs(job_cz - pcz) <= radius + 2 &&
                !world_commit_chunk(job_cx, job_cz, job_blocks) && L.radius > 2)
                L.radius--;
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

static int area_progress(int pcx, int pcz, int radius, int *total)
{
    int dx, dz, done = 0;
    *total = (2 * radius + 1) * (2 * radius + 1);
    for (dz = -radius; dz <= radius; dz++)
        for (dx = -radius; dx <= radius; dx++) {
            Chunk *c = world_chunk(pcx + dx, pcz + dz);
            if (c && c->meshed) done++;
        }
    return done;
}

/* ------------------------------------------------------------------ level */

static void level_defaults(void)
{
    static const int HOTBAR_DEFAULT[HOTBAR_N] = {
        B_PANEL, B_PANEL_WINDOW, B_BRICK, B_PLASTER, B_ROOF, B_PLANKS, B_GLASS, B_ASPHALT, B_SUNFLOWER,
    };
    memset(&L, 0, sizeof(L));
    memcpy(L.magic, LEVEL_MAGIC, sizeof(LEVEL_MAGIC));
    L.seed = now_us() ^ 0x1996u;
    L.time = 0.08f;
    memcpy(L.hotbar, HOTBAR_DEFAULT, sizeof(HOTBAR_DEFAULT));
    L.radius = 5;
    L.look_speed = 3;
    L.day_cycle = 1;
    L.music_vol = 7;
    L.sfx_vol = 9;
    L.third_person = 0;
    story_start(&L.story);
    memcpy(L.pos, POI_SPAWN, sizeof(L.pos));
    L.yaw = POI_SPAWN_YAW;
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
        if (L.radio < 0 || L.radio >= RADIO_STATIONS) L.radio = 0;
        if (L.music_vol < 0 || L.music_vol > 10) L.music_vol = 7;
        if (L.sfx_vol < 0 || L.sfx_vol > 10) L.sfx_vol = 9;
    }
    return ok;
}

static void level_save(void)
{
    FILE *f;
    int own = -1, i;
    if (car_idx >= 0) {
        car_exit_point(&cars[car_idx], L.pos);
    } else {
        memcpy(L.pos, player.pos, sizeof(L.pos));
    }
    L.flying = player.flying;
    for (i = 0; i < MAX_CARS; i++)
        if (cars[i].kind != CAR_FREE && cars[i].own) own = i;
    if (own >= 0) {
        memcpy(L.car_pos, cars[own].pos, sizeof(L.car_pos));
        L.car_yaw = cars[own].yaw;
    }
    world_save_all();
    f = fopen(LEVEL_FILE, "wb");
    if (!f) return;
    fwrite(&L, 1, sizeof(L), f);
    fclose(f);
    has_save = 1;
}

static void spawn_family_car(const float pos[3], float yaw)
{
    int i = car_spawn(CAR_PARKED, pos[0], pos[1], pos[2], yaw, FAMILY_CAR_COLOR);
    if (i >= 0) cars[i].own = 1;
    L.has_car = 1;
    memcpy(L.car_pos, pos, sizeof(L.car_pos));
    L.car_yaw = yaw;
}

static void spawn_chaser(void)
{
    float fx = -sinf(cam_yaw), fz = -cosf(cam_yaw);
    float x = player.pos[0] - fx * 26.0f, z = player.pos[2] - fz * 26.0f;
    int top = world_top((int)floorf(x), (int)floorf(z));
    car_spawn(CAR_CHASER, x, (float)(top >= 0 ? top + 1 : TOWN_Y + 1), z, cam_yaw, CHASER_COLOR);
}

static void begin_session(int next)
{
    cars_clear();
    car_idx = -1;
    memset(&player, 0, sizeof(player));
    memcpy(player.pos, L.pos, sizeof(player.pos));
    player.flying = L.flying;
    cam_yaw = body_yaw = L.yaw;
    if (L.has_car) spawn_family_car(L.car_pos, L.car_yaw);
    loading_next = next;
    mode = MODE_LOADING;
    mode_time = 0.0f;
    synth_music(-1);
}

static void new_game(void)
{
    Level keep = L;
    worker_drain();
    world_wipe(WORLD_DIR);
    remove(LEVEL_FILE);
    level_defaults();
    L.radius = keep.radius;
    L.look_speed = keep.look_speed;
    L.music_vol = keep.music_vol;
    L.sfx_vol = keep.sfx_vol;
    L.show_info = keep.show_info;
    L.day_cycle = keep.day_cycle;
    world_init(L.seed, WORLD_DIR);
    has_save = 0;
    begin_session(MODE_INTRO);
}

/* -------------------------------------------------------------------- sky */

static void compute_sky(SkyState *s, float time_of_day, float time_s, int underwater, int radius)
{
    float a = time_of_day * 2.0f * PI, sh = sinf(a);
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
    s->fog = 0xFF000000u;
    for (sh8 = 0; sh8 < 24; sh8 += 8) {
        float h = (float)((s->horizon >> sh8) & 0xFF), o = (float)((night_overlay >> sh8) & 0xFF);
        float f = (h - o * ov) / (1.0f - ov);
        s->fog |= (unsigned int)clampf(f, 0.0f, 255.0f) << sh8;
    }
    s->fog_end = radius * CH - 4.0f;
    s->fog_start = s->fog_end * 0.55f;
    if (underwater) {
        s->fog = mix(rgb(6, 14, 36), rgb(24, 64, 140), light);
        s->fog_start = 1.0f;
        s->fog_end = 22.0f;
    }
}

/* ------------------------------------------------------------------ radio */

static void radio_play(void)
{
    int t = STATION_TRACKS[L.radio][radio_track_i & 1];
    synth_music(t);
    if (t >= 0) snprintf(radio_buf, sizeof(radio_buf), "%s — «%s»", STATION_NAMES[L.radio], synth_track_name(t));
    else snprintf(radio_buf, sizeof(radio_buf), "%s", STATION_NAMES[L.radio]);
    radio_timer = 3.5f;
}

static void radio_tick(void)
{
    int a = STATION_TRACKS[L.radio][0], b = STATION_TRACKS[L.radio][1];
    if (a >= 0 && a != b && synth_music_loops() >= 2) {
        radio_track_i++;
        radio_play();
    }
}

/* ---------------------------------------------------------------- driving */

static void enter_car(int i)
{
    Car *c = &cars[i];
    if (c->kind == CAR_TRAFFIC) show_banner("Водій утік!", NULL);
    c->kind = CAR_PLAYER;
    car_idx = i;
    cam_yaw = c->yaw;
    player.flying = 0;
    synth_sfx(SFX_DOOR);
    radio_play();
}

static void exit_car(void)
{
    Car *c = &cars[car_idx];
    car_exit_point(c, player.pos);
    memset(player.vel, 0, sizeof(player.vel));
    c->kind = CAR_PARKED;
    car_idx = -1;
    L.yaw = c->yaw;
    synth_sfx(SFX_DOOR);
    synth_music(-1);
    synth_engine(0, 0.0f);
}

/* ------------------------------------------------------------------ blocks */

static void break_block(const int hit[3])
{
    int above = world_get(hit[0], hit[1] + 1, hit[2]);
    if (world_get(hit[0], hit[1], hit[2]) == B_BEDROCK) return;
    world_set(hit[0], hit[1], hit[2], B_AIR);
    if (block_shape(above) == SHAPE_CROSS || above == B_CACTUS) world_set(hit[0], hit[1] + 1, hit[2], B_AIR);
    synth_sfx(SFX_BREAK);
}

static void place_block(const int hit[3], const int prev[3])
{
    int b = L.hotbar[L.sel];
    const int *t = prev;
    int target, below;
    if (block_shape(world_get(hit[0], hit[1], hit[2])) == SHAPE_CROSS) t = hit;
    if (t[1] < 0 || t[1] >= WH) return;
    target = world_get(t[0], t[1], t[2]);
    if (target != B_AIR && target != B_WATER && block_shape(target) != SHAPE_CROSS) return;
    below = world_get(t[0], t[1] - 1, t[2]);
    if (block_shape(b) == SHAPE_CROSS && below != B_GRASS && below != B_DIRT && below != B_SNOW_GRASS) return;
    if (block_solid(b) && player_intersects_block(&player, t[0], t[1], t[2])) return;
    world_set(t[0], t[1], t[2], b);
    synth_sfx(SFX_PLACE);
}

/* --------------------------------------------------------------- dialogue */

static void queue_dialog(const StoryOut *o)
{
    int i;
    for (i = 0; i < o->lines && dlg_n < DLG_MAX; i++) {
        dlg_speaker[dlg_n] = o->speaker[i];
        dlg_text[dlg_n] = o->text[i];
        dlg_n++;
    }
    if (dlg_n > dlg_i && mode == MODE_PLAY) {
        mode = MODE_DIALOG;
        mode_time = 0.0f;
    }
}

static void run_story(float dt)
{
    StoryView v;
    StoryOut o;
    int ch = car_find_kind(CAR_CHASER);
    memset(&v, 0, sizeof(v));
    memcpy(v.pos, player.pos, sizeof(v.pos));
    v.in_car = car_idx >= 0;
    v.in_own_car = car_idx >= 0 && cars[car_idx].own;
    v.chaser_alive = ch >= 0;
    if (ch >= 0) {
        float dx = cars[ch].pos[0] - player.pos[0], dz = cars[ch].pos[2] - player.pos[2];
        v.chaser_dist = sqrtf(dx * dx + dz * dz);
    }
    story_update(&L.story, &v, dt, &o);
    if (o.spawn_own_car && car_find_kind(CAR_PARKED) < 0) spawn_family_car(POI_CAR, POI_CAR_YAW);
    else if (o.spawn_own_car) {
        int i, found = 0;
        for (i = 0; i < MAX_CARS; i++)
            if (cars[i].kind != CAR_FREE && cars[i].own) found = 1;
        if (!found) spawn_family_car(POI_CAR, POI_CAR_YAW);
    }
    if (o.spawn_chaser) spawn_chaser();
    if (o.remove_chaser && ch >= 0) cars[ch].kind = CAR_FREE;
    if (o.mission_passed) {
        synth_sfx(SFX_MISSION);
        show_banner("Місію виконано!", NULL);
    }
    if (o.mission_failed) {
        synth_sfx(SFX_FAIL);
        show_banner("Не встиг!", NULL);
    }
    queue_dialog(&o);
}

/* ------------------------------------------------------------------- main */

int main(void)
{
    SceCtrlData pad;
    SkyState sky;
    HudInfo hud;
    unsigned int old_buttons = 0, last_time;
    float action_timer = 0.0f, swing = 0.0f, bob = 0.0f, name_timer = 0.0f;
    float last_up = -1.0f, clock_s = 0.0f, autosave = 0.0f;
    float fps = 60.0f, fps_acc = 0.0f;
    int fps_frames = 0, menu_sel = 0, inv_cursor = 0, last_step = 0;

    setup_callbacks();
    sceCtrlSetSamplingCycle(0);
    sceCtrlSetSamplingMode(PSP_CTRL_MODE_ANALOG);
    scePowerSetClockFrequency(333, 333, 166);

    r_init();
    audio_start();
    build_offsets();
    start_worker();

    has_save = level_load();
    if (!has_save) {
        level_defaults();
        world_wipe(WORLD_DIR);
    }
    apply_volumes();
    world_init(L.seed, WORLD_DIR);
    cars_clear();
    synth_music(MUS_MENU);
    menu_sel = has_save ? MENU_CONTINUE : MENU_NEW;

    last_time = now_us();

    while (running) {
        unsigned int now = now_us(), pressed;
        float dt = (now - last_time) / 1000000.0f;
        float eye[3], view_yaw, view_pitch, dir[3], time_of_day = L.time;
        int hit[3] = { 0, 0, 0 }, prev[3], has_hit = 0, pcx, pcz, underwater = 0, world_visible;
        int focus_cx, focus_cz, stream_r = L.radius;

        last_time = now;
        if (dt > 0.05f) dt = 0.05f;
        clock_s += dt;
        mode_time += dt;
        fps_acc += dt;
        fps_frames++;
        if (fps_acc >= 0.5f) {
            fps = fps_frames / fps_acc;
            fps_acc = 0.0f;
            fps_frames = 0;
        }
        if (banner_t > 0.0f) banner_t -= dt;
        if (radio_timer > 0.0f) radio_timer -= dt;
        if (name_timer > 0.0f) name_timer -= dt;
        swing = swing > 0.0f ? swing - dt * 5.0f : 0.0f;

        sceCtrlReadBufferPositive(&pad, 1);
        pressed = pad.Buttons & ~old_buttons;
        old_buttons = pad.Buttons;

        /* -------------------------------------------------- menus & input */
        switch (mode) {
        case MODE_SPLASH:
            if ((pressed & (PSP_CTRL_CROSS | PSP_CTRL_START)) || mode_time > 7.0f) {
                mode = MODE_MENU;
                mode_time = 0.0f;
                synth_sfx(SFX_CONFIRM);
            }
            break;

        case MODE_MENU:
            if (pressed & PSP_CTRL_UP) {
                do menu_sel = (menu_sel + MENU_COUNT - 1) % MENU_COUNT;
                while (menu_sel == MENU_CONTINUE && !has_save);
                synth_sfx(SFX_UI);
            }
            if (pressed & PSP_CTRL_DOWN) {
                do menu_sel = (menu_sel + 1) % MENU_COUNT;
                while (menu_sel == MENU_CONTINUE && !has_save);
                synth_sfx(SFX_UI);
            }
            if (pressed & PSP_CTRL_CROSS) {
                synth_sfx(SFX_CONFIRM);
                switch (menu_sel) {
                case MENU_CONTINUE: begin_session(MODE_PLAY); break;
                case MENU_NEW:
                    if (has_save) { mode = MODE_CONFIRM; mode_time = 0.0f; }
                    else new_game();
                    break;
                case MENU_SETTINGS: settings_return = MODE_MENU; mode = MODE_SETTINGS; menu_sel = 0; break;
                default: running = 0; break;
                }
            }
            break;

        case MODE_CONFIRM:
            if (pressed & PSP_CTRL_CROSS) new_game();
            if (pressed & PSP_CTRL_CIRCLE) { mode = MODE_MENU; menu_sel = MENU_NEW; }
            break;

        case MODE_SETTINGS:
            if (pressed & PSP_CTRL_UP) { menu_sel = (menu_sel + SET_COUNT - 1) % SET_COUNT; synth_sfx(SFX_UI); }
            if (pressed & PSP_CTRL_DOWN) { menu_sel = (menu_sel + 1) % SET_COUNT; synth_sfx(SFX_UI); }
            if (pressed & (PSP_CTRL_LEFT | PSP_CTRL_RIGHT | PSP_CTRL_CROSS)) {
                int d = (pressed & PSP_CTRL_LEFT) ? -1 : 1;
                synth_sfx(SFX_UI);
                switch (menu_sel) {
                case SET_RADIUS: L.radius = L.radius + d < 2 ? MAX_RADIUS : (L.radius + d > MAX_RADIUS ? 2 : L.radius + d); break;
                case SET_LOOK:   L.look_speed = L.look_speed + d < 1 ? 5 : (L.look_speed + d > 5 ? 1 : L.look_speed + d); break;
                case SET_MUSIC:  L.music_vol = (int)clampf((float)(L.music_vol + d), 0, 10); apply_volumes(); break;
                case SET_SFX:    L.sfx_vol = (int)clampf((float)(L.sfx_vol + d), 0, 10); apply_volumes(); break;
                case SET_CYCLE:  L.day_cycle = !L.day_cycle; break;
                case SET_TIME:   L.time = (L.time > 0.02f && L.time < 0.48f) ? 0.55f : 0.08f; break;
                case SET_INFO:   L.show_info = !L.show_info; break;
                default:
                    if (pressed & PSP_CTRL_CROSS) { mode = settings_return; menu_sel = 0; }
                    break;
                }
            }
            if (pressed & PSP_CTRL_CIRCLE) { mode = settings_return; menu_sel = 0; }
            break;

        case MODE_LOADING:
            break;

        case MODE_INTRO: {
            int slide = (int)L.story.step;
            if (pressed & PSP_CTRL_CROSS) {
                synth_sfx(SFX_UI);
                if (slide < STORY_INTRO_SLIDES && mode_time * 35.0f < (float)strlen(STORY_INTRO[slide])) mode_time = 100.0f;
                else {
                    L.story.step = slide + 1;
                    mode_time = 0.0f;
                    if (L.story.step >= STORY_INTRO_SLIDES) {
                        L.story.step = 0;
                        mode = MODE_PLAY;
                        in_game = 1;
                        synth_music(-1);
                        show_banner(story_title(&L.story), "Розділ 1");
                        level_save();
                    }
                }
            }
            break;
        }

        case MODE_PAUSE:
            if (pressed & (PSP_CTRL_START | PSP_CTRL_CIRCLE)) mode = MODE_PLAY;
            if (pressed & PSP_CTRL_UP) { menu_sel = (menu_sel + PAUSE_COUNT - 1) % PAUSE_COUNT; synth_sfx(SFX_UI); }
            if (pressed & PSP_CTRL_DOWN) { menu_sel = (menu_sel + 1) % PAUSE_COUNT; synth_sfx(SFX_UI); }
            if (pressed & PSP_CTRL_CROSS) {
                synth_sfx(SFX_CONFIRM);
                switch (menu_sel) {
                case PAUSE_RESUME: mode = MODE_PLAY; break;
                case PAUSE_BLOCKS: mode = MODE_INVENTORY; break;
                case PAUSE_SETTINGS: settings_return = MODE_PAUSE; mode = MODE_SETTINGS; menu_sel = 0; break;
                default:
                    level_save();
                    in_game = 0;
                    synth_engine(0, 0.0f);
                    synth_music(MUS_MENU);
                    mode = MODE_MENU;
                    menu_sel = MENU_CONTINUE;
                    break;
                }
            }
            break;

        case MODE_INVENTORY: {
            int n = B_COUNT - 1;
            if (pressed & (PSP_CTRL_SELECT | PSP_CTRL_CIRCLE | PSP_CTRL_START)) mode = MODE_PLAY;
            if (pressed & PSP_CTRL_LEFT) inv_cursor = (inv_cursor + n - 1) % n;
            if (pressed & PSP_CTRL_RIGHT) inv_cursor = (inv_cursor + 1) % n;
            if ((pressed & PSP_CTRL_UP) && inv_cursor >= INV_COLS) inv_cursor -= INV_COLS;
            if ((pressed & PSP_CTRL_DOWN) && inv_cursor + INV_COLS < n) inv_cursor += INV_COLS;
            if (pressed & PSP_CTRL_LTRIGGER) L.sel = (L.sel + HOTBAR_N - 1) % HOTBAR_N;
            if (pressed & PSP_CTRL_RTRIGGER) L.sel = (L.sel + 1) % HOTBAR_N;
            if (pressed & PSP_CTRL_CROSS) {
                L.hotbar[L.sel] = inv_cursor + 1;
                name_timer = 2.0f;
                synth_sfx(SFX_UI);
            }
            break;
        }

        case MODE_DIALOG:
            if (pressed & PSP_CTRL_CROSS) {
                synth_sfx(SFX_UI);
                if (mode_time * 45.0f < (float)strlen(dlg_text[dlg_i])) mode_time = 100.0f;
                else if (++dlg_i >= dlg_n) {
                    dlg_i = dlg_n = 0;
                    mode = MODE_PLAY;
                } else {
                    mode_time = 0.0f;
                }
            }
            break;

        case MODE_PLAY: {
            float ls = 1.2f + 0.6f * L.look_speed;
            if (pressed & PSP_CTRL_START) { mode = MODE_PAUSE; menu_sel = 0; synth_sfx(SFX_UI); break; }

            if (car_idx >= 0) {
                /* ---------------- driving ---------------- */
                Car *c = &cars[car_idx];
                float ax = analog(pad.Lx), ay = -analog(pad.Ly), throttle = ay, steer = -ax;
                int ev;
                if (pad.Buttons & PSP_CTRL_CROSS) throttle = 1.0f;
                if (pad.Buttons & PSP_CTRL_SQUARE) throttle = -1.0f;
                if (pad.Buttons & PSP_CTRL_LTRIGGER) steer = 1.0f;
                if (pad.Buttons & PSP_CTRL_RTRIGGER) steer = -1.0f;
                ev = car_physics(c, throttle, steer, dt);
                if (ev & CAR_EV_CRASH) synth_sfx(SFX_CRASH);
                if (pressed & PSP_CTRL_UP) synth_sfx(SFX_HORN);
                if (pressed & PSP_CTRL_RIGHT) { L.radio = (L.radio + 1) % RADIO_STATIONS; radio_track_i = 0; radio_play(); }
                if (pressed & PSP_CTRL_LEFT) { L.radio = (L.radio + RADIO_STATIONS - 1) % RADIO_STATIONS; radio_track_i = 0; radio_play(); }
                if (pressed & PSP_CTRL_SELECT) L.third_person = !L.third_person;
                synth_engine(1, fabsf(c->speed) / CAR_MAX_SPEED);
                radio_tick();
                memcpy(player.pos, c->pos, sizeof(player.pos));
                cam_yaw = wrapf(cam_yaw + wrapf(c->yaw - cam_yaw) * clampf(dt * 3.0f, 0.0f, 1.0f));
                if (pad.Buttons & PSP_CTRL_TRIANGLE) cam_yaw = wrapf(c->yaw + PI); /* look back */
                if ((pressed & PSP_CTRL_DOWN) && fabsf(c->speed) < 4.0f) exit_car();
            } else {
                /* ---------------- on foot ---------------- */
                float fx, fz, ax, ay, speed, mx, mz;
                int up;
                if (pad.Buttons & PSP_CTRL_SQUARE)   L.yaw += ls * dt;
                if (pad.Buttons & PSP_CTRL_CIRCLE)   L.yaw -= ls * dt;
                if (pad.Buttons & PSP_CTRL_TRIANGLE) L.pitch += ls * 0.75f * dt;
                if (pad.Buttons & PSP_CTRL_CROSS)    L.pitch -= ls * 0.75f * dt;
                L.pitch = clampf(L.pitch, -1.55f, 1.55f);
                L.yaw = wrapf(L.yaw);

                if (pressed & PSP_CTRL_RIGHT) { L.sel = (L.sel + 1) % HOTBAR_N; name_timer = 2.0f; }
                if (pressed & PSP_CTRL_LEFT)  { L.sel = (L.sel + HOTBAR_N - 1) % HOTBAR_N; name_timer = 2.0f; }
                if (pressed & PSP_CTRL_SELECT) L.third_person = !L.third_person;

                if (pressed & PSP_CTRL_UP) {
                    if (clock_s - last_up < 0.3f) {
                        player.flying = !player.flying;
                        player.vel[1] = 0.0f;
                        last_up = -1.0f;
                    } else {
                        last_up = clock_s;
                    }
                }
                if (pressed & PSP_CTRL_DOWN) {
                    int ci = car_nearest(player.pos, 3.4f);
                    if (ci >= 0) {
                        enter_car(ci);
                        break;
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
                {
                    int i;
                    for (i = 0; i < MAX_CARS; i++)
                        if (cars[i].kind != CAR_FREE && fabsf(cars[i].pos[1] - player.pos[1]) < 1.8f)
                            car_push_out(&cars[i], &player.pos[0], &player.pos[2], PLAYER_HALF_W);
                }
                if ((ax != 0.0f || ay != 0.0f)) {
                    body_yaw = atan2f(-mx, -mz);
                    if (player.on_ground) {
                        bob += dt * 9.0f;
                        if ((int)(bob / PI) != last_step) {
                            last_step = (int)(bob / PI);
                            synth_sfx(SFX_STEP);
                        }
                    }
                }
                cam_yaw = L.yaw;
            }
            break;
        }
        }

        /* -------------------------------------------------------- update */
        in_game = mode == MODE_PLAY || mode == MODE_PAUSE || mode == MODE_INVENTORY || mode == MODE_DIALOG ||
                  mode == MODE_INTRO || (mode == MODE_SETTINGS && settings_return == MODE_PAUSE);
        if (mode == MODE_PLAY) {
            int ev;
            if (L.day_cycle) {
                L.time += dt / DAY_LENGTH;
                if (L.time >= 1.0f) L.time -= 1.0f;
            }
            ev = cars_update(dt, player.pos, car_idx >= 0 ? NULL : player.pos, 1);
            if (ev & CAR_EV_HONK) synth_sfx(SFX_HORN);
            if (ev & CAR_EV_CRASH) synth_sfx(SFX_CRASH);
            run_story(dt);
            autosave += dt;
            if (autosave > AUTOSAVE_EVERY) {
                autosave = 0.0f;
                level_save();
            }
            if (car_idx < 0) synth_engine(0, 0.0f);
        }

        /* --------------------------------------------------------- camera */
        world_visible = mode != MODE_SPLASH && mode != MODE_LOADING;
        if (in_game) {
            float head[3], dist = 0.0f;
            if (car_idx >= 0) {
                const Car *c = &cars[car_idx];
                head[0] = c->pos[0]; head[1] = c->pos[1] + 1.8f; head[2] = c->pos[2];
                view_yaw = cam_yaw;
                view_pitch = -0.2f;
                dist = L.third_person ? 10.0f : 7.0f;
            } else {
                head[0] = player.pos[0]; head[1] = player.pos[1] + PLAYER_EYE; head[2] = player.pos[2];
                view_yaw = L.yaw;
                view_pitch = L.pitch;
                dist = L.third_person ? 4.0f : 0.0f;
            }
            dir[0] = -sinf(view_yaw) * cosf(view_pitch);
            dir[1] = sinf(view_pitch);
            dir[2] = -cosf(view_yaw) * cosf(view_pitch);
            eye[0] = head[0]; eye[1] = head[1]; eye[2] = head[2];
            if (dist > 0.0f) {
                float d;
                for (d = 0.3f; d <= dist; d += 0.25f) {
                    float px = head[0] - dir[0] * d, py = head[1] - dir[1] * d + 0.3f, pz = head[2] - dir[2] * d;
                    if (block_solid(world_get((int)floorf(px), (int)floorf(py), (int)floorf(pz)))) break;
                    eye[0] = px; eye[1] = py; eye[2] = pz;
                }
            }
            if (car_idx < 0 && mode == MODE_PLAY) {
                if (dist > 0.0f) {
                    float back = sqrtf((eye[0] - head[0]) * (eye[0] - head[0]) + (eye[2] - head[2]) * (eye[2] - head[2]));
                    has_hit = world_raycast(eye, dir, REACH + back, hit, prev);
                } else {
                    has_hit = world_raycast(eye, dir, REACH, hit, prev);
                }
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
            focus_cx = (int)floorf(player.pos[0]) >> 4;
            focus_cz = (int)floorf(player.pos[2]) >> 4;
        } else {
            /* menu panorama: slow orbit over the town at sunset */
            float a = clock_s * 0.04f, dx, dz;
            eye[0] = cosf(a) * 70.0f;
            eye[1] = TOWN_Y + 26.0f;
            eye[2] = sinf(a) * 70.0f;
            dx = -eye[0];
            dz = -eye[2];
            view_yaw = atan2f(-dx, -dz) + 0.35f;
            view_pitch = -0.28f;
            time_of_day = 0.46f;
            focus_cx = 0;
            focus_cz = 0;
            stream_r = L.radius < 4 ? L.radius : 4;
            if (mode == MODE_LOADING) {
                focus_cx = (int)floorf(player.pos[0]) >> 4;
                focus_cz = (int)floorf(player.pos[2]) >> 4;
                stream_r = L.radius;
            }
        }

        stream_chunks(focus_cx, focus_cz, stream_r);
        pcx = focus_cx;
        pcz = focus_cz;

        if (mode == MODE_LOADING) {
            int total, r = L.radius < 3 ? L.radius : 3;
            int done = area_progress(focus_cx, focus_cz, r, &total);
            if (done >= total) {
                if (loading_next == MODE_PLAY && !has_save) loading_next = MODE_INTRO;
                if (loading_next == MODE_INTRO) {
                    int top = world_top((int)floorf(player.pos[0]), (int)floorf(player.pos[2]));
                    if (top >= 0) player.pos[1] = (float)(top + 1);
                    L.story.step = 0;
                    synth_music(MUS_MENU);
                } else {
                    in_game = 1;
                    if (L.story.mission == M_TAIL && car_find_kind(CAR_CHASER) < 0) spawn_chaser();
                    show_banner(story_title(&L.story), NULL);
                }
                mode = loading_next;
                mode_time = 0.0f;
            }
            r_begin_frame(0xFF000000u);
            hud_loading(done, total, "Завантаження Соняшниківська...");
            r_end_frame();
            continue;
        }

        if (in_game)
            underwater = world_get((int)floorf(eye[0]), (int)floorf(eye[1]), (int)floorf(eye[2])) == B_WATER;
        compute_sky(&sky, time_of_day, clock_s, underwater, in_game ? L.radius : stream_r);

        /* -------------------------------------------------------- render */
        r_begin_frame(underwater ? sky.fog : sky.zenith);
        if (world_visible) {
            int i, lights = sky.night > 0.35f;
            float fade = sky.fog_end + 8.0f;
            r_sky(&sky, view_yaw, view_pitch);
            r_camera(eye, view_yaw, view_pitch);
            r_world(eye, view_yaw, view_pitch, pcx, pcz, in_game ? L.radius : stream_r, &sky);
            for (i = 0; i < MAX_CARS; i++) {
                const Car *c = &cars[i];
                float dx, dz;
                if (c->kind == CAR_FREE) continue;
                dx = c->pos[0] - eye[0];
                dz = c->pos[2] - eye[2];
                if (dx * dx + dz * dz > fade * fade) continue;
                r_car(c->pos, c->yaw, c->color, lights, &sky);
            }
            if (in_game && car_idx < 0 && L.third_person)
                r_player_model(player.pos, body_yaw, bob, 1.0f, &sky);
            if (has_hit && mode == MODE_PLAY) r_outline(hit);
            r_night_overlay(&sky);
            r_camera(eye, view_yaw, view_pitch);
            r_clouds(eye, &sky);
            if (in_game) {
                float mp[3];
                if (story_marker(&L.story, mp)) r_marker(mp, clock_s);
                if (mode == MODE_PLAY && car_idx < 0 && !L.third_person)
                    r_held_block(L.hotbar[L.sel], swing, bob, 1.0f - sky.night * 0.65f);
            }
        }

        switch (mode) {
        case MODE_SPLASH:    hud_splash(mode_time); break;
        case MODE_MENU:      hud_main_menu(menu_sel, has_save, clock_s); break;
        case MODE_CONFIRM:
            hud_main_menu(MENU_NEW, has_save, clock_s);
            hud_confirm("Почати нову гру? Поточне збереження буде втрачено.", "✕ — так, нова гра", "○ — ні");
            break;
        case MODE_SETTINGS:  hud_settings(menu_sel, &L); break;
        case MODE_INTRO:     hud_intro(L.story.step < STORY_INTRO_SLIDES ? L.story.step : 0, mode_time); break;
        case MODE_PAUSE:     hud_pause(menu_sel); break;
        case MODE_INVENTORY: hud_inventory(inv_cursor, &L); break;
        case MODE_DIALOG:
            if (dlg_i < dlg_n) hud_dialog(dlg_speaker[dlg_i], dlg_text[dlg_i], mode_time);
            break;
        default: {
            float mp[3];
            int bio;
            memset(&hud, 0, sizeof(hud));
            hud.fps = fps;
            hud.has_hit = has_hit;
            hud.hit_block = has_hit ? world_get(hit[0], hit[1], hit[2]) : 0;
            if (hud.hit_block >= B_COUNT) hud.hit_block = 0;
            hud.in_car = car_idx >= 0;
            hud.crosshair = car_idx < 0;
            hud.hotbar = car_idx < 0;
            hud.near_car = car_idx < 0 && car_nearest(player.pos, 3.4f) >= 0;
            hud.can_exit = car_idx >= 0;
            hud.speed_kmh = car_idx >= 0 ? fabsf(cars[car_idx].speed) * 3.6f : 0.0f;
            hud.radio_text = radio_buf;
            hud.radio_timer = radio_timer;
            hud.banner = banner;
            hud.banner_sub = banner_sub;
            hud.banner_timer = banner_t;
            hud.name_timer = name_timer;
            hud.underwater = underwater;
            hud.px = (int)floorf(player.pos[0]);
            hud.py = (int)floorf(player.pos[1]);
            hud.pz = (int)floorf(player.pos[2]);
            gen_column(L.seed, hud.px, hud.pz, &bio);
            hud.biome = biome_name(bio);
            if (story_marker(&L.story, mp)) {
                float dx = mp[0] - player.pos[0], dz = mp[2] - player.pos[2];
                hud.marker = 1;
                hud.marker_dist = sqrtf(dx * dx + dz * dz);
                hud.marker_angle = -wrapf(atan2f(-dx, -dz) - view_yaw);
            }
            hud_play(&L, &hud);
            break;
        }
        }
        r_end_frame();
    }

    worker_drain();
    if (in_game) level_save();
    world_shutdown();
    audio_stop();
    sceKernelExitGame();
    return 0;
}
