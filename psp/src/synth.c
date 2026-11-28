/* All music here is original, written for this game in the spirit of 90s
 * eurodance, estrada, a street waltz and garage rock. */
#include "synth.h"

#include <math.h>
#include <string.h>

#define NCH 4          /* lead, harmony, bass, drums */
#define MAX_STEPS 256
#define HOLD (-2)
#define OFF (-1)
#define MAX_SFX 6
#define PI2 6.2831853f

typedef struct {
    const char *name;
    int bpm;
    int steps_per_beat;
    float lead_duty;
    const char *ch[NCH];
} Song;

static const Song SONGS[MUS_COUNT] = {
    { "Соняшниківськ", 96, 4, 0.25f, {
        /* lead */
        "E5 - D5 - C5 - B4 - A4 - - - C5 - E5 - "
        "F5 - E5 - D5 - C5 - A4 - - - . . A4 - "
        "G4 - C5 - E5 - G5 - E5 - D5 - C5 - - - "
        "B4 - D5 - G5 - F5 - D5 - - - B4 - . . "
        "A5 - G5 - E5 - C5 - D5 - E5 - A4 - - - "
        "F5 - - - A5 - G5 - F5 - E5 - D5 - C5 - "
        "D5 - F5 - A5 - F5 - E5 - D5 - C5 - B4 - "
        "B4 - - - E5 - - - G#4 - - - . . . . ",
        /* harmony: arpeggios */
        "A3 C4 E4 C4 A3 C4 E4 C4 A3 C4 E4 C4 A3 C4 E4 C4 "
        "F3 A3 C4 A3 F3 A3 C4 A3 F3 A3 C4 A3 F3 A3 C4 A3 "
        "C4 E4 G4 E4 C4 E4 G4 E4 C4 E4 G4 E4 C4 E4 G4 E4 "
        "G3 B3 D4 B3 G3 B3 D4 B3 G3 B3 D4 B3 G3 B3 D4 B3 "
        "A3 C4 E4 C4 A3 C4 E4 C4 A3 C4 E4 C4 A3 C4 E4 C4 "
        "F3 A3 C4 A3 F3 A3 C4 A3 F3 A3 C4 A3 F3 A3 C4 A3 "
        "D4 F4 A4 F4 D4 F4 A4 F4 D4 F4 A4 F4 D4 F4 A4 F4 "
        "E3 G#3 B3 G#3 E3 G#3 B3 G#3 E3 G#3 B3 G#3 E3 G#3 B3 G#3 ",
        /* bass */
        "A2 - - - A2 - - - A3 - - - E3 - - - "
        "F2 - - - F2 - - - F3 - - - C3 - - - "
        "C3 - - - C3 - - - C4 - - - G2 - - - "
        "G2 - - - G2 - - - G3 - - - D3 - - - "
        "A2 - - - A2 - - - A3 - - - E3 - - - "
        "F2 - - - F2 - - - F3 - - - C3 - - - "
        "D3 - - - D3 - - - D4 - - - A2 - - - "
        "E2 - - - E2 - - - B2 - - - E3 - - - ",
        /* drums */
        "k - h - s - h - k - h - s - h h ",
    } },
    { "Дискотека-96", 132, 4, 0.5f, {
        "A4 - A4 - C5 - A4 - E5 - - - D5 - C5 - "
        "F4 - F4 - A4 - F4 - C5 - - - B4 - A4 - "
        "G4 - G4 - C5 - G4 - E5 - - - D5 - C5 - "
        "B4 - B4 - D5 - B4 - G5 - - - F5 - D5 - "
        "E5 - E5 - E5 - D5 - C5 - - - B4 - A4 - "
        "C5 - C5 - C5 - B4 - A4 - - - G4 - F4 - "
        "E5 - E5 - G5 - E5 - D5 - C5 - D5 - E5 - "
        "D5 - - - B4 - - - G4 - - - . - . - ",
        "A4 C5 E5 C5 A4 C5 E5 C5 A4 C5 E5 C5 A4 C5 E5 C5 "
        "F4 A4 C5 A4 F4 A4 C5 A4 F4 A4 C5 A4 F4 A4 C5 A4 "
        "C5 E5 G5 E5 C5 E5 G5 E5 C5 E5 G5 E5 C5 E5 G5 E5 "
        "G4 B4 D5 B4 G4 B4 D5 B4 G4 B4 D5 B4 G4 B4 D5 B4 ",
        "A2 - A3 - A2 - A3 - A2 - A3 - A2 - A3 - "
        "F2 - F3 - F2 - F3 - F2 - F3 - F2 - F3 - "
        "C3 - C4 - C3 - C4 - C3 - C4 - C3 - C4 - "
        "G2 - G3 - G2 - G3 - G2 - G3 - G2 - G3 - ",
        "k - h - x - h - k - h - x - h h ",
    } },
    { "Вальс на районі", 150, 4, 0.5f, {
        "A4 - - - D5 - - - F5 - E5 - "
        "E5 - - - C#5 - - - A4 - - - "
        "D5 - - - F5 - - - A5 - G5 - "
        "G5 - - - A#4 - - - D5 - - - "
        "F5 - - - E5 - - - D5 - - - "
        "C#5 - - - E5 - - - A4 - G4 - "
        "F4 - - - A4 - - - D5 - C#5 - "
        "D5 - - - - - - - . - - - ",
        ". - - - F4 - - - A4 - - - "
        ". - - - E4 - - - C#4 - - - "
        ". - - - F4 - - - A4 - - - "
        ". - - - A#3 - - - D4 - - - "
        ". - - - F4 - - - A4 - - - "
        ". - - - E4 - - - C#4 - - - "
        ". - - - F4 - - - A4 - - - "
        ". - - - F4 - - - A4 - - - ",
        "D3 - - - . - - - . - - - "
        "A2 - - - . - - - . - - - "
        "D3 - - - . - - - . - - - "
        "G2 - - - . - - - . - - - "
        "D3 - - - . - - - . - - - "
        "A2 - - - . - - - . - - - "
        "D3 - - - . - - - . - - - "
        "D3 - - - . - - - . - - - ",
        "k - - - h - - - h - - - ",
    } },
    { "Гаражний рок", 140, 4, 0.5f, {
        "E4 - - G4 - - A4 - B4 - A4 - G4 - E4 - "
        "E4 - - G4 - - A4 - G4 - E4 - D4 - E4 - "
        "D4 - - F#4 - - A4 - B4 - A4 - F#4 - D4 - "
        "B3 - - D#4 - - F#4 - B4 - A4 - F#4 - D#4 - "
        "B4 - B4 - A4 - G4 - A4 - B4 - E5 - - - "
        "E5 - D5 - C5 - B4 - A4 - G4 - E4 - - - "
        "F#4 - G4 - A4 - B4 - D5 - - - B4 - A4 - "
        "B4 - - - . - B4 - D#5 - - - B4 - . - ",
        "E3 - B3 - E3 - B3 - E3 - B3 - E3 - B3 - "
        "C3 - G3 - C3 - G3 - C3 - G3 - C3 - G3 - "
        "D3 - A3 - D3 - A3 - D3 - A3 - D3 - A3 - "
        "B2 - F#3 - B2 - F#3 - B2 - F#3 - B2 - F#3 - ",
        "E2 - E2 - E2 - E2 - E2 - E2 - G2 - A2 - "
        "C2 - C2 - C2 - C2 - C2 - C2 - D2 - E2 - "
        "D2 - D2 - D2 - D2 - D2 - D2 - E2 - F#2 - "
        "B1 - B1 - B1 - B1 - B1 - B1 - D#2 - F#2 - ",
        "k - h - s - h - k - k - s - s s ",
    } },
};

