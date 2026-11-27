/* 2D screens: splash, menus, cutscene subtitles, in-game HUD. */
#include "game.h"
#include "render.h"
#include "texgen.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#define WHITE 0xFFFFFFFFu
#define GREY 0xFFA0A0A0u
#define YELLOW 0xFF30D8FFu
#define BLUE_UA 0xFFB75700u
#define YELLOW_UA 0xFF00D7FFu

/* ----------------------------------------------------------------- helpers */

static void text_center(float y, int scale, unsigned int color, const char *s)
{
    r_text((SCR_W - r_text_width(scale, s)) * 0.5f, y, scale, color, s);
}

static void utf8_prefix(char *dst, size_t cap, const char *src, int n)
{
    const char *p = src;
    size_t len;
    while (*p && n-- > 0) utf8_next(&p);
    len = (size_t)(p - src);
    if (len >= cap) len = cap - 1;
    memcpy(dst, src, len);
    dst[len] = 0;
}

static int wrap(const char *text, int width, char lines[][160], int max_lines)
{
    int n = 0;
    const char *p = text;
    while (*p && n < max_lines) {
        const char *start = p, *last_space = NULL, *q = p;
        int count = 0;
        while (*q && count < width) {
            if (*q == ' ') last_space = q;
            utf8_next(&q);
            count++;
        }
        if (*q && last_space && last_space > start) q = last_space;
        {
            size_t len = (size_t)(q - start);
            if (len > 159) len = 159;
            memcpy(lines[n], start, len);
            lines[n][len] = 0;
        }
        n++;
        p = q;
        while (*p == ' ') p++;
    }
    return n;
}

static void fmt_money(int v, char *out, size_t cap)
{
    char digits[16];
    int n, i, o = 0;
    snprintf(digits, sizeof(digits), "%d", v < 0 ? 0 : v);
    n = (int)strlen(digits);
    for (i = 0; i < n && o < (int)cap - 2; i++) {
        if (i > 0 && (n - i) % 3 == 0) out[o++] = ' ';
        out[o++] = digits[i];
    }
    out[o] = 0;
}

static void overlay(unsigned int color) { r_rect(0, 0, SCR_W, SCR_H, color); }

static void logo(float x, float y, int scale)
{
    r_text(x, y, scale, YELLOW_UA, "UKRAINE");
    r_text(x + 7 * 6 * scale, y, scale, 0xFFFFC060u, "CRAFT");
}

/* ----------------------------------------------------------------- screens */

void hud_splash(float t)
{
    float fade = t < 1.0f ? 1.0f - t : 0.0f;
    int w = r_text_width(5, "UKRAINECRAFT");
    r_begin_2d();
    r_gradient(0, 0, SCR_W, SCR_H / 2, 0xFFD06A10u, BLUE_UA);
    r_gradient(0, SCR_H / 2, SCR_W, SCR_H / 2, YELLOW_UA, 0xFF00B0E0u);
    r_rect(0, 86, SCR_W, 92, 0xA0000000u);
    logo((SCR_W - w) * 0.5f, 104, 5);
    text_center(148, 2, WHITE, "Соняшниківськ • 1996");
    if (t > 1.5f && fmodf(t, 1.0f) < 0.65f) text_center(214, 2, 0xFF202020u, "Натисни ✕");
    text_center(SCR_H - 14, 1, 0xFF404040u, "Оригінальна гра для PSP. Усі персонажі вигадані.");
    if (fade > 0.0f) overlay((unsigned int)(fade * 255.0f) << 24);
}

void hud_main_menu(int sel, int has_save)
{
    static const char *ITEMS[MENU_COUNT] = { "Продовжити", "Нова гра", "Налаштування", "Вихід" };
    int i;
    r_begin_2d();
    r_gradient(0, 0, 210, SCR_H, 0xD0000000u, 0xB0000000u);
    r_rect(0, 0, 4, SCR_H / 2, BLUE_UA);
    r_rect(0, SCR_H / 2, 4, SCR_H / 2, YELLOW_UA);
    logo(16, 22, 3);
    r_text(16, 50, 1, GREY, "Соняшниківськ • 1996");
    for (i = 0; i < MENU_COUNT; i++) {
        int disabled = i == MENU_CONTINUE && !has_save;
        unsigned int col = disabled ? 0xFF606060u : (i == sel ? YELLOW : WHITE);
        float y = 100 + i * 28;
        if (i == sel) {
            r_rect(12, y - 5, 186, 24, 0x40FFFFFFu);
            r_arrow(22, y + 7, 1.5708f, 5, YELLOW);
        }
        r_text(34, y, 2, col, ITEMS[i]);
    }
    r_text(16, SCR_H - 26, 1, GREY, "↑↓ — вибір   ✕ — OK");
    r_text(16, SCR_H - 14, 1, 0xFF707070u, "v0.4");
}

