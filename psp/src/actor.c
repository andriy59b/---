/* Characters: 15-bone skeleton with procedural animation (idle breathing,
 * distance-locked walk/run cycles, jumps with anticipation and landing,
 * turn lean, gestures, driving, knock-down), movement physics, meshes. */
#include "actor.h"
#include "city.h"
#include "texgen.h"

#include <string.h>

#define WHITE 0xFFFFFFFFu

/* ABGR colours. Columns: name, face, height, width, top (tile, front x/w, tint), trousers or skirt
 * (tile, tint), skin, hair, shoes, hat (kind, tint), hair style, skirt, temper, voice pitch, formant */
const Look LOOKS[CH_COUNT] = {
    { "Андрій",      FACE_ANDRIY,   1.00f, 1.00f, T_CLOTH_ARMY,     0, 64, 0xFFFFFFFFu, T_CLOTH_DENIM,    0xFFFFFFFFu, 0xFF8CB0DCu, 0xFF203048u, 0xFF303438u, HAT_NONE,   0,           HAIR_SHORT, 0, TEMPER_FIGHT, 118.0f, 1.00f },
    { "Тарас",       FACE_TARAS,    0.98f, 1.06f, T_CLOTH_OVERALLS, 0, 64, 0xFFFFFFFFu, T_CLOTH_OVERALLS, 0xFFFFFFFFu, 0xFF7CA0D0u, 0xFF283850u, 0xFF2A3A50u, HAT_CAP,    0xFF5A3A2Au, HAIR_SHORT, 0, TEMPER_FIGHT, 102.0f, 0.95f },
    { "Тітка Галя",  FACE_GALYA,    0.93f, 1.12f, T_CLOTH_DRESS,    0, 64, 0xFFFFFFFFu, T_CLOTH_DRESS,    0xFFFFFFFFu, 0xFF94B8E4u, 0xFF4060A0u, 0xFF203048u, HAT_SCARF,  0xFFFFFFFFu, HAIR_SHORT, 1, TEMPER_FIGHT, 215.0f, 1.18f },
    { "Кабан",       FACE_KABAN,    1.05f, 1.25f, T_CLOTH_BLAZER,   0, 32, 0xFFFFFFFFu, CLOTH_SHIRT,      0xFF262626u, 0xFF7098D0u, 0xFF303030u, 0xFF181818u, HAT_NONE,   0,           HAIR_BALD,  0, TEMPER_FIGHT,  86.0f, 0.88f },
    { "Продавець",   FACE_SELLER,   0.97f, 0.98f, CLOTH_SHIRT,      0, 64, 0xFFB09070u, T_CLOTH_DENIM,    0xFFFFFFFFu, 0xFF88ACD8u, 0xFF505868u, 0xFF404448u, HAT_NONE,   0,           HAIR_SHORT, 0, TEMPER_FLEE,  135.0f, 1.02f },
    { "Браток",      FACE_THUG,     1.02f, 1.10f, T_CLOTH_LEATHER,  0, 64, 0xFFFFFFFFu, T_CLOTH_TRACK,    0xFF3A3A3Au, 0xFF80A4D4u, 0xFF181818u, 0xFFE8E8E8u, HAT_NONE,   0,           HAIR_BUZZ,  0, TEMPER_FIGHT,  96.0f, 0.92f },
    /* passers-by */
    { "Баба Надя",   FACE_NADIA,    0.88f, 1.10f, T_CLOTH_KNIT,     0, 64, 0xFF4A6A8Au, T_CLOTH_DRESS,    0xFF6A5A58u, 0xFF98B4D8u, 0xFFB8B8B8u, 0xFF283038u, HAT_SCARF,  0xFFFFFFFFu, HAIR_SHORT, 1, TEMPER_FIGHT, 230.0f, 1.20f },
    { "Дід Петро",   FACE_PETRO,    0.95f, 1.00f, CLOTH_SHIRT,      0, 64, 0xFF5A6A74u, T_CLOTH_DENIM,    0xFF506070u, 0xFF809CC8u, 0xFFC8C8C8u, 0xFF303030u, HAT_CAP,    0xFF404850u, HAIR_SHORT, 0, TEMPER_FLEE,   95.0f, 0.90f },
    { "Оля",         FACE_OLIA,     0.94f, 0.90f, T_CLOTH_DENIM,    0, 64, 0xFFE0D8D0u, T_CLOTH_DENIM,    0xFF483830u, 0xFF9CC0ECu, 0xFF60B8E0u, 0xFFE0E0E0u, HAT_NONE,   0,           HAIR_LONG,  0, TEMPER_FLEE,  225.0f, 1.20f },
    { "Вован",       FACE_VOVAN,    1.00f, 1.05f, T_CLOTH_TRACK,    0, 64, 0xFF803020u, T_CLOTH_TRACK,    0xFF803020u, 0xFF84A8D8u, 0xFF384048u, 0xFFF0F0F0u, HAT_CAP,    0xFF202020u, HAIR_BUZZ,  0, TEMPER_FIGHT, 105.0f, 0.95f },
    { "Степанович",  FACE_STEPAN,   1.02f, 1.14f, CLOTH_SHIRT,      0, 64, 0xFF806858u, CLOTH_SHIRT,      0xFF503828u, 0xFF7898C8u, 0xFF203040u, 0xFF181818u, HAT_POLICE, 0xFF503020u, HAIR_SHORT, 0, TEMPER_FIGHT,  92.0f, 0.90f },
    { "Ірина",       FACE_IRYNA,    0.97f, 0.94f, T_CLOTH_BLAZER,   0, 32, 0xFF3030B8u, CLOTH_SHIRT,      0xFF242424u, 0xFF94B4E0u, 0xFF102038u, 0xFF181818u, HAT_NONE,   0,           HAIR_LONG,  1, TEMPER_FLEE,  210.0f, 1.15f },
    { "Михайло",     FACE_MYKHAILO, 1.03f, 1.16f, T_CLOTH_KNIT,     0, 64, 0xFF3A5A3Au, T_CLOTH_OVERALLS, 0xFF707070u, 0xFF6C90C0u, 0xFF283038u, 0xFF283038u, HAT_BEANIE, 0xFF303438u, HAIR_SHORT, 0, TEMPER_FIGHT,  90.0f, 0.88f },
    { "Петрик",      FACE_PETRYK,   0.72f, 0.86f, T_CLOTH_TRACK,    0, 64, 0xFF2020C0u, T_CLOTH_DENIM,    0xFFFFFFFFu, 0xFFA0C4ECu, 0xFF4090C8u, 0xFF3048A0u, HAT_NONE,   0,           HAIR_SHORT, 0, TEMPER_FLEE,  260.0f, 1.30f },
};

static UVRect cloth_uv(int tile)
{
    return tile == CLOTH_SHIRT ? uv_sub(T_HAIR, SC_SHIRT) : uv_tile(tile);
}

typedef struct {
    int parent;
    float ox, oy, oz;
    float x0, y0, z0, x1, y1, z1;
} BoneDef;

