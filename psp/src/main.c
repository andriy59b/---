/* UkraineCraft — an original open-world game for the PSP set in 1996.
 * Main loop: modes, input, third-person camera, getting in and out of cars,
 * story and cutscenes, radio, saving. */
#include <pspkernel.h>
#include <pspctrl.h>
#include <psppower.h>

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "actor.h"
#include "audio.h"
#include "car.h"
#include "city.h"
#include "cutscene.h"
#include "game.h"
#include "render.h"
#include "ride.h"
#include "story.h"
#include "synth.h"

PSP_MODULE_INFO("UkraineCraft", 0, 1, 2);
PSP_MAIN_THREAD_ATTR(THREAD_ATTR_USER | THREAD_ATTR_VFPU);
PSP_HEAP_SIZE_KB(-1024);

#define DAY_LENGTH 900.0f
#define AUTOSAVE_EVERY 120.0f
#define FAMILY_CAR_PAINT 0xFF3A2A9Au
#define KABAN_CAR_PAINT 0xFF181818u
#define FOV 62.0f
#define MAX_EXTRAS 4

enum { MODE_SPLASH, MODE_MENU, MODE_CONFIRM, MODE_SETTINGS, MODE_PLAY, MODE_PAUSE, MODE_CUTSCENE };

static const char LEVEL_MAGIC[8] = { 'U', 'K', 'R', 'C', 'R', '0', '0', '2' };

static Level L;
static Ped peds[N_ACTORS];
static Ped extras[MAX_EXTRAS];
static float extra_t[MAX_EXTRAS];
static Ped *player = &peds[A_ANDRIY];
static volatile int running = 1;

static int mode = MODE_SPLASH, settings_return = MODE_MENU, has_save, in_game;
static float mode_time;
static Ride ride;
#define car_idx (ride.state != RIDE_FOOT ? ride.car : -1)
#define P_FOOT RIDE_FOOT
#define P_DRIVE RIDE_DRIVE
#define pstate ride.state
static float cam_yaw, cam_pitch = -0.12f;
static Cutscene cs;
static float letterbox;

static const char *banner, *banner_sub;
static float banner_t;
static char banner_buf[160];

static const char *const STATION_NAMES[RADIO_STATIONS] = {
    "Радіо Соняшник FM 103.5", "Шансон-хвиля 88.2", "Радіо вимкнено",
};
static const int STATION_TRACKS[RADIO_STATIONS][2] = {
    { MUS_DISCO, MUS_ROCK }, { MUS_WALTZ, MUS_WALTZ }, { -1, -1 },
};
static int radio_track_i;
static float radio_timer;
static char radio_buf[96];

static GMesh dyn;

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

static void show_banner(const char *text, const char *sub, float t)
{
    snprintf(banner_buf, sizeof(banner_buf), "%s", text);
    banner = banner_buf;
    banner_sub = sub;
    banner_t = t;
}

static void apply_volumes(void)
{
    synth_volumes(L.music_vol / 10.0f, L.sfx_vol / 10.0f);
}

static CsIO cs_io(void)
{
    CsIO io;
    int i;
    memset(&io, 0, sizeof(io));
    for (i = 0; i < N_ACTORS; i++) io.actors[i] = &peds[i];
    return io;
}

/* ------------------------------------------------------------------- radio */

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

/* ------------------------------------------------------------------- level */

static void level_defaults(void)
{
    memset(&L, 0, sizeof(L));
    memcpy(L.magic, LEVEL_MAGIC, sizeof(LEVEL_MAGIC));
    L.pos = POI_SPAWN;
    L.yaw = POI_SPAWN_YAW;
    L.cam_yaw = POI_SPAWN_YAW;
    L.time = 0.1f;
    L.radius = 3;
    L.look_speed = 3;
    L.day_cycle = 1;
    L.music_vol = 7;
    L.sfx_vol = 9;
    L.has_car = 1;
    L.car_pos = POI_CAR;
    L.car_yaw = POI_CAR_YAW;
    story_start(&L.story);
}

static int level_load(void)
{
    FILE *f = fopen(LEVEL_FILE, "rb");
    int ok;
    if (!f) return 0;
    ok = fread(&L, 1, sizeof(L), f) == sizeof(L) && memcmp(L.magic, LEVEL_MAGIC, sizeof(LEVEL_MAGIC)) == 0;
    fclose(f);
    if (ok) {
        L.radius = L.radius < 2 || L.radius > 6 ? 4 : L.radius;
        L.look_speed = L.look_speed < 1 || L.look_speed > 5 ? 3 : L.look_speed;
        L.radio = L.radio < 0 || L.radio >= RADIO_STATIONS ? 0 : L.radio;
    }
    return ok;
}