void hud_confirm(const char *question, const char *yes, const char *no)
{
    char lines[4][160];
    int n = wrap(question, 34, lines, 4), i;
    r_begin_2d();
    overlay(0xA0000000u);
    r_rect(40, 70, SCR_W - 80, 132, 0xF0282420u);
    r_rect(40, 70, SCR_W - 80, 3, YELLOW_UA);
    for (i = 0; i < n; i++) text_center(88 + i * 20, 2, WHITE, lines[i]);
    text_center(170, 2, YELLOW, yes);
    text_center(186, 1, GREY, no);
}

void hud_settings(int sel, const Level *L)
{
    char buf[96];
    int i;
    r_begin_2d();
    overlay(0xB0000000u);
    text_center(16, 3, WHITE, "Налаштування");
    for (i = 0; i < SET_COUNT; i++) {
        unsigned int col = i == sel ? YELLOW : 0xFFE0E0E0u;
        switch (i) {
        case SET_RADIUS: snprintf(buf, sizeof(buf), "Дальність прорисовки: < %d м >", L->radius * 64); break;
        case SET_LOOK:   snprintf(buf, sizeof(buf), "Чутливість камери: < %d >", L->look_speed); break;
        case SET_MUSIC:  snprintf(buf, sizeof(buf), "Музика і радіо: < %d >", L->music_vol); break;
        case SET_SFX:    snprintf(buf, sizeof(buf), "Звуки: < %d >", L->sfx_vol); break;
        case SET_CYCLE:  snprintf(buf, sizeof(buf), "Зміна дня і ночі: %s", L->day_cycle ? "увімк" : "вимк"); break;
        case SET_TIME:   snprintf(buf, sizeof(buf), "Час доби: %s", (L->time > 0.02f && L->time < 0.48f) ? "день" : "ніч"); break;
        case SET_INFO:   snprintf(buf, sizeof(buf), "Інформація на екрані: %s", L->show_info ? "увімк" : "вимк"); break;
        default:         snprintf(buf, sizeof(buf), "Назад"); break;
        }
        text_center(52 + i * 22, 2, col, buf);
    }
    text_center(SCR_H - 14, 1, GREY, "↑↓ — вибір   ←→ — змінити   ✕ — OK   ○ — назад");
}

void hud_pause(int sel)
{
    static const char *ITEMS[PAUSE_COUNT] = { "Продовжити", "Налаштування", "Зберегти і вийти в меню" };
    int i;
    r_begin_2d();
    overlay(0xA0000000u);
    text_center(40, 4, WHITE, "Пауза");
    for (i = 0; i < PAUSE_COUNT; i++) text_center(104 + i * 30, 2, i == sel ? YELLOW : 0xFFE0E0E0u, ITEMS[i]);
    text_center(SCR_H - 16, 1, GREY, "↑↓ — вибір   ✕ — OK   START — назад");
}

void hud_loading(float progress, const char *what)
{
    int w = r_text_width(4, "UKRAINECRAFT");
    r_begin_2d();
    r_gradient(0, 0, SCR_W, SCR_H, 0xFF3A2010u, 0xFF100804u);
    logo((SCR_W - w) * 0.5f, 96, 4);
    text_center(150, 1, 0xFFC0C0C0u, what);
    r_rect(140, 166, 200, 6, 0xFF404040u);
    r_rect(140, 166, 200.0f * clampf(progress, 0, 1), 6, YELLOW_UA);
}

void hud_cutscene(const char *who, const char *text, float sub_t, float letterbox,
                  const char *title, const char *title2, float title_t, float fade)
{
    float bar = 30.0f * clampf(letterbox, 0.0f, 1.0f);
    r_begin_2d();
    if (bar > 0.5f) {
        r_rect(0, 0, SCR_W, bar, 0xFF000000u);
        r_rect(0, SCR_H - bar, SCR_W, bar, 0xFF000000u);
    }
    if (text) {
        char shown[512], lines[4][160];
        int n, i;
        utf8_prefix(shown, sizeof(shown), text, (int)(sub_t * 50.0f));
        n = wrap(shown, 66, lines, 4);
        for (i = 0; i < n; i++) {
            if (i == 0 && who && who[0]) {
                char line[200];
                snprintf(line, sizeof(line), "%s: %s", who, lines[0]);
                text_center(SCR_H - bar - 12 - (n - i) * 11, 1, i == 0 ? 0xFFF0F0F0u : WHITE, line);
                r_text((SCR_W - r_text_width(1, line)) * 0.5f, SCR_H - bar - 12 - (n - i) * 11, 1, YELLOW, who);
            } else {
                text_center(SCR_H - bar - 12 - (n - i) * 11, 1, 0xFFF0F0F0u, lines[i]);
            }
        }
    }
    if (title && title_t > 0.0f) {
        unsigned int a = (unsigned int)(clampf(title_t, 0.0f, 1.0f) * 255.0f);
        text_center(100, 3, (a << 24) | 0x00D7FF, title);
        if (title2) text_center(132, 2, (a << 24) | 0xFFFFFF, title2);
    }
    if (fade > 0.01f) overlay((unsigned int)(clampf(fade, 0, 1) * 255.0f) << 24);
    if (text) r_text(SCR_W - 64, SCR_H - bar + 10 > SCR_H - 10 ? SCR_H - 10 : SCR_H - bar + 10, 1, 0x80FFFFFFu, "START — пропуск");
}