static const BoneDef BONE_DEF[BONES] = {
    { -1,       0.0f,   1.00f, 0.0f,  -0.17f, -0.10f, -0.11f, 0.17f, 0.12f, 0.11f },   /* pelvis */
    { B_PELVIS, 0.0f,   0.10f, 0.0f,  -0.20f,  0.00f, -0.12f, 0.20f, 0.50f, 0.12f },   /* spine (chest) */
    { B_SPINE,  0.0f,   0.53f, 0.0f,  -0.11f,  0.02f, -0.12f, 0.11f, 0.29f, 0.12f },   /* head */
    { B_SPINE, -0.255f, 0.46f, 0.0f,  -0.06f, -0.30f, -0.06f, 0.06f, 0.03f, 0.06f },   /* upper arm L */
    { B_UARM_L, 0.0f,  -0.29f, 0.0f,  -0.05f, -0.27f, -0.05f, 0.05f, 0.01f, 0.05f },   /* forearm L */
    { B_FARM_L, 0.0f,  -0.27f, 0.0f,  -0.04f, -0.10f, -0.05f, 0.04f, 0.00f, 0.03f },   /* hand L */
    { B_SPINE,  0.255f, 0.46f, 0.0f,  -0.06f, -0.30f, -0.06f, 0.06f, 0.03f, 0.06f },   /* upper arm R */
    { B_UARM_R, 0.0f,  -0.29f, 0.0f,  -0.05f, -0.27f, -0.05f, 0.05f, 0.01f, 0.05f },   /* forearm R */
    { B_FARM_R, 0.0f,  -0.27f, 0.0f,  -0.04f, -0.10f, -0.05f, 0.04f, 0.00f, 0.03f },   /* hand R */
    { B_PELVIS,-0.095f,-0.03f, 0.0f,  -0.075f,-0.46f, -0.08f, 0.075f, 0.02f, 0.08f },  /* thigh L */
    { B_THIGH_L, 0.0f, -0.45f, 0.0f,  -0.06f, -0.44f, -0.065f, 0.06f, 0.01f, 0.065f }, /* shin L */
    { B_SHIN_L,  0.0f, -0.44f, 0.0f,  -0.055f,-0.08f, -0.17f, 0.055f, 0.01f, 0.06f },  /* foot L */
    { B_PELVIS, 0.095f,-0.03f, 0.0f,  -0.075f,-0.46f, -0.08f, 0.075f, 0.02f, 0.08f },  /* thigh R */
    { B_THIGH_R, 0.0f, -0.45f, 0.0f,  -0.06f, -0.44f, -0.065f, 0.06f, 0.01f, 0.065f }, /* shin R */
    { B_SHIN_R,  0.0f, -0.44f, 0.0f,  -0.055f,-0.08f, -0.17f, 0.055f, 0.01f, 0.06f },  /* foot R */
};

/* ------------------------------------------------------------- animation */

static void set(Pose *p, int b, float x, float y, float z)
{
    p->rx[b] = x;
    p->ry[b] = y;
    p->rz[b] = z;
}

static void blend_pose(Pose *dst, const Pose *src, float w)
{
    int i;
    if (w <= 0.0f) return;
    if (w > 1.0f) w = 1.0f;
    for (i = 0; i < BONES; i++) {
        dst->rx[i] = lerpf(dst->rx[i], src->rx[i], w);
        dst->ry[i] = lerpf(dst->ry[i], src->ry[i], w);
        dst->rz[i] = lerpf(dst->rz[i], src->rz[i], w);
    }
    dst->pelvis_y = lerpf(dst->pelvis_y, src->pelvis_y, w);
    dst->pelvis_x = lerpf(dst->pelvis_x, src->pelvis_x, w);
}

static void blend_arms(Pose *dst, const Pose *src, float w)
{
    int i;
    if (w <= 0.0f) return;
    for (i = B_UARM_L; i <= B_HAND_R; i++) {
        dst->rx[i] = lerpf(dst->rx[i], src->rx[i], w);
        dst->ry[i] = lerpf(dst->ry[i], src->ry[i], w);
        dst->rz[i] = lerpf(dst->rz[i], src->rz[i], w);
    }
}

static void idle_pose(const AnimState *a, Pose *o)
{
    float t = a->t, breath = sinf(t * 1.7f), shift = sinf(t * 0.45f);
    float glance = 0.25f * sinf(t * 0.31f) * smooth01(sinf(t * 0.13f) * 2.0f);
    memset(o, 0, sizeof(*o));
    o->pelvis_x = 0.012f * shift;
    o->pelvis_y = -0.006f * (breath * 0.5f + 0.5f);
    set(o, B_PELVIS, 0.0f, 0.03f * shift, 0.025f * shift);
    set(o, B_SPINE, -0.02f + 0.014f * breath, -0.03f * shift, -0.02f * shift);
    set(o, B_HEAD, 0.02f - 0.01f * breath, glance, 0.0f);
    set(o, B_UARM_L, 0.04f + 0.01f * breath, 0.0f, -0.07f);
    set(o, B_UARM_R, 0.04f + 0.01f * breath, 0.0f, 0.07f);
    set(o, B_FARM_L, 0.18f, 0.0f, 0.0f);
    set(o, B_FARM_R, 0.18f, 0.0f, 0.0f);
    set(o, B_HAND_L, 0.12f, 0.0f, 0.0f);
    set(o, B_HAND_R, 0.12f, 0.0f, 0.0f);
    /* weight on one leg, the other slightly relaxed */
    set(o, B_THIGH_L, 0.03f + 0.03f * shift, 0.05f, -0.05f - 0.025f * shift);
    set(o, B_THIGH_R, 0.03f - 0.03f * shift, -0.05f, 0.05f - 0.025f * shift);
    set(o, B_SHIN_L, -0.05f - 0.06f * (shift > 0 ? shift : 0), 0.0f, 0.0f);
    set(o, B_SHIN_R, -0.05f - 0.06f * (shift < 0 ? -shift : 0), 0.0f, 0.0f);
    set(o, B_FOOT_L, 0.02f, 0.0f, 0.05f);
    set(o, B_FOOT_R, 0.02f, 0.0f, -0.05f);
}

static void leg_cycle(Pose *o, int thigh, float ph, float run)
{
    float A = lerpf(0.42f, 0.74f, run), K = lerpf(0.95f, 1.7f, run), St = lerpf(0.06f, 0.32f, run);
    float th = A * sinf(ph) + 0.08f * run;
    float swing = cosf(ph - 0.35f);
    float knee = St + K * (swing > 0 ? powf(swing, 1.5f) : 0.0f) + 0.12f * (sinf(ph) < 0 ? -sinf(ph) : 0.0f) * (1.0f - run);
    float foot = -(th - knee) * 0.7f + 0.22f * (sinf(ph) > 0 ? sinf(ph) : 0.0f) - 0.3f * run * (cosf(ph) < 0 ? -cosf(ph) : 0.0f);
    set(o, thigh, th, 0.0f, 0.0f);
    set(o, thigh + 1, -knee, 0.0f, 0.0f);
    set(o, thigh + 2, foot - th * 0.3f, 0.0f, 0.0f);
}

static void walk_pose(const AnimState *a, Pose *o)
{
    float ph = a->phase, run = a->run;
    float B = lerpf(0.022f, 0.055f, run), Aa = lerpf(0.34f, 0.88f, run), elbow = lerpf(0.28f, 1.5f, run);
    float sp = sinf(ph);
    memset(o, 0, sizeof(*o));
    leg_cycle(o, B_THIGH_L, ph, run);
    leg_cycle(o, B_THIGH_R, ph + PI_F, run);
    o->pelvis_y = -B * cosf(2.0f * ph) - 0.055f * run;
    set(o, B_PELVIS, 0.0f, -0.10f * sp * (1.0f - 0.3f * run), 0.02f * cosf(ph));
    set(o, B_SPINE, -lerpf(0.03f, 0.16f, run) + a->accel_lean, 0.12f * sp, -0.5f * 0.02f * cosf(ph));
    set(o, B_HEAD, lerpf(0.05f, 0.22f, run) * 0.8f + 0.02f * cosf(2.0f * ph), -0.15f * sp, 0.0f);
    set(o, B_UARM_L, -Aa * sp, 0.0f, -0.08f - 0.1f * run);
    set(o, B_UARM_R, Aa * sp, 0.0f, 0.08f + 0.1f * run);
    set(o, B_FARM_L, elbow + 0.3f * (sp < 0 ? -sp : 0.0f), 0.0f, 0.0f);
    set(o, B_FARM_R, elbow + 0.3f * (sp > 0 ? sp : 0.0f), 0.0f, 0.0f);
    set(o, B_HAND_L, 0.15f, 0.0f, 0.0f);
    set(o, B_HAND_R, 0.15f, 0.0f, 0.0f);
}

