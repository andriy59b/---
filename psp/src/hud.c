/* 2D screens: splash, menus, intro, dialogue, in-game HUD. */
#include "game.h"
#include "render.h"
#include "textures.h"

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

/* Copies at most n code points of src into dst. */
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

static int cp_count(const char *s)
{
    int n = 0;
    while (*s) {
        utf8_next(&s);
        n++;
    }
    return n;
}

/* Word-wraps text to lines of at most `width` code points. Returns line count. */
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

static void overlay(unsigned int color)
{
    r_rect(0, 0, SCR_W, SCR_H, color);
}

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
    r_block_icon(10, 106, 52, B_SUNFLOWER);
    r_block_icon(SCR_W - 62, 106, 52, B_SUNFLOWER);
    logo((SCR_W - w) * 0.5f, 104, 5);
    text_center(148, 2, WHITE, "Соняшниківськ • 1996");
    if (t > 1.5f && fmodf(t, 1.0f) < 0.65f) text_center(214, 2, 0xFF202020u, "Натисни ✕");
    text_center(SCR_H - 14, 1, 0xFF404040u, "Оригінальна гра для PSP");
    if (fade > 0.0f) overlay((unsigned int)(fade * 255.0f) << 24);
}

void hud_main_menu(int sel, int has_save, float t)
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
    r_text(16, SCR_H - 14, 1, 0xFF707070u, "v0.3");
    (void)t;
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
    text_center(20, 3, WHITE, "Налаштування");
    for (i = 0; i < SET_COUNT; i++) {
        unsigned int col = i == sel ? YELLOW : 0xFFE0E0E0u;
        switch (i) {
        case SET_RADIUS: snprintf(buf, sizeof(buf), "Дальність прорисовки: < %d >", L->radius); break;
        case SET_LOOK:   snprintf(buf, sizeof(buf), "Чутливість камери: < %d >", L->look_speed); break;
        case SET_MUSIC:  snprintf(buf, sizeof(buf), "Музика: < %d >", L->music_vol); break;
        case SET_SFX:    snprintf(buf, sizeof(buf), "Звуки: < %d >", L->sfx_vol); break;
        case SET_CYCLE:  snprintf(buf, sizeof(buf), "Зміна дня і ночі: %s", L->day_cycle ? "увімк" : "вимк"); break;
        case SET_TIME:   snprintf(buf, sizeof(buf), "Час доби: %s", (L->time > 0.02f && L->time < 0.48f) ? "день" : "ніч"); break;
        case SET_INFO:   snprintf(buf, sizeof(buf), "Інформація на екрані: %s", L->show_info ? "увімк" : "вимк"); break;
        default:         snprintf(buf, sizeof(buf), "Назад"); break;
        }
        text_center(62 + i * 22, 2, col, buf);
    }
    text_center(SCR_H - 16, 1, GREY, "↑↓ — вибір   ←→ — змінити   ✕ — OK   ○ — назад");
}

void hud_pause(int sel)
{
    static const char *ITEMS[PAUSE_COUNT] = { "Продовжити", "Блоки", "Налаштування", "Зберегти і вийти в меню" };
    int i;
    r_begin_2d();
    overlay(0xA0000000u);
    text_center(40, 4, WHITE, "Пауза");
    for (i = 0; i < PAUSE_COUNT; i++)
        text_center(100 + i * 28, 2, i == sel ? YELLOW : 0xFFE0E0E0u, ITEMS[i]);
    text_center(SCR_H - 16, 1, GREY, "↑↓ — вибір   ✕ — OK   START — назад");
}

static void hotbar(const Level *L)
{
    const float slot = 26.0f, gap = 2.0f;
    float total = HOTBAR_N * slot + (HOTBAR_N - 1) * gap;
    float x0 = (SCR_W - total) * 0.5f, y0 = SCR_H - slot - 4.0f;
    int i;
    for (i = 0; i < HOTBAR_N; i++) {
        float x = x0 + i * (slot + gap);
        if (i == L->sel) r_rect(x - 2, y0 - 2, slot + 4, slot + 4, WHITE);
        r_rect(x, y0, slot, slot, 0xB0201C18u);
        r_block_icon(x + 3, y0 + 3, slot - 6, L->hotbar[i]);
    }
}