void hud_play(const Level *L, const HudInfo *h)
{
    char buf[128], money[32];
    const Story *s = &L->story;
    r_begin_2d();

    if (h->crosshair) {
        r_rect(SCR_W / 2 - 1, SCR_H / 2 - 1, 2, 2, 0xC0FFFFFFu);
    }

    /* mission */
    r_rect(0, 0, 250, 30, 0x70000000u);
    r_text(6, 4, 1, YELLOW, story_title(s));
    {
        char lines[2][160];
        int n = wrap(story_objective(s), 40, lines, 2), i;
        for (i = 0; i < n; i++) r_text(6, 14 + i * 9, 1, WHITE, lines[i]);
    }

    /* money and timer */
    fmt_money(s->money, money, sizeof(money));
    snprintf(buf, sizeof(buf), "%s крб", money);
    r_text(SCR_W - 8 - r_text_width(2, buf), 6, 2, 0xFF70E070u, buf);
    if (s->timer >= 0.0f) {
        int sec = (int)s->timer;
        snprintf(buf, sizeof(buf), "%d:%02d", sec / 60, sec % 60);
        r_text(SCR_W - 8 - r_text_width(2, buf), 26, 2, s->timer < 20.0f ? 0xFF4040FFu : WHITE, buf);
    }

    if (h->marker) {
        r_rect(SCR_W / 2 - 18, 4, 36, 40, 0x60000000u);
        r_arrow(SCR_W / 2, 20, h->marker_angle, 9, YELLOW);
        snprintf(buf, sizeof(buf), "%d м", (int)h->marker_dist);
        r_text(SCR_W / 2 - r_text_width(1, buf) / 2, 34, 1, WHITE, buf);
    }

    if (h->in_car) {
        /* speedometer with gear and a rev bar */
        float rev = clampf((h->rpm - 800.0f) / 5400.0f, 0.0f, 1.0f);
        r_rect(SCR_W - 104, SCR_H - 40, 98, 34, 0x70000000u);
        snprintf(buf, sizeof(buf), "%d", (int)(h->speed_kmh + 0.5f));
        r_text(SCR_W - 98, SCR_H - 36, 3, WHITE, buf);
        r_text(SCR_W - 44, SCR_H - 30, 1, GREY, "км/год");
        snprintf(buf, sizeof(buf), "%c", h->gear < 0 ? 'R' : (char)('0' + h->gear));
        r_text(SCR_W - 20, SCR_H - 36, 1, YELLOW, buf);
        r_rect(SCR_W - 98, SCR_H - 12, 86, 3, 0xFF303030u);
        r_rect(SCR_W - 98, SCR_H - 12, 86 * rev, 3, rev > 0.85f ? 0xFF3040FFu : 0xFF40C0FFu);
    }
    if (h->hint) text_center(SCR_H - 16, 1, 0xFFE0E0E0u, h->hint);
    if (h->radio_timer > 0.0f && h->radio_text) {
        unsigned int a = (unsigned int)(clampf(h->radio_timer, 0, 1) * 255.0f);
        int w = r_text_width(1, h->radio_text);
        r_rect((SCR_W - w) / 2 - 6, 52, w + 12, 14, (a * 3 / 4) << 24);
        text_center(55, 1, (a << 24) | 0x30D8FF, h->radio_text);
    }
    if (h->banner_timer > 0.0f && h->banner) {
        unsigned int a = (unsigned int)(clampf(h->banner_timer, 0, 1) * 255.0f);
        char lines[3][160];
        int n = wrap(h->banner, 36, lines, 3), i;
        for (i = 0; i < n; i++) text_center(90 + i * 22, n > 1 ? 2 : 3, (a << 24) | 0x30D8FF, lines[i]);
        if (h->banner_sub) text_center(96 + n * 24, 2, (a << 24) | 0xFFFFFF, h->banner_sub);
    }
    if (L->show_info) {
        snprintf(buf, sizeof(buf), "FPS %d  XYZ %d %d %d", (int)(h->fps + 0.5f), h->px, h->py, h->pz);
        r_text(6, 40, 1, WHITE, buf);
        snprintf(buf, sizeof(buf), "%s  %02d:%02d", h->area ? h->area : "", ((int)(L->time * 24.0f) + 6) % 24,
                 (int)(L->time * 24.0f * 60.0f) % 60);
        r_text(6, 50, 1, WHITE, buf);
    }
}
