#include "cutscene.h"
#include "city.h"

#include <stdio.h>
#include <string.h>

enum {
    C_CAM = 1, C_AUTOCAM, C_PLACE, C_WALK, C_FACE, C_SAY, C_GEST, C_WAIT, C_FADE, C_TITLE, C_REQ, C_END
};

static const char *SCENE_NAMES[CS_COUNT] = { "", "intro", "garage", "market", "kiosk", "fail", "kaban" };

/* who each actor looks at (-1: nobody) */
static int look_target[N_ACTORS];

/* ---------------------------------------------------------- script API */

static CsCmd *push(Cutscene *c, int type)
{
    CsCmd *k = &c->cmd[c->n < CS_MAX_CMDS ? c->n++ : CS_MAX_CMDS - 1];
    memset(k, 0, sizeof(*k));
    k->type = type;
    k->other = -1;
    return k;
}

static void cam(Cutscene *c, V3 eye, V3 tgt, V3 eye2, V3 tgt2, float dur)
{
    CsCmd *k = push(c, C_CAM);
    k->p = eye;
    k->q = tgt;
    k->r = eye2;
    k->s = tgt2;
    k->t = dur;
}

static void cam_still(Cutscene *c, V3 eye, V3 tgt) { cam(c, eye, tgt, eye, tgt, 0.0f); }

static void autocam(Cutscene *c) { push(c, C_AUTOCAM); }

static void place(Cutscene *c, int a, V3 p, float yaw)
{
    CsCmd *k = push(c, C_PLACE);
    k->actor = a;
    k->p = p;
    k->t = yaw;
}

static void walk(Cutscene *c, int a, V3 p, int wait_arrival)
{
    CsCmd *k = push(c, C_WALK);
    k->actor = a;
    k->p = p;
    k->gest = wait_arrival;
}

static void face(Cutscene *c, int a, int other)
{
    CsCmd *k = push(c, C_FACE);
    k->actor = a;
    k->other = other;
}

static void say(Cutscene *c, int a, int listener, int g, const char *text)
{
    CsCmd *k = push(c, C_SAY);
    k->actor = a;
    k->other = listener;
    k->gest = g;
    k->text = text;
}

static void gest(Cutscene *c, int a, int g)
{
    CsCmd *k = push(c, C_GEST);
    k->actor = a;
    k->gest = g;
}

static void wait(Cutscene *c, float t) { push(c, C_WAIT)->t = t; }

static void fade(Cutscene *c, int to_black, float t)
{
    CsCmd *k = push(c, C_FADE);
    k->gest = to_black;
    k->t = t;
}

static void title(Cutscene *c, const char *a, const char *b, float t)
{
    CsCmd *k = push(c, C_TITLE);
    k->text = a;
    k->text2 = b;
    k->t = t;
}

static void request(Cutscene *c, int what) { push(c, C_REQ)->gest = what; }

static float yaw_to(V3 from, V3 to) { return yaw_of(to.x - from.x, to.z - from.z); }

/* --------------------------------------------------------------- scenes */

static void scene_intro(Cutscene *c)
{
    V3 door = v3(26.0f, WALK_H, 40.4f), out = v3(26.0f, WALK_H, 44.0f);
    place(c, A_ANDRIY, door, PI_F);
    c->fade = 1.0f;
    cam(c, v3(-40, 48, 150), v3(20, 0, 20), v3(70, 34, 120), v3(30, 4, 40), 9.0f);
    fade(c, 0, 2.0f);
    title(c, "Соняшниківськ", "Літо 1996-го", 4.0f);
    wait(c, 2.5f);
    cam(c, v3(33.5f, 1.4f, 50.5f), v3(26, 1.5f, 41.5f), v3(32.0f, 1.6f, 49.0f), v3(26, 1.5f, 42.5f), 5.0f);
    walk(c, A_ANDRIY, out, 1);
    say(c, A_ANDRIY, -1, GEST_NONE, "Ну здрастуй, рідне місто...");
    cam_still(c, v3(27.6f, 1.72f, 46.6f), v3(25.9f, 1.55f, 44.0f));
    say(c, A_ANDRIY, -1, GEST_SHRUG, "Два роки тебе не бачив. Нічого не змінилося, хіба що ларьків побільшало.");
    say(c, A_ANDRIY, -1, GEST_NONE, "Треба знайти Тараса. Він, мабуть, як завжди, у своєму гаражі.");
}