static void level_save(void)
{
    FILE *f;
    int i;
    L.pos = player->pos;
    L.yaw = player->yaw;
    L.cam_yaw = cam_yaw;
    if (car_idx >= 0) L.pos = car_door_point(&cars[car_idx], 0);
    for (i = 0; i < MAX_CARS; i++)
        if (cars[i].kind != CK_FREE && cars[i].own) {
            L.car_pos = cars[i].pos;
            L.car_yaw = cars[i].yaw;
        }
    mkdir(SAVE_DIR, 0777);
    f = fopen(LEVEL_FILE, "wb");
    if (!f) return;
    fwrite(&L, 1, sizeof(L), f);
    fclose(f);
    has_save = 1;
}

/* ---------------------------------------------------------- world set-up */

static void kaban_setup(void)
{
    V3 k = POI_KABAN_NPC;
    int i;
    ped_init(&peds[A_KABAN], CH_KABAN, k, 0.0f);
    ped_init(&peds[A_THUG1], CH_THUG, v3(k.x - 2.3f, k.y, k.z - 0.9f), 0.2f);
    ped_init(&peds[A_THUG2], CH_THUG, v3(k.x + 2.4f, k.y, k.z - 1.2f), -0.2f);
    peds[A_KABAN].anim.gesture = GEST_HIPS;
    peds[A_THUG1].anim.gesture = GEST_CROSSED;
    peds[A_THUG2].anim.gesture = GEST_CROSSED;
    for (i = 0; i < MAX_CARS; i++)
        if (cars[i].kind == CK_SCRIPT && cars[i].type == CAR_GELENDER) return;
    car_spawn(CK_SCRIPT, CAR_GELENDER, v3(-19.5f, WALK_H, -85.0f), PI_F / 2, KABAN_CAR_PAINT);
    L.kaban_setup = 1;
}

static void begin_session(void)
{
    int i;
    cars_clear();
    memset(peds, 0, sizeof(peds));
    memset(extras, 0, sizeof(extras));
    ped_init(player, CH_ANDRIY, L.pos, L.yaw);
    ped_init(&peds[A_TARAS], CH_TARAS, POI_TARAS, 0.0f);
    ped_init(&peds[A_GALYA], CH_GALYA, POI_GALYA, 0.0f);
    ped_init(&peds[A_SELLER], CH_SELLER, POI_SELLER, 0.3f);
    for (i = A_KABAN; i <= A_THUG2; i++) peds[i].active = 0;
    if (L.kaban_setup) kaban_setup();
    if (L.has_car) {
        int k = car_spawn(CK_PARKED, CAR_LASTIVKA, L.car_pos, L.car_yaw, FAMILY_CAR_PAINT);
        if (k >= 0) cars[k].own = 1;
    }
    if (L.story.mission == M_TAIL)
        car_spawn(CK_CHASER, CAR_GELENDER, v3(player->pos.x + 30, 0, player->pos.z), 0, KABAN_CAR_PAINT);
    ride_reset(&ride);
    cam_yaw = L.cam_yaw;
    cam_pitch = -0.12f;
    apply_volumes();
    synth_music(-1);
    in_game = 1;
    mode = MODE_PLAY;
    mode_time = 0.0f;
}

/* -------------------------------------------------------------------- sky */

static void compute_sky(SkyState *s, float time_of_day, float time_s, float radius)
{
    float a = time_of_day * 2.0f * PI_F, sh = sinf(a);
    float light = clampf(0.5f + sh * 2.5f, 0.0f, 1.0f);
    float glow = clampf(1.0f - fabsf(sh) * 4.0f, 0.0f, 1.0f);
    float ov = (1.0f - light) * 0.7f;
    unsigned int night_overlay = rgb(8, 10, 30);
    int sh8;
    s->sun_angle = a;
    s->time_s = time_s;
    s->night = 1.0f - light;
    s->zenith = mix(rgb(4, 6, 20), rgb(70, 132, 226), light);
    s->horizon = mix(mix(rgb(16, 20, 44), rgb(186, 214, 244), light), rgb(244, 150, 90), glow * 0.55f);
    s->cloud = mix(mix(rgb(46, 50, 68), rgb(255, 255, 255), light), rgb(255, 196, 160), glow * 0.45f);
    s->fog = 0xFF000000u;
    for (sh8 = 0; sh8 < 24; sh8 += 8) {
        float h = (float)((s->horizon >> sh8) & 0xFF), o = (float)((night_overlay >> sh8) & 0xFF);
        float f = (h - o * ov) / (1.0f - ov);
        s->fog |= (unsigned int)clampf(f, 0.0f, 255.0f) << sh8;
    }
    s->fog_end = radius;
    s->fog_start = radius * 0.45f;
}