static void air_pose(const AnimState *a, Pose *o)
{
    float rise = clampf(a->vy / 4.0f, -1.0f, 1.0f);
    float tuck = rise > 0 ? rise : 0.0f, fall = rise < 0 ? -rise : 0.0f;
    memset(o, 0, sizeof(*o));
    set(o, B_THIGH_L, 0.75f * tuck + 0.35f * fall, 0.0f, -0.08f);
    set(o, B_SHIN_L, -1.2f * tuck - 0.35f * fall, 0.0f, 0.0f);
    set(o, B_FOOT_L, 0.3f, 0.0f, 0.0f);
    set(o, B_THIGH_R, 0.15f * tuck + 0.1f * fall, 0.0f, 0.08f);
    set(o, B_SHIN_R, -0.55f * tuck - 0.25f * fall, 0.0f, 0.0f);
    set(o, B_FOOT_R, 0.2f, 0.0f, 0.0f);
    set(o, B_SPINE, -0.12f, 0.0f, 0.0f);
    set(o, B_HEAD, 0.08f, 0.0f, 0.0f);
    set(o, B_UARM_L, 0.35f * tuck, 0.0f, -0.45f - 0.5f * fall);
    set(o, B_UARM_R, 0.35f * tuck, 0.0f, 0.45f + 0.5f * fall);
    set(o, B_FARM_L, 0.6f, 0.0f, 0.0f);
    set(o, B_FARM_R, 0.6f, 0.0f, 0.0f);
}

static void sit_pose(Pose *o)
{
    memset(o, 0, sizeof(*o));
    o->pelvis_y = -0.42f;
    set(o, B_THIGH_L, 1.45f, 0.06f, -0.06f);
    set(o, B_THIGH_R, 1.45f, -0.06f, 0.06f);
    set(o, B_SHIN_L, -1.4f, 0.0f, 0.0f);
    set(o, B_SHIN_R, -1.4f, 0.0f, 0.0f);
    set(o, B_FOOT_L, 0.2f, 0.0f, 0.0f);
    set(o, B_FOOT_R, 0.2f, 0.0f, 0.0f);
    set(o, B_SPINE, 0.1f, 0.0f, 0.0f);
    set(o, B_HEAD, -0.08f, 0.0f, 0.0f);
    set(o, B_UARM_L, 0.9f, 0.0f, -0.18f);
    set(o, B_UARM_R, 0.9f, 0.0f, 0.18f);
    set(o, B_FARM_L, 0.7f, 0.0f, 0.0f);
    set(o, B_FARM_R, 0.7f, 0.0f, 0.0f);
}

static void gesture_pose(const AnimState *a, Pose *o)
{
    float t = a->t;
    switch (a->gesture) {
    case GEST_EXPLAIN:
        set(o, B_UARM_R, 0.45f + 0.2f * sinf(t * 2.6f), 0.0f, 0.22f);
        set(o, B_FARM_R, 1.15f + 0.35f * sinf(t * 3.7f), 0.3f * sinf(t * 2.1f), 0.0f);
        set(o, B_HAND_R, 0.2f, 0.0f, 0.4f * sinf(t * 3.1f));
        set(o, B_UARM_L, 0.25f + 0.12f * sinf(t * 2.0f + 1.0f), 0.0f, -0.15f);
        set(o, B_FARM_L, 0.9f + 0.2f * sinf(t * 2.9f + 0.5f), 0.0f, 0.0f);
        break;
    case GEST_POINT:
        set(o, B_UARM_R, 1.45f, -0.2f, 0.1f);
        set(o, B_FARM_R, 0.1f, 0.0f, 0.0f);
        set(o, B_HAND_R, 0.0f, 0.0f, 0.0f);
        set(o, B_UARM_L, 0.05f, 0.0f, -0.08f);
        set(o, B_FARM_L, 0.2f, 0.0f, 0.0f);
        break;
    case GEST_SHRUG:
        set(o, B_UARM_L, 0.2f, 0.0f, -0.4f);
        set(o, B_UARM_R, 0.2f, 0.0f, 0.4f);
        set(o, B_FARM_L, 1.2f, -0.6f, 0.0f);
        set(o, B_FARM_R, 1.2f, 0.6f, 0.0f);
        break;
    case GEST_HIPS:
        set(o, B_UARM_L, -0.3f, 0.0f, -0.65f);
        set(o, B_UARM_R, -0.3f, 0.0f, 0.65f);
        set(o, B_FARM_L, 1.9f, 0.4f, 0.0f);
        set(o, B_FARM_R, 1.9f, -0.4f, 0.0f);
        break;
    case GEST_CROSSED:
        set(o, B_UARM_L, 0.5f, 0.0f, -0.12f);
        set(o, B_UARM_R, 0.55f, 0.0f, 0.12f);
        set(o, B_FARM_L, 1.75f, -1.05f, 0.0f);
        set(o, B_FARM_R, 1.7f, 1.05f, 0.0f);
        break;
    case GEST_WAVE:
        set(o, B_UARM_R, 0.1f, 0.0f, 2.5f);
        set(o, B_FARM_R, 0.2f, 0.0f, 0.5f * sinf(t * 9.0f));
        break;
    default:
        break;
    }
}

void anim_pose(const AnimState *a, Pose *o)
{
    Pose walk, air, extra;
    idle_pose(a, o);
    if (a->move > 0.0f) {
        walk_pose(a, &walk);
        blend_pose(o, &walk, a->move);
    }
    if (a->air > 0.0f) {
        air_pose(a, &air);
        blend_pose(o, &air, a->air);
    }
    /* landing squash and crouching */
    {
        float k = a->land * 0.8f + a->crouch;
        o->pelvis_y -= 0.2f * k;
        o->rx[B_THIGH_L] += 0.75f * k;
        o->rx[B_THIGH_R] += 0.75f * k;
        o->rx[B_SHIN_L] -= 1.3f * k;
        o->rx[B_SHIN_R] -= 1.3f * k;
        o->rx[B_FOOT_L] += 0.55f * k;
        o->rx[B_FOOT_R] += 0.55f * k;
        o->rx[B_SPINE] -= 0.3f * k;
        o->rx[B_UARM_L] += 0.3f * k;
        o->rx[B_UARM_R] += 0.3f * k;
    }
    if (a->sit > 0.0f) {
        sit_pose(&extra);
        blend_pose(o, &extra, a->sit);
    }
    /* talking: nods and a living upper body; gestures on the arms */
    if (a->talk > 0.0f) {
        o->rx[B_HEAD] += a->talk * (0.05f * sinf(a->t * 7.0f) + 0.03f * sinf(a->t * 3.1f));
        o->rz[B_HEAD] += a->talk * 0.04f * sinf(a->t * 2.3f);
        o->ry[B_SPINE] += a->talk * 0.05f * sinf(a->t * 1.3f);
    }
    if (a->gesture != GEST_NONE && a->gesture_w > 0.0f) {
        extra = *o;
        gesture_pose(a, &extra);
        blend_arms(o, &extra, a->gesture_w);
    }
    /* fists up, and the punch itself: the arm shoots forward from the guard
     * while the shoulders turn into it */
    if (a->fight > 0.0f || a->punch > 0.0f) {
        float g = a->fight > a->punch ? a->fight : a->punch, k = a->punch, side = a->punch_arm ? 1.0f : -1.0f;
        int ua = a->punch_arm ? B_UARM_R : B_UARM_L, fa = a->punch_arm ? B_FARM_R : B_FARM_L;
        extra = *o;
        set(&extra, B_UARM_L, 0.35f, 0.3f, 0.1f);
        set(&extra, B_UARM_R, 0.35f, -0.3f, -0.1f);
        set(&extra, B_FARM_L, 1.75f, 0.0f, 0.0f);
        set(&extra, B_FARM_R, 1.75f, 0.0f, 0.0f);
        set(&extra, B_HAND_L, 0.0f, 0.0f, 0.0f);
        set(&extra, B_HAND_R, 0.0f, 0.0f, 0.0f);
        extra.rx[ua] = lerpf(0.35f, 1.5f, k);
        extra.ry[ua] = lerpf(-0.3f * side, -0.1f * side, k);
        extra.rx[fa] = lerpf(1.75f, 0.1f, k);
        blend_arms(o, &extra, g);
        o->ry[B_SPINE] += 0.4f * side * k;
        o->ry[B_PELVIS] += 0.12f * side * k;
        o->rx[B_SPINE] -= 0.08f * g;
        o->rx[B_HEAD] += 0.08f * g;
    }
    if (a->hurt > 0.0f) {
        o->rx[B_SPINE] += 0.35f * a->hurt;
        o->rx[B_HEAD] += 0.3f * a->hurt;
        o->rz[B_UARM_L] -= 0.4f * a->hurt;
        o->rz[B_UARM_R] += 0.4f * a->hurt;
    }
    o->rx[B_HEAD] += a->head_pitch;
    o->ry[B_HEAD] += a->head_yaw;
    o->root_roll = a->lean;
    /* knocked down: fall on the back, arms thrown out */
    if (a->knock > 0.0f) {
        o->root_pitch = 1.5f * smooth01(a->knock);
        o->rz[B_UARM_L] -= 1.1f * a->knock;
        o->rz[B_UARM_R] += 1.1f * a->knock;
        o->rx[B_THIGH_L] += 0.3f * a->knock;
        o->rx[B_SHIN_L] -= 0.6f * a->knock;
    }
}