static void scene_garage(Cutscene *c)
{
    V3 me = POI_GARAGE, taras = POI_TARAS;
    place(c, A_ANDRIY, v3(me.x, me.y, me.z - 0.4f), yaw_to(me, taras));
    place(c, A_TARAS, taras, PI_F);
    cam_still(c, v3(91.5f, 1.9f, 15.0f), v3(86.4f, 1.1f, 21.6f));
    say(c, A_ANDRIY, -1, GEST_NONE, "Тарасе! Ти тут?");
    face(c, A_TARAS, A_ANDRIY);
    wait(c, 0.4f);
    autocam(c);
    say(c, A_TARAS, A_ANDRIY, GEST_WAVE, "Андрюха! Живий! Ну ти й вимахав у тій армії!");
    walk(c, A_TARAS, v3(taras.x + 0.2f, taras.y, taras.z - 1.1f), 0);
    say(c, A_ANDRIY, A_TARAS, GEST_SHRUG, "Здоров, брате. Що тут у вас робиться?");
    say(c, A_TARAS, A_ANDRIY, GEST_EXPLAIN, "Та що... Завод стоїть, зарплату пів року не платять.");
    say(c, A_TARAS, A_ANDRIY, GEST_EXPLAIN, "Я ось людям машини ремонтую, тим і живу.");
    say(c, A_TARAS, A_ANDRIY, GEST_POINT, "Тітка Галя з ринку просила допомогти. Бери мою Ластівку.");
    cam_still(c, v3(99.5f, 1.5f, 14.2f), v3(95.0f, 0.8f, 19.0f));
    say(c, A_TARAS, -1, GEST_EXPLAIN, "Тільки обережно, гальма там... ну, ти зрозумієш.");
    autocam(c);
    say(c, A_ANDRIY, A_TARAS, GEST_NONE, "Розберуся.");
}

static void scene_market(Cutscene *c)
{
    V3 me = POI_MARKET, galya = POI_GALYA;
    place(c, A_ANDRIY, me, PI_F);
    place(c, A_GALYA, galya, 0.0f);
    cam_still(c, v3(-87.6f, 1.75f, 93.2f), v3(-91.0f, 1.3f, 96.6f));
    say(c, A_GALYA, A_ANDRIY, GEST_WAVE, "О, Тарасів братик! Ти диви, який козак виріс!");
    autocam(c);
    say(c, A_ANDRIY, A_GALYA, GEST_NONE, "Добрий день, тітко Галю.");
    say(c, A_GALYA, A_ANDRIY, GEST_EXPLAIN, "Слухай, синку. Відвези пакунок до ларька з пивом, що за школою.");
    say(c, A_GALYA, A_ANDRIY, GEST_EXPLAIN, "Тільки швиденько, там чекати не люблять. І не питай, що всередині.");
    say(c, A_ANDRIY, A_GALYA, GEST_SHRUG, "А що всередині?");
    say(c, A_GALYA, A_ANDRIY, GEST_HIPS, "Я ж сказала — не питай!");
}

static void scene_kiosk(Cutscene *c)
{
    V3 me = POI_KIOSK, seller = POI_SELLER;
    place(c, A_ANDRIY, me, yaw_to(me, seller));
    place(c, A_SELLER, seller, yaw_to(seller, me));
    cam_still(c, v3(161.2f, 1.8f, -98.0f), v3(166.5f, 1.2f, -93.2f));
    say(c, A_SELLER, A_ANDRIY, GEST_EXPLAIN, "Від Галі? Нарешті. Тримай п'ятсот тисяч за доставку.");
    autocam(c);
    say(c, A_SELLER, A_ANDRIY, GEST_POINT, "І ще, хлопче... Тобою вже цікавляться люди Кабана.");
    request(c, CS_REQ_CHASER);
    cam(c, v3(170.0f, 1.6f, -112.0f), v3(186.0f, 0.9f, -122.5f), v3(170.5f, 1.5f, -112.5f), v3(178.0f, 0.9f, -122.5f), 3.0f);
    wait(c, 2.6f);
    autocam(c);
    say(c, A_ANDRIY, A_SELLER, GEST_NONE, "Ще чого не вистачало...");
}

