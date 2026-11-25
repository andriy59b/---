#include "story.h"
#include "town.h"

#include <string.h>

const char *const STORY_INTRO[STORY_INTRO_SLIDES] = {
    "Літо 1996-го. Соняшниківськ.",
    "Андрій повернувся з армії. У кишені — 300 000 карбованців і дембельський альбом.",
    "Місто змінилося: на ринку продають усе, що можна продати, а районом заправляє якийсь Кабан.",
    "Зарплату на заводі не платять уже пів року. Кожен крутиться як може.",
    "Треба знайти старшого брата Тараса. Кажуть, він днює й ночує у своєму гаражі.",
};

#define ARRIVE_FOOT 3.0f
#define ARRIVE_CAR 7.0f
#define PARCEL_TIME 150.0f

static float dist2(const float a[3], const float b[3])
{
    float dx = a[0] - b[0], dz = a[2] - b[2];
    return dx * dx + dz * dz;
}

static int arrived(const StoryView *v, const float poi[3])
{
    float r = v->in_car ? ARRIVE_CAR : ARRIVE_FOOT;
    return dist2(v->pos, poi) < r * r;
}

static void say(StoryOut *o, const char *who, const char *text)
{
    if (o->lines >= STORY_MAX_LINES) return;
    o->speaker[o->lines] = who;
    o->text[o->lines] = text;
    o->lines++;
}

void story_start(Story *s)
{
    memset(s, 0, sizeof(*s));
    s->mission = M_HOME;
    s->timer = -1.0f;
    s->money = 300000;
}

const char *story_title(const Story *s)
{
    switch (s->mission) {
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
    case M_HOME:   return "Знайди брата Тараса в гаражах";
    case M_MARKET: return s->step == 0 ? "Сідай у Тарасову машину (↓)" : "Їдь на ринок до тітки Галі";
    case M_PARCEL: return "Відвези пакунок до ларька з пивом";
    case M_TAIL:   return "Відірвися від чорної машини";
    case M_KABAN:  return "Їдь до дев'ятиповерхівки на зустріч із Кабаном";
    default:       return "Розділ 1 завершено. Далі буде...";
    }
}

int story_marker(const Story *s, float out[3])
{
    const float *p = 0;
    switch (s->mission) {
    case M_HOME:   p = POI_GARAGE; break;
    case M_MARKET: p = s->step == 0 ? POI_CAR : POI_MARKET; break;
    case M_PARCEL: p = POI_KIOSK; break;
    case M_KABAN:  p = POI_KABAN; break;
    default:       return 0;
    }
    out[0] = p[0];
    out[1] = p[1];
    out[2] = p[2];
    return 1;
}

void story_update(Story *s, const StoryView *v, float dt, StoryOut *o)
{
    memset(o, 0, sizeof(*o));

    switch (s->mission) {
    case M_HOME:
        if (arrived(v, POI_GARAGE)) {
            say(o, "Тарас", "Андрюха! Живий! Ну ти й вимахав у тій армії.");
            say(o, "Андрій", "Привіт, брате. Що тут у вас робиться?");
            say(o, "Тарас", "Та що... Завод стоїть, грошей нема. Я ось машини ремонтую, тим і живу.");
            say(o, "Тарас", "Тітка Галя з ринку просила допомогти. Бери мою Ластівку — вона біля гаражів.");
            say(o, "Тарас", "Тільки обережно, вона в мене одна. І гальма... ну, ти зрозумієш.");
            o->spawn_own_car = 1;
            o->mission_passed = 1;
            s->mission = M_MARKET;
            s->step = 0;
        }
        break;

    case M_MARKET:
        if (s->step == 0 && v->in_own_car) {
            s->step = 1;
        } else if (s->step == 1 && arrived(v, POI_MARKET)) {
            say(o, "Тітка Галя", "О, Тарасів братик! Ти диви, який козак.");
            say(o, "Тітка Галя", "Слухай, синку, відвези пакунок до ларька з пивом за школою.");
            say(o, "Тітка Галя", "Тільки швиденько, там чекати не люблять. І не питай, що всередині.");
            say(o, "Андрій", "А що всередині?");
            say(o, "Тітка Галя", "Я ж сказала — не питай!");
            o->mission_passed = 1;
            s->mission = M_PARCEL;
            s->step = 0;
            s->timer = PARCEL_TIME;
        }
        break;

    case M_PARCEL:
        s->timer -= dt;
        if (arrived(v, POI_KIOSK)) {
            say(o, "Продавець", "Від Галі? Нарешті. Тримай 500 000 за доставку.");
            say(o, "Продавець", "І ще, хлопче... Тобою вже цікавляться люди Кабана. Он їхня чорна тачка.");
            say(o, "Андрій", "Ще чого не вистачало.");
            s->money += 500000;
            s->timer = -1.0f;
            o->spawn_chaser = 1;
            o->mission_passed = 1;
            s->mission = M_TAIL;
            s->aux = 0.0f;
        } else if (s->timer <= 0.0f) {
            say(o, "Тітка Галя", "Ех, синку, не встиг... Ну нічого, давай ще раз, тільки швидше!");
            o->mission_failed = 1;
            s->timer = PARCEL_TIME;
        }
        break;

    case M_TAIL:
        if (!v->chaser_alive || v->chaser_dist > 90.0f) s->aux += dt;
        else s->aux = 0.0f;
        if (s->aux > 4.0f) {
            say(o, "Андрій", "Фух... Здається, відірвався.");
            say(o, "Пейджер", "ТАРАС: ПРИЇЖДЖАЙ ДО ДЕВ'ЯТИПОВЕРХІВКИ. КАБАН ХОЧЕ ГОВОРИТИ.");
            o->remove_chaser = 1;
            o->mission_passed = 1;
            s->mission = M_KABAN;
        }
        break;

    case M_KABAN:
        if (arrived(v, POI_KABAN)) {
            say(o, "Кабан", "Так от ти який, солдатик. Швидко їздиш, мої хлопці аж засмутилися.");
            say(o, "Кабан", "Мені такі люди потрібні. Робота чиста, гроші — у доларах.");
            say(o, "Андрій", "Я подумаю.");
            say(o, "Кабан", "Думай. Тільки недовго. У цьому місті довго думати — шкідливо.");
            say(o, "", "КІНЕЦЬ ПЕРШОГО РОЗДІЛУ. ДАЛІ БУДЕ...");
            s->money += 200000;
            o->mission_passed = 1;
            s->mission = M_FREE;
        }
        break;

    default:
        break;
    }
}
