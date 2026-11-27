/* Getting into and out of cars: walk to the nearest door (around the body,
 * not through it), open it, duck in (slide over from the passenger side),
 * close it; and the reverse. Pure logic, testable on a PC. */
#ifndef UC_RIDE_H
#define UC_RIDE_H

#include "actor.h"
#include "car.h"

enum {
    RIDE_FOOT = 0, RIDE_WALK, RIDE_OPEN, RIDE_IN, RIDE_CLOSE, RIDE_DRIVE,
    RIDE_EXIT_OPEN, RIDE_EXIT_OUT, RIDE_EXIT_CLOSE
};

#define RIDE_EV_DOOR 1      /* play the door sound */
#define RIDE_EV_ENTERED 2   /* now driving */
#define RIDE_EV_EXITED 4    /* back on foot */
#define RIDE_EV_FLEE 8      /* the previous driver jumped out: see flee_pos */

typedef struct {
    int state, car, side;
    float t;
    V3 flee_pos;
    float flee_yaw;
} Ride;

void ride_reset(Ride *r);
void ride_enter(Ride *r, Ped *p, int car, int side);
int  ride_exit(Ride *r, Ped *p);          /* 0 if the car is still moving */
void ride_force_out(Ride *r, Ped *p);     /* instant, for cutscenes */
int  ride_update(Ride *r, Ped *p, float dt);
int  ride_busy(const Ride *r);            /* getting in or out */

#endif