/* ------------------------------------------------------------- skeleton */

void ped_bones(const Ped *p, const Pose *pose, Xf out[BONES])
{
    const Look *L = &LOOKS[p->who];
    int b;
    Xf root, t, r;
    t = xf_translate(v3(p->pos.x, p->pos.y, p->pos.z));
    r = xf_rot_y(p->yaw);
    root = xf_mul(&t, &r);
    r = xf_rot_z(pose->root_roll);
    root = xf_mul(&root, &r);
    r = xf_rot_x(pose->root_pitch);
    root = xf_mul(&root, &r);
    for (b = 0; b < BONES; b++) {
        const BoneDef *d = &BONE_DEF[b];
        Xf base = d->parent < 0 ? root : out[d->parent], off, rot;
        float ox = d->ox * L->width, oy = d->oy * L->height, oz = d->oz * L->width;
        if (b == B_PELVIS) {
            ox += pose->pelvis_x;
            oy += pose->pelvis_y * L->height;
        }
        off = xf_translate(v3(ox, oy, oz));
        rot = xf_euler(pose->rx[b], pose->ry[b], pose->rz[b]);
        base = xf_mul(&base, &off);
        out[b] = xf_mul(&base, &rot);
    }
}

static UVRect top_front_uv(const Look *L)
{
    return L->top_tile == CLOTH_SHIRT ? uv_sub(T_HAIR, SC_SHIRT) : uv_sub(L->top_tile, L->top_front_x, 0, L->top_front_w, 64);
}

static UVRect top_side_uv(const Look *L)
{
    if (L->top_tile == CLOTH_SHIRT) return uv_sub(T_HAIR, SC_SHIRT);
    if (L->top_front_w < 64) return uv_sub(L->top_tile, 32, 0, 32, 64);
    if (L->top_tile == T_CLOTH_ARMY) return uv_sub(L->top_tile, 0, 26, 24, 38);
    return uv_tile(L->top_tile);
}

/* face sub-rect in pixels of the character's face cell */
static UVRect face_sub(int face, int open, int x, int y, int w, int h)
{
    int tile, fx, fy;
    face_cell(face, open, &tile, &fx, &fy);
    return uv_sub(tile, fx + x, fy + y, w, h);
}

static void part(GMesh *m, const Xf *xf, const Look *L, int bone, const UVRect uv[6], unsigned int tint, float grow)
{
    const BoneDef *d = &BONE_DEF[bone];
    float w = (bone == B_SPINE || bone == B_PELVIS || bone >= B_THIGH_L) ? L->width : 1.0f + (L->width - 1.0f) * 0.6f;
    V3 lo = v3(d->x0 * w - grow, d->y0 * L->height - grow, d->z0 * w - grow);
    V3 hi = v3(d->x1 * w + grow, d->y1 * L->height + grow, d->z1 * w + grow);
    gm_box_xf(m, xf, lo, hi, uv, tint, BOX_ALL);
}

static void all6(UVRect uv[6], UVRect r)
{
    int i;
    for (i = 0; i < 6; i++) uv[i] = r;
}

static void mesh_boxes(GMesh *m, const Ped *p)
{
    const Look *L = &LOOKS[p->who];
    Pose pose;
    Xf bones[BONES];
    UVRect uv[6];
    UVRect top_front = top_front_uv(L), top_side = top_side_uv(L);
    UVRect skin = uv_sub(T_WHITE, 8, 8, 16, 16);
    int open = p->anim.mouth > 0.35f, b;

    anim_pose(&p->anim, &pose);
    ped_bones(p, &pose, bones);
    (void)top_front;

    /* pelvis and legs */
    all6(uv, cloth_uv(L->pants_tile));
    part(m, &bones[B_PELVIS], L, B_PELVIS, uv, L->pants_tint, 0.0f);
    for (b = B_THIGH_L; b <= B_THIGH_R; b += 3) {
        if (L->skirt) {
            all6(uv, cloth_uv(L->pants_tile));
            part(m, &bones[b], L, b, uv, L->pants_tint, 0.03f);
            all6(uv, skin);
            part(m, &bones[b + 1], L, b + 1, uv, gfx_mul(L->skin, 0.85f), 0.0f);
        } else {
            all6(uv, cloth_uv(L->pants_tile));
            part(m, &bones[b], L, b, uv, L->pants_tint, 0.0f);
            part(m, &bones[b + 1], L, b + 1, uv, L->pants_tint, 0.0f);
        }
        all6(uv, uv_sub(T_HAIR, SC_SHOE));
        part(m, &bones[b + 2], L, b + 2, uv, L->shoes, 0.0f);
    }
    if (L->skirt) {
        /* skirt around the hips */
        all6(uv, cloth_uv(L->pants_tile));
        part(m, &bones[B_PELVIS], L, B_PELVIS, uv, L->pants_tint, 0.04f);
    }

    /* torso: chest front shows the outfit's front */
    all6(uv, top_side);
    uv[5] = top_front;
    part(m, &bones[B_SPINE], L, B_SPINE, uv, L->top_tint, 0.0f);

    /* arms: sleeves, hands */
    for (b = B_UARM_L; b <= B_UARM_R; b += 3) {
        all6(uv, top_side);
        part(m, &bones[b], L, b, uv, L->top_tint, 0.0f);
        if (L->skirt) {
            all6(uv, skin);
            part(m, &bones[b + 1], L, b + 1, uv, L->skin, 0.0f);
        } else {
            part(m, &bones[b + 1], L, b + 1, uv, L->top_tint, 0.0f);
        }
        all6(uv, skin);
        part(m, &bones[b + 2], L, b + 2, uv, L->skin, 0.0f);
    }

    /* neck and head */
    {
        Xf neck = bones[B_HEAD];
        all6(uv, skin);
        gm_box_xf(m, &neck, v3(-0.05f, -0.04f, -0.05f), v3(0.05f, 0.04f, 0.05f), uv, L->skin, BOX_ALL);
        uv[0] = face_sub(L->face, open, 28, 14, 3, 6);   /* +x side: cheek / ear */
        uv[1] = face_sub(L->face, open, 1, 14, 3, 6);
        uv[2] = face_sub(L->face, open, 4, 1, 24, 3);    /* top: hair */
        uv[3] = skin;
        uv[4] = face_sub(L->face, open, 4, 1, 24, 5);    /* back: hair */
        uv[5] = face_sub(L->face, open, 0, 0, 32, 32);   /* face */
        part(m, &bones[B_HEAD], L, B_HEAD, uv, WHITE, 0.0f);
        if (L->hat == HAT_CAP || L->hat == HAT_POLICE || L->hat == HAT_BEANIE) {
            all6(uv, uv_sub(T_HAIR, SC_SHIRT));
            gm_box_xf(m, &bones[B_HEAD], v3(-0.12f, 0.25f, -0.125f), v3(0.12f, 0.33f, 0.125f), uv, L->hat_tint, BOX_ALL);
            gm_box_xf(m, &bones[B_HEAD], v3(-0.11f, 0.25f, -0.23f), v3(0.11f, 0.27f, -0.12f), uv, gfx_mul(L->hat_tint, 0.8f), BOX_ALL);
        } else if (L->hat == HAT_SCARF) {
            all6(uv, uv_sub(T_HAIR, SC_SCARF));
            gm_box_xf(m, &bones[B_HEAD], v3(-0.125f, 0.12f, -0.10f), v3(0.125f, 0.315f, 0.135f), uv, WHITE, BOX_ALL & ~32);
            gm_box_xf(m, &bones[B_HEAD], v3(-0.06f, -0.04f, 0.10f), v3(0.06f, 0.14f, 0.15f), uv, WHITE, BOX_ALL);
        } else if (L->hair_style != HAIR_BALD) {
            all6(uv, uv_sub(T_HAIR, SC_HAIR));
            gm_box_xf(m, &bones[B_HEAD], v3(-0.115f, 0.265f, -0.125f), v3(0.115f, 0.305f, 0.125f), uv, L->hair, BOX_ALL);
        }
    }
}


