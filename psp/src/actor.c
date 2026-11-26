/* Characters: 15-bone skeleton with procedural animation (idle breathing,
 * distance-locked walk/run cycles, jumps with anticipation and landing,
 * turn lean, gestures, driving, knock-down), movement physics, meshes. */
#include "actor.h"
#include "city.h"
#include "texgen.h"

#include <string.h>

#define WHITE 0xFFFFFFFFu

/*  name       face          h      w     top tile         fx fw  top tint     pants tile        pants tint   skin         hair         shoes        hat skirt pitch formant */
const Look LOOKS[CH_COUNT] = {
    { "Андрій",    FACE_ANDRIY, 1.00f, 1.00f, T_CLOTH_ARMY,     0, 64, 0xFFFFFFFFu, T_CLOTH_DENIM,    0xFFFFFFFFu, 0xFF8CB0DCu, 0xFF203048u, 0xFF303438u, 0, 0, 118.0f, 1.00f },
    { "Тарас",     FACE_TARAS,  0.98f, 1.06f, T_CLOTH_OVERALLS, 0, 64, 0xFFFFFFFFu, T_CLOTH_OVERALLS, 0xFFFFFFFFu, 0xFF7CA0D0u, 0xFF283850u, 0xFF2A3A50u, 1, 0, 102.0f, 0.95f },
    { "Тітка Галя", FACE_GALYA, 0.93f, 1.12f, T_CLOTH_DRESS,    0, 64, 0xFFFFFFFFu, T_CLOTH_DRESS,    0xFFFFFFFFu, 0xFF94B8E4u, 0xFF4060A0u, 0xFF203048u, 2, 1, 215.0f, 1.18f },
    { "Кабан",     FACE_KABAN,  1.05f, 1.25f, T_CLOTH_BLAZER,   0, 32, 0xFFFFFFFFu, T_CLOTH_SHIRT,    0xFF262626u, 0xFF7098D0u, 0xFF303030u, 0xFF181818u, 0, 0,  86.0f, 0.88f },
    { "Продавець", FACE_SELLER, 0.97f, 0.98f, T_CLOTH_SHIRT,    0, 64, 0xFFB09070u, T_CLOTH_DENIM,    0xFFFFFFFFu, 0xFF88ACD8u, 0xFF505868u, 0xFF404448u, 0, 0, 135.0f, 1.02f },
    { "Браток",    FACE_THUG,   1.02f, 1.10f, T_CLOTH_LEATHER,  0, 64, 0xFFFFFFFFu, T_CLOTH_TRACK,    0xFF3A3A3Au, 0xFF80A4D4u, 0xFF181818u, 0xFFE8E8E8u, 0, 0,  96.0f, 0.92f },
};

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
    set(o, B_PELVIS, 0.0f, -0.13f * sp * (1.0f - 0.3f * run), 0.045f * cosf(ph));
    set(o, B_SPINE, -lerpf(0.05f, 0.26f, run) + a->accel_lean, 0.17f * sp, -0.5f * 0.045f * cosf(ph));
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