/* parsed songs */
static signed char notes[MUS_COUNT][NCH][MAX_STEPS];
static int lengths[MUS_COUNT][NCH];
static float note_freq[128];

typedef struct {
    float phase, freq, env, target_env;
    int gate;
} Voice;

typedef struct {
    int id;
    float t;
    float phase, phase2;
    float lp;
    int voice;          /* horn: which vehicle */
} Sfx;

static Voice voices[3];
static float kick_t = 10.0f, snare_t = 10.0f, hat_t = 10.0f, kick_phase;
static Sfx sfx[MAX_SFX];
static unsigned int noise_state = 0x12345u;
static float hat_prev;

static volatile int req_track = -1;
static int cur_track = -1, step, loops;
static float step_pos, samples_per_step;
static volatile int engine_on, engine_voice, horn_voice, siren_on;
static volatile float engine_rpm, engine_load;
static float engine_smooth, engine_load_s, engine_amp;
static volatile float vol_music = 0.8f, vol_sfx = 1.0f;

#define QSIZE 16
static volatile int q_head, q_tail;
static int queue[QSIZE];

/* ------------------------------------------------------------------ parse */

static int parse_note(const char *t, int len)
{
    static const int SEMI[7] = { 9, 11, 0, 2, 4, 5, 7 }; /* A B C D E F G */
    int n, i = 1, oct;
    if (t[0] < 'A' || t[0] > 'G') return OFF;
    n = SEMI[t[0] - 'A'];
    if (i < len && t[i] == '#') { n++; i++; }
    else if (i < len && t[i] == 'b') { n--; i++; }
    if (i >= len) return OFF;
    oct = t[i] - '0';
    n += 12 * (oct + 1);
    return n < 0 ? 0 : (n > 127 ? 127 : n);
}