void hud_inventory(int cursor, const Level *L)
{
    const float cell = 26.0f;
    int n = B_COUNT - 1, rows = (n + INV_COLS - 1) / INV_COLS, i;
    float w = INV_COLS * cell + 16, h = rows * cell + 40;
    float x0 = (SCR_W - w) * 0.5f, y0 = 8.0f;
    r_begin_2d();
    overlay(0x80000000u);
    r_rect(x0, y0, w, h, 0xE0302A26u);
    r_text(x0 + 8, y0 + 7, 2, WHITE, "Блоки");
    for (i = 0; i < n; i++) {
        float x = x0 + 8 + (i % INV_COLS) * cell, y = y0 + 28 + (i / INV_COLS) * cell;
        if (i == cursor) r_rect(x, y, cell - 2, cell - 2, WHITE);
        r_rect(x + 1, y + 1, cell - 4, cell - 4, 0xFF4A443Eu);
        r_block_icon(x + 4, y + 4, cell - 10, i + 1);
    }
    text_center(y0 + h + 4, 2, WHITE, BLOCKS[cursor + 1].name);
    hotbar(L);
    text_center(y0 + h + 24, 1, GREY, "✕ — у слот   L/R — слот   ○ — закрити");
}

void hud_intro(int slide, float t)
{
    char shown[512], lines[8][160], year[24];
    int n, i;
    r_begin_2d();
    overlay(0x90000000u);
    r_rect(0, 0, SCR_W, 34, 0xFF000000u);
    r_rect(0, SCR_H - 34, SCR_W, 34, 0xFF000000u);
    snprintf(year, sizeof(year), "%d/%d", slide + 1, STORY_INTRO_SLIDES);
    r_text(12, 12, 2, YELLOW_UA, "1996");
    r_text(SCR_W - 12 - r_text_width(1, year), 16, 1, GREY, year);
    utf8_prefix(shown, sizeof(shown), STORY_INTRO[slide], (int)(t * 35.0f));
    n = wrap(shown, 34, lines, 8);
    for (i = 0; i < n; i++) text_center(SCR_H / 2 - n * 10 + i * 20, 2, WHITE, lines[i]);
    if (fmodf(t, 1.0f) < 0.7f) r_text(SCR_W - 12 - r_text_width(1, "✕ — далі"), SCR_H - 22, 1, GREY, "✕ — далі");
}

void hud_dialog(const char *speaker, const char *text, float t)
{
    char shown[512], lines[6][160];
    int n, i, scale = cp_count(text) > 100 ? 1 : 2;
    r_begin_2d();
    r_rect(0, 0, SCR_W, 22, 0xFF000000u);
    r_rect(10, 176, SCR_W - 20, 88, 0xD8101010u);
    r_rect(10, 176, SCR_W - 20, 2, YELLOW_UA);
    if (speaker && speaker[0]) r_text(20, 184, 2, YELLOW, speaker);
    utf8_prefix(shown, sizeof(shown), text, (int)(t * 45.0f));
    n = wrap(shown, scale == 2 ? 36 : 72, lines, 6);
    for (i = 0; i < n; i++) r_text(20, 206 + i * (scale == 2 ? 18 : 11), scale, WHITE, lines[i]);
    if (fmodf(t, 1.0f) < 0.7f) r_text(SCR_W - 30, 250, 1, GREY, "✕");
}

void hud_loading(int done, int total, const char *what)
{
    char buf[96];
    int w = r_text_width(4, "UKRAINECRAFT");
    r_begin_2d();
    r_gradient(0, 0, SCR_W, SCR_H, 0xFF3A2010u, 0xFF100804u);
    logo((SCR_W - w) * 0.5f, 96, 4);
    snprintf(buf, sizeof(buf), "%s %d%%", what, total ? done * 100 / total : 0);
    text_center(150, 1, 0xFFC0C0C0u, buf);
    r_rect(140, 166, 200, 6, 0xFF404040u);
    r_rect(140, 166, total ? 200.0f * done / total : 0, 6, YELLOW_UA);
}