static void scene_fail(Cutscene *c)
{
    say(c, A_ANDRIY, -1, GEST_SHRUG, "Не встиг... Галя буде не в захваті. Треба спробувати ще раз, і швидше.");
}

static void scene_kaban(Cutscene *c)
{
    V3 me = POI_KABAN, kaban = POI_KABAN_NPC;
    request(c, CS_REQ_KABAN_SETUP);
    place(c, A_ANDRIY, me, yaw_to(me, kaban));
    place(c, A_KABAN, kaban, yaw_to(kaban, me));
    place(c, A_THUG1, v3(kaban.x - 2.3f, kaban.y, kaban.z - 0.9f), yaw_to(v3(kaban.x - 2.3f, 0, kaban.z - 0.9f), me));
    place(c, A_THUG2, v3(kaban.x + 2.4f, kaban.y, kaban.z - 1.2f), yaw_to(v3(kaban.x + 2.4f, 0, kaban.z - 1.2f), me));
    gest(c, A_KABAN, GEST_HIPS);
    gest(c, A_THUG1, GEST_CROSSED);
    gest(c, A_THUG2, GEST_CROSSED);
    cam(c, v3(-33.0f, 2.4f, -73.0f), v3(-24.5f, 1.2f, -84.0f), v3(-31.0f, 2.0f, -75.0f), v3(-25.5f, 1.3f, -83.5f), 4.0f);
    say(c, A_KABAN, A_ANDRIY, GEST_HIPS, "Так от ти який, солдатик.");
    autocam(c);
    say(c, A_KABAN, A_ANDRIY, GEST_EXPLAIN, "Швидко їздиш. Мої хлопці аж засмутилися.");
    say(c, A_ANDRIY, A_KABAN, GEST_NONE, "То нехай вчаться кермувати.");
    say(c, A_THUG1, A_ANDRIY, GEST_POINT, "Слиш, ти...");
    say(c, A_KABAN, A_THUG1, GEST_HIPS, "Тихо.");
    say(c, A_KABAN, A_ANDRIY, GEST_EXPLAIN, "Мені такі люди потрібні. Робота чиста, гроші — в доларах.");
    say(c, A_ANDRIY, A_KABAN, GEST_NONE, "Я подумаю.");
    say(c, A_KABAN, A_ANDRIY, GEST_POINT, "Думай. Тільки недовго. У цьому місті довго думати — шкідливо.");
    fade(c, 1, 1.6f);
    title(c, "Кінець першого розділу", "Далі буде...", 4.5f);
    fade(c, 0, 1.2f);
}

/* ------------------------------------------------------------- runtime */

const char *cs_speaker_name(int actor)
{
    static const int WHO[N_ACTORS] = { CH_ANDRIY, CH_TARAS, CH_GALYA, CH_KABAN, CH_SELLER, CH_THUG, CH_THUG };
    return actor >= 0 && actor < N_ACTORS ? LOOKS[WHO[actor]].name : "";
}

void cs_start(Cutscene *c, int id)
{
    int i;
    memset(c, 0, sizeof(*c));
    c->id = id;
    c->running = 1;
    c->speaker = -1;
    c->i = -1;
    for (i = 0; i < N_ACTORS; i++) look_target[i] = -1;
    switch (id) {
    case CS_INTRO: scene_intro(c); break;
    case CS_GARAGE: scene_garage(c); break;
    case CS_MARKET: scene_market(c); break;
    case CS_KIOSK: scene_kiosk(c); break;
    case CS_FAIL: scene_fail(c); break;
    case CS_KABAN: scene_kaban(c); break;
    default: break;
    }
    push(c, C_END);
}

static V3 head(const Ped *p) { return v3(p->pos.x, p->pos.y + 1.58f * LOOKS[p->who].height, p->pos.z); }