static void parse_channel(int song, int ch)
{
    const char *p = SONGS[song].ch[ch];
    int n = 0;
    while (*p && n < MAX_STEPS) {
        const char *t;
        int len;
        while (*p == ' ') p++;
        if (!*p) break;
        t = p;
        while (*p && *p != ' ') p++;
        len = (int)(p - t);
        if (ch == 3) {
            /* drums: k kick, s snare, h hat, x kick+snare */
            notes[song][ch][n++] = (signed char)(t[0] == 'k' ? 1 : t[0] == 's' ? 2 : t[0] == 'h' ? 3 : t[0] == 'x' ? 4 : 0);
        } else if (t[0] == '-') {
            notes[song][ch][n++] = HOLD;
        } else if (t[0] == '.') {
            notes[song][ch][n++] = OFF;
        } else {
            notes[song][ch][n++] = (signed char)parse_note(t, len);
        }
    }
    lengths[song][ch] = n;
}

void synth_init(void)
{
    int s, c, i;
    for (i = 0; i < 128; i++) note_freq[i] = 440.0f * powf(2.0f, (i - 69) / 12.0f);
    for (s = 0; s < MUS_COUNT; s++)
        for (c = 0; c < NCH; c++) parse_channel(s, c);
    memset(voices, 0, sizeof(voices));
    memset(sfx, 0, sizeof(sfx));
    for (i = 0; i < MAX_SFX; i++) sfx[i].id = -1;
    cur_track = -1;
    req_track = -1;
}

int synth_track_steps(int track, int channel) { return lengths[track][channel]; }
const char *synth_track_name(int track) { return (track >= 0 && track < MUS_COUNT) ? SONGS[track].name : ""; }

/* ---------------------------------------------------------------- control */

void synth_music(int track) { req_track = track; }
int synth_current_track(void) { return req_track; }
int synth_music_loops(void) { return loops; }

void synth_sfx(int id)
{
    int next = (q_head + 1) % QSIZE;
    if (next == q_tail) return;
    queue[q_head] = id;
    q_head = next;
}

void synth_engine(int on, float rpm)
{
    engine_on = on;
    engine_rpm = rpm < 0.0f ? 0.0f : (rpm > 1.0f ? 1.0f : rpm);
}

void synth_engine_voice(int voice) { engine_voice = voice < 0 || voice >= ENG_COUNT ? 0 : voice; }
void synth_engine_load(float throttle) { engine_load = throttle < 0.0f ? 0.0f : (throttle > 1.0f ? 1.0f : throttle); }
void synth_siren(int on) { siren_on = on; }

