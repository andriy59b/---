/* Cars: six 90s-era models (fictional names), rigid-body tyre physics,
 * collisions, traffic / chaser drivers. */
#ifndef UC_CAR_H
#define UC_CAR_H

#include "gfx.h"

enum { CAR_KAISER = 0, CAR_LASTIVKA, CAR_KLASYKA, CAR_PANTERA, CAR_GELENDER, CAR_POLE, CAR_TYPES };
enum { DRIVE_RWD = 0, DRIVE_FWD, DRIVE_AWD };

typedef struct {
    const char *name;
    const char *nickname;
    float length, width, height, wheelbase, track, wheel_r, clearance;
    /* side profile (metres from the front bumper; heights from the ground) */
    float bumper_h, hood_front_h, hood_h, ws_base, roof_front, roof_rear, rear_glass_base, trunk_h, rear_h;
    float glass_inset;
    int round_lights, chrome_grille, spare_wheel;
    /* physics */
    float mass, power_kw, top_kmh, grip, cg_h, max_steer, soft;
    int drive;
} CarSpec;

extern const CarSpec CAR_SPECS[CAR_TYPES];

enum { CK_FREE = 0, CK_PARKED, CK_PLAYER, CK_TRAFFIC, CK_CHASER, CK_SCRIPT };

typedef struct {
    int kind, type, own;
    V3 pos;               /* ground point under the centre of the car */
    float yaw;
    V3 vel;               /* world velocity (y unused) */
    float yaw_rate;
    /* driver inputs */
    float in_throttle;    /* -1 (brake / reverse) .. 1 */
    float in_steer;       /* -1 right .. 1 left */
    int in_handbrake;
    /* drivetrain */
    float steer, rpm;
    int gear;             /* 1.., -1 reverse */
    /* visuals */
    float wheel_spin, body_y, body_pitch, body_roll, pitch_v, roll_v, y_v;
    float curb_pitch, curb_roll, ax;
    float door;           /* driver door 0..1 */
    unsigned int paint;
    float skid;           /* 0..1 tyre squeal amount */
    float impact;         /* strongest impact this frame (m/s) */
    /* AI */
    int dir, next_dir, band_done, turning;
    float turn_at, blocked, honk_cd, stuck, reverse_t, cruise;
    float tcx, tcz, tr, tex, tez, tsx, tsz;  /* turn arc: centre, radius, end, start */
    int tsgn;
} Car;

#define MAX_CARS 16
extern Car cars[MAX_CARS];

void cars_clear(void);
int  car_spawn(int kind, int type, V3 pos, float yaw, unsigned int paint);
int  car_find_kind(int kind);
int  car_nearest(V3 pos, float maxd);          /* enterable car */
float car_speed(const Car *c);                 /* signed forward speed m/s */

/* One physics step for one car using its in_* inputs. */
void car_step(Car *c, float dt);
/* AI + physics for all cars. player_pos: the walking player (NULL when driving).
 * Returns CAR_EV_* flags. */
#define CAR_EV_HONK 1
#define CAR_EV_CRASH 2
#define CAR_EV_HIT_PED 4
int  cars_update(float dt, V3 focus, const V3 *player_pos, int traffic);

/* Character vs cars: pushes the point out; returns impact speed if hit by a moving car. */
float car_push_point(V3 *p, float radius);
V3   car_door_point(const Car *c);             /* where the driver stands to get in */
V3   car_seat_point(const Car *c);
V3   car_local(const Car *c, V3 local);        /* car space -> world */

/* Rendering: meshes in car space (built once). paint mesh vertex colours are
 * multiplied by the car paint. */
void carmodels_build(void);
void car_append_mesh(GMesh *out, const Car *c);   /* body, details, door, wheels in world space */
V3   car_light_pos(const Car *c, int i);          /* 0,1 head lights; 2,3 tail lights */

#endif