/* ----------------------------------------------------- detailed model
 * Bodies of revolution with smooth (per-vertex) lighting: the head is a
 * rounded shell with the face projected on its front, nose and ears; the
 * torso narrows at the waist and widens to the shoulders; limbs are tapered
 * six-sided tubes. Built per frame in world space like the box model. */

typedef struct { float y, rx, rz, cz; } Ring;

#define SEG_ALL 0xFFFF


static void vtx(GMesh *m, const Xf *xf, V3 p, V3 n, float u, float v, unsigned int tint)
{
    GVertex *g = &m->v[m->n++];
    V3 w = xf_point(xf, p);
    g->u = u;
    g->v = v;
    g->color = gfx_shade(xf_dir(xf, n), tint);
    g->x = w.x;
    g->y = w.y;
    g->z = w.z;
}

typedef struct {
    UVRect side, front, back, cap;
    float front_w, front_y0, front_y1;  /* planar projection of `front` */
    int planar;                          /* 1: front segments use the planar projection */
    unsigned int tint, front_tint;
} LatheLook;

/* Rings around the bone's y axis, `sides` segments; angle 0 faces the
 * character's front (-z). seg_mask picks segments (bit j). */
static void lathe(GMesh *m, const Xf *xf, const Ring *r, int nr, int sides, float sx, float sz, float sy,
                  const LatheLook *lk, unsigned int seg_mask, int cap_top, int cap_bottom)
{
    int i, j;
    float ylo = r[0].y, yhi = r[nr - 1].y;
    if (!gm_reserve(m, (nr - 1) * sides * 6 + (cap_top + cap_bottom) * sides * 3)) return;
    for (j = 0; j < sides; j++) {
        float a0 = (j - 0.5f) * 2.0f * PI_F / sides, a1 = (j + 0.5f) * 2.0f * PI_F / sides, ac = j * 2.0f * PI_F / sides;
        float c = cosf(ac);
        int front = lk->planar && c > 0.5f;
        const UVRect *uv = front ? &lk->front : (c < -0.5f ? &lk->back : &lk->side);
        unsigned int tint = front ? lk->front_tint : lk->tint;
        float s0 = sinf(a0), c0 = cosf(a0), s1 = sinf(a1), c1 = cosf(a1);
        if (!(seg_mask & (1u << j))) continue;
        for (i = 0; i + 1 < nr; i++) {
            const Ring *A = &r[i], *B = &r[i + 1];
            V3 p[4], n[4];
            float u[4], v[4];
            int k;
            p[0] = v3(s0 * A->rx * sx, A->y * sy, (A->cz - c0 * A->rz) * sz);
            p[1] = v3(s0 * B->rx * sx, B->y * sy, (B->cz - c0 * B->rz) * sz);
            p[2] = v3(s1 * B->rx * sx, B->y * sy, (B->cz - c1 * B->rz) * sz);
            p[3] = v3(s1 * A->rx * sx, A->y * sy, (A->cz - c1 * A->rz) * sz);
            n[0] = n[1] = v3(s0 / (A->rx + 1e-3f), 0.0f, -c0 / (A->rz + 1e-3f));
            n[2] = n[3] = v3(s1 / (A->rx + 1e-3f), 0.0f, -c1 / (A->rz + 1e-3f));
            /* tilt the normals with the taper so shoulders and the crown catch light */
            {
                float dr = (B->rx - A->rx) / (B->y - A->y + 1e-4f);
                for (k = 0; k < 4; k++) n[k].y = -dr * 4.0f;
            }
            for (k = 0; k < 4; k++) {
                float yy = (k == 1 || k == 2) ? B->y : A->y;
                if (front) {
                    u[k] = uv->u0 + (uv->u1 - uv->u0) * clampf(p[k].x / (lk->front_w * sx) + 0.5f, 0.0f, 1.0f);
                    v[k] = uv->v0 + (uv->v1 - uv->v0) * clampf((lk->front_y1 - yy) / (lk->front_y1 - lk->front_y0), 0.0f, 1.0f);
                } else {
                    u[k] = (k < 2) ? uv->u0 : uv->u1;
                    v[k] = uv->v0 + (uv->v1 - uv->v0) * clampf((yhi - yy) / (yhi - ylo + 1e-4f), 0.0f, 1.0f);
                }
            }
            vtx(m, xf, p[0], n[0], u[0], v[0], tint);
            vtx(m, xf, p[1], n[1], u[1], v[1], tint);
            vtx(m, xf, p[2], n[2], u[2], v[2], tint);
            vtx(m, xf, p[0], n[0], u[0], v[0], tint);
            vtx(m, xf, p[2], n[2], u[2], v[2], tint);
            vtx(m, xf, p[3], n[3], u[3], v[3], tint);
        }
        for (i = 0; i < 2; i++) {
            const Ring *R = i ? &r[nr - 1] : &r[0];
            float cy = R->y * sy, cu = (lk->cap.u0 + lk->cap.u1) * 0.5f, cv = (lk->cap.v0 + lk->cap.v1) * 0.5f;
            V3 nn = v3(0, i ? 1.0f : -1.0f, 0), q0, q1;
            if (!(i ? cap_top : cap_bottom)) continue;
            q0 = v3(s0 * R->rx * sx, cy, (R->cz - c0 * R->rz) * sz);
            q1 = v3(s1 * R->rx * sx, cy, (R->cz - c1 * R->rz) * sz);
            vtx(m, xf, v3(0, cy, R->cz * sz), nn, cu, cv, lk->tint);
            vtx(m, xf, i ? q1 : q0, nn, lk->cap.u0, lk->cap.v0, lk->tint);
            vtx(m, xf, i ? q0 : q1, nn, lk->cap.u1, lk->cap.v1, lk->tint);
        }
    }
}

static void plain_look(LatheLook *lk, UVRect uv, unsigned int tint)
{
    lk->side = lk->front = lk->back = lk->cap = uv;
    lk->planar = 0;
    lk->tint = lk->front_tint = tint;
    lk->front_w = 1.0f;
    lk->front_y0 = 0.0f;
    lk->front_y1 = 1.0f;
}

/* tapered tube between two radii */
static void tube(GMesh *m, const Xf *xf, float y0, float y1, float r0x, float r0z, float r1x, float r1z, float sx,
                 UVRect uv, unsigned int tint, int cap_bottom)
{
    Ring r[2];
    LatheLook lk;
    r[0].y = y1; r[0].rx = r1x; r[0].rz = r1z; r[0].cz = 0.0f;   /* lower end first: rings go up */
    r[1].y = y0; r[1].rx = r0x; r[1].rz = r0z; r[1].cz = 0.0f;
    plain_look(&lk, uv, tint);
    lathe(m, xf, r, 2, 6, sx, sx, 1.0f, &lk, SEG_ALL, 0, cap_bottom);
}

