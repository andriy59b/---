#include "story.h"
#include "city.h"
#include "cutscene.h"

#include <string.h>

#define ARRIVE_FOOT 2.2f
#define ARRIVE_CAR 6.5f
#define PARCEL_TIME 150.0f

static int arrived(const StoryView *v, V3 poi)
{
    float dx = v->pos.x - poi.x, dz = v->pos.z - poi.z, r = v->in_car ? ARRIVE_CAR : ARRIVE_FOOT;
    return dx * dx + dz * dz < r * r;
}

void story_start(Story *s)
{
    memset(s, 0, sizeof(*s));
    s->mission = M_INTRO;
    s->timer = -1.0f;
    s->money = 300000;
}

const char *story_title(const Story *s)
{
    switch (s->mission) {
    case M_INTRO:
    case M_HOME:   return "Повернення";
    case M_MARKET: return "Ластівка";
    case M_PARCEL: return "Пакунок";
    case M_TAIL:   return "Хвіст";
    case M_KABAN:  return "Кабан";
    default:       return "Вільна гра";
    }
}

const char *story_objective(const Story *s)
{
    switch (s->mission) {
    case M_INTRO:
    case M_HOME:   return "Знайди брата Тараса в гаражах";
    case M_MARKET: return s->step == 0 ? "Сідай у Тарасову «Ластівку» (△)" : "Їдь на ринок до тітки Галі";
    case M_PARCEL: return "Відвези пакунок до ларька з пивом";
    case M_TAIL:   return "Відірвися від чорного «гелендера»";
    case M_KABAN:  return "Їдь до дев'ятиповерхівки на зустріч із Кабаном";
    default:       return "Розділ 1 завершено. Далі буде...";
    }
}

int story_marker(const Story *s, V3 *out)
{
    switch (s->mission) {
    case M_HOME:   *out = POI_GARAGE; return 1;
    case M_MARKET: *out = s->step == 0 ? POI_CAR : POI_MARKET; return 1;
    case M_PARCEL: *out = POI_KIOSK; return 1;
    case M_KABAN:  *out = POI_KABAN; return 1;
    default:       return 0;
    }
}

void story_update(Story *s, const StoryView *v, float dt, StoryOut *o)
{
    memset(o, 0, sizeof(*o));
    switch (s->mission) {
    case M_INTRO:
        if (s->step == 0) {
            s->step = 1;
            o->scene = CS_INTRO;
        }
        break;
    case M_HOME:
        if (arrived(v, POI_GARAGE)) o->scene = CS_GARAGE;
        break;
    case M_MARKET:
        if (s->step == 0 && v->in_own_car) s->step = 1;
        else if (s->step == 1 && arrived(v, POI_MARKET)) o->scene = CS_MARKET;
        break;
    case M_PARCEL:
        s->timer -= dt;
        if (arrived(v, POI_KIOSK)) {
            o->scene = CS_KIOSK;
        } else if (s->timer <= 0.0f) {
            s->timer = PARCEL_TIME;
            o->mission_failed = 1;
            o->scene = CS_FAIL;
        }
        break;
    case M_TAIL:
        if (!v->chaser_alive || v->chaser_dist > 90.0f) s->aux += dt;
        else s->aux = 0.0f;
        if (s->aux > 4.0f) {
            o->remove_chaser = 1;
            o->mission_passed = 1;
            o->pager = "ПЕЙДЖЕР: «ТАРАС: ПРИЇЖДЖАЙ ДО ДЕВ'ЯТИПОВЕРХІВКИ. КАБАН ХОЧЕ ГОВОРИТИ.»";
            o->kaban_setup = 1;
            s->mission = M_KABAN;
        }
        break;
    case M_KABAN:
        if (arrived(v, POI_KABAN)) o->scene = CS_KABAN;
        break;
    default:
        break;
    }
}

void story_scene_done(Story *s, int scene, StoryOut *o)
{
    memset(o, 0, sizeof(*o));
    switch (scene) {
    case CS_INTRO:
        s->mission = M_HOME;
        s->step = 0;
        break;
    case CS_GARAGE:
        s->mission = M_MARKET;
        s->step = 0;
        o->mission_passed = 1;
        break;
    case CS_MARKET:
        s->mission = M_PARCEL;
        s->timer = PARCEL_TIME;
        o->mission_passed = 1;
        break;
    case CS_KIOSK:
        s->mission = M_TAIL;
        s->timer = -1.0f;
        s->aux = 0.0f;
        s->money += 500000;
        o->mission_passed = 1;
        break;
    case CS_KABAN:
        s->mission = M_FREE;
        s->money += 200000;
        o->mission_passed = 1;
        break;
    default:
        break;
    }
}