void hud_play(const Level *L, const HudInfo *h)
{
    char buf[128], money[32];
    const Story *s = &L->story;
    r_begin_2d();

    if (h->underwater) overlay(0x60702810u);

    if (h->crosshair) {
        r_rect(SCR_W / 2 - 7, SCR_H / 2 - 1, 14, 2, 0xC0FFFFFFu);
        r_rect(SCR_W / 2 - 1, SCR_H / 2 - 7, 2, 14, 0xC0FFFFFFu);
    }
    if (h->hotbar) hotbar(L);
    if (h->hotbar && h->name_timer > 0.0f) {
        unsigned int a = (unsigned int)((h->name_timer > 1.0f ? 1.0f : h->name_timer) * 255.0f);
        text_center(SCR_H - 46, 1, (a << 24) | 0xFFFFFF, BLOCKS[L->hotbar[L->sel]].name);
    }

    /* mission */
    r_rect(0, 0, 250, s->mission < M_FREE ? 30 : 18, 0x70000000u);
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

    /* direction to the marker */
    if (h->marker) {
        r_rect(SCR_W / 2 - 18, 4, 36, 40, 0x60000000u);
        r_arrow(SCR_W / 2, 20, h->marker_angle, 9, YELLOW);
        snprintf(buf, sizeof(buf), "%d м", (int)h->marker_dist);
        r_text(SCR_W / 2 - r_text_width(1, buf) / 2, 34, 1, WHITE, buf);
    }

    if (h->in_car) {
        snprintf(buf, sizeof(buf), "%d км/год", (int)(h->speed_kmh + 0.5f));
        r_text(SCR_W - 8 - r_text_width(2, buf), SCR_H - 24, 2, WHITE, buf);
        if (h->can_exit) r_text(6, SCR_H - 12, 1, GREY, "✕ газ  □ гальмо  ↑ сигнал  ←→ радіо  ↓ вийти");
    } else if (h->near_car) {
        text_center(SCR_H - 60, 1, WHITE, "↓ — сісти в машину");
    }
    if (h->radio_timer > 0.0f && h->radio_text) {
        unsigned int a = (unsigned int)((h->radio_timer > 1.0f ? 1.0f : h->radio_timer) * 255.0f);
        int w = r_text_width(1, h->radio_text);
        r_rect((SCR_W - w) / 2 - 6, 52, w + 12, 14, (a * 3 / 4) << 24);
        text_center(55, 1, (a << 24) | 0x30D8FF, h->radio_text);
    }
    if (h->banner_timer > 0.0f && h->banner) {
        unsigned int a = (unsigned int)((h->banner_timer > 1.0f ? 1.0f : h->banner_timer) * 255.0f);
        text_center(96, 3, (a << 24) | 0x30D8FF, h->banner);
        if (h->banner_sub) text_center(124, 2, (a << 24) | 0xFFFFFF, h->banner_sub);
    }
    if (L->flying) r_text(SCR_W - 40, 46, 1, 0xFFA0E0FFu, "ПОЛІТ");

    if (L->show_info) {
        snprintf(buf, sizeof(buf), "FPS %d  XYZ %d %d %d", (int)(h->fps + 0.5f), h->px, h->py, h->pz);
        r_text(6, 40, 1, WHITE, buf);
        snprintf(buf, sizeof(buf), "%s  %02d:%02d", h->biome ? h->biome : "",
                 ((int)(L->time * 24.0f) + 6) % 24, (int)(L->time * 24.0f * 60.0f) % 60);
        r_text(6, 50, 1, WHITE, buf);
        if (h->has_hit) r_text(6, 60, 1, 0xFFD0D0D0u, BLOCKS[h->hit_block].name);
    }
}
