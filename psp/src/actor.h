/* Characters: skeleton, procedural animation, appearance, movement physics. */
#ifndef UC_ACTOR_H
#define UC_ACTOR_H

#include "gfx.h"

enum {
    B_PELVIS = 0, B_SPINE, B_HEAD,
    B_UARM_L, B_FARM_L, B_HAND_L, B_UARM_R, B_FARM_R, B_HAND_R,
    B_THIGH_L, B_SHIN_L, B_FOOT_L, B_THIGH_R, B_SHIN_R, B_FOOT_R,
    BONES
};

enum {
    CH_ANDRIY = 0, CH_TARAS, CH_GALYA, CH_KABAN, CH_SELLER, CH_THUG,
    /* passers-by */
    CH_NADIA, CH_PETRO, CH_OLIA, CH_VOVAN, CH_STEPAN, CH_IRYNA, CH_MYKHAILO, CH_PETRYK,
    CH_COUNT
};
#define CH_FIRST_PASSER CH_NADIA
#define CH_PASSERS (CH_COUNT - CH_FIRST_PASSER)

enum { HAT_NONE = 0, HAT_CAP, HAT_SCARF, HAT_POLICE, HAT_BEANIE };
enum { HAIR_SHORT = 0, HAIR_LONG, HAIR_BALD, HAIR_BUZZ };
enum { TEMPER_FLEE = 0, TEMPER_FIGHT };
#define CLOTH_SHIRT (-1)   /* pseudo tile: the plain shirt quarter of T_HAIR */

enum {
    GEST_NONE = 0, GEST_EXPLAIN, GEST_POINT, GEST_SHRUG, GEST_HIPS, GEST_CROSSED, GEST_WAVE, GEST_COUNT
};

typedef struct {
    const char *name;
    int face;
    float height, width;      /* scale factors */
    int top_tile;             /* T_* or CLOTH_SHIRT */
    int top_front_x, top_front_w; /* sub-rect of the top tile for the chest front */
    unsigned int top_tint;
    int pants_tile;           /* trousers, or the skirt when skirt is set */
    unsigned int pants_tint;
    unsigned int skin, hair, shoes;
    int hat;                  /* HAT_* */
    unsigned int hat_tint;
    int hair_style;           /* HAIR_* */
    int skirt;
    int temper;               /* TEMPER_*: a passer-by in trouble runs or fights */
    float voice_pitch;        /* Hz */
    float voice_formant;      /* formant scale */
} Look;

extern const Look LOOKS[CH_COUNT];

typedef struct {
    float t;            /* running clock */
    float phase;        /* walk cycle (radians, advances with distance) */
    float move;         /* 0 idle .. 1 moving */
    float run;          /* 0 walk .. 1 run */
    float air;          /* 0 grounded .. 1 in the air */
    float vy;
    float land;         /* landing squash, decays */
    float crouch;       /* jump anticipation / getting into a car */
    float lean;         /* roll into turns */
    float accel_lean;   /* pitch from acceleration */
    float sit;          /* driving pose */
    float knock;        /* 0..1 knocked down */
    float talk;         /* 0..1 talking */
    float mouth;        /* 0..1 mouth open (lip sync) */
    int gesture;
    float gesture_w;    /* blend weight of the gesture */
    float head_yaw, head_pitch; /* look-at offsets */
    float fight;        /* 0..1 fists up */
    float punch;        /* 0..1 arm extension of the current punch */
    int punch_arm;      /* 0 left, 1 right */
    float hurt;         /* 0..1 flinch after a blow */
} AnimState;

typedef struct {
    float rx[BONES], ry[BONES], rz[BONES];
    float pelvis_y, pelvis_x, root_pitch, root_roll;
} Pose;

typedef struct {
    int active;
    int who;            /* CH_* */
    V3 pos;             /* feet */
    V3 vel;
    float yaw;          /* facing */
    float yaw_vel;
    int on_ground;
    float jump_t;       /* anticipation timer */
    float knock_t;
    AnimState anim;
    /* NPC / cutscene control */
    int has_goal;
    V3 goal;
    float goal_yaw;
    float walk_speed;
    int face_goal_yaw;
    V3 look_at;
    int has_look_at;
    /* fists */
    float punch_t;      /* time into the current punch, < 0: none */
    int fight_mode;     /* guard up */
} Ped;

/* Events returned by ped_update */
#define PED_EV_STEP 1
#define PED_EV_LAND 2
#define PED_EV_JUMP 4
#define PED_EV_KNOCKED 8
#define PED_EV_PUNCH 16     /* the fist reached full extension: test for a hit now */

#define PUNCH_REACH 1.05f   /* m in front of the feet */

void ped_init(Ped *p, int who, V3 pos, float yaw);
/* move: desired direction (world xz, length 0..1); sprint; jump pressed */
int  ped_update(Ped *p, V3 move, int sprint, int jump, float dt);
void ped_face(Ped *p, float yaw, float dt);
int  ped_punch(Ped *p);                 /* starts a punch (alternating fists); 0 if still busy */
void ped_take_hit(Ped *p, V3 from, float strength); /* flinch and stagger away from `from` */

void anim_pose(const AnimState *a, Pose *out);
void ped_bones(const Ped *p, const Pose *pose, Xf out[BONES]);
void ped_append_mesh(GMesh *m, const Ped *p);                /* detailed model */
void ped_append_mesh_lod(GMesh *m, const Ped *p, int detail); /* 0: cheap boxes for far away */

#endif