/* ------------------------------------------------------------------ cars */

static void flee_driver(V3 at, float away_yaw)
{
    int i;
    for (i = 0; i < MAX_EXTRAS; i++)
        if (!extras[i].active) {
            V3 f = yaw_fwd(away_yaw);
            ped_init(&extras[i], (i & 1) ? CH_SELLER : CH_THUG, at, away_yaw);
            extras[i].goal = v3(at.x + f.x * 25.0f, at.y, at.z + f.z * 25.0f);
            extras[i].has_goal = 1;
            extras[i].walk_speed = 5.5f;
            extra_t[i] = 7.0f;
            return;
        }
}

static void player_drive_tick(float dt, unsigned int buttons, unsigned int pressed, const SceCtrlData *pad)
{
    Car *c = &cars[car_idx];
    float ax = analog(pad->Lx), ay = -analog(pad->Ly), throttle = ay;
    if (buttons & PSP_CTRL_CROSS) throttle = 1.0f;
    if (buttons & PSP_CTRL_SQUARE) throttle = -1.0f;
    c->in_throttle = throttle;
    c->in_steer = -ax;
    c->in_handbrake = (buttons & PSP_CTRL_CIRCLE) != 0;
    car_step(c, dt);
    if (c->impact > 5.0f) synth_sfx(SFX_CRASH);
    if (pressed & PSP_CTRL_UP) synth_sfx(SFX_HORN);
    if (pressed & PSP_CTRL_RIGHT) { L.radio = (L.radio + 1) % RADIO_STATIONS; radio_track_i = 0; radio_play(); }
    if (pressed & PSP_CTRL_LEFT) { L.radio = (L.radio + RADIO_STATIONS - 1) % RADIO_STATIONS; radio_track_i = 0; radio_play(); }
    synth_engine(1, clampf((c->rpm - 800.0f) / 5400.0f, 0.0f, 1.0f));
    radio_tick();
    player->pos = car_seat_point(c);
    player->yaw = c->yaw;
    if (pressed & PSP_CTRL_TRIANGLE) {
        c->in_throttle = 0.0f;
        c->in_handbrake = 1;
        if (ride_exit(&ride, player)) {
            synth_engine(0, 0.0f);
            synth_music(-1);
        }
    }
}

static void force_exit_car(void)
{
    ride_force_out(&ride, player);
    synth_engine(0, 0.0f);
    synth_music(-1);
}

static void ride_tick(float dt)
{
    int ev = ride_update(&ride, player, dt);
    if (ev & RIDE_EV_DOOR) synth_sfx(SFX_DOOR);
    if (ev & RIDE_EV_FLEE) {
        flee_driver(ride.flee_pos, ride.flee_yaw);
        show_banner("Водій утік!", NULL, 2.0f);
    }
    if (ev & RIDE_EV_ENTERED) radio_play();
    if (ev & RIDE_EV_GAVE_UP) show_banner("Не наздогнав.", NULL, 1.5f);
}

/* ----------------------------------------------------------------- story */

static void apply_story_out(const StoryOut *o)
{
    int ch = car_find_kind(CK_CHASER);
    if (o->remove_chaser && ch >= 0) cars[ch].kind = CK_FREE;
    if (o->kaban_setup) kaban_setup();
    if (o->mission_passed && !o->pager) {
        synth_sfx(SFX_MISSION);
        show_banner("Місію виконано!", story_title(&L.story), 3.5f);
    }
    if (o->mission_failed) synth_sfx(SFX_FAIL);
    if (o->pager) {
        synth_sfx(SFX_CONFIRM);
        show_banner(o->pager, NULL, 6.0f);
    }
    if (o->scene) {
        if (car_idx >= 0 && o->scene != CS_FAIL) force_exit_car();
        cs_start(&cs, o->scene);
        mode = MODE_CUTSCENE;
    }
}

