/* Chapter one of Andriy's story: missions, markers, dialogue. */
#ifndef UC_STORY_H
#define UC_STORY_H

enum {
    M_HOME = 1,   /* find brother Taras at the garages */
    M_MARKET,     /* get in the car, drive to the market */
    M_PARCEL,     /* deliver the parcel to the kiosk in time */
    M_TAIL,       /* lose the black car */
    M_KABAN,      /* meet Kaban */
    M_FREE        /* chapter finished: free roam */
};

#define STORY_MAX_LINES 8

typedef struct {
    int mission;
    int step;
    float timer;       /* countdown in seconds, < 0 when none */
    float aux;
    int money;         /* karbovanets */
} Story;

typedef struct {
    float pos[3];      /* player (or the car the player drives) */
    int in_car;
    int in_own_car;
    int chaser_alive;
    float chaser_dist;
} StoryView;

typedef struct {
    int lines;
    const char *speaker[STORY_MAX_LINES];
    const char *text[STORY_MAX_LINES];
    int spawn_own_car;
    int spawn_chaser;
    int remove_chaser;
    int mission_passed;   /* play the jingle, show the banner */
    int mission_failed;
} StoryOut;

/* Intro slides shown before the first mission */
#define STORY_INTRO_SLIDES 5
extern const char *const STORY_INTRO[STORY_INTRO_SLIDES];

void story_start(Story *s);
void story_update(Story *s, const StoryView *v, float dt, StoryOut *out);

const char *story_title(const Story *s);      /* mission name */
const char *story_objective(const Story *s);  /* current goal */
int story_marker(const Story *s, float out[3]); /* 1 if a marker should be shown */

#endif
