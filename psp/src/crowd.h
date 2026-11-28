/* Passers-by: walk the pavements around the player, run from trouble or
 * fight back, get knocked down by fists and by cars (and get up again).
 * Pure logic, testable on a PC. */
#ifndef UC_CROWD_H
#define UC_CROWD_H

#include "actor.h"

#define CROWD_MAX 12

enum { WK_FREE = 0, WK_WANDER, WK_FLEE, WK_FIGHT, WK_DOWN };

typedef struct {
    Ped ped;
    int state;
    float t;          /* time in the state */
    int hp;
    float punch_cd;   /* NPC: time to the next punch */
    V3 threat;        /* what it runs from */
    int axis;         /* pavement direction: 0 along x, 1 along z */
} Walker;

extern Walker crowd[CROWD_MAX];

/* events */
#define CROWD_EV_HIT 1          /* the player's fist landed */
#define CROWD_EV_PLAYER_HIT 2   /* someone punched the player */
#define CROWD_EV_PLAYER_DOWN 4  /* ... hard enough to knock him down */
#define CROWD_EV_RUN_OVER 8     /* a car knocked a passer-by down */
#define CROWD_EV_SWING 16       /* an NPC threw a punch (sound) */

void crowd_reset(unsigned int seed);
/* player: on foot or NULL (driving); focus: where to keep the crowd;
 * spawn: 0 to only update the ones there are (cutscenes, menus) */
int  crowd_update(float dt, Ped *player, V3 focus, int spawn);
/* the player's punch reached full extension */
int  crowd_player_punch(const Ped *player);
int  crowd_fighting(void);           /* someone is fighting the player */
int  crowd_positions(V3 *out, int cap); /* standing walkers, for traffic to stop for */
/* meshes of those within radius of the eye; detailed ones nearer than `near` */
void crowd_append_meshes(GMesh *m, V3 eye, float radius, float near);

#endif