static void finish_cutscene(void)
{
    StoryOut o;
    int ch = car_find_kind(CK_SCRIPT);
    int id = cs.id;
    (void)ch;
    if (id == CS_KIOSK) {
        int i;
        for (i = 0; i < MAX_CARS; i++)
            if (cars[i].kind == CK_SCRIPT && cars[i].type == CAR_GELENDER && !L.kaban_setup) cars[i].kind = CK_CHASER;
    }
    story_scene_done(&L.story, id, &o);
    apply_story_out(&o);
    mode = MODE_PLAY;
    level_save();
}

static void handle_requests(int req)
{
    if (req & CS_REQ_CHASER)
        car_spawn(CK_SCRIPT, CAR_GELENDER, v3(186.0f, 0.0f, -122.5f), PI_F / 2, KABAN_CAR_PAINT);
    if (req & CS_REQ_KABAN_SETUP) kaban_setup();
}

/* ---------------------------------------------------------------- render */

static void build_dynamic(V3 eye, float yaw, float radius)
{
    V3 f = yaw_fwd(yaw);
    int i;
    dyn.n = 0;
    for (i = 0; i < MAX_CARS; i++) {
        const Car *c = &cars[i];
        V3 d;
        if (c->kind == CK_FREE) continue;
        d = v3_sub(c->pos, eye);
        if (d.x * d.x + d.z * d.z > radius * radius) continue;
        if (d.x * f.x + d.z * f.z < -8.0f) continue;
        car_append_mesh(&dyn, c);
    }
    for (i = 0; i < N_ACTORS; i++) {
        const Ped *p = &peds[i];
        V3 d;
        if (!p->active) continue;
        if (p == player && L.first_person && pstate == P_FOOT && mode != MODE_CUTSCENE) continue;
        d = v3_sub(p->pos, eye);
        if (d.x * d.x + d.z * d.z > 90.0f * 90.0f) continue;
        ped_append_mesh(&dyn, p);
    }
    for (i = 0; i < MAX_EXTRAS; i++)
        if (extras[i].active) ped_append_mesh(&dyn, &extras[i]);
}

static void draw_world(V3 eye, float yaw, float pitch, const SkyState *sky, float radius, int markers)
{
    int i;
    r_sky(sky, yaw, pitch, FOV);
    r_camera(eye, yaw, pitch, FOV);
    r_set_billboard_basis(yaw_right(yaw), v3(0, 1, 0));
    r_city(eye, yaw, pitch, radius, sky);
    for (i = 0; i < MAX_CARS; i++)
        if (cars[i].kind != CK_FREE)
            r_shadow(cars[i].pos, CAR_SPECS[cars[i].type].width * 0.62f, CAR_SPECS[cars[i].type].length * 0.58f,
                     cars[i].yaw, 0.85f);
    for (i = 0; i < N_ACTORS; i++)
        if (peds[i].active)
            r_shadow(peds[i].pos, 0.42f, 0.42f, 0.0f, 0.7f);
    build_dynamic(eye, yaw, radius);
    r_mesh(&dyn, sky);
    r_night_overlay(sky);
    r_glow(eye, radius, sky->night);
    if (sky->night > 0.3f)
        for (i = 0; i < MAX_CARS; i++) {
            const Car *c = &cars[i];
            V3 d;
            if (c->kind == CK_FREE || c->kind == CK_PARKED) continue;
            d = v3_sub(c->pos, eye);
            if (d.x * d.x + d.z * d.z > 120.0f * 120.0f) continue;
            r_flare(car_light_pos(c, 0), 0.9f, 0xE0B0F0FFu);
            r_flare(car_light_pos(c, 1), 0.9f, 0xE0B0F0FFu);
            r_flare(car_light_pos(c, 2), 0.45f, 0xC02020FFu);
            r_flare(car_light_pos(c, 3), 0.45f, 0xC02020FFu);
        }
    r_clouds(eye, sky);
    if (markers) {
        V3 m;
        if (story_marker(&L.story, &m)) r_marker(m, sky->time_s);
    }
}

/* ------------------------------------------------------------------- main */

