/* Cars: arcade driving physics, lane-following town traffic, a chaser. */
#ifndef UC_VEHICLE_H
#define UC_VEHICLE_H

#define MAX_CARS 14
#define CAR_HALF_W 0.9f
#define CAR_HALF_L 2.0f
#define CAR_MAX_SPEED 24.0f

enum { CAR_FREE = 0, CAR_PARKED, CAR_PLAYER, CAR_TRAFFIC, CAR_CHASER };

/* bits returned by cars_update / car_physics */
#define CAR_EV_HONK 1
#define CAR_EV_CRASH 2

typedef struct {
    int kind;
    float pos[3];       /* centre of the car at ground level */
    float yaw;          /* forward = (-sin yaw, 0, -cos yaw) */
    float speed;        /* m/s along forward */
    float vy;
    unsigned int color; /* ABGR body colour */
    int own;            /* the family car from the story */
    /* traffic */
    int dir;            /* 0 +x, 1 +z, 2 -x, 3 -z */
    int next_dir;
    float turn_at;
    int band_done;
    float blocked, honk_cd, stuck, reverse_t;
} Car;

extern Car cars[MAX_CARS];

void cars_clear(void);
int  car_spawn(int kind, float x, float y, float z, float yaw, unsigned int color);
int  car_find_kind(int kind);                       /* first car of this kind or -1 */
int  car_nearest(const float pos[3], float maxd);   /* enterable car or -1 */

/* Drives one car. throttle -1..1 (negative brakes/reverses), steer -1..1 (+ = left). */
int  car_physics(Car *c, float throttle, float steer, float dt);

/* Traffic + chaser AI, spawning and despawning around the player.
 * player_foot: player's feet position when walking (NULL while driving). */
int  cars_update(float dt, const float focus[3], const float *player_foot, int allow_traffic);

/* Pushes point (x, z) out of the car's footprint. Returns 1 if it was inside. */
int  car_push_out(const Car *c, float *x, float *z, float margin);
/* Free spot next to the car for getting out. */
void car_exit_point(const Car *c, float out[3]);

float car_dir_yaw(int dir);

#endif
