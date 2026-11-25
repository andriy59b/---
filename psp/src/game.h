/* UkraineCraft: shared game state between the main loop and the HUD. */
#ifndef UC_GAME_H
#define UC_GAME_H

#include "story.h"
#include "world.h"

#define GAME_TITLE "UkraineCraft"
#define WORLD_DIR "ukrainecraft_world"
#define LEVEL_FILE WORLD_DIR "/level.dat"
#define HOTBAR_N 9
#define RADIO_STATIONS 3

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
    int music_vol;       /* 0..10 */
    int sfx_vol;         /* 0..10 */
    int third_person;
    int radio;           /* station index */
    Story story;
    int has_car;         /* the family car exists */
    float car_pos[3];
    float car_yaw;
} Level;

/* What the HUD needs to know about the current frame. */
typedef struct {
    float fps;
    int has_hit, hit_block;
    int crosshair;
    int hotbar;
    int near_car;
    int in_car;
    int can_exit;
    float speed_kmh;
    const char *radio_text;
    float radio_timer;
    const char *banner;
    const char *banner_sub;
    float banner_timer;
    int marker;
    float marker_angle;  /* radians relative to the view direction */
    float marker_dist;
    float name_timer;
    int underwater;
    const char *biome;
    int px, py, pz;
} HudInfo;

enum { SET_RADIUS, SET_LOOK, SET_MUSIC, SET_SFX, SET_CYCLE, SET_TIME, SET_INFO, SET_BACK, SET_COUNT };
enum { PAUSE_RESUME, PAUSE_BLOCKS, PAUSE_SETTINGS, PAUSE_QUIT, PAUSE_COUNT };
enum { MENU_CONTINUE, MENU_NEW, MENU_SETTINGS, MENU_EXIT, MENU_COUNT };

#define INV_COLS 9

void hud_splash(float t);
void hud_main_menu(int sel, int has_save, float t);
void hud_confirm(const char *question, const char *yes, const char *no);
void hud_settings(int sel, const Level *L);
void hud_pause(int sel);
void hud_inventory(int cursor, const Level *L);
void hud_intro(int slide, float t);
void hud_dialog(const char *speaker, const char *text, float t);
void hud_play(const Level *L, const HudInfo *h);
void hud_loading(int done, int total, const char *what);

#endif