void synth_horn(int voice)
{
    horn_voice = voice;
    synth_sfx(SFX_HORN);
}

void synth_volumes(float music, float sfx_vol)
{
    vol_music = music;
    vol_sfx = sfx_vol;
}

/* ----------------------------------------------------------------- render */

static float noise(void)
{
    noise_state = noise_state * 1664525u + 1013904223u;
    return (float)(int)(noise_state >> 9 & 0x7FFF) / 16384.0f - 1.0f;
}

static float pulse(float phase, float duty) { return phase < duty ? 0.7f : -0.7f; }

static float tri(float phase)
{
    return phase < 0.5f ? phase * 4.0f - 1.0f : 3.0f - phase * 4.0f;
}

static void start_track(int t)
{
    const Song *s;
    cur_track = t;
    step = 0;
    loops = 0;
    step_pos = 0.0f;
    memset(voices, 0, sizeof(voices));
    if (t < 0) return;
    s = &SONGS[t];
    samples_per_step = SYNTH_RATE * 60.0f / (s->bpm * s->steps_per_beat);
}

static void trigger_step(void)
{
    int c;
    for (c = 0; c < 3; c++) {
        int len = lengths[cur_track][c], n;
        if (!len) continue;
        n = notes[cur_track][c][step % len];
        if (n == HOLD) continue;
        if (n == OFF) {
            voices[c].gate = 0;
        } else {
            voices[c].gate = 1;
            voices[c].freq = note_freq[n];
            voices[c].target_env = 1.0f;
        }
    }
    {
        int len = lengths[cur_track][3];
        int d = len ? notes[cur_track][3][step % len] : 0;
        if (d == 1 || d == 4) { kick_t = 0.0f; kick_phase = 0.0f; }
        if (d == 2 || d == 4) snare_t = 0.0f;
        if (d == 3) hat_t = 0.0f;
    }
}

static float music_sample(void)
{
    static const float AMP[3] = { 0.17f, 0.07f, 0.26f };
    const float dt = 1.0f / SYNTH_RATE;
    float out = 0.0f;
    int c;

    step_pos += 1.0f;
    if (step_pos >= samples_per_step) {
        step_pos -= samples_per_step;
        step++;
        if (lengths[cur_track][0] && step % lengths[cur_track][0] == 0) loops++;
        trigger_step();
    }

    for (c = 0; c < 3; c++) {
        Voice *v = &voices[c];
        float s;
        if (v->gate) v->env += (0.55f - v->env) * 0.00012f + (v->target_env - v->env) * 0.01f;
        else v->env *= 0.9994f;
        v->target_env *= 0.99985f;
        if (v->env < 0.0005f && !v->gate) continue;
        v->phase += v->freq * dt;
        if (v->phase >= 1.0f) v->phase -= 1.0f;
        if (c == 0) s = pulse(v->phase, SONGS[cur_track].lead_duty);
        else if (c == 1) s = pulse(v->phase, 0.125f);
        else s = tri(v->phase);
        out += s * v->env * AMP[c];
    }

    if (kick_t < 0.3f) {
        float f = 45.0f + 100.0f * expf(-kick_t * 30.0f);
        kick_phase += f * dt;
        if (kick_phase >= 1.0f) kick_phase -= 1.0f;
        out += tri(kick_phase) * expf(-kick_t * 11.0f) * 0.5f;
        kick_t += dt;
    }
    if (snare_t < 0.25f) {
        out += noise() * expf(-snare_t * 22.0f) * 0.22f;
        snare_t += dt;
    }
    if (hat_t < 0.08f) {
        float n = noise();
        out += (n - hat_prev) * expf(-hat_t * 80.0f) * 0.07f;
        hat_prev = n;
        hat_t += dt;
    }
    return out;
}

static void start_sfx(int id)
{
    int i, slot = 0;
    float oldest = -1.0f;
    for (i = 0; i < MAX_SFX; i++) {
        if (sfx[i].id < 0) { slot = i; oldest = 1e9f; break; }
        if (sfx[i].t > oldest) { oldest = sfx[i].t; slot = i; }
    }
    memset(&sfx[slot], 0, sizeof(Sfx));
    sfx[slot].id = id;
    sfx[slot].voice = horn_voice;
}