int main(void)
{
    SceCtrlData pad;
    SkyState sky;
    HudInfo hud;
    unsigned int old_buttons = 0, last_time;
    float clock_s = 0.0f, autosave = 0.0f, fps = 60.0f, fps_acc = 0.0f;
    int fps_frames = 0, menu_sel = 0;

    setup_callbacks();
    sceCtrlSetSamplingCycle(0);
    sceCtrlSetSamplingMode(PSP_CTRL_MODE_ANALOG);
    scePowerSetClockFrequency(333, 333, 166);

    r_init();
    audio_start();
    gm_init(&dyn);

    /* build the town once, with a loading screen */
    r_begin_frame(0xFF000000u);
    hud_loading(0.1f, "Будуємо Соняшниківськ...");
    r_end_frame();
    city_build(1996);
    carmodels_build();
    r_flush_cache();

    has_save = level_load();
    if (!has_save) level_defaults();
    apply_volumes();
    synth_music(MUS_MENU);
    menu_sel = has_save ? MENU_CONTINUE : MENU_NEW;
    last_time = now_us();

    while (running) {
        unsigned int now = now_us(), pressed, buttons;
        float dt = (now - last_time) / 1000000.0f, view_yaw = 0, view_pitch = 0, radius;
        V3 eye = v3(0, 0, 0), focus;
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

        sceCtrlReadBufferPositive(&pad, 1);
        buttons = pad.Buttons;
        pressed = buttons & ~old_buttons;
        old_buttons = buttons;
        radius = L.radius * 64.0f;

        /* ---------------------------------------------------- menus */
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
                if (menu_sel == MENU_CONTINUE) begin_session();
                else if (menu_sel == MENU_NEW) {
                    if (has_save) mode = MODE_CONFIRM;
                    else { Level keep = L; level_defaults(); L.radius = keep.radius; L.music_vol = keep.music_vol; begin_session(); }
                } else if (menu_sel == MENU_SETTINGS) { settings_return = MODE_MENU; mode = MODE_SETTINGS; menu_sel = 0; }
                else running = 0;
            }
            break;
        case MODE_CONFIRM:
            if (pressed & PSP_CTRL_CROSS) {
                Level keep = L;
                level_defaults();
                L.radius = keep.radius;
                L.look_speed = keep.look_speed;
                L.music_vol = keep.music_vol;
                L.sfx_vol = keep.sfx_vol;
                remove(LEVEL_FILE);
                has_save = 0;
                begin_session();
            }
            if (pressed & PSP_CTRL_CIRCLE) { mode = MODE_MENU; menu_sel = MENU_NEW; }
            break;
        case MODE_SETTINGS:
            if (pressed & PSP_CTRL_UP) { menu_sel = (menu_sel + SET_COUNT - 1) % SET_COUNT; synth_sfx(SFX_UI); }
            if (pressed & PSP_CTRL_DOWN) { menu_sel = (menu_sel + 1) % SET_COUNT; synth_sfx(SFX_UI); }
            if (pressed & (PSP_CTRL_LEFT | PSP_CTRL_RIGHT | PSP_CTRL_CROSS)) {
                int d = (pressed & PSP_CTRL_LEFT) ? -1 : 1;
                synth_sfx(SFX_UI);
                switch (menu_sel) {
                case SET_RADIUS: L.radius = L.radius + d < 2 ? 6 : (L.radius + d > 6 ? 2 : L.radius + d); break;
                case SET_LOOK:   L.look_speed = L.look_speed + d < 1 ? 5 : (L.look_speed + d > 5 ? 1 : L.look_speed + d); break;
                case SET_MUSIC:  L.music_vol = (int)clampf((float)(L.music_vol + d), 0, 10); apply_volumes(); break;
                case SET_SFX:    L.sfx_vol = (int)clampf((float)(L.sfx_vol + d), 0, 10); apply_volumes(); break;
                case SET_CYCLE:  L.day_cycle = !L.day_cycle; break;
                case SET_TIME:   L.time = (L.time > 0.02f && L.time < 0.48f) ? 0.55f : 0.1f; break;
                case SET_INFO:   L.show_info = !L.show_info; break;
                default: if (pressed & PSP_CTRL_CROSS) { mode = settings_return; menu_sel = 0; } break;
                }
            }
            if (pressed & PSP_CTRL_CIRCLE) { mode = settings_return; menu_sel = 0; }
            break;
        case MODE_PAUSE:
            if (pressed & (PSP_CTRL_START | PSP_CTRL_CIRCLE)) mode = MODE_PLAY;
            if (pressed & PSP_CTRL_UP) { menu_sel = (menu_sel + PAUSE_COUNT - 1) % PAUSE_COUNT; synth_sfx(SFX_UI); }
            if (pressed & PSP_CTRL_DOWN) { menu_sel = (menu_sel + 1) % PAUSE_COUNT; synth_sfx(SFX_UI); }
            if (pressed & PSP_CTRL_CROSS) {
                synth_sfx(SFX_CONFIRM);
                if (menu_sel == PAUSE_RESUME) mode = MODE_PLAY;
                else if (menu_sel == PAUSE_SETTINGS) { settings_return = MODE_PAUSE; mode = MODE_SETTINGS; menu_sel = 0; }
                else {
                    level_save();
                    in_game = 0;
                    synth_engine(0, 0.0f);
                    synth_music(MUS_MENU);
                    mode = MODE_MENU;
                    menu_sel = MENU_CONTINUE;
                }
            }
            break;
        default:
            break;
        }

        /* ---------------------------------------------------- gameplay */
        if (mode == MODE_PLAY) {
            float ls = 1.0f + 0.5f * L.look_speed;
            StoryView v;
            StoryOut o;
            int ev, i;
            if (pressed & PSP_CTRL_START) { mode = MODE_PAUSE; menu_sel = 0; synth_sfx(SFX_UI); }
            if (L.day_cycle) {
                L.time += dt / DAY_LENGTH;
                if (L.time >= 1.0f) L.time -= 1.0f;
            }
            /* camera control */
            if (buttons & PSP_CTRL_LTRIGGER && !(buttons & PSP_CTRL_RTRIGGER)) cam_yaw += ls * dt;
            if (buttons & PSP_CTRL_RTRIGGER && !(buttons & PSP_CTRL_LTRIGGER)) cam_yaw -= ls * dt;
            if (pstate == P_FOOT) {
                if (buttons & PSP_CTRL_UP) cam_pitch += 1.2f * dt;
                if (buttons & PSP_CTRL_DOWN) cam_pitch -= 1.2f * dt;
                cam_pitch = clampf(cam_pitch, L.first_person ? -1.3f : -0.75f, L.first_person ? 1.2f : 0.35f);
                if (pressed & PSP_CTRL_SELECT) L.first_person = !L.first_person;
            } else if (pstate == P_DRIVE && (pressed & PSP_CTRL_SELECT)) {
                L.first_person = !L.first_person;
            }
            cam_yaw = wrap_pi(cam_yaw);

            if (pstate == P_FOOT) {
                float ax = analog(pad.Lx), ay = -analog(pad.Ly);
                V3 f = yaw_fwd(cam_yaw), r = yaw_right(cam_yaw);
                V3 move = v3(f.x * ay + r.x * ax, 0, f.z * ay + r.z * ax);
                int pev;
                if (L.first_person) move = v3(f.x * ay + r.x * ax, 0, f.z * ay + r.z * ax);
                pev = ped_update(player, move, (buttons & PSP_CTRL_CROSS) != 0, (pressed & PSP_CTRL_SQUARE) != 0, dt);
                if (L.first_person) player->yaw = cam_yaw;
                if (pev & (PED_EV_STEP | PED_EV_LAND)) synth_sfx(SFX_STEP);
                {
                    float hit = car_push_point(&player->pos, 0.32f);
                    if (hit > 5.0f && player->knock_t <= 0.0f) {
                        player->knock_t = 2.6f;
                        synth_sfx(SFX_CRASH);
                    }
                }
                /* camera follows behind when walking */
                if ((ax != 0.0f || ay != 0.0f) && !L.first_person && !(buttons & (PSP_CTRL_LTRIGGER | PSP_CTRL_RTRIGGER)))
                    cam_yaw = wrap_pi(cam_yaw + wrap_pi(player->yaw - cam_yaw) * clampf(dt * 1.2f, 0.0f, 1.0f));
                if (pressed & PSP_CTRL_TRIANGLE) {
                    int side = 0, k = car_nearest(player->pos, 4.5f, &side);
                    if (k >= 0) ride_enter(&ride, player, k, side);
                }
            } else if (pstate == P_DRIVE) {
                player_drive_tick(dt, buttons, pressed, &pad);
            } else if (pstate == RIDE_WALK && (pressed & PSP_CTRL_TRIANGLE)) {
                ride_cancel(&ride, player); /* changed our mind */
            } else {
                ride_tick(dt);
            }

            focus = car_idx >= 0 ? cars[car_idx].pos : player->pos;
            /* traffic also stops for us while we run after a car */
            ev = cars_update(dt, focus, (pstate == P_FOOT || pstate == RIDE_WALK) ? &player->pos : NULL, 1);
            if (ev & CAR_EV_HONK) synth_sfx(SFX_HORN);
            if ((ev & CAR_EV_CRASH) && car_idx < 0) synth_sfx(SFX_CRASH);

            /* NPCs: idle, look at the player when close */
            for (i = 1; i < N_ACTORS; i++) {
                Ped *p = &peds[i];
                V3 d;
                if (!p->active) continue;
                d = v3_sub(player->pos, p->pos);
                p->has_look_at = d.x * d.x + d.z * d.z < 64.0f;
                p->look_at = v3(player->pos.x, player->pos.y + 1.6f, player->pos.z);
                ped_update(p, v3(0, 0, 0), 0, 0, dt);
                if (car_push_point(&p->pos, 0.32f) > 5.0f && p->knock_t <= 0.0f) {
                    p->knock_t = 2.6f;
                    synth_sfx(SFX_CRASH);
                }
            }
            for (i = 0; i < MAX_EXTRAS; i++)
                if (extras[i].active) {
                    ped_update(&extras[i], v3(0, 0, 0), 0, 0, dt);
                    extra_t[i] -= dt;
                    if (extra_t[i] <= 0.0f) extras[i].active = 0;
                }

            /* story */
            memset(&v, 0, sizeof(v));
            v.pos = focus;
            v.in_car = pstate == P_DRIVE;
            v.in_own_car = pstate == P_DRIVE && cars[car_idx].own;
            {
                int ch = car_find_kind(CK_CHASER);
                v.chaser_alive = ch >= 0;
                if (ch >= 0) {
                    V3 d = v3_sub(cars[ch].pos, focus);
                    v.chaser_dist = sqrtf(d.x * d.x + d.z * d.z);
                }
            }
            if (pstate == P_FOOT || pstate == P_DRIVE) {
                story_update(&L.story, &v, dt, &o);
                apply_story_out(&o);
            }
            autosave += dt;
            if (autosave > AUTOSAVE_EVERY && mode == MODE_PLAY) {
                autosave = 0.0f;
                level_save();
            }
        } else if (mode == MODE_CUTSCENE) {
            CsIO io = cs_io();
            int i;
            cs_update(&cs, &io, dt, (pressed & PSP_CTRL_CROSS) != 0, (pressed & PSP_CTRL_START) != 0);
            handle_requests(cs.requests);
            for (i = 0; i < N_ACTORS; i++)
                if (peds[i].active) ped_update(&peds[i], v3(0, 0, 0), 0, 0, dt);
            cars_update(dt, player->pos, &player->pos, 0);
            if (!cs.running) finish_cutscene();
        }
        letterbox = approach(letterbox, mode == MODE_CUTSCENE ? 1.0f : 0.0f, dt * 2.5f);

        /* ---------------------------------------------------- camera */
        if (mode == MODE_CUTSCENE) {
            CsIO io = cs_io();
            V3 tgt, d;
            cs_camera(&cs, &io, &eye, &tgt);
            d = v3_sub(tgt, eye);
            view_yaw = yaw_of(d.x, d.z);
            view_pitch = atan2f(d.y, sqrtf(d.x * d.x + d.z * d.z));
        } else if (in_game && (mode == MODE_PLAY || mode == MODE_PAUSE || (mode == MODE_SETTINGS && settings_return == MODE_PAUSE))) {
            if (car_idx >= 0 && pstate == P_DRIVE) {
                Car *c = &cars[car_idx];
                float sp = v3_len(c->vel), want = c->yaw, dist = 6.0f + sp * 0.06f;
                V3 tgt = v3(c->pos.x, c->pos.y + 1.4f, c->pos.z), back;
                if (sp > 3.0f && car_speed(c) > 0.0f) want = yaw_of(c->vel.x, c->vel.z);
                if ((buttons & PSP_CTRL_LTRIGGER) && (buttons & PSP_CTRL_RTRIGGER)) want = wrap_pi(c->yaw + PI_F);
                cam_yaw = wrap_pi(cam_yaw + wrap_pi(want - cam_yaw) * clampf(dt * 3.0f, 0.0f, 1.0f));
                if (L.first_person) {
                    eye = car_local(c, v3(0, CAR_SPECS[c->type].height + 0.1f, -0.3f));
                    view_yaw = c->yaw;
                    view_pitch = -0.08f;
                } else {
                    view_yaw = cam_yaw;
                    view_pitch = -0.2f;
                    back = v3_scale(v3(-sinf(view_yaw) * cosf(view_pitch), sinf(view_pitch), -cosf(view_yaw) * cosf(view_pitch)), -1.0f);
                    dist = city_raycast(tgt, back, dist) - 0.3f;
                    eye = v3_add(tgt, v3_add(v3_scale(back, dist), v3(0, 0.6f, 0)));
                }
            } else {
                V3 head = v3(player->pos.x, player->pos.y + 1.55f, player->pos.z);
                view_yaw = cam_yaw;
                view_pitch = cam_pitch;
                if (L.first_person && pstate == P_FOOT) {
                    eye = v3(head.x, head.y + 0.1f, head.z);
                } else {
                    V3 dir = v3(-sinf(view_yaw) * cosf(view_pitch), sinf(view_pitch), -cosf(view_yaw) * cosf(view_pitch));
                    V3 back = v3_scale(dir, -1.0f);
                    float dist = city_raycast(head, back, 3.8f) - 0.25f;
                    if (dist < 0.4f) dist = 0.4f;
                    eye = v3_add(head, v3_scale(back, dist));
                    eye.y += 0.25f;
                }
            }
        } else {
            /* menu panorama: slow orbit over the town at sunset */
            float a = clock_s * 0.035f;
            V3 d;
            eye = v3(cosf(a) * 150.0f, 46.0f, sinf(a) * 150.0f);
            d = v3_sub(v3(0, 0, 0), eye);
            view_yaw = yaw_of(d.x, d.z) + 0.3f;
            view_pitch = -0.25f;
            radius = 256.0f;
        }

        /* ---------------------------------------------------- render */
        compute_sky(&sky, in_game ? L.time : 0.46f, clock_s, radius);
        r_begin_frame(sky.zenith);
        if (mode != MODE_SPLASH) draw_world(eye, view_yaw, view_pitch, &sky, radius, mode == MODE_PLAY);

        switch (mode) {
        case MODE_SPLASH: hud_splash(mode_time); break;
        case MODE_MENU: hud_main_menu(menu_sel, has_save); break;
        case MODE_CONFIRM:
            hud_main_menu(MENU_NEW, has_save);
            hud_confirm("Почати нову гру? Поточне збереження буде втрачено.", "✕ — так, нова гра", "○ — ні");
            break;
        case MODE_SETTINGS: hud_settings(menu_sel, &L); break;
        case MODE_PAUSE: hud_pause(menu_sel); break;
        case MODE_CUTSCENE:
            hud_cutscene(cs.sub_text ? cs.sub_who : NULL, cs.sub_text, cs.sub_t, letterbox, cs.title, cs.title2,
                         cs.title_t, cs.fade);
            break;
        default: {
            V3 m;
            memset(&hud, 0, sizeof(hud));
            hud.fps = fps;
            hud.in_car = pstate == P_DRIVE;
            if (hud.in_car) {
                hud.speed_kmh = fabsf(car_speed(&cars[car_idx])) * 3.6f;
                hud.rpm = cars[car_idx].rpm;
                hud.gear = cars[car_idx].gear;
                hud.hint = "✕ газ  □ гальмо  ○ ручник  △ вийти  ↑ сигнал  ←→ радіо";
            } else if (pstate == P_FOOT && car_nearest(player->pos, 4.5f, NULL) >= 0) {
                hud.hint = "△ — сісти в машину";
            }
            hud.radio_text = radio_buf;
            hud.radio_timer = radio_timer;
            hud.banner = banner;
            hud.banner_sub = banner_sub;
            hud.banner_timer = banner_t;
            hud.crosshair = L.first_person && pstate == P_FOOT;
            hud.px = (int)player->pos.x;
            hud.py = (int)player->pos.y;
            hud.pz = (int)player->pos.z;
            hud.area = city_in_town(player->pos.x, player->pos.z) ? "Соняшниківськ" : "Околиці";
            if (story_marker(&L.story, &m)) {
                V3 p = car_idx >= 0 ? cars[car_idx].pos : player->pos;
                float dx = m.x - p.x, dz = m.z - p.z;
                hud.marker = 1;
                hud.marker_dist = sqrtf(dx * dx + dz * dz);
                hud.marker_angle = -wrap_pi(yaw_of(dx, dz) - view_yaw);
            }
            hud_play(&L, &hud);
            break;
        }
        }
        r_end_frame();
    }

    if (in_game) level_save();
    audio_stop();
    sceKernelExitGame();
    return 0;
}