/* over-the-shoulder framing of a speaker, or a front shot when alone */
static void auto_shot(Cutscene *c, const CsIO *io, int speaker, int listener)
{
    const Ped *s = io->actors[speaker];
    V3 hs, eye, tgt;
    if (!s) return;
    hs = head(s);
    if (listener >= 0 && io->actors[listener]) {
        const Ped *l = io->actors[listener];
        V3 hl = head(l), d = v3_norm(v3(hl.x - hs.x, 0, hl.z - hs.z)), r = v3(-d.z, 0, d.x);
        float side = (c->line & 1) ? 1.0f : -1.0f;
        eye = v3(hl.x + d.x * 1.9f + r.x * side, hl.y + 0.02f, hl.z + d.z * 1.9f + r.z * side);
        tgt = v3(hs.x + r.x * side * 0.3f, hs.y - 0.12f, hs.z + r.z * side * 0.3f);
    } else {
        V3 f = yaw_fwd(s->yaw), r = yaw_right(s->yaw);
        eye = v3(hs.x + f.x * 2.2f + r.x * 0.5f, hs.y + 0.05f, hs.z + f.z * 2.2f + r.z * 0.5f);
        tgt = v3(hs.x, hs.y - 0.1f, hs.z);
    }
    c->eye0 = c->eye1 = eye;
    c->tgt0 = c->tgt1 = tgt;
    c->cam_t = c->cam_dur = 0.0f;
    c->has_cam = 1;
}

static int step_cmd(Cutscene *c, const CsIO *io)
{
    CsCmd *k = &c->cmd[c->i];
    Ped *a = (k->actor >= 0 && k->actor < N_ACTORS) ? io->actors[k->actor] : NULL;
    int i;
    switch (k->type) {
    case C_CAM:
        c->has_cam = 1;
        c->auto_cam = 0;
        c->eye0 = k->p; c->tgt0 = k->q; c->eye1 = k->r; c->tgt1 = k->s;
        c->cam_t = 0.0f;
        c->cam_dur = k->t;
        return 0;
    case C_AUTOCAM:
        c->auto_cam = 1;
        return 0;
    case C_PLACE:
        if (a) {
            a->pos = k->p;
            a->yaw = k->t;
            a->vel = v3(0, 0, 0);
            a->has_goal = 0;
            a->active = 1;
        }
        return 0;
    case C_WALK:
        if (a) {
            a->goal = k->p;
            a->has_goal = 1;
            a->walk_speed = 1.35f;
        }
        c->wait = k->gest ? 30.0f : 0.0f;
        return k->gest;
    case C_FACE:
        if (a && k->other >= 0 && io->actors[k->other]) {
            a->goal_yaw = yaw_to(a->pos, io->actors[k->other]->pos);
            a->face_goal_yaw = 1;
        }
        return 0;
    case C_GEST:
        if (a) a->anim.gesture = k->gest;
        return 0;
    case C_SAY:
        snprintf(c->voice_id, sizeof(c->voice_id), "%s_%02d", SCENE_NAMES[c->id], ++c->line);
        c->sub_who = cs_speaker_name(k->actor);
        c->sub_text = k->text;
        c->sub_t = 0.0f;
        c->speaker = k->actor;
        c->min_say = 0.6f;
        if (a) a->anim.gesture = k->gest;
        if (k->other >= 0 && io->actors[k->other]) {
            int g = io->actors[k->other]->anim.gesture;
            if (g == GEST_WAVE || g == GEST_POINT || g == GEST_EXPLAIN || g == GEST_SHRUG)
                io->actors[k->other]->anim.gesture = GEST_NONE;
        }
        if (io->say) io->say(k->actor, k->text, c->voice_id);
        for (i = 0; i < N_ACTORS; i++) look_target[i] = k->actor;
        look_target[k->actor] = k->other;
        if (a && k->other >= 0 && io->actors[k->other] && !a->has_goal) {
            a->goal_yaw = yaw_to(a->pos, io->actors[k->other]->pos);
            a->face_goal_yaw = 1;
        }
        if (c->auto_cam || !c->has_cam) auto_shot(c, io, k->actor, k->other);
        return 1;
    case C_WAIT:
        c->wait = k->t;
        return 1;
    case C_FADE:
        c->fade_from = c->fade;
        c->fade_to = k->gest ? 1.0f : 0.0f;
        c->fade_t = 0.0f;
        c->fade_dur = k->t;
        c->wait = k->t;
        return 1;
    case C_TITLE:
        c->title = k->text;
        c->title2 = k->text2;
        c->title_t = k->t;
        c->wait = k->t * 0.6f;
        return 1;
    case C_REQ:
        c->requests |= k->gest;
        return 0;
    default:
        c->running = 0;
        return 1;
    }
}