static void head_model(GMesh *m, const Xf *head, const Look *L, int open)
{
    /* chin to crown; cz pushes the jaw a little forward */
    static const Ring HEAD[5] = {
        { 0.015f, 0.058f, 0.062f, -0.018f },
        { 0.075f, 0.097f, 0.106f, -0.004f },
        { 0.155f, 0.113f, 0.122f, 0.0f },
        { 0.235f, 0.112f, 0.122f, 0.004f },
        { 0.29f,  0.085f, 0.095f, 0.008f },
    };
    static const Ring NECK[2] = { { -0.06f, 0.052f, 0.05f, 0.01f }, { 0.04f, 0.05f, 0.048f, 0.01f } };
    const float hs = 0.55f + 0.45f * L->height, ws = 1.0f + (L->width - 1.0f) * 0.35f;
    int tile, fx, fy;
    LatheLook lk;
    UVRect skin = uv_sub(T_WHITE, 8, 8, 16, 16), hair = uv_sub(T_HAIR, SC_HAIR);
    unsigned int ear = gfx_mul(L->skin, 0.92f);
    UVRect su[6];
    int i;

    face_cell(L->face, open, &tile, &fx, &fy);
    plain_look(&lk, skin, L->skin);
    lathe(m, head, NECK, 2, 6, ws, ws, hs, &lk, SEG_ALL, 0, 0);

    lk.front = uv_sub(tile, fx, fy, 32, 32);
    lk.side = uv_sub(tile, fx + 28, fy + 14, 3, 6);
    lk.back = L->hair_style == HAIR_BALD ? uv_sub(tile, fx + 2, fy + 20, 4, 4) : uv_sub(tile, fx + 4, fy + 1, 24, 5);
    lk.cap = L->hair_style == HAIR_BALD ? uv_sub(tile, fx + 4, fy + 1, 24, 2) : uv_sub(tile, fx + 4, fy + 1, 24, 3);
    lk.planar = 1;
    lk.front_w = 0.235f;
    lk.front_y0 = 0.015f * hs;
    lk.front_y1 = 0.31f * hs;
    lk.tint = lk.front_tint = 0xFFFFFFFFu;
    lathe(m, head, HEAD, 5, 8, ws, ws, hs, &lk, SEG_ALL, 1, 0);

    /* nose and ears */
    for (i = 0; i < 6; i++) su[i] = skin;
    gm_box_xf(m, head, v3(-0.017f * ws, 0.105f * hs, -0.142f * ws), v3(0.017f * ws, 0.16f * hs, -0.108f * ws), su,
              gfx_mul(L->skin, 0.97f), BOX_ALL & ~16);
    gm_box_xf(m, head, v3(-0.124f * ws, 0.12f * hs, -0.012f * ws), v3(-0.104f * ws, 0.18f * hs, 0.03f * ws), su, ear, BOX_ALL & ~1);
    gm_box_xf(m, head, v3(0.104f * ws, 0.12f * hs, -0.012f * ws), v3(0.124f * ws, 0.18f * hs, 0.03f * ws), su, ear, BOX_ALL & ~2);

    /* hair volume over the painted hairline */
    if (L->hair_style != HAIR_BALD && L->hat != HAT_SCARF) {
        static const Ring CAP[3] = { { 0.215f, 0.12f, 0.13f, 0.006f }, { 0.275f, 0.108f, 0.118f, 0.008f }, { 0.322f, 0.07f, 0.08f, 0.012f } };
        static const Ring BACK[3] = { { 0.07f, 0.108f, 0.118f, 0.008f }, { 0.155f, 0.12f, 0.13f, 0.004f }, { 0.216f, 0.12f, 0.13f, 0.006f } };
        static const Ring LONG[3] = { { -0.16f, 0.13f, 0.09f, 0.06f }, { 0.05f, 0.125f, 0.12f, 0.02f }, { 0.216f, 0.12f, 0.13f, 0.006f } };
        static const Ring BUZZ[2] = { { 0.2f, 0.116f, 0.126f, 0.005f }, { 0.3f, 0.09f, 0.1f, 0.008f } };
        plain_look(&lk, hair, L->hair);
        if (L->hair_style == HAIR_BUZZ) {
            lathe(m, head, BUZZ, 2, 8, ws, ws, hs, &lk, SEG_ALL, 1, 0);
        } else if (L->hat == HAT_NONE) {
            lathe(m, head, CAP, 3, 8, ws, ws, hs, &lk, SEG_ALL, 1, 0);
            /* back and the back corners: bits 3..5 of 8 segments */
            if (L->hair_style == HAIR_LONG) lathe(m, head, LONG, 3, 8, ws, ws, hs, &lk, 0x7Cu, 0, 0);
            else lathe(m, head, BACK, 3, 8, ws, ws, hs, &lk, 0x38u, 0, 0);
        } else {
            lathe(m, head, BACK, 3, 8, ws, ws, hs, &lk, 0x38u, 0, 0);
        }
    }

    /* hats */
    if (L->hat == HAT_CAP || L->hat == HAT_BEANIE || L->hat == HAT_POLICE) {
        static const Ring CAPH[3] = { { 0.235f, 0.122f, 0.132f, 0.004f }, { 0.3f, 0.118f, 0.128f, 0.006f }, { 0.34f, 0.07f, 0.08f, 0.01f } };
        static const Ring BEANIE[3] = { { 0.2f, 0.125f, 0.135f, 0.004f }, { 0.3f, 0.115f, 0.125f, 0.006f }, { 0.37f, 0.05f, 0.06f, 0.01f } };
        static const Ring POLICE[3] = { { 0.235f, 0.122f, 0.132f, 0.004f }, { 0.31f, 0.13f, 0.14f, 0.004f }, { 0.36f, 0.15f, 0.155f, 0.0f } };
        UVRect hat = L->hat == HAT_BEANIE ? uv_tile(T_CLOTH_KNIT) : uv_sub(T_HAIR, SC_SHIRT);
        plain_look(&lk, hat, L->hat_tint);
        lathe(m, head, L->hat == HAT_BEANIE ? BEANIE : (L->hat == HAT_POLICE ? POLICE : CAPH), 3, 8, ws, ws, hs, &lk,
              SEG_ALL, 1, L->hat == HAT_POLICE);
        if (L->hat != HAT_BEANIE) {
            for (i = 0; i < 6; i++) su[i] = hat;
            gm_box_xf(m, head, v3(-0.1f * ws, 0.235f * hs, -0.23f * ws), v3(0.1f * ws, 0.252f * hs, -0.11f * ws), su,
                      gfx_mul(L->hat_tint, 0.7f), BOX_ALL);
            if (L->hat == HAT_POLICE) {
                plain_look(&lk, uv_sub(T_WHITE, 8, 8, 16, 16), 0xFF3030B0u); /* red band */
                {
                    static const Ring BAND[2] = { { 0.24f, 0.125f, 0.135f, 0.004f }, { 0.275f, 0.127f, 0.137f, 0.004f } };
                    lathe(m, head, BAND, 2, 8, ws, ws, hs, &lk, SEG_ALL, 0, 0);
                }
            }
        }
    } else if (L->hat == HAT_SCARF) {
        /* headscarf: over the crown and the sides, knotted under the chin */
        static const Ring SC[4] = { { 0.02f, 0.11f, 0.12f, 0.0f }, { 0.12f, 0.126f, 0.136f, 0.0f },
                                    { 0.25f, 0.126f, 0.136f, 0.004f }, { 0.33f, 0.075f, 0.085f, 0.01f } };
        UVRect sc = uv_sub(T_HAIR, SC_SCARF);
        plain_look(&lk, sc, L->hat_tint);
        lathe(m, head, SC, 4, 8, ws, ws, hs, &lk, 0xFEu, 1, 0);           /* not the face segment */
        lathe(m, head, SC + 2, 2, 8, ws, ws, hs, &lk, 0x01u, 0, 0);       /* forehead band */
        for (i = 0; i < 6; i++) su[i] = sc;
        gm_box_xf(m, head, v3(-0.05f * ws, -0.03f * hs, -0.07f * ws), v3(0.05f * ws, 0.03f * hs, -0.02f * ws), su, L->hat_tint, BOX_ALL);
    }
}