static float arp_note(float t, const float *f, int n, float each, float *phase)
{
    int k = (int)(t / each);
    if (k >= n) k = n - 1;
    *phase += f[k] / SYNTH_RATE;
    if (*phase >= 1.0f) *phase -= 1.0f;
    return pulse(*phase, 0.5f);
}

static float sfx_sample(Sfx *s)
{
    static const float WIN[4] = { 523.25f, 659.25f, 783.99f, 1046.5f };
    static const float LOSE[4] = { 392.0f, 349.2f, 293.7f, 261.6f };
    const float dt = 1.0f / SYNTH_RATE;
    float t = s->t, out = 0.0f, len = 0.0f;
    switch (s->id) {
    case SFX_STEP:
        /* soft, low thud */
        len = 0.05f;
        s->lp += (noise() - s->lp) * 0.025f;
        out = s->lp * expf(-t * 90.0f) * 0.22f;
        break;
    case SFX_BREAK:
        len = 0.2f;
        s->lp += (noise() - s->lp) * 0.25f;
        out = s->lp * expf(-t * 18.0f) * 0.55f;
        break;
    case SFX_PLACE:
        len = 0.09f;
        s->phase += (300.0f - t * 1600.0f) * dt;
        if (s->phase >= 1.0f) s->phase -= 1.0f;
        out = tri(s->phase) * expf(-t * 35.0f) * 0.45f;
        break;
    case SFX_UI:
        len = 0.05f;
        s->phase += 880.0f * dt;
        if (s->phase >= 1.0f) s->phase -= 1.0f;
        out = pulse(s->phase, 0.5f) * 0.12f;
        break;
    case SFX_CONFIRM: {
        static const float F[2] = { 659.25f, 987.77f };
        len = 0.14f;
        out = arp_note(t, F, 2, 0.07f, &s->phase) * 0.13f;
        break;
    }
    case SFX_HORN: {
        /* two-tone car horn; the old off-roaders buzz on one note, the truck
         * blares low, the motorcycle beeps */
        float f1 = 349.2f, f2 = 440.0f, a = 0.1f;
        switch (s->voice) {
        case ENG_UAZ:     f1 = 410.0f; f2 = 0.0f; a = 0.14f; break;
        case ENG_TRUCK:   f1 = 180.0f; f2 = 227.0f; a = 0.16f; len = 0.7f; break;
        case ENG_2STROKE: f1 = 560.0f; f2 = 0.0f; a = 0.1f; break;
        case ENG_I6: case ENG_V8: case ENG_V8_BIG: f1 = 415.3f; f2 = 523.3f; break;
        default: break;
        }
        if (len == 0.0f) len = 0.45f;
        s->phase += f1 * dt;
        s->phase2 += f2 * dt;
        if (s->phase >= 1.0f) s->phase -= 1.0f;
        if (s->phase2 >= 1.0f) s->phase2 -= 1.0f;
        s->lp += ((pulse(s->phase, 0.4f) + (f2 > 0.0f ? pulse(s->phase2, 0.5f) : 0.0f)) - s->lp) * 0.35f;  /* takes the fizz off */
        out = s->lp * a * (t < 0.02f ? t / 0.02f : 1.0f);
        break;
    }
    case SFX_MISSION:
        len = 0.7f;
        out = arp_note(t, WIN, 4, 0.1f, &s->phase) * 0.15f * (t > 0.4f ? expf(-(t - 0.4f) * 8.0f) : 1.0f);
        break;
    case SFX_FAIL:
        len = 0.7f;
        out = arp_note(t, LOSE, 4, 0.15f, &s->phase) * 0.15f;
        break;
    case SFX_DOOR:
        len = 0.15f;
        s->lp += (noise() - s->lp) * 0.05f;
        s->phase += 70.0f * dt;
        if (s->phase >= 1.0f) s->phase -= 1.0f;
        out = (s->lp * 0.8f + tri(s->phase) * 0.5f) * expf(-t * 30.0f);
        break;
    case SFX_CRASH:
        len = 0.45f;
        s->lp += (noise() - s->lp) * 0.4f;
        out = s->lp * expf(-t * 9.0f) * 0.7f;
        break;
    default:
        s->id = -1;
        return 0.0f;
    }
    s->t += dt;
    if (s->t > len) s->id = -1;
    return out;
}