static int blocked(Cutscene *c, const CsIO *io, int advance)
{
    CsCmd *k = &c->cmd[c->i];
    if (!c->running) return 0;
    switch (k->type) {
    case C_SAY: {
        int talking = io->speaking ? io->speaking() : 0;
        if (advance && c->sub_t > 0.25f) {
            if (io->say) io->say(-1, NULL, NULL);
            return 0;
        }
        return talking || c->sub_t < c->min_say;
    }
    case C_WALK: {
        Ped *a = io->actors[k->actor];
        return a && a->has_goal && c->wait > 0.0f;
    }
    case C_WAIT:
    case C_FADE:
    case C_TITLE:
        if (advance && k->type == C_WAIT) return 0;
        return c->wait > 0.0f;
    default:
        return 0;
    }
}

void cs_update(Cutscene *c, const CsIO *io, float dt, int advance, int skip)
{
    int i, guard = 0;
    c->requests = 0;
    if (!c->running) return;
    c->t += dt;
    c->cam_t += dt;
    c->sub_t += dt;
    if (c->wait > 0.0f) c->wait -= dt;
    if (c->title_t > 0.0f) c->title_t -= dt;
    if (c->fade_dur > 0.0f) {
        c->fade_t += dt;
        c->fade = lerpf(c->fade_from, c->fade_to, clampf(c->fade_t / c->fade_dur, 0.0f, 1.0f));
    }

    if (skip) {
        /* jump to the end, keeping the scene's lasting effects */
        if (c->i < 0) c->i = 0;
        for (; c->i < c->n; c->i++) {
            CsCmd *k = &c->cmd[c->i];
            Ped *a = (k->actor >= 0 && k->actor < N_ACTORS) ? io->actors[k->actor] : NULL;
            if (k->type == C_PLACE && a) { a->pos = k->p; a->yaw = k->t; a->has_goal = 0; }
            if (k->type == C_WALK && a) { a->pos = k->p; a->has_goal = 0; }
            if (k->type == C_REQ) c->requests |= k->gest;
        }
        if (io->say) io->say(-1, NULL, NULL);
        c->running = 0;
        c->fade = 0.0f;
    }

    while (c->running && guard++ < 64) {
        if (c->i >= 0 && blocked(c, io, advance)) break;
        advance = 0;
        if (c->i >= 0 && c->cmd[c->i].type == C_SAY) {
            c->sub_text = NULL;
            c->speaker = -1;
        }
        c->i++;
        if (c->i >= c->n) {
            c->running = 0;
            break;
        }
        step_cmd(c, io);
    }

    /* actors look at whoever matters right now */
    for (i = 0; i < N_ACTORS; i++) {
        Ped *a = io->actors[i];
        if (!a) continue;
        if (look_target[i] >= 0 && io->actors[look_target[i]]) {
            a->has_look_at = 1;
            a->look_at = head(io->actors[look_target[i]]);
        } else {
            a->has_look_at = 0;
        }
        if (i == c->speaker) a->anim.talk = 1.0f;
    }
    if (!c->running)
        for (i = 0; i < N_ACTORS; i++)
            if (io->actors[i]) {
                io->actors[i]->anim.gesture = GEST_NONE;
                io->actors[i]->has_look_at = 0;
            }
}

void cs_camera(const Cutscene *c, const CsIO *io, V3 *eye, V3 *target)
{
    float t = c->cam_dur > 0.0f ? smooth01(c->cam_t / c->cam_dur) : 1.0f;
    (void)io;
    *eye = v3_lerp(c->eye0, c->eye1, t);
    *target = v3_lerp(c->tgt0, c->tgt1, t);
}