/* face sub-rect in pixels of the character's face cell */
static UVRect face_sub(int face, int open, int x, int y, int w, int h)
{
    int tile = T_FACES_A + face / 2;
    return uv_sub(tile, (open ? 32 : 0) + x, (face % 2) * 32 + y, w, h);
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

void ped_append_mesh(GMesh *m, const Ped *p)
{
    const Look *L = &LOOKS[p->who];
    Pose pose;
    Xf bones[BONES];
    UVRect uv[6];
    UVRect top_front = uv_sub(L->top_tile, L->top_front_x, 0, L->top_front_w, 64);
    UVRect top_side = L->top_front_w < 64 ? uv_sub(L->top_tile, 32, 0, 32, 64)
                    : L->top_tile == T_CLOTH_ARMY ? uv_sub(L->top_tile, 0, 26, 24, 38) : uv_tile(L->top_tile);
    UVRect skin = uv_sub(T_WHITE, 8, 8, 16, 16);
    int open = p->anim.mouth > 0.35f, b;

    anim_pose(&p->anim, &pose);
    ped_bones(p, &pose, bones);

    /* pelvis and legs */
    all6(uv, uv_tile(L->pants_tile));
    part(m, &bones[B_PELVIS], L, B_PELVIS, uv, L->pants_tint, 0.0f);
    for (b = B_THIGH_L; b <= B_THIGH_R; b += 3) {
        if (L->skirt) {
            all6(uv, uv_tile(L->top_tile));
            part(m, &bones[b], L, b, uv, L->top_tint, 0.03f);
            all6(uv, skin);
            part(m, &bones[b + 1], L, b + 1, uv, gfx_mul(L->skin, 0.85f), 0.0f);
        } else {
            all6(uv, uv_tile(L->pants_tile));
            part(m, &bones[b], L, b, uv, L->pants_tint, 0.0f);
            part(m, &bones[b + 1], L, b + 1, uv, L->pants_tint, 0.0f);
        }
        all6(uv, uv_tile(T_SHOE));
        part(m, &bones[b + 2], L, b + 2, uv, L->shoes, 0.0f);
    }
    if (L->skirt) {
        /* skirt around the hips */
        all6(uv, uv_tile(L->top_tile));
        part(m, &bones[B_PELVIS], L, B_PELVIS, uv, L->top_tint, 0.04f);
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
        if (L->hat == 1) {
            all6(uv, uv_tile(T_CLOTH_SHIRT));
            gm_box_xf(m, &bones[B_HEAD], v3(-0.12f, 0.25f, -0.125f), v3(0.12f, 0.33f, 0.125f), uv, 0xFF5A3A2Au, BOX_ALL);
            gm_box_xf(m, &bones[B_HEAD], v3(-0.11f, 0.25f, -0.23f), v3(0.11f, 0.27f, -0.12f), uv, 0xFF4A3020u, BOX_ALL);
        } else if (L->hat == 2) {
            all6(uv, uv_tile(T_HEADSCARF));
            gm_box_xf(m, &bones[B_HEAD], v3(-0.125f, 0.12f, -0.10f), v3(0.125f, 0.315f, 0.135f), uv, WHITE, BOX_ALL & ~32);
            gm_box_xf(m, &bones[B_HEAD], v3(-0.06f, -0.04f, 0.10f), v3(0.06f, 0.14f, 0.15f), uv, WHITE, BOX_ALL);
        } else if (L->face != FACE_KABAN) {
            all6(uv, uv_tile(T_HAIR));
            gm_box_xf(m, &bones[B_HEAD], v3(-0.115f, 0.265f, -0.125f), v3(0.115f, 0.305f, 0.125f), uv, L->hair, BOX_ALL);
        }
    }
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
    p->anim.t = (float)(who * 1.7);
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
        a->accel_lean = approach(a->accel_lean, clampf(-fwd_acc * 0.02f, -0.18f, 0.12f), dt * 1.5f);
    }

    /* facing follows the movement; lean into turns */
    spd = sqrtf(hv.x * hv.x + hv.z * hv.z);
    if (spd > 0.25f && a->knock < 0.1f) {
        float want = yaw_of(hv.x, hv.z), d = wrap_pi(want - p->yaw);
        float rate = clampf(d * 10.0f, -9.0f, 9.0f);
        p->yaw_vel = approach(p->yaw_vel, rate, dt * 60.0f);
    } else if (p->face_goal_yaw) {
        float d = wrap_pi(p->goal_yaw - p->yaw);
        p->yaw_vel = approach(p->yaw_vel, clampf(d * 6.0f, -5.0f, 5.0f), dt * 40.0f);
        if (fabsf(d) < 0.02f) p->face_goal_yaw = 0;
    } else {
        p->yaw_vel = approach(p->yaw_vel, 0.0f, dt * 40.0f);
    }
    p->yaw = wrap_pi(p->yaw + p->yaw_vel * dt);
    a->lean = approach(a->lean, clampf(p->yaw_vel * spd * 0.028f, -0.28f, 0.28f), dt * 2.5f);

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
