/* UkraineCraft: state shared between the main loop and the HUD. */
#ifndef UC_GAME_H
#define UC_GAME_H

#include "mathx.h"
#include "story.h"

#define GAME_TITLE "UkraineCraft"
#define SAVE_DIR "ukrainecraft_save"
#define LEVEL_FILE SAVE_DIR "/level.dat"
#define RADIO_STATIONS 3

typedef struct {
    char magic[8];
    V3 pos;
    float yaw, cam_yaw;
    float time;           /* 0..1, 0 = sunrise, 0.25 = noon */
    int radius;           /* view distance in 64 m cells, 2..6 */
    int look_speed;       /* 1..5 */
    int day_cycle;
    int show_info;
    int music_vol, sfx_vol, unused_vol; /* 0..10 */
    int first_person;
    int radio;
    Story story;
    int has_car;
    V3 car_pos;
    float car_yaw;
    int kaban_setup;
} Level;

typedef struct {
    float fps;
    int in_car;
    float speed_kmh, rpm;
    int gear;
    const char *radio_text;
    float radio_timer;
    const char *banner, *banner_sub;
    float banner_timer;
    int marker;
    float marker_angle, marker_dist;
    const char *hint;
    int crosshair;
    int px, py, pz;
    const char *area;
} HudInfo;

enum { SET_RADIUS, SET_LOOK, SET_MUSIC, SET_SFX, SET_CYCLE, SET_TIME, SET_INFO, SET_BACK, SET_COUNT };
enum { PAUSE_RESUME, PAUSE_SETTINGS, PAUSE_QUIT, PAUSE_COUNT };
enum { MENU_CONTINUE, MENU_NEW, MENU_SETTINGS, MENU_EXIT, MENU_COUNT };

void hud_splash(float t);
void hud_main_menu(int sel, int has_save);
void hud_confirm(const char *question, const char *yes, const char *no);
void hud_settings(int sel, const Level *L);
void hud_pause(int sel);
void hud_loading(float progress, const char *what);
void hud_play(const Level *L, const HudInfo *h);
/* cutscene: letterbox, subtitles, title, fade */
void hud_cutscene(const char *who, const char *text, float sub_t, float letterbox,
                  const char *title, const char *title2, float title_t, float fade);

#endif