static void mesh_detailed(GMesh *m, const Ped *p)
{
    const Look *L = &LOOKS[p->who];
    Pose pose;
    Xf bones[BONES];
    LatheLook lk;
    UVRect top_side = top_side_uv(L), pants = cloth_uv(L->pants_tile), skin = uv_sub(T_WHITE, 8, 8, 16, 16);
    const float w = L->width, lw = 1.0f + (L->width - 1.0f) * 0.6f, h = L->height;
    int open = p->anim.mouth > 0.35f && LOOKS[p->who].face < FACE_TALKING, b, k;
    unsigned int legs = L->skirt ? gfx_mul(L->skin, 0.8f) : L->pants_tint;
    UVRect leg_uv = L->skirt ? skin : pants;

    anim_pose(&p->anim, &pose);
    ped_bones(p, &pose, bones);

    /* hips */
    {
        static const Ring PELVIS[3] = { { -0.12f, 0.15f, 0.1f, 0.0f }, { 0.0f, 0.172f, 0.112f, 0.0f }, { 0.13f, 0.162f, 0.105f, 0.0f } };
        plain_look(&lk, pants, L->pants_tint);
        lathe(m, &bones[B_PELVIS], PELVIS, 3, 8, w, w, h, &lk, SEG_ALL, 0, 1);
    }
    /* torso: waist, chest, shoulders, collar; the outfit's front on the chest */
    {
        Ring T[5] = { { 0.0f, 0.162f, 0.104f, 0.0f }, { 0.17f, 0.178f, 0.118f, -0.004f }, { 0.33f, 0.2f, 0.125f, -0.006f },
                      { 0.46f, 0.215f, 0.108f, 0.0f }, { 0.535f, 0.11f, 0.075f, 0.005f } };
        if (L->skirt || L->hair_style == HAIR_LONG) {
            T[1].rx = 0.165f;   /* narrower waist, fuller chest */
            T[2].rz = 0.138f;
            T[2].cz = -0.014f;
        }
        lk.side = lk.back = lk.cap = top_side;
        lk.front = top_front_uv(L);
        lk.planar = 1;
        lk.front_w = 0.36f;
        lk.front_y0 = 0.0f;
        lk.front_y1 = 0.54f * h;
        lk.tint = lk.front_tint = L->top_tint;
        lathe(m, &bones[B_SPINE], T, 5, 8, w, w, h, &lk, SEG_ALL, 1, 0);
    }
    if (L->skirt) {
        static const Ring SK[2] = { { -0.5f, 0.25f, 0.2f, 0.0f }, { 0.06f, 0.18f, 0.12f, 0.0f } };
        plain_look(&lk, pants, L->pants_tint);
        lathe(m, &bones[B_PELVIS], SK, 2, 8, w, w, h, &lk, SEG_ALL, 0, 0);
    }
    /* legs */
    for (b = B_THIGH_L; b <= B_THIGH_R; b += 3) {
        UVRect shoe[6];
        tube(m, &bones[b], 0.05f * h, -0.45f * h, 0.085f, 0.09f, 0.062f, 0.066f, w, leg_uv, legs, 0);
        {
            /* shin with a calf */
            static const Ring SH[3] = { { -0.44f, 0.045f, 0.05f, 0.0f }, { -0.2f, 0.058f, 0.068f, 0.008f }, { 0.03f, 0.064f, 0.066f, 0.0f } };
            Ring r[3];
            for (k = 0; k < 3; k++) { r[k] = SH[k]; r[k].y *= h; }
            plain_look(&lk, leg_uv, legs);
            lathe(m, &bones[b + 1], r, 3, 6, w, w, 1.0f, &lk, SEG_ALL, 0, 0);
        }
        for (k = 0; k < 6; k++) shoe[k] = uv_sub(T_HAIR, SC_SHOE);
        gm_box_xf(m, &bones[b + 2], v3(-0.055f * w, -0.08f * h, -0.17f * w), v3(0.055f * w, 0.0f, 0.06f * w), shoe, L->shoes, BOX_ALL);
        gm_box_xf(m, &bones[b + 2], v3(-0.05f * w, -0.08f * h, -0.2f * w), v3(0.05f * w, -0.03f * h, -0.17f * w), shoe,
                  gfx_mul(L->shoes, 0.9f), BOX_ALL & ~16);
    }
    /* arms */
    for (b = B_UARM_L; b <= B_UARM_R; b += 3) {
        UVRect sleeve = top_side;
        unsigned int fore = L->top_tint;
        UVRect fore_uv = sleeve;
        if (L->skirt && L->top_tile == T_CLOTH_DRESS) {
            fore = L->skin;
            fore_uv = skin;
        }
        tube(m, &bones[b], 0.05f * h, -0.3f * h, 0.064f, 0.064f, 0.05f, 0.052f, lw, sleeve, L->top_tint, 0);
        tube(m, &bones[b + 1], 0.03f * h, -0.27f * h, 0.051f, 0.051f, 0.04f, 0.038f, lw, fore_uv, fore, 0);
        {
            /* hand: a flattened rounded block, a fist when fighting */
            static const Ring HAND[3] = { { -0.1f, 0.03f, 0.022f, 0.0f }, { -0.05f, 0.042f, 0.03f, 0.0f }, { 0.01f, 0.036f, 0.032f, 0.0f } };
            Ring r[3];
            for (k = 0; k < 3; k++) { r[k] = HAND[k]; r[k].y *= h; }
            plain_look(&lk, skin, L->skin);
            lathe(m, &bones[b + 2], r, 3, 6, lw, lw, 1.0f, &lk, SEG_ALL, 0, 1);
        }
    }
    head_model(m, &bones[B_HEAD], L, open);
}

void ped_append_mesh_lod(GMesh *m, const Ped *p, int detail)
{
    if (detail) mesh_detailed(m, p);
    else mesh_boxes(m, p);
}

void ped_append_mesh(GMesh *m, const Ped *p)
{
    mesh_detailed(m, p);
}

/* ------------------------------------------------------------- movement */

void ped_init(Ped *p, int who, V3 pos, float yaw)
{
    memset(p, 0, sizeof(*p));
    p->active = 1;
    p->who = who;
    p->pos = pos;
    p->yaw = yaw;
    p->on_ground = 1;
    p->walk_speed = 1.4f;
    p->punch_t = -1.0f;
    p->anim.t = (float)(who * 1.7);
}

#define PUNCH_OUT 0.11f    /* fist reaches the target */
#define PUNCH_HOLD 0.05f
#define PUNCH_BACK 0.2f

int ped_punch(Ped *p)
{
    if (p->punch_t >= 0.0f || p->knock_t > 0.0f || p->anim.sit > 0.5f) return 0;
    p->punch_t = 0.0f;
    p->anim.punch_arm ^= 1;
    return 1;
}

void ped_take_hit(Ped *p, V3 from, float strength)
{
    V3 d = v3_sub(p->pos, from);
    float l = sqrtf(d.x * d.x + d.z * d.z);
    if (l < 1e-3f) d = yaw_fwd(p->yaw + PI_F), l = 1.0f;
    p->vel.x += d.x / l * 2.2f * strength;
    p->vel.z += d.z / l * 2.2f * strength;
    p->anim.hurt = 1.0f;   /* a punch already on its way still lands */
}

void ped_face(Ped *p, float yaw, float dt)
{
    float d = wrap_pi(yaw - p->yaw);
    p->yaw_vel = clampf(d * 8.0f, -7.0f, 7.0f);
    p->yaw = wrap_pi(p->yaw + p->yaw_vel * dt);
}

static void collide_boxes(Ped *p)
{
    const Box *hits[16];
    const float r = 0.3f;
    int n = city_boxes(p->pos.x - 0.6f, p->pos.z - 0.6f, p->pos.x + 0.6f, p->pos.z + 0.6f, hits, 16), k;
    for (k = 0; k < n; k++) {
        const Box *b = hits[k];
        float cx, cz, dx, dz, d2;
        if (b->y1 < p->pos.y + 0.45f || b->y0 > p->pos.y + 1.7f) continue;
        cx = clampf(p->pos.x, b->x0, b->x1);
        cz = clampf(p->pos.z, b->z0, b->z1);
        dx = p->pos.x - cx;
        dz = p->pos.z - cz;
        d2 = dx * dx + dz * dz;
        if (d2 >= r * r) continue;
        if (d2 > 1e-8f) {
            float d = sqrtf(d2);
            p->pos.x = cx + dx / d * r;
            p->pos.z = cz + dz / d * r;
            {
                float vn = (p->vel.x * dx + p->vel.z * dz) / d;
                if (vn < 0) {
                    p->vel.x -= vn * dx / d;
                    p->vel.z -= vn * dz / d;
                }
            }
        } else {
            /* centre inside the box: push out along the shallowest side */
            float l = p->pos.x - b->x0, rr = b->x1 - p->pos.x, f = p->pos.z - b->z0, bk = b->z1 - p->pos.z;
            float mn = fminf(fminf(l, rr), fminf(f, bk));
            if (mn == l) p->pos.x = b->x0 - r;
            else if (mn == rr) p->pos.x = b->x1 + r;
            else if (mn == f) p->pos.z = b->z0 - r;
            else p->pos.z = b->z1 + r;
        }
    }
}