/* Engines: every firing of a cylinder is a short exhaust pulse that dies
 * away, a little stronger or weaker per cylinder (the lope of a V8, the
 * rasp of a tired four), through a resonant "exhaust" filter that opens
 * with revs and throttle; plus the cam beat and, on the old off-roaders, the
 * whine of the transfer box. */
typedef struct {
    float idle_hz, max_hz;   /* crankshaft revolutions per second */
    int fires;               /* firings per two revolutions */
    float decay;             /* how fast a pulse dies (1/s) */
    float body_hz, body_q;   /* exhaust resonance */
    float rasp, sub, whine, gain;
    float uneven[8];
} EngineVoice;

static const EngineVoice ENGINES[ENG_COUNT] = {
    /* idle  max   fires decay body  q     rasp  sub   whine gain */
    { 13.5f, 105.0f, 6, 260.0f, 170.0f, 1.0f, 0.15f, 0.05f, 0.0f, 0.85f, { 1, 0.96f, 1.03f, 0.98f, 1.02f, 0.97f, 1, 1 } },   /* I6 */
    { 14.0f, 100.0f, 4, 230.0f, 140.0f, 0.9f, 0.3f, 0.06f, 0.0f, 0.85f, { 1, 0.88f, 1.06f, 0.92f, 1, 1, 1, 1 } },           /* I4 */
    { 13.0f, 95.0f, 4, 180.0f, 110.0f, 0.8f, 0.45f, 0.12f, 0.0f, 0.9f, { 1, 0.8f, 1.12f, 0.86f, 1, 1, 1, 1 } },             /* raspy I4 */
    { 11.0f, 95.0f, 8, 210.0f, 95.0f, 1.3f, 0.2f, 0.35f, 0.0f, 0.95f, { 1, 0.7f, 1.12f, 0.78f, 1.04f, 0.74f, 1.16f, 0.82f } }, /* V8 */
    { 10.0f, 85.0f, 8, 170.0f, 75.0f, 1.5f, 0.22f, 0.45f, 0.0f, 1.0f, { 1, 0.66f, 1.15f, 0.75f, 1.06f, 0.7f, 1.2f, 0.8f } },  /* big V8 */
    { 12.0f, 70.0f, 4, 150.0f, 82.0f, 0.9f, 0.38f, 0.2f, 0.16f, 0.95f, { 1, 0.78f, 1.14f, 0.84f, 1, 1, 1, 1 } },            /* UAZ */
    { 9.0f, 52.0f, 8, 120.0f, 58.0f, 1.2f, 0.3f, 0.42f, 0.1f, 1.1f, { 1, 0.75f, 1.1f, 0.8f, 1.05f, 0.72f, 1.12f, 0.85f } },   /* truck */
    { 22.0f, 125.0f, 4, 520.0f, 360.0f, 2.6f, 0.5f, 0.0f, 0.0f, 0.75f, { 1, 0.92f, 1.04f, 0.95f, 1, 1, 1, 1 } },            /* two-stroke twin */
};

static float eng_fire_phase, eng_cam_phase, eng_whine_phase, eng_env, eng_low, eng_band, eng_hp_prev, eng_out_lp;
static int eng_cyl;

/* siren: a slow wail between two notes */
static float siren_phase, siren_lfo, siren_amp;

