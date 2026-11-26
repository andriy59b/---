/* Chapter one of Andriy's story: mission flow, markers, cutscene triggers. */
#ifndef UC_STORY_H
#define UC_STORY_H

#include "mathx.h"

enum {
    M_INTRO = 0,  /* opening cutscene */
    M_HOME,       /* find brother Taras at the garages */
    M_MARKET,     /* take the car, drive to the market */
    M_PARCEL,     /* deliver the parcel to the kiosk in time */
    M_TAIL,       /* lose Kaban's black car */
    M_KABAN,      /* meet Kaban */
    M_FREE        /* chapter finished: free roam */
};

typedef struct {
    int mission;
    int step;
    float timer;   /* countdown in seconds, < 0 when none */
    float aux;
    int money;     /* karbovanets */
} Story;

typedef struct {
    V3 pos;        /* player (or the car the player drives) */
    int in_car;
    int in_own_car;
    int chaser_alive;
    float chaser_dist;
} StoryView;

typedef struct {
    int scene;           /* CS_* to play now, 0 = none */
    int remove_chaser;
    int kaban_setup;     /* place Kaban, his men and his car */
    int mission_passed;
    int mission_failed;
    const char *pager;   /* pager message to show */
} StoryOut;

void story_start(Story *s);
void story_update(Story *s, const StoryView *v, float dt, StoryOut *out);
void story_scene_done(Story *s, int scene, StoryOut *out);

const char *story_title(const Story *s);
const char *story_objective(const Story *s);
int story_marker(const Story *s, V3 *out);

#endif