int ped_update(Ped *p, V3 move, int sprint, int jump, float dt)
{
    AnimState *a = &p->anim;
    const Look *L = &LOOKS[p->who];
    V3 hv = v3(p->vel.x, 0, p->vel.z), vd;
    float mlen = sqrtf(move.x * move.x + move.z * move.z), target = 0.0f, spd, g;
    int ev = 0;

    a->t += dt;
    /* punching: out fast, a short hold, back to the guard */
    if (p->punch_t >= 0.0f) {
        float before = p->punch_t;
        p->punch_t += dt;
        if (before < PUNCH_OUT && p->punch_t >= PUNCH_OUT) ev |= PED_EV_PUNCH;
        if (p->punch_t < PUNCH_OUT) a->punch = smooth01(p->punch_t / PUNCH_OUT);
        else if (p->punch_t < PUNCH_OUT + PUNCH_HOLD) a->punch = 1.0f;
        else a->punch = 1.0f - smooth01((p->punch_t - PUNCH_OUT - PUNCH_HOLD) / PUNCH_BACK);
        if (p->punch_t >= PUNCH_OUT + PUNCH_HOLD + PUNCH_BACK) {
            p->punch_t = -1.0f;
            a->punch = 0.0f;
        }
        mlen *= 0.3f;   /* plant the feet */
    }
    a->fight = approach(a->fight, p->fight_mode && p->knock_t <= 0.0f ? 1.0f : 0.0f, dt * 5.0f);
    a->hurt = approach(a->hurt, 0.0f, dt * 2.5f);
    if (p->knock_t > 0.0f) {
        p->knock_t -= dt;
        a->knock = approach(a->knock, p->knock_t > 0.7f ? 1.0f : 0.0f, dt * (p->knock_t > 0.7f ? 4.0f : 1.6f));
        mlen = 0.0f;
        jump = 0;
    } else {
        a->knock = approach(a->knock, 0.0f, dt * 1.6f);
    }
    if (a->sit > 0.5f) mlen = 0.0f;

    if (p->has_goal) {
        V3 d = v3_sub(p->goal, p->pos);
        float dist = sqrtf(d.x * d.x + d.z * d.z);
        if (dist < 0.2f) {
            p->has_goal = 0;
            mlen = 0.0f;
        } else {
            move = v3(d.x / dist, 0, d.z / dist);
            mlen = 1.0f;
            target = fminf(p->walk_speed, 0.6f + dist * 1.5f);
        }
    }
    if (mlen > 1.0f) {
        move = v3_scale(move, 1.0f / mlen);
        mlen = 1.0f;
    }
    if (!p->has_goal && mlen > 0.05f)
        target = sprint ? 6.4f : (mlen < 0.65f ? 1.65f * mlen / 0.65f : 4.3f);
    vd = mlen > 0.05f ? v3_scale(move, target / mlen) : v3(0, 0, 0);

    /* acceleration with momentum */
    {
        V3 dv = v3_sub(vd, hv);
        float dl = sqrtf(dv.x * dv.x + dv.z * dv.z);
        float acc = p->on_ground ? (v3_len(vd) > v3_len(hv) ? 10.0f : 14.0f) : 2.5f;
        float fwd_acc;
        if (dl > acc * dt) dv = v3_scale(dv, acc * dt / dl);
        hv = v3_add(hv, dv);
        fwd_acc = v3_dot(v3_scale(dv, 1.0f / dt), yaw_fwd(p->yaw));
        a->accel_lean = approach(a->accel_lean, clampf(-fwd_acc * 0.006f, -0.06f, 0.04f), dt * 1.0f);
    }

    /* facing follows the movement; lean into turns */
    spd = sqrtf(hv.x * hv.x + hv.z * hv.z);
    if (spd > 0.25f && a->knock < 0.1f && !(p->fight_mode && p->face_goal_yaw)) {
        float want = yaw_of(hv.x, hv.z), d = wrap_pi(want - p->yaw);
        float rate = clampf(d * 10.0f, -9.0f, 9.0f);
        p->yaw_vel = approach(p->yaw_vel, rate, dt * 60.0f);
    } else if (p->face_goal_yaw) {
        float d = wrap_pi(p->goal_yaw - p->yaw);
        p->yaw_vel = approach(p->yaw_vel, clampf(d * 6.0f, -5.0f, 5.0f), dt * 40.0f);
        if (fabsf(d) < 0.02f && !p->fight_mode) p->face_goal_yaw = 0;
    } else {
        p->yaw_vel = approach(p->yaw_vel, 0.0f, dt * 40.0f);
    }
    p->yaw = wrap_pi(p->yaw + p->yaw_vel * dt);
    /* lean into turns only when running fast, and only a little */
    a->lean = approach(a->lean, clampf(p->yaw_vel * (spd > 3.0f ? spd - 3.0f : 0.0f) * 0.012f, -0.07f, 0.07f), dt * 1.5f);

    /* jumping: a short crouch before take-off */
    if (jump && p->on_ground && p->jump_t <= 0.0f && a->knock < 0.1f) p->jump_t = 0.1f;
    if (p->jump_t > 0.0f) {
        p->jump_t -= dt;
        a->crouch = approach(a->crouch, 0.45f, dt * 8.0f);
        if (p->jump_t <= 0.0f) {
            p->vel.y = 5.4f;
            p->on_ground = 0;
            ev |= PED_EV_JUMP;
        }
    } else {
        a->crouch = approach(a->crouch, 0.0f, dt * 5.0f);
    }

    if (!p->on_ground) p->vel.y -= 20.0f * dt;
    p->pos.x += hv.x * dt;
    p->pos.z += hv.z * dt;
    p->pos.y += p->vel.y * dt;
    p->vel.x = hv.x;
    p->vel.z = hv.z;
    collide_boxes(p);

    /* ground, kerbs (step up/down) */
    g = city_ground(p->pos.x, p->pos.z);
    if (p->pos.y <= g + 0.001f && p->vel.y <= 0.0f) {
        if (!p->on_ground) {
            a->land = clampf(-p->vel.y / 7.0f, 0.15f, 1.0f);
            ev |= PED_EV_LAND;
        }
        p->pos.y = g;
        p->vel.y = 0.0f;
        p->on_ground = 1;
    } else if (p->on_ground && p->pos.y - g < 0.4f) {
        p->pos.y = g;
    } else if (p->on_ground) {
        p->on_ground = 0;
    }

    /* animation parameters */
    hv = v3(p->vel.x, 0, p->vel.z);
    spd = sqrtf(hv.x * hv.x + hv.z * hv.z);
    a->move = approach(a->move, clampf(spd / 1.1f, 0.0f, 1.0f), dt * 5.0f);
    a->run = approach(a->run, clampf((spd - 1.9f) / 2.4f, 0.0f, 1.0f), dt * 3.5f);
    a->air = approach(a->air, p->on_ground ? 0.0f : 1.0f, dt * (p->on_ground ? 12.0f : 6.0f));
    a->vy = p->vel.y;
    a->land = approach(a->land, 0.0f, dt * 3.0f);
    if (p->on_ground && spd > 0.05f) {
        float stride = lerpf(1.45f, 2.7f, a->run) * L->height;
        float before = a->phase;
        a->phase += spd * dt * 2.0f * PI_F / stride;
        if ((int)(before / PI_F) != (int)(a->phase / PI_F)) ev |= PED_EV_STEP;
        if (a->phase > 1000.0f) a->phase -= 2.0f * PI_F * 150.0f;
    }
    /* look at a point */
    if (p->has_look_at) {
        V3 d = v3_sub(p->look_at, p->pos);
        float hy = wrap_pi(yaw_of(d.x, d.z) - p->yaw);
        float hp = atan2f(d.y - 1.6f * L->height, sqrtf(d.x * d.x + d.z * d.z));
        a->head_yaw = approach(a->head_yaw, clampf(hy, -1.1f, 1.1f), dt * 3.0f);
        a->head_pitch = approach(a->head_pitch, clampf(hp, -0.5f, 0.5f), dt * 3.0f);
    } else {
        a->head_yaw = approach(a->head_yaw, 0.0f, dt * 2.0f);
        a->head_pitch = approach(a->head_pitch, 0.0f, dt * 2.0f);
    }
    a->gesture_w = approach(a->gesture_w, a->gesture != GEST_NONE ? 1.0f : 0.0f, dt * 3.0f);
    a->talk = approach(a->talk, 0.0f, dt * 1.5f);
    return ev;
}