static float engine_sample(void)
{
    const float dt = 1.0f / SYNTH_RATE;
    const EngineVoice *v = &ENGINES[engine_voice];
    float target = engine_on ? v->gain * (0.12f + engine_smooth * 0.05f + engine_load_s * 0.05f) : 0.0f;
    float rev, fire, x, f, fc, out;
    engine_amp += (target - engine_amp) * 0.0005f;
    engine_smooth += (engine_rpm - engine_smooth) * 0.0003f;
    engine_load_s += (engine_load - engine_load_s) * 0.0004f;
    if (engine_amp < 0.0005f) return 0.0f;
    rev = v->idle_hz + (v->max_hz - v->idle_hz) * engine_smooth;
    fire = rev * (float)v->fires * 0.5f;
    eng_fire_phase += fire * dt;
    if (eng_fire_phase >= 1.0f) {
        eng_fire_phase -= 1.0f;
        eng_cyl = (eng_cyl + 1) % v->fires;
        eng_env = v->uneven[eng_cyl & 7] * (0.55f + 0.45f * engine_load_s) * (0.9f + 0.2f * (noise() * 0.5f + 0.5f));
    }
    x = eng_env * (1.0f - v->rasp + v->rasp * noise());
    eng_env -= eng_env * v->decay * dt;
    /* cam beat and gear whine */
    eng_cam_phase += rev * 0.5f * dt;
    if (eng_cam_phase >= 1.0f) eng_cam_phase -= 1.0f;
    x += v->sub * tri(eng_cam_phase) * 0.35f;
    /* resonant exhaust: a state-variable filter, opening with revs and load */
    fc = v->body_hz * (1.0f + 1.6f * engine_smooth) * (0.75f + 0.5f * engine_load_s);
    f = 2.0f * 3.14159f * fc * dt;
    if (f > 0.9f) f = 0.9f;
    {
        float hp = x - eng_low - eng_band / v->body_q;
        eng_band += f * hp;
        eng_low += f * eng_band;
        eng_hp_prev = hp;
    }
    out = eng_low * 0.8f + eng_band * 0.7f;
    if (v->whine > 0.0f) {
        eng_whine_phase += rev * 7.0f * dt;
        if (eng_whine_phase >= 1.0f) eng_whine_phase -= 1.0f;
        out += v->whine * tri(eng_whine_phase) * engine_smooth * 0.25f;
    }
    eng_out_lp += (out - eng_out_lp) * 0.6f;
    return eng_out_lp * engine_amp * 2.2f;
}

static float siren_sample(void)
{
    const float dt = 1.0f / SYNTH_RATE;
    float f, w;
    siren_amp += ((siren_on ? 0.11f : 0.0f) - siren_amp) * 0.0008f;
    if (siren_amp < 0.0005f) return 0.0f;
    siren_lfo += 0.32f * dt;
    if (siren_lfo >= 1.0f) siren_lfo -= 1.0f;
    w = tri(siren_lfo) * 0.5f + 0.5f;
    f = 620.0f + 720.0f * w * w;
    siren_phase += f * dt;
    if (siren_phase >= 1.0f) siren_phase -= 1.0f;
    return (tri(siren_phase) * 0.8f + pulse(siren_phase, 0.5f) * 0.2f) * siren_amp;
}

void synth_render(short *stereo, int frames)
{
    int i, k;
    float vm = vol_music, vs = vol_sfx;

    if (req_track != cur_track) start_track(req_track);
    while (q_tail != q_head) {
        start_sfx(queue[q_tail]);
        q_tail = (q_tail + 1) % QSIZE;
    }

    for (i = 0; i < frames; i++) {
        float m = cur_track >= 0 ? music_sample() * vm : 0.0f, e = 0.0f, mix;
        int v;
        for (k = 0; k < MAX_SFX; k++)
            if (sfx[k].id >= 0) e += sfx_sample(&sfx[k]);
        e += engine_sample() + siren_sample();
        mix = m + e * vs;
        /* soft clip */
        if (mix > 1.0f) mix = 1.0f;
        else if (mix < -1.0f) mix = -1.0f;
        else mix = mix * (1.5f - 0.5f * mix * mix);
        v = (int)(mix * 30000.0f);
        stereo[i * 2] = (short)v;
        stereo[i * 2 + 1] = (short)v;
    }
}
